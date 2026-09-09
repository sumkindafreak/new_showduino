#include <stdio.h>
#include <string.h>
#include "showduino_audio_node.h"

static int gFail = 0;

static void expect(int cond, const char *name) {
  if (cond) printf("PASS %s\n", name);
  else {
    printf("FAIL %s\n", name);
    gFail++;
  }
}

int main() {
  char path[96];
  char arg[80];
  int vol = -1;

  expect(showduino_audio_resolve_path("effects/thunder.wav", path, sizeof(path)) ==
             SHOWDUINO_AUDIO_PATH_OK, "valid relative wav");
  expect(strcmp(path, "/showduino/audio/effects/thunder.wav") == 0, "resolved path");
  expect(showduino_audio_resolve_path("system-test.wav", path, sizeof(path)) ==
             SHOWDUINO_AUDIO_PATH_OK, "test file path");
  expect(showduino_audio_resolve_path("../etc/x.wav", path, sizeof(path)) ==
             SHOWDUINO_AUDIO_PATH_TRAVERSAL, "dotdot rejected");
  expect(showduino_audio_resolve_path("effects\\x.wav", path, sizeof(path)) ==
             SHOWDUINO_AUDIO_PATH_TRAVERSAL, "backslash rejected");
  expect(showduino_audio_resolve_path("", path, sizeof(path)) ==
             SHOWDUINO_AUDIO_PATH_EMPTY, "empty path");
  expect(showduino_audio_resolve_path("effects/thunder.mp3", path, sizeof(path)) ==
             SHOWDUINO_AUDIO_PATH_BAD_EXT, "mp3 rejected");
  expect(showduino_audio_resolve_path("/etc/passwd.wav", path, sizeof(path)) ==
             SHOWDUINO_AUDIO_PATH_OUTSIDE_ROOT, "outside root");
  expect(showduino_audio_resolve_path("/showduino/audio/music/a.wav", path, sizeof(path)) ==
             SHOWDUINO_AUDIO_PATH_OK, "absolute audio root ok");

  expect(showduino_audio_parse_command("AUDIO:NODE:PLAY:effects/thunder.wav", arg, sizeof(arg), &vol) ==
             SHOWDUINO_AUDIO_CMD_PLAY, "parse play");
  expect(strcmp(arg, "effects/thunder.wav") == 0, "play arg");
  expect(showduino_audio_parse_command("AUDIO:NODE:LOOP:ambience/room.wav", arg, sizeof(arg), &vol) ==
             SHOWDUINO_AUDIO_CMD_LOOP, "parse loop");
  expect(showduino_audio_parse_command("AUDIO:NODE:STOP", arg, sizeof(arg), &vol) ==
             SHOWDUINO_AUDIO_CMD_STOP, "parse stop");
  expect(showduino_audio_parse_command("AUDIO:NODE:PAUSE", arg, sizeof(arg), &vol) ==
             SHOWDUINO_AUDIO_CMD_PAUSE, "parse pause");
  expect(showduino_audio_parse_command("AUDIO:NODE:RESUME", arg, sizeof(arg), &vol) ==
             SHOWDUINO_AUDIO_CMD_RESUME, "parse resume");
  expect(showduino_audio_parse_command("AUDIO:NODE:VOLUME:80", arg, sizeof(arg), &vol) ==
             SHOWDUINO_AUDIO_CMD_VOLUME, "parse volume");
  expect(vol == 80, "volume 80");
  expect(showduino_audio_parse_command("AUDIO:NODE:VOLUME:101", arg, sizeof(arg), &vol) ==
             SHOWDUINO_AUDIO_CMD_NONE, "volume 101 rejected");
  expect(showduino_audio_parse_command("AUDIO:NODE:VOLUME:-1", arg, sizeof(arg), &vol) ==
             SHOWDUINO_AUDIO_CMD_NONE, "volume negative rejected");
  expect(showduino_audio_parse_command("AUDIO:LOCAL:PLAY:x.wav", arg, sizeof(arg), &vol) ==
             SHOWDUINO_AUDIO_CMD_LOCAL_REJECT, "local rejected");
  expect(showduino_audio_parse_command("EMERGENCY:STOP", arg, sizeof(arg), &vol) ==
             SHOWDUINO_AUDIO_CMD_EMERGENCY_STOP, "estop");
  expect(showduino_audio_parse_command("EMERGENCY:CLEAR", arg, sizeof(arg), &vol) ==
             SHOWDUINO_AUDIO_CMD_EMERGENCY_CLEAR, "eclear");

  expect(showduino_audio_can_accept(SHOWDUINO_AUDIO_ST_IDLE, SHOWDUINO_AUDIO_CMD_PLAY) ==
             SHOWDUINO_AUDIO_FAIL_NONE, "idle accepts play");
  expect(showduino_audio_can_accept(SHOWDUINO_AUDIO_ST_EMERGENCY, SHOWDUINO_AUDIO_CMD_PLAY) ==
             SHOWDUINO_AUDIO_FAIL_EMERGENCY, "emergency rejects play");
  expect(showduino_audio_can_accept(SHOWDUINO_AUDIO_ST_FAULT, SHOWDUINO_AUDIO_CMD_PLAY) ==
             SHOWDUINO_AUDIO_FAIL_FAULT, "fault rejects play");
  expect(showduino_audio_can_accept(SHOWDUINO_AUDIO_ST_IDLE, SHOWDUINO_AUDIO_CMD_PAUSE) ==
             SHOWDUINO_AUDIO_FAIL_BAD_COMMAND, "idle pause rejected");
  expect(showduino_audio_can_accept(SHOWDUINO_AUDIO_ST_EMERGENCY, SHOWDUINO_AUDIO_CMD_STOP) ==
             SHOWDUINO_AUDIO_FAIL_NONE, "stop always accepted");

  expect(showduino_audio_next_state(SHOWDUINO_AUDIO_ST_IDLE, SHOWDUINO_AUDIO_LIFE_STARTED,
                                    SHOWDUINO_AUDIO_CMD_PLAY) == SHOWDUINO_AUDIO_ST_PLAYING,
         "started -> playing");
  expect(showduino_audio_next_state(SHOWDUINO_AUDIO_ST_PLAYING, SHOWDUINO_AUDIO_LIFE_COMPLETED,
                                    SHOWDUINO_AUDIO_CMD_PLAY) == SHOWDUINO_AUDIO_ST_IDLE,
         "completed -> idle");
  expect(showduino_audio_next_state(SHOWDUINO_AUDIO_ST_PLAYING, SHOWDUINO_AUDIO_LIFE_NONE,
                                    SHOWDUINO_AUDIO_CMD_EMERGENCY_STOP) == SHOWDUINO_AUDIO_ST_EMERGENCY,
         "estop -> emergency");
  expect(showduino_audio_next_state(SHOWDUINO_AUDIO_ST_EMERGENCY, SHOWDUINO_AUDIO_LIFE_NONE,
                                    SHOWDUINO_AUDIO_CMD_EMERGENCY_CLEAR) == SHOWDUINO_AUDIO_ST_IDLE,
         "clear -> idle");
  expect(showduino_audio_next_state(SHOWDUINO_AUDIO_ST_PLAYING, SHOWDUINO_AUDIO_LIFE_NONE,
                                    SHOWDUINO_AUDIO_CMD_PAUSE) == SHOWDUINO_AUDIO_ST_PAUSED,
         "pause");
  expect(showduino_audio_next_state(SHOWDUINO_AUDIO_ST_PAUSED, SHOWDUINO_AUDIO_LIFE_NONE,
                                    SHOWDUINO_AUDIO_CMD_RESUME) == SHOWDUINO_AUDIO_ST_PLAYING,
         "resume");
  expect(showduino_audio_on_comms_timeout(SHOWDUINO_AUDIO_ST_PLAYING) == SHOWDUINO_AUDIO_ST_IDLE,
         "timeout stops playing");
  expect(showduino_audio_on_comms_timeout(SHOWDUINO_AUDIO_ST_EMERGENCY) == SHOWDUINO_AUDIO_ST_EMERGENCY,
         "timeout does not clear emergency");

  expect(showduino_audio_button_allowed(SHOWDUINO_AUDIO_ST_IDLE, 0, SHOWDUINO_AUDIO_BTN_PLAY) == 1,
         "local play idle");
  expect(showduino_audio_button_allowed(SHOWDUINO_AUDIO_ST_PLAYING, 1, SHOWDUINO_AUDIO_BTN_PLAY) == 0,
         "show play blocks local play");
  expect(showduino_audio_button_allowed(SHOWDUINO_AUDIO_ST_EMERGENCY, 0, SHOWDUINO_AUDIO_BTN_VOL_UP) == 0,
         "emergency blocks volume");
  expect(showduino_audio_button_allowed(SHOWDUINO_AUDIO_ST_IDLE, 0, SHOWDUINO_AUDIO_BTN_SET) == 0,
         "set disabled");
  expect(showduino_audio_button_allowed(SHOWDUINO_AUDIO_ST_IDLE, 0, SHOWDUINO_AUDIO_BTN_MODE) == 0,
         "mode disabled");

  expect(showduino_audio_wav_pcm_ok(1, 2, 44100, 16) == 1, "44.1 stereo pcm");
  expect(showduino_audio_wav_pcm_ok(1, 1, 48000, 16) == 1, "48k mono pcm");
  expect(showduino_audio_wav_pcm_ok(85, 2, 44100, 16) == 0, "mp3 format rejected");
  expect(showduino_audio_wav_pcm_ok(1, 2, 44100, 8) == 0, "8bit rejected");

  expect(strcmp(showduino_audio_fail_name(SHOWDUINO_AUDIO_FAIL_FILE_NOT_FOUND),
                "FILE_NOT_FOUND") == 0, "fail name");
  expect(strcmp(showduino_audio_state_name(SHOWDUINO_AUDIO_ST_IDLE), "IDLE") == 0, "state name");

  int fade = -1;
  int pri = -1;
  expect(showduino_audio_parse_command_ex("AUDIO:NODE:PLAY:effects/boom.wav:FADE=2000:PRI=SFX",
                                          arg, sizeof(arg), &vol, &fade, &pri) ==
             SHOWDUINO_AUDIO_CMD_PLAY, "parse play fade pri");
  expect(strcmp(arg, "effects/boom.wav") == 0, "fade stripped from path");
  expect(fade == 2000, "fade 2000");
  expect(pri == SHOWDUINO_AUDIO_PRI_SFX, "pri sfx");
  expect(showduino_audio_parse_command_ex("AUDIO:NODE:STOP:FADE=1500",
                                          arg, sizeof(arg), &vol, &fade, &pri) ==
             SHOWDUINO_AUDIO_CMD_STOP, "parse stop fade");
  expect(fade == 1500, "stop fade 1500");
  expect(showduino_audio_parse_command("AUDIO:NODE:DUCK", arg, sizeof(arg), &vol) ==
             SHOWDUINO_AUDIO_CMD_DUCK, "parse duck");
  expect(showduino_audio_parse_command("AUDIO:NODE:UNDUCK", arg, sizeof(arg), &vol) ==
             SHOWDUINO_AUDIO_CMD_UNDUCK, "parse unduck");
  expect(showduino_audio_parse_command("AUDIO:NODE:INVENTORY:2", arg, sizeof(arg), &vol) ==
             SHOWDUINO_AUDIO_CMD_INVENTORY, "parse inventory");
  expect(strcmp(arg, "2") == 0, "inventory page");

  expect(showduino_audio_can_accept(SHOWDUINO_AUDIO_ST_NO_STORAGE, SHOWDUINO_AUDIO_CMD_PLAY) ==
             SHOWDUINO_AUDIO_FAIL_NO_STORAGE, "no storage blocks play");
  expect(showduino_audio_can_accept(SHOWDUINO_AUDIO_ST_LOOPING, SHOWDUINO_AUDIO_CMD_PAUSE) ==
             SHOWDUINO_AUDIO_FAIL_NONE, "looping accepts pause");
  expect(showduino_audio_can_accept(SHOWDUINO_AUDIO_ST_NO_STORAGE, SHOWDUINO_AUDIO_CMD_INVENTORY) ==
             SHOWDUINO_AUDIO_FAIL_NONE, "inventory always accepted");
  expect(showduino_audio_next_state(SHOWDUINO_AUDIO_ST_IDLE, SHOWDUINO_AUDIO_LIFE_STARTED,
                                    SHOWDUINO_AUDIO_CMD_LOOP) == SHOWDUINO_AUDIO_ST_LOOPING,
         "started loop -> looping");
  expect(showduino_audio_next_state(SHOWDUINO_AUDIO_ST_NO_STORAGE, SHOWDUINO_AUDIO_LIFE_NONE,
                                    SHOWDUINO_AUDIO_CMD_EMERGENCY_CLEAR) ==
             SHOWDUINO_AUDIO_ST_NO_STORAGE,
         "clear keeps no storage");
  expect(showduino_audio_on_comms_timeout(SHOWDUINO_AUDIO_ST_LOOPING) == SHOWDUINO_AUDIO_ST_IDLE,
         "timeout stops looping");
  expect(showduino_audio_on_comms_timeout(SHOWDUINO_AUDIO_ST_LOADING) == SHOWDUINO_AUDIO_ST_IDLE,
         "timeout stops loading");

  expect(showduino_audio_fade_level(80, 20, 0, 1000) == 80, "fade start");
  expect(showduino_audio_fade_level(80, 20, 500, 1000) == 50, "fade mid");
  expect(showduino_audio_fade_level(80, 20, 1000, 1000) == 20, "fade end");
  expect(showduino_audio_fade_level(0, 100, 250, 1000) == 25, "fade in");

  expect(showduino_audio_priority_from_path("effects/x.wav") == SHOWDUINO_AUDIO_PRI_SFX,
         "effects are sfx");
  expect(showduino_audio_priority_from_path("stingers/hit.wav") == SHOWDUINO_AUDIO_PRI_SFX,
         "stingers are sfx");
  expect(showduino_audio_priority_from_path("dialogue/line.wav") == SHOWDUINO_AUDIO_PRI_DIALOGUE,
         "dialogue pri");
  expect(showduino_audio_priority_from_path("ambience/room.wav") == SHOWDUINO_AUDIO_PRI_AMBIENCE,
         "ambience pri");
  expect(showduino_audio_can_preempt(SHOWDUINO_AUDIO_PRI_AMBIENCE, SHOWDUINO_AUDIO_PRI_SFX) == 1,
         "sfx preempts ambience");
  expect(showduino_audio_can_preempt(SHOWDUINO_AUDIO_PRI_SFX, SHOWDUINO_AUDIO_PRI_AMBIENCE) == 0,
         "ambience cannot preempt sfx");

  uint16_t start = 99, count = 99;
  expect(showduino_audio_inventory_slice(13, 0, &start, &count) == 1, "inv page 0");
  expect(start == 0 && count == 6, "inv page 0 slice");
  expect(showduino_audio_inventory_slice(13, 2, &start, &count) == 1, "inv page 2");
  expect(start == 12 && count == 1, "inv last slice");
  expect(showduino_audio_inventory_slice(13, 3, &start, &count) == 0, "inv empty page");

  expect(strcmp(showduino_audio_wire_token(SHOWDUINO_AUDIO_ST_LOOPING), "LOOPING") == 0,
         "wire looping");
  expect(strcmp(showduino_audio_wire_token(SHOWDUINO_AUDIO_ST_NO_STORAGE), "FAULT") == 0,
         "wire no storage");
  expect(strcmp(showduino_audio_wire_token(SHOWDUINO_AUDIO_ST_IDLE), "ONLINE") == 0,
         "wire idle");

  expect(showduino_audio_button_allowed(SHOWDUINO_AUDIO_ST_IDLE, 0, SHOWDUINO_AUDIO_BTN_PREV) == 1,
         "local prev idle");
  expect(showduino_audio_button_allowed(SHOWDUINO_AUDIO_ST_PLAYING, 1, SHOWDUINO_AUDIO_BTN_NEXT) == 0,
         "show blocks next");

  ShowduinoAudioConfig cfg;
  expect(showduino_audio_config_parse("{\"formatVersion\":1,\"volume\":70,\"output\":\"LINE\"}",
                                      48, &cfg) == SHOWDUINO_AUDIO_CFG_OK, "config ok");
  expect(cfg.volume == 70, "config volume");
  expect(strcmp(cfg.output, "LINE") == 0, "config output");
  showduino_audio_config_defaults(&cfg);
  expect(showduino_audio_config_parse("{\"formatVersion\":1,\"volume\":200}", 32, &cfg) ==
             SHOWDUINO_AUDIO_CFG_BAD, "bad volume rejected");
  expect(cfg.volume == 80, "bad config not applied");
  expect(showduino_audio_config_parse("{\"formatVersion\":1,\"commsTimeoutMs\":50}", 40, &cfg) ==
             SHOWDUINO_AUDIO_CFG_BAD, "timeout too low");
  expect(showduino_audio_config_parse("{\"formatVersion\":2}", 20, &cfg) ==
             SHOWDUINO_AUDIO_CFG_BAD, "bad format version");
  expect(showduino_audio_parse_command("AUDIO:NODE:OWN:GRANT", arg, sizeof(arg), &vol) ==
             SHOWDUINO_AUDIO_CMD_OWN_GRANT, "parse own grant");
  expect(showduino_audio_parse_command("OWN:GRANT", arg, sizeof(arg), &vol) ==
             SHOWDUINO_AUDIO_CMD_OWN_GRANT, "parse own grant short");
  expect(showduino_audio_parse_command("AUDIO:OWN:GRANT", arg, sizeof(arg), &vol) ==
             SHOWDUINO_AUDIO_CMD_OWN_GRANT, "parse audio own grant");

  expect(showduino_audio_owner_can_accept(SHOWDUINO_OWNER_STANDALONE,
                                         SHOWDUINO_AUDIO_CMD_PLAY,
                                         SHOWDUINO_CMD_ORIGIN_WEB) ==
             SHOWDUINO_AUDIO_FAIL_NONE, "standalone web play");
  expect(showduino_audio_owner_can_accept(SHOWDUINO_OWNER_SHOW_CONTROLLED,
                                         SHOWDUINO_AUDIO_CMD_PLAY,
                                         SHOWDUINO_CMD_ORIGIN_WEB) ==
             SHOWDUINO_AUDIO_FAIL_SHOW_CONTROLLED, "p4 blocks web play");
  expect(showduino_audio_owner_can_accept(SHOWDUINO_OWNER_SHOW_CONTROLLED,
                                         SHOWDUINO_AUDIO_CMD_PLAY,
                                         SHOWDUINO_CMD_ORIGIN_SHOW) ==
             SHOWDUINO_AUDIO_FAIL_NONE, "p4 play when owned");
  expect(showduino_audio_owner_can_accept(SHOWDUINO_OWNER_SEARCHING,
                                         SHOWDUINO_AUDIO_CMD_PLAY,
                                         SHOWDUINO_CMD_ORIGIN_SHOW) ==
             SHOWDUINO_AUDIO_FAIL_NOT_OWNER, "show play before grant rejected");
  expect(showduino_audio_owner_can_accept(SHOWDUINO_OWNER_SEARCHING,
                                         SHOWDUINO_AUDIO_CMD_PLAY,
                                         SHOWDUINO_CMD_ORIGIN_LOCAL) ==
             SHOWDUINO_AUDIO_FAIL_NONE, "local play during search");
  expect(showduino_audio_owner_can_accept(SHOWDUINO_OWNER_SHOW_CONTROLLED,
                                         SHOWDUINO_AUDIO_CMD_OWN_GRANT,
                                         SHOWDUINO_CMD_ORIGIN_WEB) ==
             SHOWDUINO_AUDIO_FAIL_NOT_OWNER, "web cannot grant");
  expect(showduino_audio_owner_can_accept(SHOWDUINO_OWNER_STANDALONE,
                                         SHOWDUINO_AUDIO_CMD_STATUS,
                                         SHOWDUINO_CMD_ORIGIN_WEB) ==
             SHOWDUINO_AUDIO_FAIL_NONE, "web status always");
  expect(showduino_audio_button_allowed(SHOWDUINO_AUDIO_ST_PLAYING, 1,
                                        SHOWDUINO_AUDIO_BTN_VOL_UP) == 0,
         "show blocks local volume");

  {
    ShowduinoOwnerMachine m;
    showduino_owner_begin(&m, 0);
    showduino_owner_apply(&m, SHOWDUINO_OWNER_EV_TICK, 7999, 8000, 8000);
    expect(m.mode == SHOWDUINO_OWNER_SEARCHING, "searching before 8s");
    showduino_owner_apply(&m, SHOWDUINO_OWNER_EV_KEEP, 100, 8000, 8000);
    expect(m.granted == 0 && m.mode == SHOWDUINO_OWNER_SEARCHING,
           "status keep does not grant");
    showduino_owner_apply(&m, SHOWDUINO_OWNER_EV_TICK, 8000, 8000, 8000);
    expect(m.mode == SHOWDUINO_OWNER_STANDALONE && m.enteredStandalone,
           "standalone after search");
    showduino_owner_begin(&m, 0);
    showduino_owner_apply(&m, SHOWDUINO_OWNER_EV_GRANT, 500, 8000, 8000);
    expect(m.mode == SHOWDUINO_OWNER_SHOW_CONTROLLED && m.enteredShow,
           "grant takes show control");
    showduino_owner_apply(&m, SHOWDUINO_OWNER_EV_TICK, 8500, 8000, 8000);
    expect(m.mode == SHOWDUINO_OWNER_STANDALONE && m.lostAuthority,
           "lost grant after keepalive");
  }

  expect(strstr(SHOWDUINO_AUDIO_CAPS, "STANDALONE") != NULL, "standalone advertised");
  expect(strstr(SHOWDUINO_AUDIO_CAPS, "OWN") != NULL, "own advertised");
  expect(strstr(SHOWDUINO_AUDIO_CAPS, "MP3") == NULL, "mp3 not advertised");
  expect(strstr(SHOWDUINO_AUDIO_CAPS, "FADE") != NULL, "fade advertised");
  expect(strstr(SHOWDUINO_AUDIO_CAPS, "MIC") != NULL, "mic advertised");

  if (gFail) {
    printf("%d FAIL\n", gFail);
    return 1;
  }
  printf("ALL PASS\n");
  return 0;
}
