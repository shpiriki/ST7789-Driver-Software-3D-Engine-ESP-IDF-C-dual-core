#pragma once
#include "st7789.h"
#include <cmath>
#include <esp_timer.h>
#include <esp_log.h>
#include <freertos/semphr.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>
#define d 250.0f//fov
struct Vertex
{
    float x,y,z;
};
struct Face
{
    uint16_t a,b,c;
};
struct Triangle{
    int px_a, px_b, px_c,py_a,py_b,py_c;
    float z_view_a, z_view_b, z_view_c;
    float depth;
    uint16_t shadedColor;
};
struct Mesh
{
    Vertex* vertx;
    Face* faces;
    Triangle* triangle;
    int vertex_count;
    int faces_count;
    int vertex_capacity;
    int faces_capacity;
};
void core_math_deinit();
void core_math_init(Mesh* mesh);
void turn(void *pvParametres);
class PipeLine3D{
private:
    ST7789& st7789;
    Vertex* transformed;
    int transformed_capacity;
    //float d = 250.0f;
    int64_t* times = (int64_t*)heap_caps_malloc(sizeof(int64_t)*3, MALLOC_CAP_8BIT);
    void FreeMesh(Mesh* mesh);
public:
    PipeLine3D(ST7789& _st7789);
    void ReadObj(const char* obj[],int LINES, Mesh* mesh);
    void RenderMesh(Mesh* mesh,float dangle_x=0.0f, float dangle_y = 0.0f, float scale=1.0f, int x_offset = 0, int y_offset = 0, uint16_t color=WHITE);
    ~PipeLine3D();
    void getTimes();
};