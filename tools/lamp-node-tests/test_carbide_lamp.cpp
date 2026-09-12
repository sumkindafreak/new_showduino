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
  expect(showduino_lamp_audio_transport_nonblocking(),
         "BURN_LOOP must be non-blocking background audio");
  expect(showduino_lamp_sound_info(SHOWDUINO_LAMP_SND_BURN_LOOP)->loop == 1,
         "flameloop is a loop role, not a blocking wait");
  {
    ShowduinoCarbideMachine burn;
    showduino_carbide_reset(&burn, 0);
    showduino_carbide_apply(&burn, SHOWDUINO_CARBIDE_EV_IGNITE, &cfg);
    tick(&burn, &cfg, cfg.strikeMs);
    tick(&burn, &cfg, cfg.igniteMs);
    expect(burn.sound == SHOWDUINO_LAMP_SND_BURN_LOOP, "entered burn loop");
    showduino_carbide_apply(&burn, SHOWDUINO_CARBIDE_EV_BLOW, &cfg);
    expect(burn.sound == SHOWDUINO_LAMP_SND_NONE,
           "extinguish interrupts BURN_LOOP without waiting for MP3 end");
    expect(showduino_lamp_effective_sound(SHOWDUINO_LAMP_SND_BURN_LOOP, 1) ==
               SHOWDUINO_LAMP_SND_EMERGENCY,
           "emergency interrupts BURN_LOOP immediately");
  }

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

  expect(showduino_lamp_fermion_playmode(1) == 1, "loop is repeat-one");
  expect(showduino_lamp_fermion_playmode(0) == 3, "oneshot is play-and-pause");
  char play[40];
  expect(showduino_lamp_fermion_playfile_cmd("flick.mp3", play, sizeof(play)) == 0,
         "playfile ok");
  expect(strcmp(play, "AT+PLAYFILE=/flick.mp3") == 0, "leading slash");
  expect(showduino_lamp_fermion_playfile_cmd("/flameloop.mp3", play, sizeof(play)) == 0,
         "already slashed");
  expect(strcmp(play, "AT+PLAYFILE=/flameloop.mp3") == 0, "no double slash");
  expect(showduino_lamp_fermion_playfile_cmd("", play, sizeof(play)) != 0,
         "empty file rejected");

  /* Physical flame visual phases — host-testable timing, no delay(). */
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_OFF, 0, cfg.igniteMs,
                                  cfg.extinguishMs, 0, 0, 0, -1) ==
             SHOWDUINO_CARBIDE_VIS_BLACK,
         "BOOT/OFF visual is BLACK");
  expect(showduino_carbide_visual_is_black(SHOWDUINO_CARBIDE_VIS_BLACK),
         "OFF pixels conceptually black");
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_STRIKING, 0, cfg.igniteMs,
                                  cfg.extinguishMs, 0, 0, 0, -1) ==
             SHOWDUINO_CARBIDE_VIS_FLINT_CORE,
         "STRIKING t0 → flint core");
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_STRIKING,
                                  SHOWDUINO_CARBIDE_FLINT_CORE_MS, cfg.igniteMs,
                                  cfg.extinguishMs, 0, 0, 0, -1) ==
             SHOWDUINO_CARBIDE_VIS_FLINT_NEAR,
         "STRIKING → flint neighbour");
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_STRIKING,
                                  SHOWDUINO_CARBIDE_FLINT_CORE_MS +
                                      SHOWDUINO_CARBIDE_FLINT_NEAR_MS,
                                  cfg.igniteMs, cfg.extinguishMs, 0, 0, 0, -1) ==
             SHOWDUINO_CARBIDE_VIS_FLINT_DARK,
         "STRIKING → micro dark gap");
  expect(showduino_carbide_visual_is_black(SHOWDUINO_CARBIDE_VIS_FLINT_DARK),
         "dark gap is black");
  expect(SHOWDUINO_CARBIDE_FLINT_CORE_MS + SHOWDUINO_CARBIDE_FLINT_NEAR_MS +
             SHOWDUINO_CARBIDE_FLINT_DARK_MS <=
         cfg.strikeMs,
         "flint + dark gap fits inside STRIKING");

  const uint32_t ign = cfg.igniteMs ? cfg.igniteMs : SHOWDUINO_CARBIDE_IGNITE_MS;
  const uint32_t a = (ign * 18u) / 100u;
  const uint32_t b = (ign * 26u) / 100u;
  const uint32_t dip = (ign * 10u) / 100u;
  const uint32_t c = (ign * 26u) / 100u;
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_IGNITING, 0, ign,
                                  cfg.extinguishMs, 0, 0, 0, -1) ==
             SHOWDUINO_CARBIDE_VIS_CATCH_A,
         "IGNITING → CATCH_A first gas");
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_IGNITING, a, ign,
                                  cfg.extinguishMs, 0, 0, 0, -1) ==
             SHOWDUINO_CARBIDE_VIS_CATCH_B,
         "IGNITING → CATCH_B");
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_IGNITING, a + b, ign,
                                  cfg.extinguishMs, 0, 0, 0, -1) ==
             SHOWDUINO_CARBIDE_VIS_CATCH_DIP,
         "IGNITING → CATCH_DIP");
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_IGNITING, a + b + dip, ign,
                                  cfg.extinguishMs, 0, 0, 0, -1) ==
             SHOWDUINO_CARBIDE_VIS_CATCH_C,
         "IGNITING → CATCH_C");
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_IGNITING, a + b + dip + c, ign,
                                  cfg.extinguishMs, 0, 0, 0, -1) ==
             SHOWDUINO_CARBIDE_VIS_CATCH_D,
         "IGNITING → CATCH_D establish");

  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_BURNING, 0, ign,
                                  cfg.extinguishMs, 0, 0, 0, -1) ==
             SHOWDUINO_CARBIDE_VIS_BURN,
         "BURNING living flame");
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_BURNING, 0, ign,
                                  cfg.extinguishMs, 1, 0, 0, -1) ==
             SHOWDUINO_CARBIDE_VIS_PUFF,
         "short puff struggles");
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_BURNING, 0, ign,
                                  cfg.extinguishMs, 0, 1, 0, -1) ==
             SHOWDUINO_CARBIDE_VIS_RECOVER,
         "puff end recovers");

  const uint32_t ext = cfg.extinguishMs;
  const uint32_t o = (ext * 28u) / 100u;
  const uint32_t k = (ext * 28u) / 100u;
  const uint32_t r = (ext * 24u) / 100u;
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_EXTINGUISHING, 0, ign, ext, 0,
                                  0, 0, -1) == SHOWDUINO_CARBIDE_VIS_EXT_OUTER,
         "extinguish outer collapse");
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_EXTINGUISHING, o, ign, ext, 0,
                                  0, 0, -1) == SHOWDUINO_CARBIDE_VIS_EXT_CORE,
         "extinguish shrink to core");
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_EXTINGUISHING, o + k, ign, ext,
                                  0, 0, 0, -1) ==
             SHOWDUINO_CARBIDE_VIS_EXT_REMNANT,
         "extinguish remnant");
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_EXTINGUISHING, o + k + r, ign,
                                  ext, 0, 0, 0, -1) ==
             SHOWDUINO_CARBIDE_VIS_EXT_EMBER,
         "extinguish ember");
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_OFF, 0, ign, ext, 0, 0, 0,
                                  -1) == SHOWDUINO_CARBIDE_VIS_BLACK,
         "OFF after extinguish is black");

  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_STRIKING, 0, ign, ext, 1, 1, 1,
                                  3) == SHOWDUINO_CARBIDE_VIS_EMERGENCY,
         "emergency from STRIKING");
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_IGNITING, 10, ign, ext, 0, 0,
                                  1, -1) == SHOWDUINO_CARBIDE_VIS_EMERGENCY,
         "emergency from IGNITING");
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_BURNING, 10, ign, ext, 1, 0, 1,
                                  -1) == SHOWDUINO_CARBIDE_VIS_EMERGENCY,
         "emergency from PUFF/BURN");
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_BURNING, 10, ign, ext, 0, 1, 1,
                                  -1) == SHOWDUINO_CARBIDE_VIS_EMERGENCY,
         "emergency from RECOVER");
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_EXTINGUISHING, 10, ign, ext, 0,
                                  0, 1, -1) == SHOWDUINO_CARBIDE_VIS_EMERGENCY,
         "emergency from EXTINGUISHING");
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_FLARE, 10, ign, ext, 0, 0, 1,
                                  -1) == SHOWDUINO_CARBIDE_VIS_EMERGENCY,
         "emergency from FLARE");
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_OFF, 0, ign, ext, 0, 0, 0, 2) ==
             SHOWDUINO_CARBIDE_VIS_IDENTIFY,
         "identify overlays when not emergency");
  expect(showduino_carbide_visual(SHOWDUINO_CARBIDE_BURNING, 0, ign, ext, 0, 0, 1,
                                  2) == SHOWDUINO_CARBIDE_VIS_EMERGENCY,
         "emergency beats identify");

  expect(showduino_jewel_core_index(0) == 0, "default core 0 until mapped");
  expect(showduino_jewel_core_index(9) == 0, "invalid core clamps");
  expect(showduino_jewel_ring_index(0, 0) == 1, "ring0 beside core0");
  expect(showduino_jewel_ring_index(0, 5) == 6, "last ring pixel");
  expect(showduino_jewel_ring_index(3, 0) != 3, "ring skips physical core");

  expect(showduino_carbide_event_from_cmd(SHOWDUINO_LAMP_CMD_IGNITE,
                                          SHOWDUINO_LAMP_FX_STEADY_FLAME) ==
             SHOWDUINO_CARBIDE_EV_IGNITE,
         "P4 IGNITE uses the same IGNITE event as the striker");
  expect(showduino_carbide_event_from_cmd(SHOWDUINO_LAMP_CMD_EXTINGUISH,
                                          SHOWDUINO_LAMP_FX_STEADY_FLAME) ==
             SHOWDUINO_CARBIDE_EV_EXTINGUISH,
         "P4 EXTINGUISH uses the same extinguish renderer path");

  {
    ShowduinoCarbideMachine again;
    showduino_carbide_reset(&again, 5000);
    showduino_carbide_apply(&again, SHOWDUINO_CARBIDE_EV_IGNITE, &cfg);
    tick(&again, &cfg, cfg.strikeMs);
    tick(&again, &cfg, cfg.igniteMs);
    showduino_carbide_apply(&again, SHOWDUINO_CARBIDE_EV_BLOW, &cfg);
    tick(&again, &cfg, cfg.extinguishMs);
    expect(again.state == SHOWDUINO_CARBIDE_OFF, "re-ignite setup OFF");
    expect(again.sound == SHOWDUINO_LAMP_SND_NONE, "re-ignite setup silent");
    showduino_carbide_apply(&again, SHOWDUINO_CARBIDE_EV_IGNITE, &cfg);
    expect(again.state == SHOWDUINO_CARBIDE_STRIKING, "re-ignite after extinguish");
    expect(again.sound == SHOWDUINO_LAMP_SND_STRIKE, "re-ignite requests STRIKE");
    expect(showduino_carbide_elapsed_ms(&again) == 0, "re-ignite resets phase time");
  }

  if (gFails) {
    std::printf("%d FAILED\n", gFails);
    return 1;
  }
  std::printf("ALL PASS\n");
  return 0;
}
