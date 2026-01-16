#include "resampler.h"

#include "web_log.h"

ResamplerState g_resampler = {0, 0, 0, 0, 0, 1.0f, false};

void resamplerInit(uint32_t srcRate, uint32_t dstRate)
{
  g_resampler.srcRate  = srcRate;
  g_resampler.dstRate  = dstRate;
  g_resampler.position = 0.0f;
  g_resampler.lastL    = 0;
  g_resampler.lastR    = 0;

  if (srcRate == dstRate || srcRate == 0 || dstRate == 0) {
    g_resampler.ratio  = 1.0f;
    g_resampler.active = false;
  } else {
    g_resampler.ratio  = (float)srcRate / (float)dstRate;
    g_resampler.active = true;
    WebLog.print("[RESAMPLE] ✅ Enabled: ");
    WebLog.print(srcRate);
    WebLog.print(" Hz -> ");
    WebLog.print(dstRate);
    WebLog.print(" Hz (ratio=");
    WebLog.print(g_resampler.ratio, 4);
    WebLog.println(")");
  }
}

void resamplerReset()
{
  g_resampler.position = 0.0f;
  g_resampler.lastL    = 0;
  g_resampler.lastR    = 0;
}

bool resamplerIsActive()
{
  return g_resampler.active;
}

size_t resamplerCalcOutputFrames(size_t srcFrames)
{
  if (!g_resampler.active)
    return srcFrames;
  return (size_t)((float)srcFrames / g_resampler.ratio + 0.5f);
}

size_t resamplerCalcInputFrames(size_t dstFrames)
{
  if (!g_resampler.active)
    return dstFrames;
  return (size_t)((float)dstFrames * g_resampler.ratio + 1.5f);
}

static inline int16_t lerp16(int16_t a, int16_t b, float t)
{
  return (int16_t)((float)a + t * ((float)b - (float)a));
}

size_t resamplerProcess(const int16_t* srcBuf, size_t srcFrames, int16_t* dstBuf,
                        size_t dstMaxFrames)
{
  if (!g_resampler.active || srcFrames == 0) {
    size_t toCopy = (srcFrames < dstMaxFrames) ? srcFrames : dstMaxFrames;
    memcpy(dstBuf, srcBuf, toCopy * 2 * sizeof(int16_t));
    return toCopy;
  }

  size_t outFrames = 0;
  float  pos       = g_resampler.position;
  float  ratio     = g_resampler.ratio;

  while (outFrames < dstMaxFrames) {
    size_t srcIdx = (size_t)pos;
    float  frac   = pos - (float)srcIdx;

    if (srcIdx >= srcFrames) {
      break;
    }

    int16_t curL = srcBuf[srcIdx * 2];
    int16_t curR = srcBuf[srcIdx * 2 + 1];

    int16_t nextL, nextR;
    if (srcIdx + 1 < srcFrames) {
      nextL = srcBuf[(srcIdx + 1) * 2];
      nextR = srcBuf[(srcIdx + 1) * 2 + 1];
    } else {
      nextL = curL;
      nextR = curR;
    }

    dstBuf[outFrames * 2]     = lerp16(curL, nextL, frac);
    dstBuf[outFrames * 2 + 1] = lerp16(curR, nextR, frac);

    outFrames++;
    pos += ratio;
  }

  if (srcFrames > 0) {
    g_resampler.lastL = srcBuf[(srcFrames - 1) * 2];
    g_resampler.lastR = srcBuf[(srcFrames - 1) * 2 + 1];
  }

  g_resampler.position = pos - (float)srcFrames;
  if (g_resampler.position < 0)
    g_resampler.position = 0;

  return outFrames;
}
