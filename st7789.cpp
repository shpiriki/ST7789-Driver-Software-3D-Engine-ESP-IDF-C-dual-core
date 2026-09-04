#include "st7789.h"


ST7789::ST7789(int MOSI, int CLK, int dc, int rst):spi{}, mosiPin(MOSI),clkPin(CLK) ,dcPin(dc),rstPin(rst) {
}
void ST7789::sendCommand(uint8_t command){
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_buffer = &command;
    my_gpio_set_level((uint8_t)dcPin, 0);
    spi_device_transmit(spi, &t);
}
void ST7789::sendData(uint8_t data){
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_buffer = &data;
    my_gpio_set_level((uint8_t)dcPin, 1);
    spi_device_transmit(spi,&t);
}
void ST7789::sendDataArray(uint8_t* array, int len){
    if(len==0){
        return;
    }
    spi_transaction_t t = {};
    t.length = len*8;
    t.tx_buffer = array;
    my_gpio_set_level((uint8_t)dcPin,1);
    spi_device_transmit(spi,&t);
}
void ST7789::init(){
    my_gpio_reset_pin((uint8_t)dcPin);//сброс пинов и выставление их режима работы
    my_gpio_reset_pin((uint8_t)rstPin);
    my_gpio_set_direction((uint8_t)dcPin,GPIO_MODE_OUTPUT);
    my_gpio_set_direction((uint8_t)rstPin, GPIO_MODE_OUTPUT);
    spi_bus_config_t bus_config = {};//создание структуры конфигурации шины(просто задаем пины)
    bus_config.mosi_io_num = mosiPin;
    bus_config.miso_io_num = -1;
    bus_config.sclk_io_num = clkPin;
    bus_config.quadwp_io_num = -1;
    bus_config.quadhd_io_num = -1;
    bus_config.max_transfer_sz = DISPLAY_HEIGHT*DISPLAY_WIDTH*2;
    spi_bus_initialize(SPI2_HOST, &bus_config, SPI_DMA_CH_AUTO);
    spi_device_interface_config_t dev_config = {};//создание конфигурцаии нашего дисплея
    dev_config.clock_speed_hz = 30000000;
    dev_config.mode = SPI_MODE;
    dev_config.spics_io_num = -1;
    dev_config.queue_size = 10;
    spi_bus_add_device(SPI2_HOST, &dev_config, &spi);
    my_gpio_set_level((uint8_t)rstPin,0);//перезагрузка
    vTaskDelay(pdMS_TO_TICKS(10));
    my_gpio_set_level((uint8_t)rstPin,1);//вкл
    //команды инициализации дисплея
    vTaskDelay(pdMS_TO_TICKS(120));
    sendCommand(ST7789_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));
    sendCommand(ST7789_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));
    sendCommand(ST7789_NORON);
    vTaskDelay(pdMS_TO_TICKS(10));
    sendCommand(ST7789_INVON);
    sendCommand(ST7789_DISPON);
    vTaskDelay(pdMS_TO_TICKS(10));
    sendCommand(ST7789_MADCTL);
    sendData(0x00);
    sendCommand(ST7789_COLMOD);
    sendData(0x55);
    canvas = (uint16_t*)heap_caps_malloc(DISPLAY_WIDTH*DISPLAY_HEIGHT*2, MALLOC_CAP_8BIT);
    memset(canvas,0,DISPLAY_WIDTH * DISPLAY_HEIGHT * 2);
    for(int i=0; i<QUEUE_DEPTH; i++){
        lines[i] = (uint16_t*)heap_caps_malloc(DISPLAY_WIDTH*DMA_LINES*2, MALLOC_CAP_DMA);
    }
    
    ESP_LOGI(display, "Дисплей инициализирован");
}
void ST7789::setWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2){
    sendCommand(ST7789_CASET);
    uint8_t colData[4] = { 
    (uint8_t)(x1 >> 8), 
    (uint8_t)(x1 & 0xFF), 
    (uint8_t)(x2 >> 8), 
    (uint8_t)(x2 & 0xFF) 
};
    sendDataArray(colData, 4);
    sendCommand(ST7789_RASET);
    uint8_t rowData[4] = { 
    (uint8_t)(y1 >> 8), 
    (uint8_t)(y1 & 0xFF), 
    (uint8_t)(y2 >> 8), 
    (uint8_t)(y2 & 0xFF) 
};
    sendDataArray(rowData, 4);
    sendCommand(ST7789_RAMWR);
}
void ST7789::fillScreen(uint16_t color){
    std::lock_guard<std::recursive_mutex> lock(d_mutex);
    if((color>>8) == (color&0xff)){
    memset(canvas, (uint8_t)(color), DISPLAY_WIDTH * DISPLAY_HEIGHT * 2);
    return;
    }
    uint16_t tcolor = gettruecolor(color);
    uint32_t *canvas32 = (uint32_t *)canvas;
    int size = DISPLAY_HEIGHT*DISPLAY_WIDTH/2;
    uint32_t color32 = ((uint32_t)tcolor << 16) | tcolor;
    for(int i=0; i<size; i++){
        canvas32[i]= color32;
    }
    /*for(int i=0; i<DISPLAY_WIDTH*DISPLAY_HEIGHT; i++){
        canvas[i]= gettruecolor(color);
    }*/
}
void ST7789::drawPixel(int x, int y, uint16_t color){
    std::lock_guard<std::recursive_mutex> lock(d_mutex);
    if (x<0||y<0||x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) return;

    int index = x +(y*DISPLAY_WIDTH);
    canvas[index]= gettruecolor(color);
}
//отрисовка линий по алгоритму Брезенхема
void ST7789::drawLine(int x1, int y1, int x2,int y2, uint16_t color){
    std::lock_guard<std::recursive_mutex> lock(d_mutex);
    if ((x1 < 0 && x2 < 0) || (x1 >= DISPLAY_WIDTH && x2 >= DISPLAY_WIDTH) ||
        (y1 < 0 && y2 < 0) || (y1 >= DISPLAY_HEIGHT && y2 >= DISPLAY_HEIGHT)) {
        return;
    }
    int dx = abs(x2 - x1);//дельта x
    int dy = abs(y2 - y1);//дельта y
    int sx = (x1 < x2) ? 1 : -1 ;//проверяем направление
    int sy = (y1 < y2) ? 1: -1;
    int err = dx - dy;//высчитываем ошибочку
    while(true){
        drawPixel(x1,y1, color);
        if(x1==x2 && y1 == y2){break;}//конец отрисовки
        int e2 = 2*err;
        if(e2 > -dy){
            err -= dy;
            x1+=sx;
        }
        if(e2 < dx){
            err +=dx;
            y1 +=sy;
        }
    }
}
void ST7789::Render(){
    std::lock_guard<std::recursive_mutex> lock(d_mutex);
    setWindow(0,0,DISPLAY_WIDTH-1,DISPLAY_HEIGHT-1);
    my_gpio_set_level((uint8_t)dcPin, 1);
    int step = 0;
    spi_transaction_t t [QUEUE_DEPTH];
    memset(t, 0,sizeof(t));
    for(int start_y = 0; start_y<DISPLAY_HEIGHT; start_y+=DMA_LINES){
        uint16_t* ptr = &canvas[start_y*DISPLAY_WIDTH];
        int t_indx = step % QUEUE_DEPTH;
        int lines_this_chunk = DMA_LINES;
        if(start_y + lines_this_chunk > DISPLAY_HEIGHT){
            lines_this_chunk = DISPLAY_HEIGHT - start_y;   // подрезаем последний кусок
        }
            if(step >= QUEUE_DEPTH){
                spi_transaction_t* rtrans;//указатель
                spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);//ждем когда 0(первая) транзакция дойдет до дислпея. указываем наше spi устройство, адрес на транзакцию(на самом деле просто пустышка в которую запишется последняя транзакция) и сколько ждать(мы ждем пока не отправится) 
            }
            __builtin_memcpy(lines[t_indx], ptr, DISPLAY_WIDTH*2*lines_this_chunk);
            t[t_indx].length = (DISPLAY_WIDTH*lines_this_chunk)*16;//размер нашей транзакции
            t[t_indx].tx_buffer = lines[t_indx];
            t[t_indx].rxlength = 0;
            esp_err_t err = spi_device_queue_trans(spi, &t[t_indx], portMAX_DELAY);
            if (err != ESP_OK) {
                ESP_LOGE("DBG", "queue_trans FAILED at step=%d: %s", step, esp_err_to_name(err));
            }
            //spi_device_queue_trans(spi, &t[t_indx], portMAX_DELAY);//отправляем ее на наше spi устройство в очередь, адрес указателя нашей заполненой строки и ожидаем(процессор спокойно может занимать своими делами)
            step++;
        }
        int pending = (step < QUEUE_DEPTH) ? step : QUEUE_DEPTH;
        for(int i=0; i<pending; i++){
            spi_transaction_t* rtrans;
            spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
        }//ожидаем отправки последней транзакции
}
uint16_t ST7789::gettruecolor(uint16_t color){
    uint16_t true_color = (color >> 8) | (color <<8);
    return true_color;
}
void ST7789::fillreg(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color){
    std::lock_guard<std::recursive_mutex> lock(d_mutex);
    uint16_t true_color = gettruecolor(color);
    for(int y = y1; y<=y2; y++){
        int row_offset = y*DISPLAY_WIDTH;
        for(int x=x1; x<=x2; x++){
            canvas[row_offset+x] = true_color;
        }
    }
}
void ST7789::fillTriangle(int x1, int y1, int x2, int y2, int x3, int y3, uint16_t color){
    std::lock_guard<std::recursive_mutex> lock(d_mutex);
    Point points[3] = {{x1,y1}, {x2,y2}, {x3,y3}};
    std::sort(points, points + 3, [](const Point& a, const Point& b) {
        return a.y < b.y; 
    });
    if (points[0].y == points[2].y) return;
     if (points[0].y != points[1].y) {
        for (int y = points[0].y; y < points[1].y; y++) {
            int x_large = points[0].x + (y - points[0].y) * (points[2].x - points[0].x) / (points[2].y - points[0].y);
            int x_short = points[0].x + (y - points[0].y) * (points[1].x - points[0].x) / (points[1].y - points[0].y);
            drawLineF(x_large, x_short,y, color);
            //drawLine(x_large, y, x_short, y, color);
        }
    }
    if (points[1].y != points[2].y) {
        for (int y = points[1].y; y <= points[2].y; y++) {
            int x_large = points[0].x + (y - points[0].y) * (points[2].x - points[0].x) / (points[2].y - points[0].y);
            int x_short = points[1].x + (y - points[1].y) * (points[2].x - points[1].x) / (points[2].y - points[1].y);
            drawLineF(x_large, x_short,y, color);
            //drawLine(x_large, y, x_short, y, color);
        }
    }
}
void ST7789::print(uint16_t x, uint16_t y, const char* str, uint16_t color,uint8_t scale){
    std::lock_guard<std::recursive_mutex> lock(d_mutex);
    int i = 0;
    while(str[i] != '\0'){
        uint8_t sim = str[i];
        int indx = sim - 32;
        if(sim<32){
            i++;
            continue;
        }
        for(int row = 0; row<16; row++){
            uint8_t byte = font8x16[indx][row];
            for(int bit=0; bit<8; bit++){
               if((byte & (1 << (7-bit))) != 0){
                for(int sy = 0; sy < scale; sy++){
                    for(int sx = 0; sx < scale; sx++){
                        drawPixel(x + bit * scale + sx, y + row * scale + sy, color);
                    }
                }
            }
        }
    }
        i++;
        x+=8*scale;
    }
}
void ST7789::drawLineF(uint16_t x1, uint16_t x2,uint16_t y ,uint16_t color){
    if(y>=DISPLAY_HEIGHT)return;
    if(x1>x2){
        std::swap(x1,x2);
    }
    if (x1 >= DISPLAY_WIDTH) return;
    if (x2 >= DISPLAY_WIDTH) x2 = DISPLAY_WIDTH - 1;
    while(x1!=x2){
        drawPixel(x1,y, color);
        x1++;
    }
}
ST7789::~ST7789(){
    clean();
}
void ST7789::clean(){
    if (canvas != nullptr) {
        heap_caps_free(canvas);
        canvas = nullptr;
    }
    for (int i = 0; i < QUEUE_DEPTH; i++) {
        if (lines[i] != nullptr) {
            heap_caps_free(lines[i]);
            lines[i] = nullptr;
        }
    }
    if (spi != nullptr) {
        spi_bus_remove_device(spi);
    }
}
//ad your display settings
void ST7789::setConfig(uint16_t width, uint16_t height, uint16_t dma_lines, uint16_t spi_mode){
    DISPLAY_WIDTH = width;
    DISPLAY_HEIGHT = height;
    DMA_LINES = dma_lines;
    SPI_MODE = spi_mode;
}
//new func, good luck)
void ST7789::setMode(bool mode){
    if(mode){
        flag = true;
    }
    if(!mode){
        clean();
        DISPLAY_HEIGHT = 0;
        DISPLAY_WIDTH = 0;
        ESP_LOGI(display, "PLEASE REINIT CLASS");

    }
}
//use this func only with UNSAFE mode.This function enables window management on the display. It is an experimental feature that is still under development.
void ST7789::sWindow(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2){
    if(!flag){
        return;
    }
    int newsize = (x2-x1+1)*(y2-y1+1)*2;
    uint16_t* newCanvas = (uint16_t*)(heap_caps_realloc(canvas, newsize, MALLOC_CAP_8BIT));
    if(newCanvas==NULL){
        heap_caps_free(canvas);
        ESP_LOGI(display, "Пизда беги");
        return;
    }
    setWindow(x1,y1,x2,y2);
    canvas = newCanvas;
    DISPLAY_WIDTH = (x2-x1+1);
    DISPLAY_HEIGHT = (y2-y1+1);
}
