#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/dac_oneshot.h"
#include "esp_timer.h"

#define DAC_CHANNEL         DAC_CHAN_0 // D 25
#define BUTTON_PIN          GPIO_NUM_26 // D 26 push button
#define FREQUENCY_HZ        440
#define SAMPLE_COUNT        32   //samples per cycle
#define SAMPLE_RATE_HZ      (FREQUENCY_HZ * SAMPLE_COUNT) // 28160hz
#define SAMPLE_PERIOD_US    (1000000 / SAMPLE_RATE_HZ)     




static volatile bool tone_enabled = false;
static volatile uint8_t sample_index = 0;
static dac_oneshot_handle_t dac_handle;




// ramp 0 to 255

static const uint8_t sawtooth_table[SAMPLE_COUNT] = {
      0,   8,  16,  24,  32,  40,  48,  56,
     64,  72,  80,  88,  96, 104, 112, 120,
    128, 136, 144, 152, 160, 168, 176, 184,
    192, 200, 208, 216, 224, 232, 240, 248
};




static void IRAM_ATTR dac_timer_callback(void* arg)
{
    if (tone_enabled) {
        dac_oneshot_output_voltage(dac_handle, sawtooth_table[sample_index]);
        sample_index = (sample_index + 1) % SAMPLE_COUNT;
    } else {
        dac_oneshot_output_voltage(dac_handle, 128);  //midpoint when off
    }
}




static void button_task(void* arg)
{
    bool last_button_state = true;     //pullup = unpressed = high

    while (1) {

        bool current_button_state = gpio_get_level(BUTTON_PIN);

        if (last_button_state && !current_button_state) {
            tone_enabled = !tone_enabled;

            vTaskDelay(pdMS_TO_TICKS(50));   // debounce delay
        }

        last_button_state = current_button_state;

        vTaskDelay(pdMS_TO_TICKS(10));       //poll 10ms
    }
}




void app_main(void)
{
    // dac channel config
    //
    dac_oneshot_config_t dac_cfg = {
        .chan_id = DAC_CHANNEL
    };

    dac_oneshot_new_channel(&dac_cfg, &dac_handle);


    gpio_config_t input_conf = {
        .pin_bit_mask = (1ULL << BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&input_conf);

    //timer for dac
    const esp_timer_create_args_t timer_args = {
        .callback = &dac_timer_callback,
        .name = "dac_waveform"
    };

    esp_timer_handle_t timer;

    esp_timer_create(&timer_args, &timer);
    esp_timer_start_periodic(timer, SAMPLE_PERIOD_US);

    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);

}
