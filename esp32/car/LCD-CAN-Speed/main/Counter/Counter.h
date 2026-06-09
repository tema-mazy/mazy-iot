#pragma once

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "driver/twai.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "RGB.h"
#include <stdbool.h>

extern volatile uint16_t CPS;
extern volatile bool can_connected;
void Counter_Init(void);
