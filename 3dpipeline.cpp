#include "3dpipeline.hpp"

SemaphoreHandle_t sem_cal_run = NULL;
SemaphoreHandle_t sem_cal_done = NULL;
TaskHandle_t taskhandle = NULL;
struct CoresData
{
Mesh* volatile cur_mesh = NULL;
float volatile cur_angle_x = 0.0f;
float volatile cur_angle_y = 0.0f;
Vertex* volatile cur_transformed = NULL;
int transformed_capacity;
};
CoresData coresData;
void core_math_deinit(){
    if(taskhandle!=NULL){
        vTaskDelete(taskhandle);
        taskhandle = NULL;
    }
}
void turn(void *pvParametres){
    while(1){
    xSemaphoreTake(sem_cal_run, portMAX_DELAY);
    Mesh* mesh = coresData.cur_mesh;
    float angle_x = coresData.cur_angle_x;
    float angle_y = coresData.cur_angle_y;
    Vertex* transformed = coresData.cur_transformed;
    int transformed_capacity = coresData.transformed_capacity;
    int64_t start = esp_timer_get_time();
    if(transformed_capacity<mesh->vertex_capacity){
        Vertex* newT = (Vertex*)heap_caps_realloc(transformed, mesh->vertex_capacity*sizeof(Vertex), MALLOC_CAP_8BIT);
        if(newT==NULL){
            heap_caps_free(transformed);
            return;
        }
        transformed = newT;
        transformed_capacity = mesh->vertex_capacity;
        coresData.cur_transformed = newT;
        coresData.transformed_capacity = mesh->vertex_capacity;
    }
    float cosx = cosf(angle_x);
    float cosy = cosf(angle_y);
    float sinx = sinf(angle_x);
    float siny = sinf(angle_y);
   for (int i = 0; i < mesh->vertex_count; i++) {
        float x = mesh->vertx[i].x;
        float y = mesh->vertx[i].y;
        float z = mesh->vertx[i].z;

        float x1 = x * cosy + z * siny;
        float z1 = -x * siny + z * cosy;
        float y1 = y;

        float y2 = y1 * cosx - z1 * sinx;
        float z2 = y1 *sinx + z1 * cosx;
        float x2 = x1;

        transformed[i].x = x2;
        transformed[i].y = y2;
        transformed[i].z = z2;
    }
    xSemaphoreGive(sem_cal_done);
    int64_t end = esp_timer_get_time();
    //times[1]= end - start;
    ESP_LOGI("TURN", "Time: %lld us", end - start);
}
}
void core_math_init(Mesh* mesh){
    sem_cal_done = xSemaphoreCreateBinary();
    sem_cal_run = xSemaphoreCreateBinary();
    coresData.cur_mesh = mesh;
    xTaskCreatePinnedToCore(turn, "turn", 8192,NULL, 5, &taskhandle, 1);
    xSemaphoreGive(sem_cal_run);
}
PipeLine3D::PipeLine3D(ST7789& _st7789): st7789(_st7789){
    transformed_capacity = 10;
    transformed = (Vertex*)heap_caps_malloc(transformed_capacity* sizeof(Vertex), MALLOC_CAP_8BIT);
    if(transformed==NULL){
        heap_caps_free(transformed);
        return;
    }
    coresData.cur_transformed = transformed;
    coresData.transformed_capacity = transformed_capacity;
}
PipeLine3D::~PipeLine3D(){
    heap_caps_free(transformed);

}
void PipeLine3D::ReadObj(const char* obj[],int  LINES, Mesh* mesh){
    int64_t start = esp_timer_get_time();
    mesh->vertex_capacity = 10;
    mesh->faces_capacity = 10;
    mesh->faces = (Face*)heap_caps_malloc(mesh->faces_capacity * sizeof(Face), MALLOC_CAP_8BIT);
    mesh->vertx = (Vertex*)heap_caps_malloc(mesh->vertex_capacity * sizeof(Vertex), MALLOC_CAP_8BIT);
    for(int i = 0; i < LINES; i++){
        if(obj[i][0] == '\0' || obj[i][0] == '#') continue;

        if(mesh->vertex_count == mesh->vertex_capacity){
            mesh->vertex_capacity *= 2;
            Vertex* newV = (Vertex*)heap_caps_realloc(mesh->vertx, mesh->vertex_capacity * sizeof(Vertex), MALLOC_CAP_8BIT);
            if(newV==NULL){
                FreeMesh(mesh);
                return;
            }
            mesh->vertx = newV;
        }

        if(mesh->faces_count == mesh->faces_capacity){
            mesh->faces_capacity *= 2;
            Face* newF = (Face*)heap_caps_realloc(mesh->faces, mesh->faces_capacity * sizeof(Face), MALLOC_CAP_8BIT);
            if(newF == NULL){
                FreeMesh(mesh);
                return;
            }
            mesh->faces = newF;
        }

        if(sscanf(obj[i], "v %f %f %f", &mesh->vertx[mesh->vertex_count].x, &mesh->vertx[mesh->vertex_count].y, &mesh->vertx[mesh->vertex_count].z) == 3){
            mesh->vertex_count++;
            continue;
        }
        int a, b, c;
        if(sscanf(obj[i], "f %d %d %d", &a, &b, &c) == 3){
        mesh->faces[mesh->faces_count].a = (uint16_t)(a - 1);
        mesh->faces[mesh->faces_count].b = (uint16_t)(b - 1);
        mesh->faces[mesh->faces_count].c = (uint16_t)(c - 1);
        mesh->faces_count++;
        continue;
        }
    }
    mesh->triangle = (Triangle*)heap_caps_malloc(mesh->faces_count*sizeof(Triangle), MALLOC_CAP_8BIT);
    mesh->faces = (Face*)heap_caps_realloc(mesh->faces,mesh->faces_count*sizeof(Face), MALLOC_CAP_8BIT);
    mesh->vertx = (Vertex*)heap_caps_realloc(mesh->vertx,mesh->vertex_count*sizeof(Vertex), MALLOC_CAP_8BIT);
    mesh->faces_capacity = mesh->faces_count;
    mesh->vertex_capacity = mesh->vertex_count;
    if(mesh->triangle==NULL || mesh->faces==NULL || mesh->vertx==NULL){
        FreeMesh(mesh);
        return;
    }
    int64_t end = esp_timer_get_time();
    times[0] = end-start;
}
void PipeLine3D:: FreeMesh(Mesh* mesh){
    if(mesh == nullptr)return;
    if(mesh->faces!=nullptr){heap_caps_free(mesh->faces);
    mesh->faces=nullptr;
    }
    if(mesh->triangle!=nullptr){heap_caps_free(mesh->triangle);
    mesh->triangle = nullptr;
    }
    if(mesh->vertx!=nullptr){heap_caps_free(mesh->vertx);
    mesh->vertx = nullptr;
    }
    mesh->faces_capacity=0;
    mesh->faces_count=0;
    mesh->vertex_capacity=0;
    mesh->vertex_count=0;
}

