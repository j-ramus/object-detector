#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"



#define DISPLAY_SDA_PIN     21
#define DISPLAY_SCL_PIN     22
#define DISPLAY_I2C_PORT    I2C_NUM_0
#define DISPLAY_I2C_ADDR    0x3C
#define DISPLAY_I2C_FREQ    400000
#define DISPLAY_WIDTH       128
#define DISPLAY_HEIGHT      64


typedef enum {
    DISPLAY_BRIGHTNESS_LOW  = 0,
    DISPLAY_BRIGHTNESS_MED  = 1,
    DISPLAY_BRIGHTNESS_HIGH = 2,
} display_brightness_t;


esp_err_t ssd1306_init(void);

void ssd1306_set_brightness(display_brightness_t level);

void ssd1306_cycle_brightness(void);



void ssd1306_render_frame(uint32_t seconds, bool colon_visible,
                          const char *top, const char *bottom,
                          bool inverted);


void ssd1306_clear(void);

