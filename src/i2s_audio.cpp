#include "i2s_audio.h"

#include "app_config.h"
#include "settings.h"
#include "web_log.h"

#include <driver/i2s.h>

void i2sDeinit()
{
  i2s_driver_uninstall(I2S_NUM_0);
}

void i2sInitFromSettings()
{
  i2s_config_t cfg = {
      .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate          = (uint32_t)g_settings.sampleRate,
      .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags     = 0,
      .dma_buf_count        = g_settings.dmaBufCount,
      .dma_buf_len          = g_settings.dmaBufLen,
      .use_apll             = false,
      .tx_desc_auto_clear   = true,
      .fixed_mclk           = 0,
  };

  i2s_pin_config_t pins = {.bck_io_num   = I2S_BCLK,
                           .ws_io_num    = I2S_LRC,
                           .data_out_num = I2S_DOUT,
                           .data_in_num  = I2S_PIN_NO_CHANGE};

  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
  i2s_zero_dma_buffer(I2S_NUM_0);

  WebLog.println("[I2S] ✅ Initialized from settings.json");
  WebLog.print("[I2S] sample_rate=");
  WebLog.println(g_settings.sampleRate);
  WebLog.print("[I2S] dma_buf_count=");
  WebLog.println(g_settings.dmaBufCount);
  WebLog.print("[I2S] dma_buf_len=");
  WebLog.println(g_settings.dmaBufLen);
}
