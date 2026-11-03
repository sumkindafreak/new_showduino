/* Byte-for-byte legacy FX renderer, included in C3 and P4 engine units. */
static void renderSegment(ShowduinoPixelSegmentState &seg, uint32_t now) {
  if (!seg.active || !seg.configured || seg.count == 0) return;
  if (segmentExpired(seg, now)) return;

  const uint32_t elapsed = now - seg.startedMs;
  const uint16_t end = min<uint32_t>((uint32_t)seg.start + seg.count,
                                     sCount);
  const uint16_t actualCount = (end > seg.start) ? (end - seg.start) : 0;
  if (!actualCount) return;

  switch (seg.effect) {
    case ShowduinoPixelFx::Off:
      break;

    case ShowduinoPixelFx::Solid:
      fillSegment(seg, seg.primary);
      break;

    case ShowduinoPixelFx::FadeIn: {
      const uint32_t duration = seg.durationMs ? seg.durationMs : periodFromSpeed(seg.speed, 5000, 250);
      uint8_t amount = (uint8_t)min<uint32_t>(255UL, elapsed * 255UL / max<uint32_t>(1, duration));
      fillSegment(seg, seg.primary, amount);
      break;
    }

    case ShowduinoPixelFx::FadeOut: {
      const uint32_t duration = seg.durationMs ? seg.durationMs : periodFromSpeed(seg.speed, 5000, 250);
      uint8_t amount = (elapsed >= duration) ? 0 : (uint8_t)(255UL - elapsed * 255UL / max<uint32_t>(1, duration));
      fillSegment(seg, seg.primary, amount);
      break;
    }

    case ShowduinoPixelFx::Pulse: {
      uint32_t period = periodFromSpeed(seg.speed, 5000, 250);
      fillSegment(seg, seg.primary, triangleWave(elapsed, period));
      break;
    }

    case ShowduinoPixelFx::Breathe: {
      const uint32_t period = periodFromSpeed(seg.speed, 7000, 700);
      const float angle = ((float)(elapsed % period) / (float)period) * 6.2831853f;
      const float shaped = (1.0f - cosf(angle)) * 0.5f;
      fillSegment(seg, seg.primary, (uint8_t)(shaped * 255.0f));
      break;
    }

    case ShowduinoPixelFx::Flicker: {
      const uint32_t interval = periodFromSpeed(seg.speed, 140, 20);
      if ((now - seg.lastStepMs) >= interval) {
        seg.lastStepMs = now;
        const uint8_t floorV = (uint8_t)(255U - ((uint16_t)seg.intensity * 180U / 100U));
        seg.aux = (uint32_t)random(floorV, 256);
      }
      fillSegment(seg, seg.primary, (uint8_t)seg.aux);
      break;
    }

    case ShowduinoPixelFx::Candle: {
      const uint32_t interval = periodFromSpeed(seg.speed, 180, 35);
      if ((now - seg.lastStepMs) >= interval) {
        seg.lastStepMs = now;
        seg.aux = (uint32_t)random(30, 120);
      }
      ShowduinoPixelColor warm = seg.secondary;
      if (warm.r == 0 && warm.g == 0 && warm.b == 0) warm = {255, 70, 0};
      ShowduinoPixelColor c = mixColor(seg.primary, warm, (uint8_t)seg.aux);
      fillSegment(seg, c, (uint8_t)random(190, 256));
      break;
    }

    case ShowduinoPixelFx::Fire: {
      for (uint16_t p = 0; p < actualCount; ++p) {
        uint8_t heat = (uint8_t)random(120, 256);
        uint8_t red = heat;
        uint8_t green = (uint8_t)((uint16_t)heat * random(20, 65) / 100U);
        uint8_t blue = (uint8_t)random(0, 8);
        setPixelScaled(seg.start + p, seg, {red, green, blue});
      }
      break;
    }

    case ShowduinoPixelFx::Lightning: {
      const uint32_t interval = periodFromSpeed(seg.speed, 900, 80);
      if ((now - seg.lastStepMs) >= interval) {
        seg.lastStepMs = now;
        const long chance = random(0, 101);
        seg.aux = (chance <= seg.randomness) ? (uint32_t)random(1, 4) : 0;
      }
      if (seg.aux > 0) {
        fillSegment(seg, seg.primary, 255);
        if (random(0, 100) < 45) --seg.aux;
      }
      break;
    }

    case ShowduinoPixelFx::Strobe: {
      const uint32_t period = periodFromSpeed(seg.speed, 1000, 60);
      if ((elapsed % period) < (period / 2U)) fillSegment(seg, seg.primary);
      break;
    }

    case ShowduinoPixelFx::RandomStrobe: {
      const uint32_t interval = periodFromSpeed(seg.speed, 700, 45);
      if ((now - seg.lastStepMs) >= interval) {
        seg.lastStepMs = now;
        seg.aux = random(0, 101) <= seg.randomness ? 1 : 0;
      }
      if (seg.aux) fillSegment(seg, seg.primary);
      break;
    }

    case ShowduinoPixelFx::Chase: {
      const uint32_t stepMs = periodFromSpeed(seg.speed, 350, 20);
      uint32_t step = elapsed / max<uint32_t>(1, stepMs);
      uint16_t pos = (uint16_t)(step % actualCount);
      if (seg.reverse) pos = actualCount - 1U - pos;
      uint16_t width = max<uint16_t>(1, (uint16_t)seg.intensity * min<uint16_t>(actualCount, 8) / 100U);
      for (uint16_t w = 0; w < width; ++w) setPixelScaled(seg.start + ((pos + w) % actualCount), seg, seg.primary);
      break;
    }

    case ShowduinoPixelFx::Bounce: {
      const uint32_t stepMs = periodFromSpeed(seg.speed, 350, 20);
      const uint32_t span = actualCount <= 1 ? 1 : (uint32_t)(actualCount - 1U) * 2UL;
      uint32_t x = (elapsed / max<uint32_t>(1, stepMs)) % span;
      uint16_t pos = x < actualCount ? (uint16_t)x : (uint16_t)(span - x);
      if (seg.reverse) pos = actualCount - 1U - pos;
      setPixelScaled(seg.start + pos, seg, seg.primary);
      break;
    }

    case ShowduinoPixelFx::Comet: {
      const uint32_t stepMs = periodFromSpeed(seg.speed, 300, 20);
      uint16_t head = (uint16_t)((elapsed / max<uint32_t>(1, stepMs)) % actualCount);
      if (seg.reverse) head = actualCount - 1U - head;
      uint16_t tail = max<uint16_t>(2, min<uint16_t>(actualCount, (uint16_t)(2U + seg.intensity / 12U)));
      for (uint16_t t = 0; t < tail; ++t) {
        int32_t pos = seg.reverse ? (int32_t)head + t : (int32_t)head - t;
        while (pos < 0) pos += actualCount;
        pos %= actualCount;
        uint8_t level = (uint8_t)(255U - ((uint32_t)t * 220U / tail));
        setPixelScaled(seg.start + (uint16_t)pos, seg, seg.primary, level);
      }
      break;
    }

    case ShowduinoPixelFx::Wipe:
    case ShowduinoPixelFx::ReverseWipe:
    case ShowduinoPixelFx::Build: {
      const uint32_t stepMs = periodFromSpeed(seg.speed, 300, 15);
      uint16_t filled = min<uint32_t>(actualCount, elapsed / max<uint32_t>(1, stepMs) + 1U);
      for (uint16_t p = 0; p < filled; ++p) {
        uint16_t pos = (seg.effect == ShowduinoPixelFx::ReverseWipe || seg.reverse)
                         ? (actualCount - 1U - p) : p;
        setPixelScaled(seg.start + pos, seg, seg.primary);
      }
      break;
    }

    case ShowduinoPixelFx::Sparkle: {
      fillSegment(seg, seg.secondary, 80);
      uint16_t sparks = max<uint16_t>(1, (uint16_t)actualCount * seg.intensity / 100U / 4U);
      for (uint16_t i = 0; i < sparks; ++i) {
        uint16_t pos = (uint16_t)random(0, actualCount);
        setPixelScaled(seg.start + pos, seg, seg.primary, (uint8_t)random(170, 256));
      }
      break;
    }

    case ShowduinoPixelFx::Twinkle: {
      const uint32_t period = periodFromSpeed(seg.speed, 4500, 400);
      for (uint16_t p = 0; p < actualCount; ++p) {
        uint32_t shifted = elapsed + (uint32_t)p * 173UL;
        uint8_t level = triangleWave(shifted, period);
        setPixelScaled(seg.start + p, seg, seg.primary, level);
      }
      break;
    }

    case ShowduinoPixelFx::Glitch: {
      const uint32_t interval = periodFromSpeed(seg.speed, 500, 35);
      if ((now - seg.lastStepMs) >= interval) {
        seg.lastStepMs = now;
        seg.aux = random(0, 0x7fffffff);
      }
      for (uint16_t p = 0; p < actualCount; ++p) {
        uint32_t x = seg.aux ^ ((uint32_t)p * 2654435761UL);
        if ((x % 100U) < seg.intensity) setPixelScaled(seg.start + p, seg, (x & 1U) ? seg.primary : seg.secondary);
      }
      break;
    }

    case ShowduinoPixelFx::Warning: {
      ShowduinoPixelColor warning = seg.primary;
      if (warning.r == 255 && warning.g == 255 && warning.b == 255) warning = {255, 0, 0};
      const uint32_t period = periodFromSpeed(seg.speed, 1800, 200);
      fillSegment(seg, warning, triangleWave(elapsed, period));
      break;
    }

    case ShowduinoPixelFx::Portal: {
      const uint32_t period = periodFromSpeed(seg.speed, 4000, 350);
      for (uint16_t p = 0; p < actualCount; ++p) {
        uint32_t shifted = elapsed + ((uint32_t)p * period / max<uint16_t>(1, actualCount));
        uint8_t level = triangleWave(shifted, period);
        ShowduinoPixelColor c = mixColor(seg.secondary, seg.primary, level);
        setPixelScaled(seg.start + p, seg, c);
      }
      break;
    }

    case ShowduinoPixelFx::Rainbow: {
      const uint32_t stepMs = periodFromSpeed(seg.speed, 80, 5);
      uint16_t baseHue = (uint16_t)((elapsed / max<uint32_t>(1, stepMs)) % 360U);
      for (uint16_t p = 0; p < actualCount; ++p) {
        uint16_t hue = (uint16_t)((baseHue + ((uint32_t)p * 360U / actualCount)) % 360U);
        setPixelScaled(seg.start + p, seg, hsv(hue, 255, 255));
      }
      break;
    }

    case ShowduinoPixelFx::CustomSequence: {
      const uint32_t unit = periodFromSpeed(seg.speed, 1600, 150);
      uint32_t step = (elapsed / max<uint32_t>(1, unit)) % 3U;
      if (step == 0) fillSegment(seg, seg.primary);
      else if (step == 1) fillSegment(seg, seg.secondary);
      break;
    }

    default:
      break;
  }
}
