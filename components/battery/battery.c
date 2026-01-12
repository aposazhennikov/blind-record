// Battery monitoring implementation for ESP32
// Reference:
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc_oneshot.html
// Reference:
// https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc_calibration.html
//
// For battery voltage measurement:
// 1. Connect battery to ADC pin via voltage divider if voltage > 3.3V
// 2. Configure ADC channel, attenuation, and bitwidth
// 3. Use calibration for accurate voltage reading
// 4. Convert voltage to percentage based on battery characteristics

#include "battery.h"

#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char* TAG = "battery";

#define BATTERY_ADC_CHANNEL ADC_CHANNEL_0
#define BATTERY_ADC_UNIT ADC_UNIT_1
#define BATTERY_ADC_ATTEN ADC_ATTEN_DB_12
#define BATTERY_ADC_BITWIDTH ADC_BITWIDTH_12

// Voltage divider: if battery is > 3.3V, use divider
// Assuming 2:1 divider (R1=R2), full scale = 6.6V
#define VOLTAGE_DIVIDER_RATIO 2.0f
#define ADC_REF_VOLTAGE 3.3f
#define BATTERY_FULL_VOLTAGE 4.2f
#define BATTERY_EMPTY_VOLTAGE 3.0f

static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t         adc_cali_handle      = NULL;
static bool                      adc_calibration_init = false;

static bool adc_calibration_init_func(adc_unit_t unit, adc_atten_t atten,
                                      adc_cali_handle_t* out_handle)
{
  adc_cali_handle_t handle     = NULL;
  esp_err_t         ret        = ESP_FAIL;
  bool              calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
  if (!calibrated) {
    ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id  = unit,
        .atten    = atten,
        .bitwidth = BATTERY_ADC_BITWIDTH,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
    if (ret == ESP_OK) {
      calibrated = true;
    }
  }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
  if (!calibrated) {
    ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id  = unit,
        .atten    = atten,
        .bitwidth = BATTERY_ADC_BITWIDTH,
    };
    ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
    if (ret == ESP_OK) {
      calibrated = true;
    }
  }
#endif

  *out_handle = handle;
  if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Calibration Success");
  } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
    ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
  } else {
    ESP_LOGE(TAG, "Invalid arg or no memory");
  }

  return calibrated;
}

esp_err_t battery_init(void)
{
  adc_oneshot_unit_init_cfg_t init_config1 = {
      .unit_id = BATTERY_ADC_UNIT,
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

  adc_oneshot_chan_cfg_t config = {
      .bitwidth = BATTERY_ADC_BITWIDTH,
      .atten    = BATTERY_ADC_ATTEN,
  };
  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, BATTERY_ADC_CHANNEL, &config));

  adc_calibration_init =
      adc_calibration_init_func(BATTERY_ADC_UNIT, BATTERY_ADC_ATTEN, &adc_cali_handle);

  ESP_LOGI(TAG, "Battery monitoring initialized");
  return ESP_OK;
}

float battery_get_voltage(void)
{
  int adc_raw;
  int voltage = 0;

  ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, BATTERY_ADC_CHANNEL, &adc_raw));
  ESP_LOGD(TAG, "ADC raw: %d", adc_raw);

  if (adc_calibration_init) {
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, adc_raw, &voltage));
    ESP_LOGD(TAG, "ADC voltage: %d mV", voltage);
  } else {
    // Fallback: approximate conversion
    voltage = (adc_raw * ADC_REF_VOLTAGE * 1000) / 4095;
  }

  // Apply voltage divider if used
  float battery_voltage = (float)voltage / 1000.0f * VOLTAGE_DIVIDER_RATIO;
  return battery_voltage;
}

uint8_t battery_get_percentage(void)
{
  float   voltage    = battery_get_voltage();
  uint8_t percentage = 0;

  if (voltage >= BATTERY_FULL_VOLTAGE) {
    percentage = 100;
  } else if (voltage <= BATTERY_EMPTY_VOLTAGE) {
    percentage = 0;
  } else {
    percentage = (uint8_t)(((voltage - BATTERY_EMPTY_VOLTAGE) /
                            (BATTERY_FULL_VOLTAGE - BATTERY_EMPTY_VOLTAGE)) *
                           100.0f);
  }

  return percentage;
}

battery_level_t battery_get_level(void)
{
  uint8_t percentage = battery_get_percentage();

  if (percentage > 70) {
    return BATTERY_LEVEL_HIGH;
  } else if (percentage >= 30) {
    return BATTERY_LEVEL_MEDIUM;
  } else {
    return BATTERY_LEVEL_LOW;
  }
}
