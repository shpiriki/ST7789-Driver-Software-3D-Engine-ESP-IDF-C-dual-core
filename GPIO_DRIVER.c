#include "GPIO_DRIVER.h"

void set_bit(uint32_t addr, uint8_t bit){
    if(bit > 31){return;}
    uint32_t reg = REG_READ(addr);
    reg |= (1<<bit);
    REG_WRITE(addr, reg);
}
void clear_bit(uint32_t addr, uint8_t bit){
    if(bit > 31){return;}
    uint32_t reg = REG_READ(addr);
    reg &= ~(1<<bit);
    REG_WRITE(addr, reg);
}
bool read_bit(uint32_t addr, uint8_t bit){
    if(bit > 31){return false;}
    uint32_t reg = REG_READ(addr);
    int is_set = (reg&(1<<bit));
    if(is_set!= 0){
        return true;
    }
    return false;
}
void gpio_pullup_enable(uint8_t pin){
    if(pin>39){return;}
    uint32_t io_mux_addr = 0x3FF44000 + 0x10 + (4 * pin);
    set_bit(io_mux_addr, 8);
}
void gpio_pullup_disable(uint8_t pin){
    if(pin>39){return;}
    uint32_t io_mux_addr = 0x3FF44000 + 0x10 + (4 * pin);
    clear_bit(io_mux_addr, 8);
}
void gpio_pulldown_enable(uint8_t pin){
    if(pin>39){return;}
    uint32_t io_mux_addr = 0x3FF44000 + 0x10 + (4 * pin);
    set_bit(io_mux_addr, 7);
}
void gpio_pulldown_disable(uint8_t pin){
    if(pin>39){return;}
    uint32_t io_mux_addr = 0x3FF44000 + 0x10 + (4 * pin);
    clear_bit(io_mux_addr, 7);
}
void my_gpio_set_direction(uint8_t pin, uint8_t mode){
    if(pin>31){return;}
    uint32_t addr = GPIO_BASE + GPIO_ENABLE_REG;
    if(mode == 1){
        set_bit(addr, pin);
        return;
    }
    clear_bit(addr, pin);
}
void my_gpio_set_level(uint8_t pin, uint8_t level){
    if(pin>31){return;}
    uint32_t addr = GPIO_BASE + GPIO_OUT_REG;
    if(level == 1){
        set_bit(addr, pin);
        return;
    }
    clear_bit(addr,pin);
}
bool gpio_read_level(uint8_t pin){
    if(pin>31){return false;}
    uint32_t addr = GPIO_BASE+GPIO_IN_REG;
    if(read_bit(addr, pin)){return true;}
    return false;
}
void my_gpio_reset_pin(uint8_t pin){
    if(pin>31){return;}
    clear_bit(GPIO_BASE+GPIO_ENABLE_REG, pin);
    clear_bit(GPIO_BASE+GPIO_OUT_REG, pin);
    gpio_pulldown_disable(pin);
    gpio_pullup_disable(pin);
    gpio_pullup_enable(pin);
}