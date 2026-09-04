#pragma once
#include <driver/spi_master.h>
//#include <driver/gpio.h>
extern "C"{
    #include "GPIO_DRIVER.h"
}
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <cstring> 
#include <esp_log.h>
#include <algorithm>
#include "font8x16.h"
#include <mutex>
#define ST7789_SWRESET  0x01
#define ST7789_SLPOUT   0x11
#define ST7789_NORON    0x13
#define ST7789_INVON    0x21
#define ST7789_DISPOFF  0x28
#define ST7789_DISPON   0x29
#define ST7789_CASET    0x2A
#define ST7789_RASET    0x2B
#define ST7789_RAMWR    0x2C
#define ST7789_MADCTL   0x36
#define ST7789_COLMOD   0x3A
#define BLACK       0x0000
#define WHITE       0xFFFF
#define RED         0xF800
#define GREEN       0x07E0
#define BLUE        0x001F
#define YELLOW      0xFFE0
#define CYAN        0x07FF
#define MAGENTA     0xF81F
#define GRAY        0x7BEF
#define ORANGE      0xFDA0  // Оранжевый
#define PURPLE      0x8010  // Фиолетовый
#define LIME        0x07E0  // Лайм
#define TEAL        0x0410  // Бирюзовый
#define DARK_RED    0x8000
#define DARK_GREEN  0x0400
#define DARK_BLUE   0x0010
inline uint16_t DISPLAY_WIDTH = 240;
inline uint16_t DISPLAY_HEIGHT = 240;
inline uint16_t SPI_MODE = 3;
inline uint16_t DMA_LINES =  20;
static const char* display = "ST7789_DRIVER";
const int QUEUE_DEPTH = 2;
#define UNSAFE true
#define SAFE false
struct Point
{
    int x;
    int y;
};

class ST7789 {
    private:
    std::recursive_mutex d_mutex;
    spi_device_handle_t spi;
    int mosiPin;
    int clkPin;
    int dcPin;
    int rstPin;
    bool flag = false;
    uint16_t* canvas = nullptr; 
    uint16_t* lines [QUEUE_DEPTH] = {nullptr};
    void sendCommand(uint8_t command);
    void sendData(uint8_t data);
    void sendDataArray(uint8_t* array, int len);
    uint16_t gettruecolor(uint16_t color);
    void setWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
    void drawLineF(uint16_t x1, uint16_t x2,uint16_t y ,uint16_t color);
    void clean();
    public:
    ST7789(int MOSI, int CLK, int dc, int rst);
    void setConfig(uint16_t width, uint16_t height, uint16_t dma_lines, uint16_t spi_mode);
    void init();
    void setMode(bool mode);
    void sWindow(uint16_t x1, uint16_t y1 , uint16_t x2, uint16_t y2);
    ~ST7789();
    void Render();
    void fillScreen(uint16_t color);
    void drawPixel(int x, int y, uint16_t color);
    void fillreg(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
    void fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3, uint16_t color);
    void drawLine(int x1, int y1, int x2,int y2, uint16_t color);
    void print(uint16_t x, uint16_t y, const char* str, uint16_t color, uint8_t scale=2);
};
