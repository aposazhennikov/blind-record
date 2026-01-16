#include "equalizer.h"

#include "web_log.h"

#include <math.h>

// Global EQ settings.
EqSettings g_eqSettings;

static const int   NUM_BANDS             = 5;
static const int   BAND_FREQS[NUM_BANDS] = {60, 250, 1000, 4000, 12000};
static const char* BAND_NAMES[NUM_BANDS] = {"Sub-bass", "Bass", "Mid", "Presence", "Brilliance"};

struct BiquadState {
  float x1, x2;
  float y1, y2;
};

struct BiquadCoeffs {
  float b0, b1, b2;
  float a1, a2;
};

static BiquadState  g_filterState[NUM_BANDS][2];
static BiquadCoeffs g_filterCoeffs[NUM_BANDS];
static uint32_t     g_lastSampleRate = 0;

void eqSetDefaults(EqSettings& eq)
{
  eq.band60Hz  = 0.0f;
  eq.band250Hz = 0.0f;
  eq.band1kHz  = 0.0f;
  eq.band4kHz  = 0.0f;
  eq.band12kHz = 0.0f;
  eq.enabled   = true;
}

void eqInit()
{
  eqSetDefaults(g_eqSettings);

  for (int b = 0; b < NUM_BANDS; b++) {
    for (int ch = 0; ch < 2; ch++) {
      g_filterState[b][ch] = {0, 0, 0, 0};
    }
  }

  g_lastSampleRate = 0;
  WebLog.println("[EQ] ✅ Equalizer initialized");
}

float eqGetBand(int index)
{
  if (index < 0 || index >= NUM_BANDS)
    return 0.0f;
  float* bands = &g_eqSettings.band60Hz;
  return bands[index];
}

void eqSetBand(int index, float gainDb)
{
  if (index < 0 || index >= NUM_BANDS)
    return;

  if (gainDb < -12.0f)
    gainDb = -12.0f;
  if (gainDb > 12.0f)
    gainDb = 12.0f;

  float* bands = &g_eqSettings.band60Hz;
  bands[index] = gainDb;

  WebLog.print("[EQ] Band ");
  WebLog.print(index);
  WebLog.print(" (");
  WebLog.print(BAND_FREQS[index]);
  WebLog.print(" Hz) = ");
  WebLog.print(gainDb);
  WebLog.println(" dB");
}

const char* eqGetBandName(int index)
{
  if (index < 0 || index >= NUM_BANDS)
    return "?";
  return BAND_NAMES[index];
}

int eqGetBandFreq(int index)
{
  if (index < 0 || index >= NUM_BANDS)
    return 0;
  return BAND_FREQS[index];
}

static void calcPeakingEQ(float Fs, float f0, float gainDb, float Q, BiquadCoeffs& c)
{
  float A     = powf(10.0f, gainDb / 40.0f);
  float w0    = 2.0f * M_PI * f0 / Fs;
  float sinW0 = sinf(w0);
  float cosW0 = cosf(w0);
  float alpha = sinW0 / (2.0f * Q);

  float b0 = 1.0f + alpha * A;
  float b1 = -2.0f * cosW0;
  float b2 = 1.0f - alpha * A;
  float a0 = 1.0f + alpha / A;
  float a1 = -2.0f * cosW0;
  float a2 = 1.0f - alpha / A;

  c.b0 = b0 / a0;
  c.b1 = b1 / a0;
  c.b2 = b2 / a0;
  c.a1 = a1 / a0;
  c.a2 = a2 / a0;
}

void eqUpdateCoefficients(uint32_t sampleRate)
{
  if (sampleRate == 0)
    return;

  static const float BAND_Q[NUM_BANDS] = {0.7f, 1.0f, 1.2f, 1.2f, 0.8f};
  float*             bands             = &g_eqSettings.band60Hz;

  for (int b = 0; b < NUM_BANDS; b++) {
    calcPeakingEQ((float)sampleRate, (float)BAND_FREQS[b], bands[b], BAND_Q[b], g_filterCoeffs[b]);
  }

  if (sampleRate != g_lastSampleRate) {
    for (int b = 0; b < NUM_BANDS; b++) {
      for (int ch = 0; ch < 2; ch++) {
        g_filterState[b][ch] = {0, 0, 0, 0};
      }
    }
    g_lastSampleRate = sampleRate;
  }
}

static inline float biquadProcess(float x, BiquadCoeffs& c, BiquadState& s)
{
  float y = c.b0 * x + c.b1 * s.x1 + c.b2 * s.x2 - c.a1 * s.y1 - c.a2 * s.y2;
  s.x2    = s.x1;
  s.x1    = x;
  s.y2    = s.y1;
  s.y1    = y;
  return y;
}

void eqProcessSample(int16_t& L, int16_t& R, uint32_t sampleRate)
{
  if (!g_eqSettings.enabled)
    return;

  if (sampleRate != g_lastSampleRate) {
    eqUpdateCoefficients(sampleRate);
  }

  float fL = (float)L;
  float fR = (float)R;

  for (int b = 0; b < NUM_BANDS; b++) {
    fL = biquadProcess(fL, g_filterCoeffs[b], g_filterState[b][0]);
    fR = biquadProcess(fR, g_filterCoeffs[b], g_filterState[b][1]);
  }

  if (fL > 32767.0f)
    fL = 32767.0f;
  if (fL < -32768.0f)
    fL = -32768.0f;
  if (fR > 32767.0f)
    fR = 32767.0f;
  if (fR < -32768.0f)
    fR = -32768.0f;

  L = (int16_t)fL;
  R = (int16_t)fR;
}

void eqProcessBuffer(int16_t* buffer, size_t frames, uint32_t sampleRate)
{
  if (!g_eqSettings.enabled)
    return;

  if (sampleRate != g_lastSampleRate) {
    eqUpdateCoefficients(sampleRate);
  }

  for (size_t i = 0; i < frames; i++) {
    int16_t& L = buffer[i * 2];
    int16_t& R = buffer[i * 2 + 1];

    float fL = (float)L;
    float fR = (float)R;

    for (int b = 0; b < NUM_BANDS; b++) {
      fL = biquadProcess(fL, g_filterCoeffs[b], g_filterState[b][0]);
      fR = biquadProcess(fR, g_filterCoeffs[b], g_filterState[b][1]);
    }

    if (fL > 32767.0f)
      fL = 32767.0f;
    if (fL < -32768.0f)
      fL = -32768.0f;
    if (fR > 32767.0f)
      fR = 32767.0f;
    if (fR < -32768.0f)
      fR = -32768.0f;

    L = (int16_t)fL;
    R = (int16_t)fR;
  }
}
