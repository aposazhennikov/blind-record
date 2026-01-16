#pragma once
#include <Arduino.h>

// Audio Resampler module.
// Converts between different sample rates using linear interpolation.
// For better quality, use simple integer ratios (e.g., 48000->24000).

// Resampler state (keeps track of fractional position).
struct ResamplerState {
  float    position; // Fractional sample position.
  int16_t  lastL;    // Last sample (L channel).
  int16_t  lastR;    // Last sample (R channel).
  uint32_t srcRate;  // Source sample rate.
  uint32_t dstRate;  // Destination sample rate.
  float    ratio;    // srcRate / dstRate.
  bool     active;   // Resampling needed.
};

// Global resampler state.
extern ResamplerState g_resampler;

// Initialize resampler for given source and destination rates.
void resamplerInit(uint32_t srcRate, uint32_t dstRate);

// Reset resampler state (call when starting new file).
void resamplerReset();

// Check if resampling is active.
bool resamplerIsActive();

// Resample a buffer of stereo samples.
// Input: srcBuf with srcFrames stereo frames.
// Output: dstBuf with up to dstMaxFrames stereo frames.
// Returns: number of output frames written.
size_t resamplerProcess(const int16_t* srcBuf, size_t srcFrames, int16_t* dstBuf,
                        size_t dstMaxFrames);

// Calculate required output buffer size for given input.
size_t resamplerCalcOutputFrames(size_t srcFrames);

// Calculate required input frames to produce given output.
size_t resamplerCalcInputFrames(size_t dstFrames);
