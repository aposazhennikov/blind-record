#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "modes.h"
#include "battery.h"
#include "audio.h"

static const char* TAG = "main";

#define AUTO_SHUTDOWN_TIMEOUT_MS (60 * 60 * 1000)  // 60 minutes
#define BUTTON_CHECK_INTERVAL_MS 100

static recorder_mode_t current_mode = MODE_MAIN_MENU;
static TickType_t last_activity_time = 0;
static bool shutdown_requested = false;

recorder_mode_t get_current_mode(void) { return current_mode; }

void set_current_mode(recorder_mode_t mode) {
  current_mode = mode;
  last_activity_time = xTaskGetTickCount();
}

static void button_task(void* pvParameters) {
  // TODO: Initialize GPIO for button input
  // This is a placeholder - actual button implementation depends on hardware
  while (1) {
    // TODO: Read button state
    // For now, just reset activity timer periodically
    // In real implementation, this would be triggered by button ISR
    vTaskDelay(pdMS_TO_TICKS(BUTTON_CHECK_INTERVAL_MS));
  }
}

static void auto_shutdown_task(void* pvParameters) {
  while (1) {
    vTaskDelay(pdMS_TO_TICKS(1000));  // Check every second

    if (shutdown_requested) {
      break;
    }

    TickType_t current_time = xTaskGetTickCount();
    TickType_t elapsed = current_time - last_activity_time;

    if (elapsed >= pdMS_TO_TICKS(AUTO_SHUTDOWN_TIMEOUT_MS)) {
      ESP_LOGW(TAG, "Auto-shutdown triggered after 60 minutes of inactivity");
      audio_speak_shutdown();
      vTaskDelay(pdMS_TO_TICKS(2000));  // Wait for announcement to finish
      set_current_mode(MODE_SHUTDOWN);
      shutdown_requested = true;
      esp_restart();  // Or proper shutdown sequence
    }
  }

  vTaskDelete(NULL);
}

static void main_menu_init(void) {
  ESP_LOGI(TAG, "Entering MAIN_MENU mode");

  // Check battery level and announce
  battery_level_t level = battery_get_level();
  ESP_LOGI(TAG, "Battery level: %d%%", battery_get_percentage());
  audio_speak_battery_level();

  // Reset activity timer
  last_activity_time = xTaskGetTickCount();
}

static void main_menu_loop(void) {
  // Main menu is in waiting state
  // Button presses will be handled by button_task
  // Activity timer is updated by set_current_mode() when mode changes
  vTaskDelay(pdMS_TO_TICKS(100));
}

void app_main(void) {
  ESP_LOGI(TAG, "Blind Record - Voice Recorder Starting");

  // Initialize components
  ESP_ERROR_CHECK(battery_init());
  ESP_ERROR_CHECK(audio_init());

  // Initialize main menu
  main_menu_init();

  // Create tasks
  xTaskCreate(button_task, "button_task", 2048, NULL, 5, NULL);
  xTaskCreate(auto_shutdown_task, "auto_shutdown", 2048, NULL, 5, NULL);

  // Main loop
  while (1) {
    switch (get_current_mode()) {
      case MODE_MAIN_MENU:
        main_menu_loop();
        break;
      case MODE_RECORDING:
        // TODO: Implement recording mode
        ESP_LOGI(TAG, "Recording mode - TODO");
        vTaskDelay(pdMS_TO_TICKS(1000));
        break;
      case MODE_PLAYBACK:
        // TODO: Implement playback mode
        ESP_LOGI(TAG, "Playback mode - TODO");
        vTaskDelay(pdMS_TO_TICKS(1000));
        break;
      case MODE_SETTINGS:
        // TODO: Implement settings mode
        ESP_LOGI(TAG, "Settings mode - TODO");
        vTaskDelay(pdMS_TO_TICKS(1000));
        break;
      case MODE_SHUTDOWN:
        ESP_LOGI(TAG, "Shutting down...");
        audio_deinit();
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
        break;
    }
  }
}

