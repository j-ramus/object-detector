#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "freertos/timers.h"
#include "alarm.h"
#include "display.h"

#define GPIO_SENSE_INPUT    GPIO_NUM_33
#define GPIO_INC_TIME       GPIO_NUM_18
#define GPIO_DEC_TIME       GPIO_NUM_17
#define GPIO_BRIGHTNESS     GPIO_NUM_19
#define ESP_INTR_FLAG_DEFAULT 0

//time adjustment constants in seconds
#define DEFAULT_TIME         60
#define INCREMENTAL_TIME     30
#define MAX_TIME             600
#define MIN_TIME             30

//globle time and countdown variable 
int set_time;
int countdown;

//**************************************************************************************
//                                 FreeRTOS Tasks
//**************************************************************************************

//Creates a reference pointor for a task
static TaskHandle_t sense_handle = NULL;
static TaskHandle_t inc_time_handle = NULL;
static TaskHandle_t dec_time_handle = NULL;
static TaskHandle_t brightness_handle = NULL;

/*--------------------------  ISR Handlers ---------------------------------------------*/
//ISR handler, will be called when the interrupt occurs
//keep short for better performance

//object Sensor ISR
static void IRAM_ATTR sense_isr_handler(void* arg)
{
    //notify the task that the interrupt has occurred
    vTaskNotifyGiveFromISR(sense_handle, NULL);
}

//inc time ISR
static void IRAM_ATTR inc_time_isr_handler(void* arg)
{
    vTaskNotifyGiveFromISR(inc_time_handle, NULL);
}

//dec time ISR
static void IRAM_ATTR dec_time_isr_handler(void* arg)
{
    vTaskNotifyGiveFromISR(dec_time_handle, NULL);
}

//brightness ISR
static void IRAM_ATTR brightness_isr_handler(void* arg)
{
    vTaskNotifyGiveFromISR(brightness_handle, NULL);
}

//-------------- Countdown Timer -------------------------

static void countdown_timer_callback(TimerHandle_t xTimer)
{
    if (countdown > 0) {
        countdown--;
        display_update_time(countdown);
    }
    if (countdown <= 0) {
        xTimerStop(xTimer, 0);   // sense_task while-loop exits on next 50ms poll
    }
}


/*-------------------------- Tasks Handlers ---------------------------------------------*/

static void sense_task(void* arg)
{
    for (;;) {
        //will wait for the notification from the ISR
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        vTaskDelay(pdMS_TO_TICKS(50));  // debounce delay
        // 
// duplicated this here, but inverted to prevent bounce when reenabled
        if (gpio_get_level(GPIO_SENSE_INPUT) != 0) {
            continue;   //object not present
        }

        //disable incremental time ISRs
        gpio_intr_disable(GPIO_INC_TIME);
        gpio_intr_disable(GPIO_DEC_TIME);
        gpio_intr_disable(GPIO_SENSE_INPUT);

        display_set_mode(DISPLAY_MODE_COUNTDOWN); // start countdown
        printf("countdown start\n"); //TODO: for debugging

        printf("Object detected! countdown started...\n"); //TODO: for debugging

        while (countdown > 0) { //FIXME: link countdown var with component
            vTaskDelay(pdMS_TO_TICKS(50));  // debounce delay
            if(gpio_get_level(GPIO_SENSE_INPUT) == 0){
                continue;
            }

           else {
                display_set_mode(DISPLAY_MODE_COUNTDOWN); // pause countdown
                printf("toggle countdown\n"); //TODO: for debugging

                alarm_start();
                printf("toggle alarm\n"); //TODO: for debugging

                while (gpio_get_level(GPIO_SENSE_INPUT) != 0){
                    vTaskDelay(pdMS_TO_TICKS(50));  // debounce delay
                    display_set_mode(DISPLAY_MODE_PAUSED); //pause countdown and prompt object to be replaced
                    printf("REPLACE OBJECT!!\n"); //TODO: for debugging
                }

                display_set_mode(DISPLAY_MODE_COUNTDOWN); // resume countdown
                printf("toggle countdown\n"); //TODO: for debugging

                alarm_stop();
                printf("toggle alarm\n"); //TODO: for debugging
           }
        }
        
        //if timer is finished, prompt to remove object, wait untill
        while (gpio_get_level(GPIO_SENSE_INPUT) == 0){
           vTaskDelay(pdMS_TO_TICKS(50));  // debounce delay
            
            display_set_mode(DISPLAY_MODE_DONE); //print "remove object"
             printf("Countdown up, REMOVE OBJECT!!\n"); //TODO: for debugging
            }
    }
        
        //enable incremental time ISRs
        gpio_intr_enable(GPIO_INC_TIME);
        gpio_intr_enable(GPIO_DEC_TIME);
        gpio_intr_enable(GPIO_SENSE_INPUT);
}


