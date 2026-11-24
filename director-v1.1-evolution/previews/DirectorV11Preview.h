#pragma once
// Local-only, fixed-memory RGB565 effect preview. No LVGL, GPIO or transport.
#include <array>
#include <cstddef>
#include <cstdint>

namespace ShowduinoV11 {
enum class PreviewEffect : uint8_t { Fire, Rain, Snow, Lightning, Flicker, Fade, Chase, Rainbow };
struct PreviewSafety {
  bool emergency = true;
  bool p4StateFresh = false;
  bool showIdle = false;
  bool pageVisible = false;
  bool memoryOk = false;
  bool uiLoadOk = false;
};
class Preview {
 public:
  static constexpr size_t width = 24, height = 12, pixels = width * height;
  static constexpr uint32_t intervalMs = 100; // maximum 10 updates/s, no catch-up
  using Frame = std::array<uint16_t, pixels>;
  const Frame &frame() const { return frame_; }
  bool running() const { return running_; }
  void select(PreviewEffect effect, uint32_t seed = 1) {
    effect_ = effect; rng_ = seed ? seed : 1; clear();
  }
  // Returns true only when the display buffer should be refreshed.
  // Caller must also hide the preview immediately when running() is false.
  bool tick(uint32_t now, const PreviewSafety &s) {
    if (s.emergency || !s.p4StateFresh || !s.showIdle ||
        !s.pageVisible || !s.memoryOk || !s.uiLoadOk) {
      if (!running_) return false;
      clear(); return true;
    }
    if (running_ && uint32_t(now - last_) < intervalMs) return false;
    running_ = true; last_ = now;
    render(); ++step_; return true;
  }
 private:
  Frame frame_{};
  std::array<uint8_t, pixels> heat_{};
  PreviewEffect effect_ = PreviewEffect::Fire;
  uint32_t rng_ = 1, step_ = 0, last_ = 0;
  bool running_ = false;
  void clear() { frame_.fill(0); heat_.fill(0); running_ = false; step_ = 0; }
  uint32_t random() {
    rng_ ^= rng_ << 13; rng_ ^= rng_ >> 17; rng_ ^= rng_ << 5; return rng_;
  }
  static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return uint16_t((uint16_t(r >> 3) << 11) | (uint16_t(g >> 2) << 5) | (b >> 3));
  }
  static uint16_t wheel(uint8_t p) {
    if (p < 85) return rgb(255 - p * 3, p * 3, 0);
    if (p < 170) { p -= 85; return rgb(0, 255 - p * 3, p * 3); }
    p -= 170; return rgb(p * 3, 0, 255 - p * 3);
  }
  void render() {
    if (effect_ == PreviewEffect::Fire) {
      // Original small heat diffusion model, not copied from an external library.
      for (size_t y = 0; y + 1 < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
          const size_t b = (y + 1) * width;
          const unsigned v = (heat_[b + x] * 2u +
            heat_[b + (x + width - 1) % width] + heat_[b + (x + 1) % width]) / 4u;
          const unsigned cooling = random() % 30u;
          heat_[y * width + x] = uint8_t(v > cooling ? v - cooling : 0);
        }
      }
      for (size_t x = 0; x < width; ++x)
        heat_[(height - 1) * width + x] = uint8_t(100 + random() % 156u);
      for (size_t i = 0; i < pixels; ++i) {
        const unsigned v = heat_[i];
        frame_[i] = rgb(uint8_t(v > 127 ? 255 : v * 2), uint8_t(v > 128 ? (v-128)*2 : 0), 0);
      }
      return;
    }
    frame_.fill(0);
    for (size_t y = 0; y < height; ++y) {
      for (size_t x = 0; x < width; ++x) {
        const size_t i = y * width + x;
        switch (effect_) {
          case PreviewEffect::Rain: {
            const unsigned offset = unsigned((x * 17 + step_) % height);
            if (y == offset) frame_[i] = rgb(90, 190, 255);
            else if (y == (offset + height - 1) % height) frame_[i] = rgb(15, 50, 120);
            break;
          }
          case PreviewEffect::Snow:
            if ((x * 13 + (y + height - step_ % height) % height * 7) % 23 == 0)
              frame_[i] = rgb(200, 220, 255);
            break;
          case PreviewEffect::Lightning:
            // Soft, brief preview pulse; not a full-screen strobe.
            if (step_ % 30 == 0 && x == width/2 + (y % 3) - 1)
              frame_[i] = rgb(110, 130, 180);
            break;
          case PreviewEffect::Flicker: {
            const uint8_t v = uint8_t(75 + random() % 65);
            frame_[i] = rgb(v, uint8_t(v/3), 0); break;
          }
          case PreviewEffect::Fade: {
            const unsigned t = step_ % 64;
            const uint8_t v = uint8_t((t < 32 ? t : 63-t)*8);
            frame_[i] = rgb(v, 0, uint8_t(248-v)); break;
          }
          case PreviewEffect::Chase:
            if ((x + width - step_ % width) % width < 4) frame_[i] = rgb(0, 180, 120);
            break;
          case PreviewEffect::Rainbow:
            frame_[i] = wheel(uint8_t(x * 10 + step_ * 3u)); break;
          case PreviewEffect::Fire: break;
        }
      }
    }
  }
};
static_assert(sizeof(Preview) <= 1024, "Preview memory budget exceeded");
} // namespace ShowduinoV11
