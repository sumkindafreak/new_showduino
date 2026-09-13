#include "EmergencyIndicate.h"
#include "../BoardConfig.h"

static uint32_t sLast = 0;
static uint8_t sBuzzOn = 0;

void emergencyIndicateBegin() {
#if SHOWDUINO_ESTOP_NODE_LED_GPIO >= 0
  pinMode(SHOWDUINO_ESTOP_NODE_LED_GPIO, OUTPUT);
  digitalWrite(SHOWDUINO_ESTOP_NODE_LED_GPIO, !SHOWDUINO_ESTOP_NODE_LED_ACTIVE);
#endif
#if SHOWDUINO_ESTOP_NODE_BUZZER_GPIO >= 0
  pinMode(SHOWDUINO_ESTOP_NODE_BUZZER_GPIO, OUTPUT);
  digitalWrite(SHOWDUINO_ESTOP_NODE_BUZZER_GPIO, !SHOWDUINO_ESTOP_NODE_BUZZER_ACTIVE);
#endif
}

void emergencyIndicateService(const ShowduinoEmergencyMachine *m, int radioUp) {
  const uint32_t now = millis();
  if ((now - sLast) < 40UL) return;
  sLast = now;
  if (!m) return;

#if SHOWDUINO_ESTOP_NODE_LED_GPIO >= 0
  int on = 0;
  if (m->latched) {
    on = 1;
  } else if (!radioUp) {
    on = ((now / 400UL) & 1) ? 1 : 0;
  } else {
    on = ((now / 2000UL) & 1) ? 1 : 0;
  }
  digitalWrite(SHOWDUINO_ESTOP_NODE_LED_GPIO,
               on ? SHOWDUINO_ESTOP_NODE_LED_ACTIVE : !SHOWDUINO_ESTOP_NODE_LED_ACTIVE);
#endif

#if SHOWDUINO_ESTOP_NODE_BUZZER_GPIO >= 0
  if (m->latched && !m->global_clear_seen) {
    const uint32_t phase = now % 1000UL;
    sBuzzOn = (phase < 200UL) ? 1 : 0;
  } else {
    sBuzzOn = 0;
  }
  digitalWrite(SHOWDUINO_ESTOP_NODE_BUZZER_GPIO,
               sBuzzOn ? SHOWDUINO_ESTOP_NODE_BUZZER_ACTIVE
                       : !SHOWDUINO_ESTOP_NODE_BUZZER_ACTIVE);
#else
  (void)sBuzzOn;
  (void)radioUp;
#endif
}
