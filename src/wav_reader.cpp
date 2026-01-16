#include "wav_reader.h"

#include "web_log.h"

static uint32_t readLE32(const uint8_t* p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t readLE16(const uint8_t* p)
{
  return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

WavInfo parseWavHeader(File& f)
{
  WavInfo info;

  uint8_t hdr[12];
  if (f.read(hdr, 12) != 12)
    return info;

  if (memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0) {
    WebLog.println("[WAV] ❌ Not RIFF/WAVE");
    return info;
  }

  bool fmtFound  = false;
  bool dataFound = false;

  while (f.available()) {
    uint8_t chunkHdr[8];
    if (f.read(chunkHdr, 8) != 8)
      break;

    char id[5] = {0};
    memcpy(id, chunkHdr, 4);
    uint32_t chunkSize = readLE32(chunkHdr + 4);

    if (memcmp(id, "fmt ", 4) == 0) {
      if (chunkSize < 16) {
        WebLog.println("[WAV] ❌ fmt chunk too small");
        return info;
      }

      uint8_t fmt[16];
      if (f.read(fmt, 16) != 16)
        return info;

      info.audioFormat   = readLE16(fmt + 0);
      info.numChannels   = readLE16(fmt + 2);
      info.sampleRate    = readLE32(fmt + 4);
      info.bitsPerSample = readLE16(fmt + 14);

      if (chunkSize > 16) {
        f.seek(f.position() + (chunkSize - 16));
      }

      fmtFound = true;
    } else if (memcmp(id, "data", 4) == 0) {
      info.dataOffset = f.position();
      info.dataSize   = chunkSize;
      dataFound       = true;
      break;
    } else {
      f.seek(f.position() + chunkSize);
    }

    if (chunkSize & 1) {
      f.seek(f.position() + 1);
    }
  }

  info.ok = fmtFound && dataFound;

  if (!info.ok) {
    WebLog.println("[WAV] ❌ Failed to find fmt/data chunks");
    return info;
  }

  WebLog.println("[WAV] ✅ Header parsed:");
  WebLog.print("[WAV] Format: ");
  WebLog.println(info.audioFormat);
  WebLog.print("[WAV] Channels: ");
  WebLog.println(info.numChannels);
  WebLog.print("[WAV] SampleRate: ");
  WebLog.println(info.sampleRate);
  WebLog.print("[WAV] Bits: ");
  WebLog.println(info.bitsPerSample);
  WebLog.print("[WAV] DataSize: ");
  WebLog.println(info.dataSize);

  return info;
}
