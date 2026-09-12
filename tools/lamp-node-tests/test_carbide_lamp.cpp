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

  expect(m.sound == SHOWDUINO_LAMP_SND_BURN_LOOP, "burn loop still after extra ticks");
  tick(&m, &cfg, 80);
  expect(m.state == SHOWDUINO_CARBIDE_BURNING, "burning holds");
  expect(m.sound == SHOWDUINO_LAMP_SND_BURN_LOOP, "burn loop remains");
  expect(m.soundLoop == 1, "burn loop still marked looping");

  showduino_carbide_apply(&m, SHOWDUINO_CARBIDE_EV_LOW_FLAME, &cfg);
  expect(m.state == SHOWDUINO_CARBIDE_LOW_FLAME, "low flame");
  expect(m.sound == SHOWDUINO_LAMP_SND_BURN_LOOP, "low flame keeps flameloop");
  showduino_carbide_apply(&m, SHOWDUINO_CARBIDE_EV_UNSTABLE, &cfg);
  expect(m.state == SHOWDUINO_CARBIDE_UNSTABLE, "unstable");
  expect(m.sound == SHOWDUINO_LAMP_SND_BURN_LOOP, "unstable keeps flameloop");
  showduino_carbide_apply(&m, SHOWDUINO_CARBIDE_EV_FLARE, &cfg);
  expect(m.state == SHOWDUINO_CARBIDE_FLARE, "flare visual");
  expect(m.sound == SHOWDUINO_LAMP_SND_BURN_LOOP, "flare has no own V1 file");
  tick(&m, &cfg, cfg.flareMs);
  expect(m.state == SHOWDUINO_CARBIDE_BURNING, "flare returns to burning");
  expect(m.sound == SHOWDUINO_LAMP_SND_BURN_LOOP, "back to burn loop");

  showduino_carbide_apply(&m, SHOWDUINO_CARBIDE_EV_EXTINGUISH, &cfg);
  expect(m.state == SHOWDUINO_CARBIDE_EXTINGUISHING, "extinguish anim");
  expect(m.sound == SHOWDUINO_LAMP_SND_NONE, "V1 extinguish stops loop, no fire_out");
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
  expect(m.sound == SHOWDUINO_LAMP_SND_BURN_LOOP, "puff keeps burning audio");
  showduino_carbide_apply(&m, SHOWDUINO_CARBIDE_EV_BLOW_END, &cfg);
  expect(m.state == SHOWDUINO_CARBIDE_BURNING, "puff recovers");
  expect(m.sound == SHOWDUINO_LAMP_SND_BURN_LOOP, "recover still burn loop");
  showduino_carbide_apply(&m, SHOWDUINO_CARBIDE_EV_BLOW, &cfg);
  expect(m.state == SHOWDUINO_CARBIDE_EXTINGUISHING, "sustained blow extinguishes");
  expect(m.sound == SHOWDUINO_LAMP_SND_NONE, "blow stops burn loop");
  tick(&m, &cfg, cfg.extinguishMs);
  expect(m.state == SHOWDUINO_CARBIDE_OFF, "blow ends off");
  expect(m.sound == SHOWDUINO_LAMP_SND_NONE, "blow ends silent");

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
  expect(strcmp(showduino_lamp_sound_info(SHOWDUINO_LAMP_SND_STRIKE)->file, "flick.mp3") == 0,
         "strike → flick.mp3");
  expect(strcmp(showduino_lamp_sound_info(SHOWDUINO_LAMP_SND_IGNITION)->file, "fire_ignite.mp3") == 0,
         "ignition → fire_ignite.mp3");
  expect(strcmp(showduino_lamp_sound_info(SHOWDUINO_LAMP_SND_BURN_LOOP)->file, "flameloop.mp3") == 0,
         "burn → flameloop.mp3");
  expect(strcmp(showduino_lamp_sound_info(SHOWDUINO_LAMP_SND_EMERGENCY)->file, "emergency.mp3") == 0,
         "emergency → emergency.mp3");
  expect(showduino_lamp_sound_info(SHOWDUINO_LAMP_SND_BURN_LOOP)->loop == 1,
         "burn loops");
  expect(showduino_lamp_sound_info(SHOWDUINO_LAMP_SND_EMERGENCY)->loop == 1,
         "emergency loops");
  expect(showduino_lamp_sound_from_role("FLICK") == SHOWDUINO_LAMP_SND_STRIKE,
         "flick alias");
  expect(showduino_lamp_sound_from_role("FLAME_LOOP") == SHOWDUINO_LAMP_SND_BURN_LOOP,
         "flame loop alias");
  expect(showduino_lamp_sound_from_role("EMERGENCY") == SHOWDUINO_LAMP_SND_EMERGENCY,
         "emergency role");
  expect(showduino_lamp_sound_from_role("NOPE") == SHOWDUINO_LAMP_SND_NONE,
         "unknown role");
  expect(showduino_lamp_v1_file_known("flick.mp3"), "v1 flick");
  expect(showduino_lamp_v1_file_known("emergency.mp3"), "v1 emergency");
  expect(!showduino_lamp_v1_file_known("strike.wav"), "old strike.wav not required");
  expect(!showduino_lamp_v1_file_known("flare.wav"), "old flare.wav not required");
  expect(!showduino_lamp_v1_file_known("fire_out.mp3"), "no fire_out in V1");
  expect(!showduino_lamp_audio_blocks_machine(), "audio failure never blocks carbide");

  showduino_carbide_reset(&m, 0);
  expect(showduino_lamp_effective_sound(m.sound, 1) == SHOWDUINO_LAMP_SND_EMERGENCY,
         "emergency during OFF");
  showduino_carbide_apply(&m, SHOWDUINO_CARBIDE_EV_IGNITE, &cfg);
  expect(m.sound == SHOWDUINO_LAMP_SND_STRIKE, "striking role");
  expect(showduino_lamp_effective_sound(m.sound, 1) == SHOWDUINO_LAMP_SND_EMERGENCY,
         "emergency interrupts STRIKE");
  tick(&m, &cfg, cfg.strikeMs);
  expect(m.sound == SHOWDUINO_LAMP_SND_IGNITION, "igniting role");
  expect(showduino_lamp_effective_sound(m.sound, 1) == SHOWDUINO_LAMP_SND_EMERGENCY,
         "emergency interrupts IGNITION");
  tick(&m, &cfg, cfg.igniteMs);
  expect(m.sound == SHOWDUINO_LAMP_SND_BURN_LOOP, "burning role");
  expect(showduino_lamp_effective_sound(m.sound, 1) == SHOWDUINO_LAMP_SND_EMERGENCY,
         "emergency interrupts BURN_LOOP");
  showduino_carbide_apply(&m, SHOWDUINO_CARBIDE_EV_FORCE_OFF, &cfg);
  expect(m.state == SHOWDUINO_CARBIDE_OFF, "emergency clear forces idle");
  expect(m.sound == SHOWDUINO_LAMP_SND_NONE, "emergency clear stops emergency");
  expect(showduino_lamp_effective_sound(m.sound, 0) == SHOWDUINO_LAMP_SND_NONE,
         "clear does not restart burn loop");

  const ShowduinoCarbideState beforeFail = m.state;
  showduino_carbide_apply(&m, SHOWDUINO_CARBIDE_EV_IGNITE, &cfg);
  expect(m.state == SHOWDUINO_CARBIDE_STRIKING, "ignite continues if audio cannot play");
  expect(beforeFail == SHOWDUINO_CARBIDE_OFF, "started from idle");
  expect(showduino_carbide_sound_for_state(SHOWDUINO_CARBIDE_STRIKING) ==
             SHOWDUINO_LAMP_SND_STRIKE,
         "state→role STRIKE");
  expect(showduino_carbide_sound_for_state(SHOWDUINO_CARBIDE_BURNING) ==
             SHOWDUINO_LAMP_SND_BURN_LOOP,
         "state→role BURN_LOOP");
  expect(showduino_carbide_sound_for_state(SHOWDUINO_CARBIDE_EXTINGUISHING) ==
             SHOWDUINO_LAMP_SND_NONE,
         "state→role silence");

  if (gFails) {
    std::printf("%d FAILED\n", gFails);
    return 1;
  }
  std::printf("ALL PASS\n");
  return 0;
}
