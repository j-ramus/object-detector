idf_component_register(SRCS "alarm.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES 
                           esp_driver_dac 
                           esp_timer
                          )