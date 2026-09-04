# ST7789 Driver + Software 3D Engine (ESP-IDF / C++, dual-core)

A from-scratch ST7789 display stack for ESP-IDF: a custom SPI/DMA driver, a hand-rolled GPIO driver that talks to ESP32 registers directly (no `driver/gpio.h`), and a small software 3D rendering pipeline that splits vertex transform and rasterization across both CPU cores.

This is a personal hobby/learning project. It runs and renders a spinning mesh on real hardware, but it has known rough edges — they're listed below on purpose, not hidden. Read "Known issues" before depending on this for anything real.

## What's in here

- **`st7789.cpp` / `st7789.h`** — SPI/DMA display driver: chunked framebuffer flush, mutex-guarded canvas, runtime-configurable resolution, an experimental partial-window mode (`sWindow`).
- **`GPIO_DRIVER.c` / `GPIO_DRIVER.h`** — a minimal GPIO driver written directly against ESP32 register addresses (`GPIO_OUT_REG`, `GPIO_ENABLE_REG`, etc.) instead of using ESP-IDF's `driver/gpio.h`. Built this way on purpose, mostly to understand what the IDF GPIO driver abstracts away.
- **`3dpipeline.cpp` / `3dpipeline.hpp`** — mesh loading from a simplified `.obj`-style text format, per-frame rotation, perspective projection, back-face culling, per-face diffuse lighting, and Painter's-algorithm depth sorting, rasterized through `ST7789::fillTriangle`.
- **`obj_to_c.py`** — a small Python script that strips a real `.obj` file down to its `v`/`f` lines and emits them as a `const char* obj[]` array (see `obj.h`) so it can be compiled straight into firmware.

## The dual-core pipeline — how it actually works

Vertex transform (rotation) runs on a task pinned to core 1. Rasterization (projection, lighting, sorting, triangle fill, SPI blit) runs on the calling task, normally core 0. The two sides hand off through two binary semaphores:

1. `PipeLine3D::RenderMesh()` waits for the *previous* transform to be done (`sem_cal_done`).
2. It reads the already-transformed vertices to build screen-space triangles for **this** frame.
3. It advances the rotation angles and immediately signals `sem_cal_run`, letting core 1 start transforming vertices for the **next** frame.
4. Only after that hand-off does it sort and rasterize the triangles for the current frame.

So while core 0 is sorting/filling triangles and pushing the frame over SPI, core 1 is already computing the next frame's rotated vertices. That's a real overlap, not a fake one — this is the actual mechanism behind the FPS improvement, and it's why the hand-off order in step 3 matters (the read in step 2 has to fully finish before the signal in step 3, or you'd get a data race on the shared vertex buffer).

## Usage Example

```cpp
#include "st7789.h"
#include "3dpipeline.hpp"
#include "obj.h"

extern "C" void app_main() {
    ST7789 lcd(23, 18, 16, 17);   // MOSI, CLK, DC, RST
    lcd.init();

    PipeLine3D engine(lcd);
    Mesh mesh = {};
    engine.ReadObj(obj, sizeof(obj) / sizeof(obj[0]), &mesh);

    core_math_init(&mesh);   // starts the core-1 transform task

    while (1) {
        lcd.fillScreen(BLACK);
        engine.RenderMesh(&mesh, 0.01f, 0.013f, 0.5f, 0, 20, RED);
        lcd.Render();
    }
}
```

Convert your own model:
```bash
python obj_to_c.py model.obj   # writes obj.h with a `v`/`f` string array
```
Only plain triangulated `v`/`f` lines are kept — no normals, UVs, materials, or quads. Triangulate and strip extras before running it.

## Known issues (real ones, not hedging)

- **`turn()` can `return` out of its own task loop on allocation failure.** If `heap_caps_realloc` fails inside the core-1 transform task, the code does `return;` from inside `while(1)`. In FreeRTOS, a task function is not allowed to return — it must call `vTaskDelete(NULL)` or loop forever. Returning here is undefined behavior on real hardware (typically an abort). This only triggers under memory pressure, but it's not handled correctly.
- **Same failure path leaks a dangling pointer.** On that same realloc failure, the code frees the *original* (still-valid) buffer via `heap_caps_free(transformed)` without also clearing `coresData.cur_transformed`, so the shared struct is left pointing at freed memory.
- **The GPIO driver only supports pins 0–31** (`my_gpio_set_level`/`my_gpio_set_direction`/etc. return early past pin 31) and its register addresses (`GPIO_BASE = 0x3FF44000`) are specific to the **original ESP32**. It will not work as-is on ESP32-S2/S3/C3/etc., which have different GPIO peripheral layouts.
- **`sWindow()` is still experimental**, as its own doxygen-style comment says — it reallocates the canvas to a sub-region and hasn't been exercised nearly as much as full-screen rendering.
- **No bounds checking against the font table's upper end in `print()`** — extended/non-ASCII bytes can index past the font array.
- **Mesh loading has partial OOM handling**: allocation failures during `ReadObj` correctly free what was allocated so far, but there's no way for the caller to distinguish "loaded fine" from "failed and mesh is now zeroed" other than checking `mesh->vertex_count == 0` afterward.
- **One `Mesh` is not safe to render from two call sites concurrently** — the transform buffer and hand-off semaphores are shared, single-instance state (`CoresData coresData;` is a global), not per-mesh. Multiple independent meshes rotating independently would need real per-instance state.
- **Projected screen coordinates aren't clamped** before rasterization — geometry very close to the camera can still produce large (but in-range `int`) coordinates that waste cycles in the scanline loop.
- **`getTimes()` divides `RenderMesh` timing by a hardcoded `30`** — that's a leftover from a specific profiling run, not a general FPS/frame-time calculation. Don't read too much into the printed number as-is.

## License

MIT. Provided as-is, without warranty — this is a personal project, not a maintained library.
