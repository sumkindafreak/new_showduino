#pragma once
#include <Adafruit_NeoPixel.h>
#include "AudioPixelOutput.h"

// RGB888 rendering -> fitted GRB wire bytes. Brightness is already applied by
// the caller's policy; Emergency frames arrive here as explicit full white.
// Buffers must be disjoint or exactly aliased (partial overlap is unsupported).
inline bool audioPixelPackRgbToGrb(const uint8_t* rgb, size_t rgbBytes,
                                   uint8_t* grb, size_t capacity) {
  if (!rgb || !grb || !rgbBytes || rgbBytes % 3U || rgbBytes > capacity) return false;
  for (size_t base = 0; base < rgbBytes; base += 3U) {
    const uint8_t r = rgb[base], g = rgb[base + 1U], b = rgb[base + 2U];
    grb[base] = g;
    grb[base + 1U] = r;
    grb[base + 2U] = b;
  }
  return true;
}

// The existing Audio-owned NeoPixel object is retained solely as its fixed
// GRB buffer; its construction, lifetime and pin ownership remain with callers.
// Both Audio callers configure NEO_GRB+NEO_KHZ800 and leave driver brightness
// at 255. No vendor show(), FastLED controller or per-frame allocation occurs.
inline bool audioPixelWriteRgbFrame(AudioPixelOutput& output,
                                    Adafruit_NeoPixel& grbBuffer,
                                    const uint8_t* rgb, size_t rgbBytes) {
  const size_t capacity = static_cast<size_t>(grbBuffer.numPixels()) * 3U;
  if (rgbBytes != capacity || grbBuffer.getBrightness() != 255) return false;
  uint8_t* wire = grbBuffer.getPixels();
  if (!audioPixelPackRgbToGrb(rgb, rgbBytes, wire, capacity)) return false;
  return output.write(wire, rgbBytes);
}

// RGB888 pixel objects (including real FastLED fl::CRGB) are packed by fields,
// never by casting object memory or assuming its layout. Same fitted GRB
// buffer, full driver brightness and actual native-write result as above.
template<class RgbPixel>
inline bool audioPixelWriteRgbPixels(AudioPixelOutput& output,
                                     Adafruit_NeoPixel& grbBuffer,
                                     const RgbPixel* pixels, size_t count) {
  if (!pixels || !count || count != grbBuffer.numPixels() || grbBuffer.getBrightness() != 255) return false;
  uint8_t* wire = grbBuffer.getPixels();
  if (!wire) return false;
  for (size_t i = 0; i < count; ++i) {
    wire[i * 3U] = pixels[i].g;
    wire[i * 3U + 1U] = pixels[i].r;
    wire[i * 3U + 2U] = pixels[i].b;
  }
  return output.write(wire, count * 3U);
}
