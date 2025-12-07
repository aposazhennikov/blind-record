#include "settings.h"
#include "audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char* TAG = "settings";

// Settings state
static bool settings_active = false;
static settings_entry_mode_t entry_mode = SETTINGS_MODE_SHORT_PRESS;
static settings_position_t settings_pos = SETTINGS_POS_VOLUME;
static settings_long_position_t settings_long_pos =
    SETTINGS_POS_DEFAULT_FOLDER;

// Settings values
static uint8_t volume_of_system_messages = 3;  // Default volume level
static language_t current_language = LANGUAGE_RUSSIAN;
static char default_folder = 'C';
static uint8_t datetime_hour = 12;
static uint8_t datetime_minute = 0;
static uint8_t datetime_day = 1;
static uint8_t datetime_month = 1;
static uint16_t datetime_year = 2024;

// Submenu states
static bool in_volume_menu = false;
static bool in_language_menu = false;
static bool in_datetime_menu = false;
static datetime_state_t datetime_state = DT_STATE_HOUR;

// Volume menu position (1-5)
static uint8_t volume_menu_pos = 1;

// Language menu position
static uint8_t language_menu_pos = 0;

// Date/time editing values (temporary, not saved until completion)
static uint8_t temp_hour = 12;
static uint8_t temp_minute = 0;
static uint8_t temp_day = 1;
static uint8_t temp_month = 1;
static uint16_t temp_year = 2024;

// Language names for TTS
static const char* language_names[LANGUAGE_COUNT] = {
    "русский", "english"  // Russian, English
};

esp_err_t settings_init(void) {
  ESP_LOGI(TAG, "Settings system initialized");
  return ESP_OK;
}

void settings_init_mode(settings_entry_mode_t entry_mode_param) {
  settings_active = true;
  entry_mode = entry_mode_param;

  // Reset submenu states
  in_volume_menu = false;
  in_language_menu = false;
  in_datetime_menu = false;

  if (entry_mode == SETTINGS_MODE_SHORT_PRESS) {
    settings_pos = SETTINGS_POS_VOLUME;
    ESP_LOGI(TAG, "Entering settings mode (short press)");
    audio_speak("Режим настроек");
    vTaskDelay(pdMS_TO_TICKS(500));
    audio_speak("Громкость системных сообщений");
  } else {
    settings_long_pos = SETTINGS_POS_DEFAULT_FOLDER;
    ESP_LOGI(TAG, "Entering settings mode (long press)");
    char msg[64];
    snprintf(msg, sizeof(msg), "Выбрана папка %c по умолчанию", default_folder);
    audio_speak(msg);
  }
}

bool settings_is_active(void) { return settings_active; }

void settings_exit_mode(void) {
  settings_active = false;
  in_volume_menu = false;
  in_language_menu = false;
  in_datetime_menu = false;
  ESP_LOGI(TAG, "Exiting settings mode");
}

static void speak_volume_level(uint8_t level) {
  char msg[32];
  snprintf(msg, sizeof(msg), "Уровень громкости %d", level);
  audio_speak(msg);
}

static void speak_language_name(language_t lang) {
  if (lang < LANGUAGE_COUNT) {
    audio_speak(language_names[lang]);
  }
}

static void speak_datetime_value(void) {
  char msg[64];
  switch (datetime_state) {
    case DT_STATE_HOUR:
      snprintf(msg, sizeof(msg), "Час %d", temp_hour);
      break;
    case DT_STATE_MINUTE:
      snprintf(msg, sizeof(msg), "Минута %d", temp_minute);
      break;
    case DT_STATE_DAY:
      snprintf(msg, sizeof(msg), "День %d", temp_day);
      break;
    case DT_STATE_MONTH:
      snprintf(msg, sizeof(msg), "Месяц %d", temp_month);
      break;
    case DT_STATE_YEAR:
      snprintf(msg, sizeof(msg), "Год %d", temp_year);
      break;
  }
  audio_speak(msg);
}

