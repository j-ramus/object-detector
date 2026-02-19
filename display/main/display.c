#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "ssd1306.h"

#define BUTTON_PIN  5   
static const char *TAG = "main";


static TaskHandle_t s_button_task = NULL;

static void IRAM_ATTR button_isr(void *arg)
{
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(s_button_task, &woken);
    portYIELD_FROM_ISR(woken);
}


static void button_task(void *arg)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);//sleep until ISR fires
        display_cycle_brightness();
        ESP_LOGI(TAG, "brightness cycled");
        vTaskDelay(pdMS_TO_TICKS(200));             
        ulTaskNotifyTake(pdTRUE, 0);    //discard bounces
    }
}


static void init_button(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,          
    };
    gpio_config(&cfg);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, button_isr, NULL);
}



static void timer_task(void *arg)
{
    uint32_t elapsed_s = 0;
    uint32_t ticks = 0;
    for (;;) {
        display_show_time(elapsed_s);
        vTaskDelay(pdMS_TO_TICKS(100));    
        if (++ticks >= 10) {                
            ticks = 0;
            elapsed_s++;
        }
    }
}






void app_main(void)
{
    esp_err_t ret = display_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "display_init failed: %s", esp_err_to_name(ret));
        return;
    }

    display_set_brightness(DISPLAY_BRIGHTNESS_HIGH);
    xTaskCreate(button_task, "button", 2048, NULL, 6, &s_button_task);
    init_button();
    xTaskCreate(timer_task, "timer", 4096, NULL, 5, NULL);
}
