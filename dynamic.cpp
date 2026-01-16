#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <driver/i2s.h>

#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK  18
#define SD_CS   5

#define I2S_DOUT 21   // DIN MAX98357A
#define I2S_BCLK 26   // BCLK
#define I2S_LRC  25   // LRC

void i2sInit() {
  i2s_config_t cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // стерео поток
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = 256,
    .use_apll = false,
    .tx_desc_auto_clear = true,
    .fixed_mclk = 0,
  };

  i2s_pin_config_t pins = {
    .bck_io_num = I2S_BCLK,
    .ws_io_num = I2S_LRC,
    .data_out_num = I2S_DOUT,
    .data_in_num = I2S_PIN_NO_CHANGE
  };

  i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pins);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

void playWavMono16(const char *path) {
  File f = SD.open(path);
  if (!f) {
    Serial.println("❌ Не удалось открыть WAV");
    return;
  }

  // Пропускаем заголовок 44 байта (мы уже знаем формат)
  uint8_t header[44];
  if (f.read(header, 44) != 44) {
    Serial.println("❌ Ошибка чтения заголовка");
    f.close();
    return;
  }

  Serial.println("▶️ Воспроизведение WAV 16-bit mono 44100 Hz");

  const size_t inBufBytes = 1024;      // моно
  const size_t inSamples = inBufBytes / 2;
  int16_t inBuf[inSamples];

  int16_t outBuf[inSamples * 2];       // стерео (L+R)
  size_t bytesRead, bytesWritten;

  while ((bytesRead = f.read((uint8_t*)inBuf, inBufBytes)) > 0) {
    size_t samples = bytesRead / 2;

    // моно → стерео
    for (size_t i = 0; i < samples; i++) {
      outBuf[2*i]     = inBuf[i]; // Left
      outBuf[2*i + 1] = inBuf[i]; // Right
    }

    size_t outBytes = samples * 2 * sizeof(int16_t);
    i2s_write(I2S_NUM_0, outBuf, outBytes, &bytesWritten, portMAX_DELAY);
  }

  f.close();
  Serial.println("✅ Воспроизведение завершено");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== WAV PLAYER 16-bit MONO ===");

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS)) {
    Serial.println("❌ SD не инициализирована");
    return;
  }
  Serial.println("✅ SD готова");

  if (!SD.exists("/test.wav")) {
    Serial.println("❌ /test.wav не найден");
    return;
  }

  i2sInit();
  playWavMono16("/test.wav");
}

void loop() {}