esp_err_t settings_handle_button(button_event_t event) {
  if (!settings_active) {
    return ESP_ERR_INVALID_STATE;
  }

  if (event == BUTTON_NONE) {
    return ESP_OK;
  }

  // Handle BACK button - exit submenus or settings mode
  if (event == BUTTON_BACK) {
    if (in_volume_menu) {
      in_volume_menu = false;
      audio_speak("Громкость системных сообщений");
      return ESP_OK;
    }
    if (in_language_menu) {
      in_language_menu = false;
      audio_speak("Выбор языка");
      return ESP_OK;
    }
    if (in_datetime_menu) {
      in_datetime_menu = false;
      // Don't save changes if exiting
      temp_hour = datetime_hour;
      temp_minute = datetime_minute;
      temp_day = datetime_day;
      temp_month = datetime_month;
      temp_year = datetime_year;
      audio_speak("Дата и время");
      return ESP_OK;
    }
    // Exit settings mode completely
    settings_exit_mode();
    audio_speak("Выход из настроек");
    return ESP_OK;
  }

  // Handle navigation in submenus
  if (in_volume_menu) {
    if (event == BUTTON_UP) {
      if (volume_menu_pos > 1) {
        volume_menu_pos--;
      } else {
        volume_menu_pos = 5;  // Wrap around
      }
      speak_volume_level(volume_menu_pos);
      return ESP_OK;
    }
    if (event == BUTTON_DOWN) {
      if (volume_menu_pos < 5) {
        volume_menu_pos++;
      } else {
        volume_menu_pos = 1;  // Wrap around
      }
      speak_volume_level(volume_menu_pos);
      return ESP_OK;
    }
    if (event == BUTTON_OK) {
      volume_of_system_messages = volume_menu_pos;
      ESP_LOGI(TAG, "Volume set to %d", volume_of_system_messages);
      in_volume_menu = false;
      audio_speak("Громкость системных сообщений");
      return ESP_OK;
    }
    return ESP_OK;
  }

  if (in_language_menu) {
    if (event == BUTTON_UP) {
      if (language_menu_pos > 0) {
        language_menu_pos--;
      } else {
        language_menu_pos = LANGUAGE_COUNT - 1;  // Wrap around
      }
      speak_language_name((language_t)language_menu_pos);
      return ESP_OK;
    }
    if (event == BUTTON_DOWN) {
      if (language_menu_pos < LANGUAGE_COUNT - 1) {
        language_menu_pos++;
      } else {
        language_menu_pos = 0;  // Wrap around
      }
      speak_language_name((language_t)language_menu_pos);
      return ESP_OK;
    }
    if (event == BUTTON_OK) {
      current_language = (language_t)language_menu_pos;
      ESP_LOGI(TAG, "Language set to %d", current_language);
      in_language_menu = false;
      audio_speak("Выбор языка");
      return ESP_OK;
    }
    return ESP_OK;
  }

  if (in_datetime_menu) {
    if (event == BUTTON_UP) {
      switch (datetime_state) {
        case DT_STATE_HOUR:
          if (temp_hour < 23) {
            temp_hour++;
          } else {
            temp_hour = 0;
          }
          break;
        case DT_STATE_MINUTE:
          if (temp_minute < 59) {
            temp_minute++;
          } else {
            temp_minute = 0;
          }
          break;
        case DT_STATE_DAY:
          if (temp_day < 31) {
            temp_day++;
          } else {
            temp_day = 1;
          }
          break;
        case DT_STATE_MONTH:
          if (temp_month < 12) {
            temp_month++;
          } else {
            temp_month = 1;
          }
          break;
        case DT_STATE_YEAR:
          if (temp_year < 2099) {
            temp_year++;
          } else {
            temp_year = 2024;
          }
          break;
      }
      speak_datetime_value();
      return ESP_OK;
    }
    if (event == BUTTON_DOWN) {
      switch (datetime_state) {
        case DT_STATE_HOUR:
          if (temp_hour > 0) {
            temp_hour--;
          } else {
            temp_hour = 23;
          }
          break;
        case DT_STATE_MINUTE:
          if (temp_minute > 0) {
            temp_minute--;
          } else {
            temp_minute = 59;
          }
          break;
        case DT_STATE_DAY:
          if (temp_day > 1) {
            temp_day--;
          } else {
            temp_day = 31;
          }
          break;
        case DT_STATE_MONTH:
          if (temp_month > 1) {
            temp_month--;
          } else {
            temp_month = 12;
          }
          break;
        case DT_STATE_YEAR:
          if (temp_year > 2024) {
            temp_year--;
          } else {
            temp_year = 2099;
          }
          break;
      }
      speak_datetime_value();
      return ESP_OK;
    }
    if (event == BUTTON_OK) {
      // Move to next state or save
      switch (datetime_state) {
        case DT_STATE_HOUR:
          datetime_state = DT_STATE_MINUTE;
          speak_datetime_value();
          break;
        case DT_STATE_MINUTE:
          datetime_state = DT_STATE_DAY;
          speak_datetime_value();
          break;
        case DT_STATE_DAY:
          datetime_state = DT_STATE_MONTH;
          speak_datetime_value();
          break;
        case DT_STATE_MONTH:
          datetime_state = DT_STATE_YEAR;
          speak_datetime_value();
          break;
        case DT_STATE_YEAR:
          // Save all values
          datetime_hour = temp_hour;
          datetime_minute = temp_minute;
          datetime_day = temp_day;
          datetime_month = temp_month;
          datetime_year = temp_year;
          ESP_LOGI(TAG, "DateTime set to %02d:%02d %02d.%02d.%04d",
                   datetime_hour, datetime_minute, datetime_day,
                   datetime_month, datetime_year);
          in_datetime_menu = false;
          datetime_state = DT_STATE_HOUR;
          audio_speak("Дата и время");
          break;
      }
      return ESP_OK;
    }
    return ESP_OK;
  }

  // Handle navigation in main settings menu
  if (entry_mode == SETTINGS_MODE_SHORT_PRESS) {
    if (event == BUTTON_UP) {
      // Cyclic scrolling: Volume -> Language
      if (settings_pos == SETTINGS_POS_VOLUME) {
        settings_pos = SETTINGS_POS_LANGUAGE;
        audio_speak("Выбор языка");
        // Speak current language
        char msg[64];
        snprintf(msg, sizeof(msg), "%s %s", "русский английский",
                 language_names[current_language]);
        audio_speak(msg);
      } else {
        settings_pos = SETTINGS_POS_VOLUME;
        audio_speak("Громкость системных сообщений");
      }
      return ESP_OK;
    }
    if (event == BUTTON_DOWN) {
      // Cyclic scrolling: Volume <-> Language
      if (settings_pos == SETTINGS_POS_VOLUME) {
        settings_pos = SETTINGS_POS_LANGUAGE;
        audio_speak("Выбор языка");
        char msg[64];
        snprintf(msg, sizeof(msg), "%s %s", "русский английский",
                 language_names[current_language]);
        audio_speak(msg);
      } else {
        settings_pos = SETTINGS_POS_VOLUME;
        audio_speak("Громкость системных сообщений");
      }
      return ESP_OK;
    }
    if (event == BUTTON_OK) {
      if (settings_pos == SETTINGS_POS_VOLUME) {
        in_volume_menu = true;
        volume_menu_pos = volume_of_system_messages;
        speak_volume_level(volume_menu_pos);
      } else if (settings_pos == SETTINGS_POS_LANGUAGE) {
        in_language_menu = true;
        language_menu_pos = (uint8_t)current_language;
        speak_language_name(current_language);
      }
      return ESP_OK;
    }
  } else {  // SETTINGS_MODE_LONG_PRESS
    if (event == BUTTON_UP) {
      // Cyclic scrolling
      if (settings_long_pos == SETTINGS_POS_DEFAULT_FOLDER) {
        settings_long_pos = SETTINGS_POS_DATE_TIME;
        audio_speak("Дата и время");
      } else {
        settings_long_pos = SETTINGS_POS_DEFAULT_FOLDER;
        char msg[64];
        snprintf(msg, sizeof(msg), "Выбрана папка %c по умолчанию",
                 default_folder);
        audio_speak(msg);
      }
      return ESP_OK;
    }
    if (event == BUTTON_DOWN) {
      // Cyclic scrolling
      if (settings_long_pos == SETTINGS_POS_DEFAULT_FOLDER) {
        settings_long_pos = SETTINGS_POS_DATE_TIME;
        audio_speak("Дата и время");
      } else {
        settings_long_pos = SETTINGS_POS_DEFAULT_FOLDER;
        char msg[64];
        snprintf(msg, sizeof(msg), "Выбрана папка %c по умолчанию",
                 default_folder);
        audio_speak(msg);
      }
      return ESP_OK;
    }
    if (event == BUTTON_OK) {
      if (settings_long_pos == SETTINGS_POS_DEFAULT_FOLDER) {
        // TODO: Implement folder selection (A, B, C, etc.)
        // For now, just cycle through folders
        if (default_folder < 'Z') {
          default_folder++;
        } else {
          default_folder = 'A';
        }
        char msg[64];
        snprintf(msg, sizeof(msg), "Выбрана папка %c по умолчанию",
                 default_folder);
        audio_speak(msg);
      } else if (settings_long_pos == SETTINGS_POS_DATE_TIME) {
        in_datetime_menu = true;
        datetime_state = DT_STATE_HOUR;
        temp_hour = datetime_hour;
        temp_minute = datetime_minute;
        temp_day = datetime_day;
        temp_month = datetime_month;
        temp_year = datetime_year;
        speak_datetime_value();
      }
      return ESP_OK;
    }
  }

  return ESP_OK;
}