void PipeLine3D:: RenderMesh(Mesh* mesh,float dangle_x, float dangle_y,float scale, int x_offset, int y_offset, uint16_t color){
    if(mesh==nullptr)return;
    int64_t start = esp_timer_get_time();
    times[1] = 0;
    coresData.cur_mesh = mesh;
    xSemaphoreTake(sem_cal_done, portMAX_DELAY);
    transformed = coresData.cur_transformed;
    int visible_count = 0;
    float Lx = 0.6f, Ly = -0.4f, Lz = 0.7f;
    float lenL = sqrt(Lx*Lx + Ly*Ly + Lz*Lz);
    Lx /= lenL; Ly /= lenL; Lz /= lenL;
    uint16_t baseColor = color;
    for (int i = 0; i < mesh->faces_count; i++) {
            int a = mesh->faces[i].a;
            int b = mesh->faces[i].b;
            int c = mesh->faces[i].c;

            // 1. Считаем глубину Z для всех трех вершин (переиспользуем дальше)
            float z_view_a = transformed[a].z + 5.0f;
            float z_view_b = transformed[b].z + 5.0f;
            float z_view_c = transformed[c].z + 5.0f;

            // ЗАЩИТА: если полигон улетел за камеру, полностью пропускаем его
            if (z_view_a < 0.1f || z_view_b < 0.1f || z_view_c < 0.1f) {
                continue;
            }

            // 2. Векторы сторон треугольника в 3D
            float v1x = transformed[b].x - transformed[a].x;
            float v1y = transformed[b].y - transformed[a].y;
            float v1z = z_view_b - z_view_a;

            float v2x = transformed[c].x - transformed[a].x;
            float v2y = transformed[c].y - transformed[a].y;
            float v2z = z_view_c - z_view_a;

            // 3. Компоненты вектора нормали треугольника
            float nx = v1y * v2z - v1z * v2y;
            float ny = v1z * v2x - v1x * v2z;
            float nz = v1x * v2y - v1y * v2x;


          // 4. Скалярное произведение нормали на луч взгляда
            float dot_product = nx * transformed[a].x + ny * transformed[a].y + nz * z_view_a;

            // Отсечение невидимых (обращённых от камеры) граней.
            // dot_product <= 0  -> грань смотрит "от" камеры или строго на ребро — не рисуем.
            if (dot_product <= 0.0f) {
                continue;
            }
            float len = sqrt(nx*nx + ny*ny + nz*nz);
            if (len > 0.0001f) {
                nx /= len; ny /= len; nz /= len;
            }
            float dotLight = nx * Lx + ny * Ly + nz * Lz;
            float brightness = 0.25f + 0.75f * (dotLight > 0.0f ? dotLight : 0.0f);
            int r = (baseColor >> 11) & 0x1F;
            int g = (baseColor >> 5) & 0x3F;
            int _b = baseColor & 0x1F;
            r = (int)(r * brightness);
            g = (int)(g * brightness);
            _b = (int)(_b * brightness);
            if (r > 31) r = 31;
            if (g > 63) g = 63;
            if (_b > 31) _b = 31;
            uint16_t shadedColor = (r << 11) | (g << 5) | _b;
            float factor_a = d / z_view_a;
            int px_a = (int)(transformed[a].x * factor_a * scale + (float)(DISPLAY_WIDTH / 2) + 0.5f+ x_offset);
            int py_a = (int)(transformed[a].y * factor_a * scale + (float)(DISPLAY_HEIGHT / 2) + 0.5f+y_offset);
            
            float factor_b = d / z_view_b;
            int px_b = (int)(transformed[b].x * factor_b * scale+ (float)(DISPLAY_WIDTH / 2) + 0.5f+x_offset);
            int py_b = (int)(transformed[b].y * factor_b * scale+ (float)(DISPLAY_HEIGHT / 2) + 0.5f+y_offset);

            float factor_c = d / z_view_c;
            int px_c = (int)(transformed[c].x * factor_c * scale + (float)(DISPLAY_WIDTH / 2) + 0.5f+x_offset);
            int py_c = (int)(transformed[c].y * factor_c * scale + (float)(DISPLAY_HEIGHT / 2) + 0.5f+y_offset);         
            
            mesh->triangle[visible_count].px_a= px_a; 
            mesh->triangle[visible_count].py_a = py_a;
            mesh->triangle[visible_count].px_b = px_b;
            mesh->triangle[visible_count].py_b = py_b;
            mesh->triangle[visible_count].px_c = px_c;
            mesh->triangle[visible_count].py_c = py_c;
            mesh->triangle[visible_count].z_view_a = z_view_a;
            mesh->triangle[visible_count].z_view_b = z_view_b;
            mesh->triangle[visible_count].z_view_c = z_view_c;
            mesh->triangle[visible_count].depth = (z_view_a+z_view_b+z_view_c)/3.0f;
            mesh->triangle[visible_count].shadedColor = shadedColor;
            visible_count++;
            // 7. Отрисовка каркаса
            /*st7789.drawLine(px_a, py_a, px_b, py_b, WHITE);
            st7789.drawLine(px_b, py_b, px_c, py_c, WHITE);
            st7789.drawLine(px_c, py_c, px_a, py_a, WHITE);*/
            }
            constexpr float TWO_PI = 2.0f * M_PI;

            coresData.cur_angle_x = fmodf(coresData.cur_angle_x + dangle_x, TWO_PI);
            coresData.cur_angle_y = fmodf(coresData.cur_angle_y + dangle_y, TWO_PI);

            xSemaphoreGive(sem_cal_run);
            int indx[visible_count];
                for (int i = 0; i < visible_count; i++) {
                    indx[i] = i;
                }
            std::sort(indx, indx + visible_count, [mesh](int a, int b) {
                        return mesh->triangle[a].depth > mesh->triangle[b].depth;
                                                            });

            for(int i = 0; i<visible_count; i++){
                int inx = indx[i];
                st7789.fillTriangle(
                    mesh->triangle[inx].px_a,
                    mesh->triangle[inx].py_a,
                    mesh->triangle[inx].px_b,
                    mesh->triangle[inx].py_b,
                    mesh->triangle[inx].px_c,
                    mesh->triangle[inx].py_c,
                    mesh->triangle[inx].shadedColor
                );
            }
    int64_t end = esp_timer_get_time();
    times[2] = end-start;
    //st7789.Render();
}
void PipeLine3D::getTimes(){
    ESP_LOGI("3D times", "ReadObj: %d", times[0]);
    //ESP_LOGI("3D times", "Turn: %d", times[1]);
    ESP_LOGI("3D_times", "RenderMesh: %d", times[2]/30);
    memset(times, 0, sizeof(int64_t) * 3);

}
