/* Hardware-independent Showduino pixel timing/colour helpers; shared by C3 and P4. */
static uint8_t scale8(uint8_t value, uint8_t a, uint8_t b) {
  return (uint8_t)(((uint32_t)value * (uint32_t)a * (uint32_t)b) / (255UL * 255UL));
}

static uint8_t effectScale(uint8_t value, uint8_t factor) {
  return (uint8_t)(((uint16_t)value * factor) / 255U);
}

static ShowduinoPixelColor mixColor(ShowduinoPixelColor a, ShowduinoPixelColor b, uint8_t amount) {
  ShowduinoPixelColor out;
  out.r = (uint8_t)(((uint16_t)a.r * (255U - amount) + (uint16_t)b.r * amount) / 255U);
  out.g = (uint8_t)(((uint16_t)a.g * (255U - amount) + (uint16_t)b.g * amount) / 255U);
  out.b = (uint8_t)(((uint16_t)a.b * (255U - amount) + (uint16_t)b.b * amount) / 255U);
  return out;
}

static ShowduinoPixelColor hsv(uint16_t hue, uint8_t sat, uint8_t val) {
  hue %= 360U;
  const uint8_t region = hue / 60U;
  const uint16_t remainder = (hue % 60U) * 255U / 60U;
  const uint8_t p = (uint8_t)((uint16_t)val * (255U - sat) / 255U);
  const uint8_t q = (uint8_t)((uint16_t)val * (255U - ((uint16_t)sat * remainder / 255U)) / 255U);
  const uint8_t t = (uint8_t)((uint16_t)val * (255U - ((uint16_t)sat * (255U - remainder) / 255U)) / 255U);
  switch (region) {
    case 0: return {val, t, p};
    case 1: return {q, val, p};
    case 2: return {p, val, t};
    case 3: return {p, q, val};
    case 4: return {t, p, val};
    default: return {val, p, q};
  }
}

static uint32_t periodFromSpeed(uint8_t speed, uint32_t slowMs, uint32_t fastMs) {
  speed = constrain(speed, 1, 100);
  return slowMs - ((slowMs - fastMs) * (uint32_t)(speed - 1U) / 99UL);
}

static uint8_t triangleWave(uint32_t phase, uint32_t period) {
  if (period < 2) return 255;
  uint32_t p = phase % period;
  const uint32_t half = period / 2U;
  if (p <= half) return (uint8_t)(p * 255UL / max<uint32_t>(1, half));
  return (uint8_t)((period - p) * 255UL / max<uint32_t>(1, period - half));
}
