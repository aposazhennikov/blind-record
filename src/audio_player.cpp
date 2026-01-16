#include "audio_player.h"

#include "audio_progress.h"
#include "auto_tuner.h"
#include "equalizer.h"
#include "i2s_audio.h"
#include "mp3_player.h"
#include "resampler.h"
#include "sd_browser.h"
#include "settings.h"
#include "wav_reader.h"
#include "web_log.h"

#include <SD.h>
#include <driver/i2s.h>

// Audio format type.
enum AudioFormat { FORMAT_UNKNOWN, FORMAT_WAV, FORMAT_MP3 };

static TaskHandle_t         audioTaskHandle      = nullptr;
static volatile bool        g_audioRunning       = false;
static volatile bool        g_audioStopRequested = false;
static volatile AudioFormat g_currentFormat      = FORMAT_UNKNOWN;

// I2S write timeout in ticks (100ms). Allows checking stop flag periodically.
static const TickType_t I2S_WRITE_TIMEOUT = pdMS_TO_TICKS(100);

// Detect audio format by file extension.
static AudioFormat detectFormat(const String& path)
{
  String lower = path;
  lower.toLowerCase();

  if (lower.endsWith(".wav"))
    return FORMAT_WAV;
  if (lower.endsWith(".mp3"))
    return FORMAT_MP3;

  return FORMAT_UNKNOWN;
}

bool audioIsRunning()
{
  return g_audioRunning;
}

