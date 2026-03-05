#include "alarm.h"
#include "driver/dac_oneshot.h"
#include "esp_timer.h"


#define DAC_CHANNEL DAC_CHAN_0
#define DAC_GPIO GPIO_NUM_25
#define FREQUENCY_HZ 440
#define SAMPLE_COUNT 32
#define SAMPLE_RATE_HZ (FREQUENCY_HZ * SAMPLE_COUNT)
#define SAMPLE_PERIOD_US (1000000 / SAMPLE_RATE_HZ)

static volatile uint8_t     sample_index = 0;
static dac_oneshot_handle_t dac_handle;
static esp_timer_handle_t   s_timer;

static const uint8_t sawtooth_table[SAMPLE_COUNT] = {
      0,   8,  16,  24,  32,  40,  48,  56,
     64,  72,  80,  88,  96, 104, 112, 120,
    128, 136, 144, 152, 160, 168, 176, 184,
    192, 200, 208, 216, 224, 232, 240, 248
};




// callback only runs while alarm is active
static void IRAM_ATTR dac_timer_callback(void *arg)
{
    dac_oneshot_output_voltage(dac_handle, sawtooth_table[sample_index]);
    sample_index = (sample_index + 1) % SAMPLE_COUNT;
}


esp_err_t alarm_init(void)
{
    dac_oneshot_config_t dac_cfg = { .chan_id = DAC_CHANNEL };
    esp_err_t ret = dac_oneshot_new_channel(&dac_cfg, &dac_handle);
    if (ret != ESP_OK) return ret;

    //creates waveform timer but does not start it
    const esp_timer_create_args_t timer_args = {
        .callback = dac_timer_callback,
        .name     = "dac_waveform"
    };
    return esp_timer_create(&timer_args, &s_timer);
}



void alarm_start(void)
{
    sample_index = 0;
    esp_timer_start_periodic(s_timer, SAMPLE_PERIOD_US);
}



void alarm_stop(void)
{
    esp_timer_stop(s_timer);
    dac_oneshot_output_voltage(dac_handle, 128); //128 = midpoint
}

