#ifndef GPIO_DRIVER
#define GPIO_DRIVER

#include "soc/soc.h"

#define GPIO_BASE 0x3FF44000
#define GPIO_OUT_REG 0x04
#define GPIO_ENABLE_REG 0x20
#define GPIO_IN_REG 0x3C

#define GPIO_BIT(bit) (1U<<bit) 

#define GPIO_MODE_OUTPUT 1
#define GPIO_MODE_INPUT 0
#define GPIO_LEVEL_HIGH 1
#define GPIO_LEVEL_LOW 0


void set_bit(uint32_t addr, uint8_t bit);
void clear_bit(uint32_t addr, uint8_t bit);
bool read_bit(uint32_t addr, uint8_t bit);

void gpio_pullup_enable(uint8_t pin);
void gpio_pullup_disable(uint8_t pin);
void gpio_pulldown_enable(uint8_t pin);
void gpio_pulldown_disable(uint8_t pin);
void my_gpio_set_direction(uint8_t pin, uint8_t mode);
void my_gpio_set_level(uint8_t pin, uint8_t level);
bool gpio_read_level(uint8_t pin);
void my_gpio_reset_pin(uint8_t pin);

#endif