// WAV playback task (existing implementation).
static void wavPlaybackTask(void* param)
{
  (void)param;

  WebLog.println("[AUDIO] WAV task started");
  g_audioRunning       = true;
  g_audioStopRequested = false;

  // Get current file from settings or sd_browser.
  String path = g_settings.currentFile;
  if (path.length() == 0) {
    path = sdGetCurrentFile();
  }

  File f = SD.open(path);
  if (!f) {
    WebLog.print("[AUDIO] ❌ Cannot open: ");
    WebLog.println(path);
    g_audioRunning = false;
    vTaskDelete(NULL);
    return;
  }

  WebLog.print("[AUDIO] ✅ Opened: ");
  WebLog.println(path);

  WavInfo info = parseWavHeader(f);
  if (!info.ok) {
    WebLog.println("[AUDIO] ❌ WAV header invalid");
    f.close();
    g_audioRunning = false;
    vTaskDelete(NULL);
    return;
  }

  if (info.audioFormat != 1 || info.bitsPerSample != 16) {
    WebLog.println("[AUDIO] ❌ WAV must be PCM 16-bit");
    f.close();
    g_audioRunning = false;
    vTaskDelete(NULL);
    return;
  }

  if (info.numChannels != 1 && info.numChannels != 2) {
    WebLog.println("[AUDIO] ❌ WAV must be MONO or STEREO");
    f.close();
    g_audioRunning = false;
    vTaskDelete(NULL);
    return;
  }

  WebLog.print("[AUDIO] WAV channels = ");
  WebLog.println(info.numChannels);

  // Initialize progress tracking.
  progressReset(path, info.dataSize, info.sampleRate, info.numChannels, info.bitsPerSample);

  // Initialize resampling if needed and enabled.
  uint32_t targetSampleRate = (uint32_t)g_settings.sampleRate;
  if (g_settings.resamplingEnabled && info.sampleRate != targetSampleRate) {
    resamplerInit(info.sampleRate, targetSampleRate);
    WebLog.print("[AUDIO] Resampling: ");
    WebLog.print(info.sampleRate);
    WebLog.print(" -> ");
    WebLog.print(targetSampleRate);
    WebLog.println(" Hz");
  } else {
    // No resampling - reconfigure I2S to match WAV.
    if (info.sampleRate != targetSampleRate) {
      WebLog.print("[AUDIO] ⚠️ WAV sampleRate (");
      WebLog.print(info.sampleRate);
      WebLog.print(") != settings (");
      WebLog.print(targetSampleRate);
      WebLog.println(")");

      if (!g_settings.resamplingEnabled) {
        WebLog.println("[AUDIO] 🔧 Reconfiguring I2S to match WAV file...");
        esp_err_t err = i2s_set_sample_rates(I2S_NUM_0, info.sampleRate);
        if (err == ESP_OK) {
          WebLog.print("[AUDIO] ✅ I2S reconfigured to ");
          WebLog.print(info.sampleRate);
          WebLog.println(" Hz");
        } else {
          WebLog.print("[AUDIO] ❌ Failed to reconfigure I2S: ");
          WebLog.println(esp_err_to_name(err));
        }
      }
    }
    resamplerInit(info.sampleRate, info.sampleRate); // Passthrough.
  }

  // Initialize EQ with current sample rate.
  if (g_settings.eqEnabled) {
    g_eqSettings.band60Hz  = g_settings.eq.band60Hz;
    g_eqSettings.band250Hz = g_settings.eq.band250Hz;
    g_eqSettings.band1kHz  = g_settings.eq.band1kHz;
    g_eqSettings.band4kHz  = g_settings.eq.band4kHz;
    g_eqSettings.band12kHz = g_settings.eq.band12kHz;
    g_eqSettings.enabled   = true;
    eqUpdateCoefficients(resamplerIsActive() ? targetSampleRate : info.sampleRate);
    WebLog.println("[AUDIO] EQ enabled");
  } else {
    g_eqSettings.enabled = false;
  }

  f.seek(info.dataOffset);

  int inBytes = g_settings.inBufBytes;
  if (inBytes < 512)
    inBytes = 512;
  if (inBytes > 8192)
    inBytes = 8192;

  int bytesPerFrame  = (int)info.numChannels * (int)sizeof(int16_t);
  int framesPerChunk = inBytes / bytesPerFrame;
  if (framesPerChunk < 1)
    framesPerChunk = 1;

  int inSamplesTotal  = framesPerChunk * info.numChannels;
  int outSamplesTotal = framesPerChunk * 2;
  int resampleOutMax  = (int)(framesPerChunk * 2.0f) + 16;

  int16_t* inBuf       = (int16_t*)malloc(inSamplesTotal * sizeof(int16_t));
  int16_t* outBuf      = (int16_t*)malloc(outSamplesTotal * sizeof(int16_t));
  int16_t* resampleBuf = nullptr;

  if (resamplerIsActive()) {
    resampleBuf = (int16_t*)malloc(resampleOutMax * 2 * sizeof(int16_t));
  }

  if (!inBuf || !outBuf || (resamplerIsActive() && !resampleBuf)) {
    WebLog.println("[AUDIO] ❌ malloc failed");
    if (inBuf)
      free(inBuf);
    if (outBuf)
      free(outBuf);
    if (resampleBuf)
      free(resampleBuf);
    f.close();
    g_audioRunning = false;
    progressStop();
    vTaskDelete(NULL);
    return;
  }

  WebLog.print("[AUDIO] Buffer inBytes=");
  WebLog.println(inBytes);
  WebLog.print("[AUDIO] FramesPerChunk=");
  WebLog.println(framesPerChunk);

  uint32_t bytesLeft   = info.dataSize;
  uint32_t bytesPlayed = 0;

  tunerResetStats();

  while (bytesLeft > 0 && !g_audioStopRequested) {
    size_t toRead    = (bytesLeft > (uint32_t)inBytes) ? (size_t)inBytes : (size_t)bytesLeft;
    size_t bytesRead = f.read((uint8_t*)inBuf, toRead);

    if (bytesRead == 0)
      break;
    bytesLeft -= bytesRead;
    bytesPlayed += bytesRead;

    progressUpdate(bytesPlayed);

    size_t framesRead = bytesRead / bytesPerFrame;
    float  vol        = g_settings.volume;

    for (size_t i = 0; i < framesRead; i++) {
      int16_t L = 0;
      int16_t R = 0;

      if (info.numChannels == 1) {
        L = inBuf[i];
        R = inBuf[i];
      } else {
        L = inBuf[i * 2];
        R = inBuf[i * 2 + 1];
      }

      int32_t sL = (int32_t)((float)L * vol);
      int32_t sR = (int32_t)((float)R * vol);

      if (sL > 32767)
        sL = 32767;
      if (sL < -32768)
        sL = -32768;
      if (sR > 32767)
        sR = 32767;
      if (sR < -32768)
        sR = -32768;

      outBuf[2 * i]     = (int16_t)sL;
      outBuf[2 * i + 1] = (int16_t)sR;
    }

    if (g_eqSettings.enabled) {
      eqProcessBuffer(outBuf, framesRead, info.sampleRate);
    }

    int16_t* finalBuf    = outBuf;
    size_t   finalFrames = framesRead;

    if (resamplerIsActive() && resampleBuf) {
      finalFrames = resamplerProcess(outBuf, framesRead, resampleBuf, resampleOutMax);
      finalBuf    = resampleBuf;
    }

    size_t outBytes     = finalFrames * 2 * sizeof(int16_t);
    size_t totalWritten = 0;

    while (totalWritten < outBytes && !g_audioStopRequested) {
      size_t    written = 0;
      esp_err_t err     = i2s_write(I2S_NUM_0, (uint8_t*)finalBuf + totalWritten,
                                    outBytes - totalWritten, &written, I2S_WRITE_TIMEOUT);

      if (err != ESP_OK || written == 0) {
        if (g_settings.autoTuneEnabled) {
          tunerReportUnderrun();
        }
      } else {
        if (g_settings.autoTuneEnabled) {
          tunerReportSuccess();
        }
      }

      totalWritten += written;
    }

    if (g_settings.autoTuneEnabled && g_tunerStats.totalChunks % 200 == 0) {
      tunerCheck();
    }

    taskYIELD();
  }

  free(inBuf);
  free(outBuf);
  if (resampleBuf)
    free(resampleBuf);
  f.close();

  i2s_zero_dma_buffer(I2S_NUM_0);
  progressStop();

  if (g_audioStopRequested)
    WebLog.println("[AUDIO] ⏹ WAV stopped by request");
  else
    WebLog.println("[AUDIO] ✅ WAV playback finished");

  g_audioRunning  = false;
  audioTaskHandle = nullptr;
  vTaskDelete(NULL);
}