void settings_loop(void) {
  // Settings loop is event-driven via button events
  // No continuous processing needed
  vTaskDelay(pdMS_TO_TICKS(100));
}

uint8_t settings_get_volume(void) { return volume_of_system_messages; }

esp_err_t settings_set_volume(uint8_t volume) {
  if (volume < 1 || volume > 5) {
    return ESP_ERR_INVALID_ARG;
  }
  volume_of_system_messages = volume;
  return ESP_OK;
}

language_t settings_get_language(void) { return current_language; }

esp_err_t settings_set_language(language_t lang) {
  if (lang >= LANGUAGE_COUNT) {
    return ESP_ERR_INVALID_ARG;
  }
  current_language = lang;
  return ESP_OK;
}

char settings_get_default_folder(void) { return default_folder; }

esp_err_t settings_set_default_folder(char folder) {
  if (folder < 'A' || folder > 'Z') {
    return ESP_ERR_INVALID_ARG;
  }
  default_folder = folder;
  return ESP_OK;
}

esp_err_t settings_get_datetime(uint8_t* hour, uint8_t* minute, uint8_t* day,
                                 uint8_t* month, uint16_t* year) {
  if (!hour || !minute || !day || !month || !year) {
    return ESP_ERR_INVALID_ARG;
  }
  *hour = datetime_hour;
  *minute = datetime_minute;
  *day = datetime_day;
  *month = datetime_month;
  *year = datetime_year;
  return ESP_OK;
}

esp_err_t settings_set_datetime(uint8_t hour, uint8_t minute, uint8_t day,
                                 uint8_t month, uint16_t year) {
  if (hour > 23 || minute > 59 || day > 31 || month > 12 || year < 2024 ||
      year > 2099) {
    return ESP_ERR_INVALID_ARG;
  }
  datetime_hour = hour;
  datetime_minute = minute;
  datetime_day = day;
  datetime_month = month;
  datetime_year = year;
  return ESP_OK;
}

