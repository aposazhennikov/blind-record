#pragma once

#include "esp_err.h"

#include <stdint.h>

typedef enum {
  BATTERY_LEVEL_HIGH,   // > 70%
  BATTERY_LEVEL_MEDIUM, // 30-70%
  BATTERY_LEVEL_LOW     // < 30%
} battery_level_t;

esp_err_t       battery_init(void);
battery_level_t battery_get_level(void);
uint8_t         battery_get_percentage(void);
float           battery_get_voltage(void);
