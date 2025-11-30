#include "audio.h"
#include "battery.h"
#include "esp_log.h"

static const char* TAG = "audio";

esp_err_t audio_init(void) {
  ESP_LOGI(TAG, "Audio system initialized");
  // TODO: Initialize audio codec, I2S, DAC, etc.
  return ESP_OK;
}

esp_err_t audio_speak(const char* text) {
  ESP_LOGI(TAG, "Speaking: %s", text);
  // TODO: Implement TTS or audio playback
  // This could use ESP-ADF, external TTS chip, or pre-recorded audio files
  return ESP_OK;
}

esp_err_t audio_speak_battery_level(void) {
  battery_level_t level = battery_get_level();
  const char* level_text = NULL;

  switch (level) {
    case BATTERY_LEVEL_HIGH:
      level_text = "Уровень заряда батареи Высокий";
      break;
    case BATTERY_LEVEL_MEDIUM:
      level_text = "Уровень заряда батареи Средний";
      break;
    case BATTERY_LEVEL_LOW:
      level_text = "Уровень заряда батареи Низкий";
      break;
    default:
      level_text = "Уровень заряда батареи Неизвестен";
      break;
  }

  return audio_speak(level_text);
}

esp_err_t audio_speak_shutdown(void) {
  return audio_speak("Диктофон выключается");
}

void audio_deinit(void) {
  ESP_LOGI(TAG, "Audio system deinitialized");
  // TODO: Cleanup audio resources
}

