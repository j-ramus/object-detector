#include "display.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"




//shared state is written by MC.c, read by display_task
static volatile display_mode_t s_mode    = DISPLAY_MODE_SETTING;
static volatile int32_t        s_seconds = 30;


static void display_task(void *arg)
{

    bool toggle = false;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(500));
        toggle = !toggle;

        switch (s_mode) {

        case DISPLAY_MODE_SETTING:
            // Colon blinks every 500 ms 
            ssd1306_render_frame((uint32_t)s_seconds, toggle,
                                 "SET TIME", "PLACE ITEM TO START",
                                 false);
            break;

        case DISPLAY_MODE_COUNTDOWN:
            ssd1306_render_frame((uint32_t)s_seconds, true,
                                 NULL, NULL,
                                 false);
            break;

        case DISPLAY_MODE_PAUSED:
            ssd1306_render_frame((uint32_t)s_seconds, false,
                                 "RETURN OBJECT", NULL,
                                 toggle);
            break;

        case DISPLAY_MODE_DONE:
            ssd1306_render_frame(0, false,
                                 "REMOVE OBJECT", NULL,
                                 toggle);
            break;
        }
    }
}





//--------------------------
// public functions
//---------------------------
esp_err_t display_init(void)
{
    esp_err_t ret = ssd1306_init();
    if (ret != ESP_OK) return ret;

    ssd1306_set_brightness(DISPLAY_BRIGHTNESS_HIGH);
    xTaskCreate(display_task, "display", 4096, NULL, 5, NULL);
    return ESP_OK;
}


void display_set_mode(display_mode_t mode)
{
    s_mode = mode;
}




void display_update_time(int32_t seconds)
{
    s_seconds = seconds;
}


void display_cycle_brightness(void)
{
    ssd1306_cycle_brightness();
}






