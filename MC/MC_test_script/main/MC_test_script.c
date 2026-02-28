#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define GPIO_SENSE_INPUT GPIO_NUM_4
#define GPIO_LED GPIO_NUM_2
#define GPIO_HIGH_1 GPIO_NUM_27
#define GPIO_HIGH_2 GPIO_NUM_14
#define GPIO_HIGH_3 GPIO_NUM_17
#define GPIO_HIGH_4 GPIO_NUM_16
#define GPIO_HIGH_5 GPIO_NUM_19

#define ESP_INTR_FLAG_DEFAULT 0

static TaskHandle_t sense_handle = NULL;

bool toggle = false;

void toggle_alarm(bool *toggle){
    *toggle = !(*toggle);
}

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    //notify the task that the interrupt has occurred
    vTaskNotifyGiveFromISR(sense_handle, NULL);
}

static void sense_task(void* arg)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        gpio_intr_disable(GPIO_SENSE_INPUT); // prevent retrigger of ISR

        printf("GPIO is pulled LOW\n");

        toggle_alarm(&toggle); //

        gpio_set_level(GPIO_LED, 1);

        while (gpio_get_level(GPIO_SENSE_INPUT) == 0) {
            vTaskDelay(pdMS_TO_TICKS(10)); // wait until object is removed
        }

        gpio_set_level(GPIO_LED, 0);

        printf("GPIO is pulled HIGH\n");

        gpio_intr_enable(GPIO_SENSE_INPUT); // prevent retrigger of ISR

    }
}

void app_main(void)
{
    //zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //interrupt of falling edge
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    //bit mask of the SENSE_INPUT pin
    io_conf.pin_bit_mask = (1ULL<<GPIO_SENSE_INPUT);
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);


    gpio_set_direction(GPIO_LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_HIGH_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_HIGH_2, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_HIGH_3, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_HIGH_4, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_HIGH_5, GPIO_MODE_OUTPUT);

    //start gpio task
    xTaskCreate(sense_task, "sense_task", 2048, NULL, 10, &sense_handle);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_SENSE_INPUT, gpio_isr_handler, NULL);

    printf("dsig test script running...\n");

    /*
    gpio_set_level(GPIO_HIGH_1, 1);
    gpio_set_level(GPIO_HIGH_2, 1);
    gpio_set_level(GPIO_HIGH_3, 1);
    gpio_set_level(GPIO_HIGH_4, 1);
    gpio_set_level(GPIO_HIGH_5, 1);
    */

    for(;;) {
        vTaskDelay(pdMS_TO_TICKS(1000)); //infinite loop to wait for interrupts
        printf("alarm is toggled: %s\n", toggle ? "true" : "false");
    }

}
