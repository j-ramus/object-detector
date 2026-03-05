#pragma once
#include "esp_err.h"
#include "ssd1306.h"    

typedef enum {
    DISPLAY_MODE_SETTING,    
    DISPLAY_MODE_COUNTDOWN,  
    DISPLAY_MODE_PAUSED, 
    DISPLAY_MODE_DONE,
} display_mode_t;


//Public Functions:

//  initialises hardware and starts render task, Call once at startup.
esp_err_t display_init(void);

// Switch display state
void display_set_mode(display_mode_t mode);

// Push current countdown/set-time value; call from MC.c each second
void display_update_time(int32_t seconds);

void display_cycle_brightness(void);