static void inc_time_task(void* arg)

{
    for (;;) {
        //will wait for the notification from the ISR
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        vTaskDelay(pdMS_TO_TICKS(50));  // debounce delay
        
        printf("in inc_time task...\n"); //TODO: for debugging


        //increment and update timer
        if(set_time < MAX_TIME){
        set_time = set_time + INCREMENTAL_TIME;
        display_update_time(set_time);
        
        printf("Time increased! Set Time:\n%02d:%02d\n", (set_time/60), (set_time%60)); //TODO: for debugging
        }
    
    }
}

static void dec_time_task(void* arg)
{
    for (;;) {
        //will wait for the notification from the ISR
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        vTaskDelay(pdMS_TO_TICKS(50));  // debounce delay

        
        printf("in dec_time task...\n"); //TODO: for debugging

        //decrement and update timer
        if(set_time > MIN_TIME){
        set_time = set_time - INCREMENTAL_TIME;
        display_update_time(set_time);
        
        printf("Time decreased! Set Time:\n%02d:%02d\n", (set_time/60), (set_time%60)); //TODO: for debugging
        }
    
    }
}

static void brightness_task(void* arg)
{
    for (;;) {
        //will wait for the notification from the ISR
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        vTaskDelay(pdMS_TO_TICKS(50));  // debounce delay
        display_cycle_brightness();     // cycle brightness
    }
}

void app_main(void)
{
//**************************************************************************************
//                             GPIO and Task Inilizations
//**************************************************************************************
    
    /*-------------------------- GPIO Pin Config ---------------------------------------------*/
    
    //zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //interrupt of falling edge
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    //bit mask of the GPIO pins to get config
    io_conf.pin_bit_mask =  
    ((1ULL<<GPIO_SENSE_INPUT) |
    (1ULL<<GPIO_INC_TIME) |
    (1ULL<<GPIO_DEC_TIME) |
    (1ULL<<GPIO_BRIGHTNESS));
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    /*-------------------------- Task Config ---------------------------------------------*/

    //Create task, name task, set priorty, link to handler
    xTaskCreate(sense_task, "sense_task", 2048, NULL, 10, &sense_handle);
    xTaskCreate(brightness_task, "brightness_task", 2048, NULL, 9, &brightness_handle);
    xTaskCreate(inc_time_task, "inc_time_task", 2048, NULL, 8, &inc_time_handle);
    xTaskCreate(dec_time_task, "dec_time_task", 2048, NULL, 8, &dec_time_handle);


    
    countdown_timer = xtimercreate("countdown", pdms_to_ticks(1000),
                                    pdtrue, null, countdown_timer_callback);



   /*---------------------------------- ISR Config ---------------------------------------*/   
    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

//**************************************************************************************
//                                  MAIN LOOP
//**************************************************************************************

    while(gpio_get_level(GPIO_SENSE_INPUT) == 0){
   
        display_set_mode(DISPLAY_MODE_DONE); //print "remove object"
        printf("Remove Object\n");      //TODO: for debugging
    }

    //initalize display
    display_init(); 

    //set default value for timer and update value in diplay component
    set_time = DEFAULT_TIME;
    display_update_time(set_time);
   
    printf("Set Time:\n%02d:%02d\n", (set_time/60), (set_time%60));
 
    //link GPIOn to specific ISR handler (enables all interrupts)
    gpio_isr_handler_add(GPIO_SENSE_INPUT, sense_isr_handler, NULL);
    gpio_isr_handler_add(GPIO_INC_TIME, inc_time_isr_handler, NULL);
    gpio_isr_handler_add(GPIO_DEC_TIME, dec_time_isr_handler, NULL);
    gpio_isr_handler_add(GPIO_BRIGHTNESS, brightness_isr_handler, NULL);

   //infinite loop to wait for interrupts
    for(;;) {
        display_set_mode(DISPLAY_MODE_SETTING); //display set time on LCD
        printf("set time, wating for object\n"); //TODO: for debugging
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

}
