#include <stdio.h>
#include <string.h>
#include "showduino_sound_input.h"

static int gFail = 0;

static void expect(int cond, const char *name) {
  if (cond) printf("PASS %s\n", name);
  else {
    printf("FAIL %s\n", name);
    gFail++;
  }
}

static void feed_ms(ShowduinoSoundEngine *e, uint8_t rms, uint8_t peak,
                    uint32_t *now, uint32_t ms, int playing, int emergency, int comms) {
  uint32_t t = 0;
  while (t < ms) {
    *now += 20;
    t += 20;
    showduino_sound_engine_feed(e, rms, peak, *now, 20, playing, emergency, comms);
  }
}

int main() {
  ShowduinoSoundEngine e;
  ShowduinoSoundInputConfig cfg;
  ShowduinoSoundEvent ev;
  uint32_t now = 1000;

  showduino_sound_config_defaults(&cfg);
  showduino_sound_engine_init(&e, &cfg);
  e.calibrated = 1;
  e.noiseFloor = 20;

  /* Noise-floor calibration */
  showduino_sound_engine_init(&e, &cfg);
  showduino_sound_engine_start_calibrate(&e, now);
  feed_ms(&e, 22, 28, &now, 4200, 0, 0, 1);
  expect(!e.calibrating && e.calibrated, "calibrate completes");
  expect(e.noiseFloor >= 18 && e.noiseFloor <= 26, "calibrate floor near 22");

  /* Threshold / level trigger */
  showduino_sound_engine_init(&e, &cfg);
  e.calibrated = 1;
  e.noiseFloor = 20;
  e.cfg.mode = SHOWDUINO_SOUND_MODE_LEVEL;
  e.cfg.autoNoiseFloor = 1;
  e.cfg.thresholdAboveNoiseFloor = 20; /* fire at 40 */
  e.cfg.minimumDurationMs = 80;
  feed_ms(&e, 18, 20, &now, 200, 0, 0, 1);
  expect(!showduino_sound_engine_take_event(&e, &ev), "quiet no level event");
  feed_ms(&e, 70, 80, &now, 120, 0, 0, 1);
  expect(showduino_sound_engine_take_event(&e, &ev), "level trigger fires");
  expect(ev.type == SHOWDUINO_SOUND_EVT_LEVEL, "event is LEVEL");

  /* Hysteresis / re-arm */
  feed_ms(&e, 70, 80, &now, 200, 0, 0, 1);
  expect(!showduino_sound_engine_take_event(&e, &ev), "hover does not retrigger during cooldown");
  now = e.cooldownUntil + 20;
  showduino_sound_engine_feed(&e, 70, 80, now, 20, 0, 0, 1);
  expect(!showduino_sound_engine_take_event(&e, &ev), "still high - not rearmed");
  feed_ms(&e, 10, 12, &now, 80, 0, 0, 1);
  feed_ms(&e, 70, 80, &now, 120, 0, 0, 1);
  expect(showduino_sound_engine_take_event(&e, &ev), "rearms after drop then rise");

  /* Transient */
  showduino_sound_engine_init(&e, &cfg);
  e.calibrated = 1;
  e.noiseFloor = 20;
  e.cfg.mode = SHOWDUINO_SOUND_MODE_TRANSIENT;
  e.cfg.sensitivity = 5;
  e.cfg.cooldownMs = 3000;
  feed_ms(&e, 20, 22, &now, 100, 0, 0, 1);
  now += 20;
  showduino_sound_engine_feed(&e, 40, 90, now, 20, 0, 0, 1);
  expect(showduino_sound_engine_take_event(&e, &ev), "transient clap fires");
  expect(ev.type == SHOWDUINO_SOUND_EVT_TRANSIENT, "event is TRANSIENT");

  /* Sustained */
  showduino_sound_engine_init(&e, &cfg);
  e.calibrated = 1;
  e.noiseFloor = 20;
  e.cfg.mode = SHOWDUINO_SOUND_MODE_SUSTAINED;
  e.cfg.autoNoiseFloor = 0;
  e.cfg.sustainedThreshold = 60;
  e.cfg.sustainedDurationMs = 400;
  e.cfg.cooldownMs = 1000;
  feed_ms(&e, 70, 80, &now, 900, 0, 0, 1);
  expect(showduino_sound_engine_take_event(&e, &ev), "sustained fires once");
  expect(ev.type == SHOWDUINO_SOUND_EVT_SUSTAINED, "event is SUSTAINED");
  feed_ms(&e, 70, 80, &now, 500, 0, 0, 1);
  expect(!showduino_sound_engine_take_event(&e, &ev), "sustained latched");

  /* Quiet - disabled by default */
  showduino_sound_engine_init(&e, &cfg);
  e.calibrated = 1;
  e.noiseFloor = 20;
  feed_ms(&e, 5, 6, &now, 5200, 0, 0, 1);
  expect(!showduino_sound_engine_take_event(&e, &ev), "quiet off by default");
  e.cfg.quietEnabled = 1;
  e.cfg.quietThreshold = 18;
  e.cfg.quietDurationMs = 400;
  e.quietLatched = 0;
  e.quietMs = 0;
  feed_ms(&e, 5, 6, &now, 440, 0, 0, 1);
  expect(showduino_sound_engine_take_event(&e, &ev), "quiet fires when enabled");
  expect(ev.type == SHOWDUINO_SOUND_EVT_QUIET, "event is QUIET");

  /* Cooldown */
  showduino_sound_engine_init(&e, &cfg);
  e.calibrated = 1;
  e.noiseFloor = 20;
  e.cfg.mode = SHOWDUINO_SOUND_MODE_LEVEL;
  e.cfg.cooldownMs = 500;
  feed_ms(&e, 80, 90, &now, 120, 0, 0, 1);
  expect(showduino_sound_engine_take_event(&e, &ev), "first cooldown subject");
  expect(showduino_sound_cooldown_remaining(&e, now) > 0, "cooldown remaining");
  feed_ms(&e, 10, 10, &now, 80, 0, 0, 1);
  feed_ms(&e, 80, 90, &now, 120, 0, 0, 1);
  expect(!showduino_sound_engine_take_event(&e, &ev), "blocked by cooldown");

  /* triggerWhilePlaying=false */
  showduino_sound_engine_init(&e, &cfg);
  e.calibrated = 1;
  e.noiseFloor = 20;
  e.cfg.mode = SHOWDUINO_SOUND_MODE_LEVEL;
  e.cfg.triggerWhilePlaying = 0;
  feed_ms(&e, 90, 99, &now, 200, 1, 0, 1);
  expect(!showduino_sound_engine_take_event(&e, &ev), "no trigger while playing");

  /* post-playback inhibit */
  showduino_sound_engine_note_playback_stop(&e, now);
  feed_ms(&e, 90, 99, &now, 200, 0, 0, 1);
  expect(!showduino_sound_engine_take_event(&e, &ev), "post-playback inhibit");
  feed_ms(&e, 10, 10, &now, 1200, 0, 0, 1);
  feed_ms(&e, 90, 99, &now, 120, 0, 0, 1);
  expect(showduino_sound_engine_take_event(&e, &ev), "fires after inhibit window");

  /* Emergency */
  showduino_sound_engine_init(&e, &cfg);
  e.calibrated = 1;
  e.noiseFloor = 20;
  e.cfg.mode = SHOWDUINO_SOUND_MODE_LEVEL;
  feed_ms(&e, 90, 99, &now, 200, 0, 1, 1);
  expect(!showduino_sound_engine_take_event(&e, &ev), "emergency inhibits");

  /* Comms-loss does not queue stale events */
  showduino_sound_engine_init(&e, &cfg);
  e.calibrated = 1;
  e.noiseFloor = 20;
  e.cfg.mode = SHOWDUINO_SOUND_MODE_LEVEL;
  feed_ms(&e, 90, 99, &now, 200, 0, 0, 0);
  expect(!showduino_sound_engine_take_event(&e, &ev), "no event while comms lost");
  feed_ms(&e, 90, 99, &now, 80, 0, 0, 1);
  expect(!showduino_sound_engine_take_event(&e, &ev), "no stale replay on comms return");

  /* Config parse */
  {
    ShowduinoSoundInputConfig c;
    const char *ok =
        "{\"formatVersion\":1,\"volume\":80,\"soundInput\":{"
        "\"enabled\":true,\"mode\":\"LEVEL_TRANSIENT\",\"threshold\":70,"
        "\"thresholdAboveNoiseFloor\":20,\"minimumDurationMs\":80,"
        "\"sustainedDurationMs\":1500,\"quietDurationMs\":5000,"
        "\"cooldownMs\":3000,\"hysteresis\":10,\"postPlaybackInhibitMs\":1000,"
        "\"triggerWhilePlaying\":false,\"autoNoiseFloor\":true}}";
    expect(showduino_sound_config_parse(ok, &c) == SHOWDUINO_AUDIO_CFG_OK, "soundInput parse ok");
    expect(c.mode == SHOWDUINO_SOUND_MODE_LEVEL_TRANSIENT, "mode LEVEL_TRANSIENT");
    expect(c.triggerWhilePlaying == 0, "triggerWhilePlaying false");
    expect(c.quietEnabled == 0, "quiet default off");
    const char *bad =
        "{\"soundInput\":{\"enabled\":true,\"mode\":\"BANANA\",\"threshold\":70}}";
    expect(showduino_sound_config_parse(bad, &c) == SHOWDUINO_AUDIO_CFG_BAD, "bad mode rejected");
    const char *none = "{\"formatVersion\":1,\"volume\":80}";
    expect(showduino_sound_config_parse(none, &c) == SHOWDUINO_AUDIO_CFG_OK, "missing section uses defaults");
  }

  if (gFail) {
    printf("%d FAIL\n", gFail);
    return 1;
  }
  printf("ALL PASS\n");
  return 0;
}
