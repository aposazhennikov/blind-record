#pragma once

#include "esp_err.h"

esp_err_t audio_init(void);
esp_err_t audio_speak(const char* text);
esp_err_t audio_speak_battery_level(void);
esp_err_t audio_speak_shutdown(void);
void      audio_deinit(void);
