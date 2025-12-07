#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stdint.h>

// Settings entry mode (short press vs long press)
typedef enum {
  SETTINGS_MODE_SHORT_PRESS, // Volume, Language
  SETTINGS_MODE_LONG_PRESS   // Default folder, Date/Time
} settings_entry_mode_t;

// Settings position for short press menu
typedef enum { SETTINGS_POS_VOLUME, SETTINGS_POS_LANGUAGE } settings_position_t;

// Settings position for long press menu
typedef enum { SETTINGS_POS_DEFAULT_FOLDER, SETTINGS_POS_DATE_TIME } settings_long_position_t;

// Language selection
typedef enum {
  LANGUAGE_RUSSIAN,
  LANGUAGE_ENGLISH,
  LANGUAGE_COUNT // For future expansion (Chinese, Uzbek, etc.)
} language_t;

// Date/Time editing state
typedef enum {
  DT_STATE_HOUR,
  DT_STATE_MINUTE,
  DT_STATE_DAY,
  DT_STATE_MONTH,
  DT_STATE_YEAR
} datetime_state_t;

// Button events
typedef enum {
  BUTTON_NONE,
  BUTTON_UP,
  BUTTON_DOWN,
  BUTTON_OK,
  BUTTON_BACK,
  BUTTON_F3_SHORT,
  BUTTON_F3_LONG
} button_event_t;

// Initialize settings system
esp_err_t settings_init(void);

// Handle button event in settings mode
esp_err_t settings_handle_button(button_event_t event);

// Get current volume level (1-5)
uint8_t settings_get_volume(void);

// Set volume level (1-5)
esp_err_t settings_set_volume(uint8_t volume);

// Get current language
language_t settings_get_language(void);

// Set language
esp_err_t settings_set_language(language_t lang);

// Get default folder ('A', 'B', 'C', etc.)
char settings_get_default_folder(void);

// Set default folder
esp_err_t settings_set_default_folder(char folder);

// Get date/time values
esp_err_t settings_get_datetime(uint8_t* hour, uint8_t* minute, uint8_t* day, uint8_t* month,
                                uint16_t* year);

// Set date/time values
esp_err_t settings_set_datetime(uint8_t hour, uint8_t minute, uint8_t day, uint8_t month,
                                uint16_t year);

// Settings mode loop (called from main loop)
void settings_loop(void);

// Settings mode init (called when entering settings mode)
void settings_init_mode(settings_entry_mode_t entry_mode);

// Check if settings mode is active
bool settings_is_active(void);

// Exit settings mode (called when BACK button is pressed at top level)
void settings_exit_mode(void);
