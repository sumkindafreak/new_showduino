#include "showduino_carbide_lamp.h"
#include <cstdio>
#include <cstring>

static int gFails = 0;

static void expect(bool ok, const char *msg) {
  if (!ok) {
    std::printf("FAIL: %s\n", msg);
    gFails++;
  }
}

static void tick(ShowduinoCarbideMachine *m, ShowduinoCarbideConfig *cfg, uint32_t ms) {
  m->nowMs += ms;
  showduino_carbide_apply(m, SHOWDUINO_CARBIDE_EV_TICK, cfg);
}

int main() {
  ShowduinoCarbideConfig cfg = showduino_carbide_config_defaults();
  expect(cfg.failStrikePercent == 0, "failed strikes disabled by default");

  ShowduinoCarbideMachine m;
  showduino_carbide_reset(&m, 0);
  expect(m.state == SHOWDUINO_CARBIDE_OFF, "start off");

  showduino_carbide_apply(&m, SHOWDUINO_CARBIDE_EV_IGNITE, &cfg);
  expect(m.state == SHOWDUINO_CARBIDE_STRIKING, "button/P4 ignite → STRIKING");
  expect(m.sound == SHOWDUINO_LAMP_SND_STRIKE, "strike sound");
  tick(&m, &cfg, cfg.strikeMs);
  expect(m.state == SHOWDUINO_CARBIDE_IGNITING, "strike completes → IGNITING");
  expect(m.sound == SHOWDUINO_LAMP_SND_IGNITION, "ignition sound");
  tick(&m, &cfg, cfg.igniteMs);
  expect(m.state == SHOWDUINO_CARBIDE_BURNING, "catch → BURNING");
  expect(m.sound == SHOWDUINO_LAMP_SND_BURN_LOOP, "burn loop");
  expect(m.soundLoop == 1, "burn loops");

  showduino_carbide_apply(&m, SHOWDUINO_CARBIDE_EV_LOW_FLAME, &cfg);
  expect(m.state == SHOWDUINO_CARBIDE_LOW_FLAME, "low flame");
  showduino_carbide_apply(&m, SHOWDUINO_CARBIDE_EV_UNSTABLE, &cfg);
  expect(m.state == SHOWDUINO_CARBIDE_UNSTABLE, "unstable");
  showduino_carbide_apply(&m, SHOWDUINO_CARBIDE_EV_FLARE, &cfg);
  expect(m.state == SHOWDUINO_CARBIDE_FLARE, "flare");
  expect(m.sound == SHOWDUINO_LAMP_SND_FLARE, "flare sound");
  tick(&m, &cfg, cfg.flareMs);
  expect(m.state == SHOWDUINO_CARBIDE_BURNING, "flare returns to burning");

  showduino_carbide_apply(&m, SHOWDUINO_CARBIDE_EV_EXTINGUISH, &cfg);
  expect(m.state == SHOWDUINO_CARBIDE_EXTINGUISHING, "extinguish anim");
  tick(&m, &cfg, cfg.extinguishMs);
  expect(m.state == SHOWDUINO_CARBIDE_OFF, "extinguish → OFF");
  expect(m.sound == SHOWDUINO_LAMP_SND_NONE, "audio silent when off");

  showduino_carbide_apply(&m, SHOWDUINO_CARBIDE_EV_LOW_FLAME, &cfg);
  expect(m.state == SHOWDUINO_CARBIDE_OFF, "FX while off ignored");

  /* Same path for local button and show IGNITE. */
  showduino_carbide_apply(&m, SHOWDUINO_CARBIDE_EV_IGNITE, &cfg);
  expect(m.state == SHOWDUINO_CARBIDE_STRIKING, "second ignite");
  tick(&m, &cfg, cfg.strikeMs);
  tick(&m, &cfg, cfg.igniteMs);
  expect(m.state == SHOWDUINO_CARBIDE_BURNING, "second burn");

  showduino_carbide_apply(&m, SHOWDUINO_CARBIDE_EV_PUFF, &cfg);
  expect(m.state == SHOWDUINO_CARBIDE_UNSTABLE, "puff flutters");
  showduino_carbide_apply(&m, SHOWDUINO_CARBIDE_EV_BLOW_END, &cfg);
  expect(m.state == SHOWDUINO_CARBIDE_BURNING, "puff recovers");
  showduino_carbide_apply(&m, SHOWDUINO_CARBIDE_EV_BLOW, &cfg);
  expect(m.state == SHOWDUINO_CARBIDE_EXTINGUISHING, "sustained blow extinguishes");
  tick(&m, &cfg, cfg.extinguishMs);
  expect(m.state == SHOWDUINO_CARBIDE_OFF, "blow ends off");

  showduino_carbide_apply(&m, SHOWDUINO_CARBIDE_EV_FORCE_OFF, &cfg);
  expect(m.state == SHOWDUINO_CARBIDE_OFF, "force off");

  cfg.failStrikePercent = 50;
  showduino_carbide_reset(&m, 0);
  m.lastRoll = 0;
  showduino_carbide_apply(&m, SHOWDUINO_CARBIDE_EV_IGNITE, &cfg);
  tick(&m, &cfg, cfg.strikeMs);
  expect(m.state == SHOWDUINO_CARBIDE_OFF, "injected failed strike only when enabled");
  cfg.failStrikePercent = 0;
  showduino_carbide_reset(&m, 0);
  m.lastRoll = 0;
  showduino_carbide_apply(&m, SHOWDUINO_CARBIDE_EV_IGNITE, &cfg);
  tick(&m, &cfg, cfg.strikeMs);
  expect(m.state == SHOWDUINO_CARBIDE_IGNITING, "default never fails");

  expect(showduino_carbide_event_from_cmd(SHOWDUINO_LAMP_CMD_IGNITE,
                                          SHOWDUINO_LAMP_FX_STEADY_FLAME) ==
             SHOWDUINO_CARBIDE_EV_IGNITE,
         "cmd ignite");
  expect(showduino_carbide_event_from_cmd(SHOWDUINO_LAMP_CMD_OFF,
                                          SHOWDUINO_LAMP_FX_STEADY_FLAME) ==
             SHOWDUINO_CARBIDE_EV_EXTINGUISH,
         "off → extinguish event");
  expect(showduino_carbide_event_from_cmd(SHOWDUINO_LAMP_CMD_FX,
                                          SHOWDUINO_LAMP_FX_FLARE) ==
             SHOWDUINO_CARBIDE_EV_FLARE,
         "fx flare");

  ShowduinoBlowConfig bcfg = showduino_blow_config_defaults();
  ShowduinoBlowDetector d;
  showduino_blow_reset(&d, &bcfg);
  expect(showduino_blow_feed(&d, 100, 20) == SHOWDUINO_BLOW_NONE, "arm baseline");
  expect(showduino_blow_feed(&d, 400, 20) == SHOWDUINO_BLOW_NONE, "transient ignored");
  expect(showduino_blow_feed(&d, 100, 20) == SHOWDUINO_BLOW_NONE, "transient recovered");

  showduino_blow_reset(&d, &bcfg);
  showduino_blow_feed(&d, 80, 20);
  ShowduinoBlowClass saw = SHOWDUINO_BLOW_NONE;
  for (int i = 0; i < 8; i++) {
    const ShowduinoBlowClass ev = showduino_blow_feed(&d, 400, 20);
    if (ev != SHOWDUINO_BLOW_NONE) saw = ev;
  }
  expect(saw == SHOWDUINO_BLOW_PUFF, "puff after puff window");
  expect(showduino_blow_feed(&d, 80, 20) == SHOWDUINO_BLOW_RELEASE, "puff release");

  showduino_blow_reset(&d, &bcfg);
  showduino_blow_feed(&d, 80, 20);
  saw = SHOWDUINO_BLOW_NONE;
  for (int i = 0; i < 24; i++) {
    const ShowduinoBlowClass ev = showduino_blow_feed(&d, 400, 20);
    if (ev != SHOWDUINO_BLOW_NONE) saw = ev;
  }
  expect(saw == SHOWDUINO_BLOW_SUSTAINED, "sustained blow");
  expect(showduino_blow_detected(&d), "detected yes");

  expect(strcmp(showduino_lamp_sound_info(SHOWDUINO_LAMP_SND_STRIKE)->role, "STRIKE") == 0,
         "strike role");
  expect(showduino_lamp_sound_info(SHOWDUINO_LAMP_SND_BURN_LOOP)->loop == 1,
         "burn loops");
  expect(showduino_lamp_sound_from_role("FLARE") == SHOWDUINO_LAMP_SND_FLARE,
         "role lookup");
  expect(showduino_lamp_sound_from_role("NOPE") == SHOWDUINO_LAMP_SND_NONE,
         "unknown role");

  if (gFails) {
    std::printf("%d FAILED\n", gFails);
    return 1;
  }
  std::printf("ALL PASS\n");
  return 0;
}
