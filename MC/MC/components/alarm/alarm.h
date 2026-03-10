#pragma once

#include "esp_err.h"

esp_err_t alarm_init(void);

void alarm_start(void);
void alarm_stop(void);

