#pragma once

#include <stdint.h>

typedef enum {
  MODE_MAIN_MENU,
  MODE_RECORDING,
  MODE_PLAYBACK,
  MODE_SETTINGS,
  MODE_SHUTDOWN
} recorder_mode_t;

recorder_mode_t get_current_mode(void);
void set_current_mode(recorder_mode_t mode);