// MP3 playback task.
static void mp3PlaybackTask(void* param)
{
  (void)param;

  WebLog.println("[AUDIO] MP3 task started");
  g_audioRunning       = true;
  g_audioStopRequested = false;

  String path = g_settings.currentFile;
  if (path.length() == 0) {
    path = sdGetCurrentFile();
  }

  // Initialize MP3 player.
  mp3Init();

  if (!mp3StartFile(path)) {
    WebLog.println("[AUDIO] ❌ Failed to start MP3");
    g_audioRunning  = false;
    audioTaskHandle = nullptr;
    vTaskDelete(NULL);
    return;
  }

  // Initialize progress tracking.
  progressReset(path, 0, mp3GetSampleRate(), mp3GetChannels(), mp3GetBitsPerSample());
  g_audioProgress.totalMs = mp3GetDurationMs();

  // Main playback loop.
  while (!g_audioStopRequested && !mp3IsStopRequested()) {
    // Pump the MP3 decoder.
    if (!mp3Loop()) {
      break; // Playback finished or stopped.
    }

    // Update progress.
    g_audioProgress.playedMs = mp3GetPositionMs();
    if (g_audioProgress.totalMs > 0) {
      g_audioProgress.percent =
          (uint8_t)((uint64_t)g_audioProgress.playedMs * 100 / g_audioProgress.totalMs);
      if (g_audioProgress.percent > 100)
        g_audioProgress.percent = 100;
    }

    // Small delay to prevent watchdog.
    vTaskDelay(1);
  }

  // Cleanup is already done by mp3Stop() or mp3Loop().
  progressStop();

  if (g_audioStopRequested || mp3IsStopRequested())
    WebLog.println("[AUDIO] ⏹ MP3 stopped by request");
  else
    WebLog.println("[AUDIO] ✅ MP3 playback finished");

  g_audioRunning  = false;
  audioTaskHandle = nullptr;
  vTaskDelete(NULL);
}

void audioStop()
{
  if (!g_audioRunning) {
    WebLog.println("[AUDIO] Stop requested, but audio not running");
    return;
  }

  WebLog.println("[AUDIO] Stop requested...");
  g_audioStopRequested = true;

  // For MP3, signal stop (cleanup will happen in task).
  if (g_currentFormat == FORMAT_MP3) {
    mp3Stop();
  }

  // Wait for task to finish.
  for (int i = 0; i < 100; i++) {
    if (!g_audioRunning)
      break;
    delay(20);
  }

  if (g_audioRunning && audioTaskHandle != nullptr) {
    WebLog.println("[AUDIO] ⚠️ Force deleting stuck audio task");
    vTaskDelete(audioTaskHandle);
    audioTaskHandle = nullptr;
    g_audioRunning  = false;

    // If MP3 was force-killed, cleanup resources.
    if (g_currentFormat == FORMAT_MP3) {
      mp3Cleanup();
    }
  }

  // Only clear DMA buffer for WAV (we manage I2S).
  // For MP3, ESP8266Audio manages its own I2S.
  if (g_currentFormat == FORMAT_WAV) {
    i2s_zero_dma_buffer(I2S_NUM_0);
  }

  progressStop();

  WebLog.println("[AUDIO] Stop complete");
}

void audioStart()
{
  audioStartFile(g_settings.currentFile);
}

void audioStartFile(const String& path)
{
  if (g_audioRunning) {
    WebLog.println("[AUDIO] Already running, stopping first...");
    audioStop();
  }

  WebLog.print("[AUDIO] Starting playback: ");
  WebLog.println(path);

  // Detect format.
  g_currentFormat = detectFormat(path);

  if (g_currentFormat == FORMAT_UNKNOWN) {
    WebLog.println("[AUDIO] ❌ Unknown audio format. Supported: .wav, .mp3");
    return;
  }

  // Update current file in settings and save.
  g_settings.currentFile = path;
  sdSetCurrentFile(path);

  // Save to settings.json so it persists across reboots.
  settingsSaveToSD();

  g_audioStopRequested = false;

  if (g_currentFormat == FORMAT_WAV) {
    // WAV uses I2S directly.
    i2sDeinit();
    i2sInitFromSettings();
    xTaskCreatePinnedToCore(wavPlaybackTask, "wavTask", 10240, nullptr, 2, &audioTaskHandle, 1);
  } else if (g_currentFormat == FORMAT_MP3) {
    // MP3 uses ESP8266Audio library which handles I2S internally.
    i2sDeinit(); // Deinit our I2S, let ESP8266Audio manage it.
    xTaskCreatePinnedToCore(mp3PlaybackTask, "mp3Task", 16384, nullptr, 2, &audioTaskHandle, 1);
  }
}

void audioRestart()
{
  WebLog.println("[AUDIO] Restart requested");
  audioStop();
  audioStart();
}

// Check if file is a supported audio format.
bool audioIsSupportedFormat(const String& path)
{
  return detectFormat(path) != FORMAT_UNKNOWN;
}
