#include "mp3_player.h"

#include "AudioFileSourceBuffer.h"
#include "AudioFileSourceSD.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"
#include "app_config.h"
#include "settings.h"
#include "web_log.h"

#include <SD.h>

// Buffer size for MP3 source (prevents SD read stuttering).
// 8KB buffer provides ~0.5 sec of 128kbps MP3 data.
static const size_t MP3_BUFFER_SIZE = 8192;

static AudioFileSourceSD*     mp3Source = nullptr;
static AudioFileSourceBuffer* mp3Buffer = nullptr;
static AudioGeneratorMP3*     mp3Gen    = nullptr;
static AudioOutputI2S*        mp3Out    = nullptr;

static volatile bool g_mp3Playing       = false;
static volatile bool g_mp3StopRequested = false;
static uint32_t      g_mp3FileSize      = 0;
static uint32_t      g_mp3StartTime     = 0;
static uint32_t      g_mp3EstDurationMs = 0;

// Approximate MP3 bitrate for duration estimation (128 kbps typical).
static const uint32_t MP3_APPROX_BITRATE = 128000;

void mp3Init()
{
  WebLog.println("[MP3] Initializing...");
  g_mp3Playing       = false;
  g_mp3StopRequested = false;
}

bool mp3StartFile(const String& path)
{
  // Make sure everything is cleaned up first.
  mp3Cleanup();

  WebLog.print("[MP3] Starting: ");
  WebLog.println(path);

  g_mp3StopRequested = false;

  // Create audio source from SD.
  mp3Source = new AudioFileSourceSD(path.c_str());
  if (!mp3Source || !mp3Source->isOpen()) {
    WebLog.println("[MP3] ❌ Cannot open file");
    mp3Cleanup();
    return false;
  }

  g_mp3FileSize = mp3Source->getSize();
  WebLog.print("[MP3] File size: ");
  WebLog.print(g_mp3FileSize);
  WebLog.println(" bytes");

  // Estimate duration based on file size and typical bitrate.
  g_mp3EstDurationMs = (uint32_t)((uint64_t)g_mp3FileSize * 8 * 1000 / MP3_APPROX_BITRATE);

  // Wrap source with buffer for smoother playback.
  mp3Buffer = new AudioFileSourceBuffer(mp3Source, MP3_BUFFER_SIZE);
  if (!mp3Buffer) {
    WebLog.println("[MP3] ❌ Failed to create buffer");
    mp3Cleanup();
    return false;
  }

  WebLog.print("[MP3] Buffer: ");
  WebLog.print(MP3_BUFFER_SIZE);
  WebLog.println(" bytes");

  // Create I2S output.
  mp3Out = new AudioOutputI2S();
  mp3Out->SetPinout(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DOUT_PIN);

  // Apply gain with headroom to prevent clipping on loud bass.
  // MP3 decoding can produce peaks above 0dB, so we reduce gain by ~20%.
  // Also note: EQ is NOT applied to MP3 (library limitation).
  float mp3Gain = g_settings.volume * 0.8f;
  mp3Out->SetGain(mp3Gain);

  WebLog.print("[MP3] Gain: ");
  WebLog.print(mp3Gain, 2);
  WebLog.println(" (80% of volume to prevent clipping)");

  if (g_settings.eqEnabled) {
    WebLog.println("[MP3] ⚠️ Note: EQ is not applied to MP3 (use WAV for EQ)");
  }

  // Create MP3 decoder.
  mp3Gen = new AudioGeneratorMP3();

  // Start playback with buffered source.
  if (!mp3Gen->begin(mp3Buffer, mp3Out)) {
    WebLog.println("[MP3] ❌ Failed to start decoder");
    mp3Cleanup();
    return false;
  }

  g_mp3Playing   = true;
  g_mp3StartTime = millis();

  WebLog.println("[MP3] ✅ Playback started");
  WebLog.print("[MP3] Est. duration: ");
  WebLog.print(g_mp3EstDurationMs / 1000);
  WebLog.println(" sec");

  return true;
}

// Internal cleanup - releases all resources.
void mp3Cleanup()
{
  // Stop generator first if running.
  if (mp3Gen) {
    if (mp3Gen->isRunning()) {
      mp3Gen->stop();
    }
    delete mp3Gen;
    mp3Gen = nullptr;
  }

  // Then output.
  if (mp3Out) {
    delete mp3Out;
    mp3Out = nullptr;
  }

  // Then buffer (which wraps source).
  if (mp3Buffer) {
    delete mp3Buffer;
    mp3Buffer = nullptr;
  }

  // Finally source.
  if (mp3Source) {
    delete mp3Source;
    mp3Source = nullptr;
  }

  g_mp3FileSize      = 0;
  g_mp3EstDurationMs = 0;
}

void mp3Stop()
{
  if (!g_mp3Playing) {
    return; // Already stopped, don't do anything.
  }

  WebLog.println("[MP3] ⏹ Stop requested");
  g_mp3StopRequested = true;
  g_mp3Playing       = false;

  // Cleanup will be done by the task or here if task already exited.
  mp3Cleanup();
}

bool mp3Loop()
{
  if (!g_mp3Playing || g_mp3StopRequested || !mp3Gen) {
    return false;
  }

  if (mp3Gen->isRunning()) {
    if (!mp3Gen->loop()) {
      // Playback finished normally.
      WebLog.println("[MP3] ✅ Playback finished");
      g_mp3Playing = false;
      mp3Cleanup();
      return false;
    }
    return true;
  }

  // Not running anymore.
  g_mp3Playing = false;
  mp3Cleanup();
  return false;
}

bool mp3IsPlaying()
{
  return g_mp3Playing && !g_mp3StopRequested && mp3Gen && mp3Gen->isRunning();
}

bool mp3IsStopRequested()
{
  return g_mp3StopRequested;
}

uint32_t mp3GetSampleRate()
{
  return 44100;
}

uint16_t mp3GetChannels()
{
  return 2;
}

uint16_t mp3GetBitsPerSample()
{
  return 16;
}

uint32_t mp3GetDurationMs()
{
  return g_mp3EstDurationMs;
}

uint32_t mp3GetPositionMs()
{
  if (!g_mp3Playing || !mp3Source)
    return 0;

  if (g_mp3FileSize > 0) {
    uint32_t pos = mp3Source->getPos();
    return (uint32_t)((uint64_t)pos * g_mp3EstDurationMs / g_mp3FileSize);
  }

  return millis() - g_mp3StartTime;
}

void mp3SetVolume(float volume)
{
  if (mp3Out) {
    // Apply 80% headroom to prevent clipping.
    float mp3Gain = volume * 0.8f;
    mp3Out->SetGain(mp3Gain);
  }
}
