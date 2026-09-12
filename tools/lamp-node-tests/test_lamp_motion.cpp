#include "showduino_lamp_motion.h"
#include <cstdio>

static int gFails = 0;

static void expect(bool ok, const char *msg) {
  if (!ok) {
    std::printf("FAIL: %s\n", msg);
    gFails++;
  }
}

static ShowduinoMotionEvent hold(ShowduinoMotionDetector *d, uint8_t raw,
                                 uint8_t activeLow, uint32_t *now,
                                 uint32_t ms) {
  ShowduinoMotionEvent last = SHOWDUINO_MOTION_EV_NONE;
  const uint32_t end = *now + ms;
  while (*now < end) {
    *now += 10;
    const ShowduinoMotionEvent ev =
        showduino_motion_feed(d, raw, activeLow, *now);
    if (ev != SHOWDUINO_MOTION_EV_NONE) last = ev;
  }
  return last;
}

int main() {
  expect(SHOWDUINO_MOTION_DEFAULT_ENABLED == 0, "NVS missing keys default disabled");
  expect(showduino_motion_action_from_u8(255) == SHOWDUINO_MOTION_ACT_DISABLED,
         "unknown action byte → DISABLED");
  expect(strcmp(showduino_motion_action_name(SHOWDUINO_MOTION_ACT_DISABLED),
                "DISABLED") == 0,
         "default action name");

  ShowduinoMotionDetector d;
  uint32_t now = 0;
  showduino_motion_reset(&d, 50, 3000);

  expect(hold(&d, 0, 0, &now, 80) == SHOWDUINO_MOTION_EV_NONE, "idle clear");
  expect(showduino_motion_decide(SHOWDUINO_MOTION_EV_ACTIVE, 1,
                                 SHOWDUINO_MOTION_ACT_IGNITE,
                                 SHOWDUINO_LAMP_ST_STANDALONE,
                                 SHOWDUINO_CARBIDE_OFF) ==
             SHOWDUINO_MOTION_DECIDE_IGNITE,
         "enabled IGNITE while OFF");
  expect(showduino_motion_decide(SHOWDUINO_MOTION_EV_ACTIVE, 0,
                                 SHOWDUINO_MOTION_ACT_IGNITE,
                                 SHOWDUINO_LAMP_ST_STANDALONE,
                                 SHOWDUINO_CARBIDE_OFF) ==
             SHOWDUINO_MOTION_DECIDE_TELEMETRY,
         "disabled master switch → telemetry only");
  expect(showduino_motion_decide(SHOWDUINO_MOTION_EV_ACTIVE, 1,
                                 SHOWDUINO_MOTION_ACT_DISABLED,
                                 SHOWDUINO_LAMP_ST_STANDALONE,
                                 SHOWDUINO_CARBIDE_OFF) ==
             SHOWDUINO_MOTION_DECIDE_TELEMETRY,
         "action DISABLED → no theatrical event");

  ShowduinoMotionEvent ev = hold(&d, 1, 0, &now, 80);
  expect(ev == SHOWDUINO_MOTION_EV_ACTIVE, "rising edge → MOTION_ACTIVE");
  expect(hold(&d, 1, 0, &now, 400) == SHOWDUINO_MOTION_EV_NONE,
         "held HIGH does not retrigger");
  expect(showduino_motion_active_ms(&d) >= 400, "active duration tracks hold");

  ShowduinoCarbideConfig cfg = showduino_carbide_config_defaults();
  ShowduinoCarbideMachine flame;
  showduino_carbide_reset(&flame, now);
  const ShowduinoMotionDecision ign =
      showduino_motion_decide(SHOWDUINO_MOTION_EV_ACTIVE, 1,
                              SHOWDUINO_MOTION_ACT_IGNITE,
                              SHOWDUINO_LAMP_ST_STANDALONE,
                              flame.state);
  expect(ign == SHOWDUINO_MOTION_DECIDE_IGNITE, "motion IGNITE maps to same event");
  expect(strcmp(showduino_motion_protocol_command(ign), "LAMP:IGNITE") == 0,
         "motion IGNITE uses LAMP:IGNITE");
  showduino_carbide_apply(&flame, showduino_motion_carbide_event(ign), &cfg);
  expect(flame.state == SHOWDUINO_CARBIDE_STRIKING, "same STRIKING path as striker");
  expect(flame.sound == SHOWDUINO_LAMP_SND_STRIKE, "flick.mp3 role");
  flame.nowMs += cfg.strikeMs;
  showduino_carbide_apply(&flame, SHOWDUINO_CARBIDE_EV_TICK, &cfg);
  expect(flame.state == SHOWDUINO_CARBIDE_IGNITING, "IGNITING");
  expect(flame.sound == SHOWDUINO_LAMP_SND_IGNITION, "fire_ignite.mp3 role");
  flame.nowMs += cfg.igniteMs;
  showduino_carbide_apply(&flame, SHOWDUINO_CARBIDE_EV_TICK, &cfg);
  expect(flame.state == SHOWDUINO_CARBIDE_BURNING, "BURNING");
  expect(flame.sound == SHOWDUINO_LAMP_SND_BURN_LOOP, "non-blocking flameloop role");
  expect(showduino_lamp_audio_transport_nonblocking(),
         "motion must not introduce blocking audio");

  expect(showduino_motion_decide(SHOWDUINO_MOTION_EV_ACTIVE, 1,
                                 SHOWDUINO_MOTION_ACT_IGNITE,
                                 SHOWDUINO_LAMP_ST_STANDALONE,
                                 flame.state) ==
             SHOWDUINO_MOTION_DECIDE_TELEMETRY,
         "IGNITE while already lit does nothing");

  expect(hold(&d, 1, 0, &now, 80) == SHOWDUINO_MOTION_EV_NONE,
         "still HIGH: no second ACTIVE");

  showduino_carbide_apply(&flame, SHOWDUINO_CARBIDE_EV_BLOW, &cfg);
  expect(flame.state == SHOWDUINO_CARBIDE_EXTINGUISHING, "blow extinguish");
  flame.nowMs += cfg.extinguishMs;
  showduino_carbide_apply(&flame, SHOWDUINO_CARBIDE_EV_TICK, &cfg);
  expect(flame.state == SHOWDUINO_CARBIDE_OFF, "OFF after blow");
  expect(hold(&d, 1, 0, &now, 80) == SHOWDUINO_MOTION_EV_NONE,
         "PIR still HIGH after extinguish: no reignite");
  expect(showduino_motion_decide(SHOWDUINO_MOTION_EV_NONE, 1,
                                 SHOWDUINO_MOTION_ACT_IGNITE,
                                 SHOWDUINO_LAMP_ST_STANDALONE,
                                 SHOWDUINO_CARBIDE_OFF) ==
             SHOWDUINO_MOTION_DECIDE_NONE,
         "no event → no action while PIR remains active");

  expect(hold(&d, 0, 0, &now, 80) == SHOWDUINO_MOTION_EV_CLEAR, "PIR clears");
  ev = hold(&d, 1, 0, &now, 80);
  expect(ev == SHOWDUINO_MOTION_EV_NONE, "clear+rise inside cooldown suppressed");
  expect(hold(&d, 0, 0, &now, 80) == SHOWDUINO_MOTION_EV_CLEAR,
         "must clear again after cooldown-suppressed edge");
  now += 3010;
  ev = hold(&d, 1, 0, &now, 80);
  expect(ev == SHOWDUINO_MOTION_EV_ACTIVE, "after cooldown + new edge: ACTIVE");
  expect(showduino_motion_decide(ev, 1, SHOWDUINO_MOTION_ACT_IGNITE,
                                 SHOWDUINO_LAMP_ST_STANDALONE,
                                 SHOWDUINO_CARBIDE_OFF) ==
             SHOWDUINO_MOTION_DECIDE_IGNITE,
         "new motion may ignite again");

  expect(showduino_motion_decide(SHOWDUINO_MOTION_EV_ACTIVE, 1,
                                 SHOWDUINO_MOTION_ACT_IGNITE,
                                 SHOWDUINO_LAMP_ST_SHOW_CONTROLLED,
                                 SHOWDUINO_CARBIDE_OFF) ==
             SHOWDUINO_MOTION_DECIDE_BLOCKED_OWNER,
         "SHOWDUINO GRANT blocks local motion action");
  expect(!showduino_motion_may_act(SHOWDUINO_LAMP_ST_SHOW_CONTROLLED),
         "may_act false when P4 owns");
  expect(showduino_motion_decide(SHOWDUINO_MOTION_EV_ACTIVE, 1,
                                 SHOWDUINO_MOTION_ACT_IGNITE,
                                 SHOWDUINO_LAMP_ST_EMERGENCY,
                                 SHOWDUINO_CARBIDE_OFF) ==
             SHOWDUINO_MOTION_DECIDE_BLOCKED_EMERGENCY,
         "emergency blocks theatrical motion");
  expect(!showduino_motion_may_act(SHOWDUINO_LAMP_ST_EMERGENCY),
         "may_act false in emergency");

  showduino_carbide_reset(&flame, now);
  showduino_carbide_apply(&flame, SHOWDUINO_CARBIDE_EV_IGNITE, &cfg);
  flame.nowMs += cfg.strikeMs;
  showduino_carbide_apply(&flame, SHOWDUINO_CARBIDE_EV_TICK, &cfg);
  flame.nowMs += cfg.igniteMs;
  showduino_carbide_apply(&flame, SHOWDUINO_CARBIDE_EV_TICK, &cfg);
  expect(flame.state == SHOWDUINO_CARBIDE_BURNING, "setup burning for FX");
  expect(showduino_motion_decide(SHOWDUINO_MOTION_EV_ACTIVE, 1,
                                 SHOWDUINO_MOTION_ACT_FLARE,
                                 SHOWDUINO_LAMP_ST_STANDALONE,
                                 flame.state) ==
             SHOWDUINO_MOTION_DECIDE_FLARE,
         "FLARE while burning");
  showduino_carbide_apply(&flame,
                          showduino_motion_carbide_event(SHOWDUINO_MOTION_DECIDE_FLARE),
                          &cfg);
  expect(flame.state == SHOWDUINO_CARBIDE_FLARE, "flare applied");
  expect(flame.sound == SHOWDUINO_LAMP_SND_BURN_LOOP, "flare keeps flameloop");
  flame.nowMs += cfg.flareMs;
  showduino_carbide_apply(&flame, SHOWDUINO_CARBIDE_EV_TICK, &cfg);
  expect(flame.state == SHOWDUINO_CARBIDE_BURNING, "flare returns to burning");

  expect(showduino_motion_decide(SHOWDUINO_MOTION_EV_ACTIVE, 1,
                                 SHOWDUINO_MOTION_ACT_UNSTABLE,
                                 SHOWDUINO_LAMP_ST_STANDALONE,
                                 flame.state) ==
             SHOWDUINO_MOTION_DECIDE_UNSTABLE,
         "UNSTABLE while burning");
  showduino_carbide_apply(
      &flame, showduino_motion_carbide_event(SHOWDUINO_MOTION_DECIDE_UNSTABLE),
      &cfg);
  expect(flame.state == SHOWDUINO_CARBIDE_UNSTABLE, "unstable applied");
  expect(showduino_motion_decide(SHOWDUINO_MOTION_EV_ACTIVE, 1,
                                 SHOWDUINO_MOTION_ACT_FLARE,
                                 SHOWDUINO_LAMP_ST_STANDALONE,
                                 SHOWDUINO_CARBIDE_OFF) ==
             SHOWDUINO_MOTION_DECIDE_TELEMETRY,
         "FLARE while OFF is telemetry only");
  expect(showduino_motion_decide(SHOWDUINO_MOTION_EV_ACTIVE, 1,
                                 SHOWDUINO_MOTION_ACT_IGNITE,
                                 SHOWDUINO_LAMP_ST_STANDALONE,
                                 SHOWDUINO_CARBIDE_EXTINGUISHING) ==
             SHOWDUINO_MOTION_DECIDE_TELEMETRY,
         "IGNITE during EXTINGUISHING is not allowed");
  expect(showduino_motion_decide_ex(SHOWDUINO_MOTION_EV_ACTIVE, 1,
                                    SHOWDUINO_MOTION_ACT_IGNITE,
                                    SHOWDUINO_LAMP_ST_STANDALONE,
                                    SHOWDUINO_CARBIDE_OFF, 0) ==
             SHOWDUINO_MOTION_DECIDE_BLOCKED_UNCONFIRMED,
         "unconfirmed pin cannot ignite");
  expect(showduino_motion_protocol_command(
             SHOWDUINO_MOTION_DECIDE_BLOCKED_UNCONFIRMED)[0] == 0,
         "unconfirmed block has no theatrical command");

  showduino_motion_reset(&d, 50, 0);
  now = 0;
  hold(&d, 1, 1, &now, 80);
  expect(d.stable == 0, "active-low HIGH raw is CLEAR");
  ev = hold(&d, 0, 1, &now, 80);
  expect(ev == SHOWDUINO_MOTION_EV_ACTIVE, "active-low LOW raw is ACTIVE");

  showduino_motion_reset(&d, 50, 3000);
  now = 0;
  hold(&d, 1, 0, &now, 80);
  expect(d.armed == 0, "boot already-active does not arm");
  expect(hold(&d, 1, 0, &now, 80) == SHOWDUINO_MOTION_EV_NONE,
         "boot-high does not fire ACTIVE");
  expect(hold(&d, 0, 0, &now, 80) == SHOWDUINO_MOTION_EV_CLEAR,
         "boot-high then clear");
  expect(hold(&d, 1, 0, &now, 80) == SHOWDUINO_MOTION_EV_ACTIVE,
         "first real edge after boot-high");

  if (gFails) {
    std::printf("%d FAILED\n", gFails);
    return 1;
  }
  std::printf("ALL PASS\n");
  return 0;
}
