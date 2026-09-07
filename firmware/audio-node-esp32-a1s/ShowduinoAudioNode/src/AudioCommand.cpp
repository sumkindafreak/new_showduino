#include "AudioCommand.h"
#include "AudioCodec.h"
#include "AudioPlayback.h"
#include "AudioStorage.h"
#include "AudioNodeState.h"
#include "AudioProtocol.h"
#include "EspNowNodeTransport.h"
#include "input/AudioInput.h"
#include "input/AudioInputConfig.h"
#include "input/SoundTriggerEngine.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_audio_node.h"
#include "../../../protocol/showduino_protocol_version.h"

static uint32_t sActiveSeq = 0;
static char sActiveRel[SHOWDUINO_AUDIO_REL_MAX + 1] = "";
static uint32_t sLastAnnounce = 0;
static uint32_t sVolume = SHOWDUINO_AUDIO_DEFAULT_VOLUME;
static ShowduinoAudioPriority sPri = SHOWDUINO_AUDIO_PRI_AMBIENCE;
static uint32_t sStartLatencyMs = 0;

static bool sFadeOn = false;
static uint8_t sFadeFrom = 0;
static uint8_t sFadeTo = 0;
static uint32_t sFadeStart = 0;
static uint32_t sFadeDur = 0;
static bool sFadeStop = false;

static bool sDuck = false;
static int16_t sLocalIndex = 0;

static void report(const char *line, uint32_t seq) {
  if (!line || !line[0]) return;
  Serial.print("[AUDIO] ");
  Serial.println(line);
  audioEspNowSend(line, seq);
}

static void fadeCancel() {
  sFadeOn = false;
  sFadeStop = false;
}

static void fadeStart(uint8_t from, uint8_t to, uint32_t ms, bool stopWhenDone) {
  if (ms == 0) {
    audioCodecSetVolume(to);
    sFadeOn = false;
    sFadeStop = false;
    return;
  }
  sFadeFrom = from;
  sFadeTo = to;
  sFadeDur = ms;
  sFadeStart = millis();
  sFadeOn = true;
  sFadeStop = stopWhenDone;
}

static uint8_t audibleVolume() {
  return sDuck ? audioStorageConfig().duckVolume : (uint8_t)sVolume;
}

static void applyMaster(uint8_t v, bool persist) {
  sVolume = showduino_audio_clamp_volume(v);
  if (!sFadeOn && !sDuck) audioCodecSetVolume((uint8_t)sVolume);
  else if (!sFadeOn && sDuck) audioCodecSetVolume(audioStorageConfig().duckVolume);
  if (persist) audioStorageSetVolume((uint8_t)sVolume);
}

void audioCommandBegin(uint8_t volume) {
  applyMaster((uint8_t)showduino_audio_clamp_volume(volume), false);
  audioCodecApplyOutput(audioStorageConfig().output);
}

static void stopPlaybackClear() {
  fadeCancel();
  sDuck = false;
  audioPlaybackStop();
  audioInputOnPlaybackStop();
  sActiveRel[0] = '\0';
  audioNodeStateSetShowControlled(false);
}

static bool startAsset(const char *rel, AudioPlayMode mode, uint32_t seq,
                       bool fromShow, int fadeMs, int priIn) {
  char absPath[SHOWDUINO_AUDIO_PATH_MAX + 1];
  const ShowduinoAudioPathStatus ps = showduino_audio_resolve_path(rel, absPath, sizeof(absPath));
  if (ps != SHOWDUINO_AUDIO_PATH_OK) {
    char line[96];
    audioProtocolFormatFailed(line, sizeof(line), seq, SHOWDUINO_AUDIO_FAIL_BAD_PATH);
    report(line, seq);
    return false;
  }
  if (!audioStorageReady() || !audioStorageExists(absPath)) {
    char line[96];
    audioProtocolFormatFailed(line, sizeof(line), seq,
                              audioStorageReady() ? SHOWDUINO_AUDIO_FAIL_FILE_NOT_FOUND
                                                  : SHOWDUINO_AUDIO_FAIL_NO_STORAGE);
    report(line, seq);
    return false;
  }
  if (!audioCodecReady()) {
    char line[96];
    audioProtocolFormatFailed(line, sizeof(line), seq, SHOWDUINO_AUDIO_FAIL_CODEC);
    report(line, seq);
    return false;
  }

  const ShowduinoAudioPriority incoming =
      (priIn >= 0) ? (ShowduinoAudioPriority)priIn : showduino_audio_priority_from_path(rel);
  if (audioPlaybackActive() || audioPlaybackPaused()) {
    if (fromShow && !showduino_audio_can_preempt(sPri, incoming)) {
      char line[96];
      audioProtocolFormatRejected(line, sizeof(line), seq, SHOWDUINO_AUDIO_FAIL_BAD_COMMAND);
      report(line, seq);
      return false;
    }
  }

  audioInputOnPlaybackStart();
  audioCodecMute(false);
  audioNodeStateSet(SHOWDUINO_AUDIO_ST_LOADING);
  char acc[48];
  audioProtocolFormatAccepted(acc, sizeof(acc), seq);
  report(acc, seq);

  const uint32_t t0 = millis();
  if (!audioPlaybackStart(absPath, mode)) {
    char line[96];
    const ShowduinoAudioFail f =
        (audioPlaybackLastError()[0] && strstr(audioPlaybackLastError(), "FILE"))
            ? SHOWDUINO_AUDIO_FAIL_FILE_NOT_FOUND
            : SHOWDUINO_AUDIO_FAIL_UNSUPPORTED;
    audioProtocolFormatFailed(line, sizeof(line), seq, f);
    report(line, seq);
    audioInputOnPlaybackStop();
    audioNodeStateSet(audioStorageReady() ? SHOWDUINO_AUDIO_ST_IDLE
                                          : SHOWDUINO_AUDIO_ST_NO_STORAGE);
    return false;
  }
  sStartLatencyMs = millis() - t0;

  strncpy(sActiveRel, rel, sizeof(sActiveRel) - 1);
  sActiveSeq = seq;
  sPri = incoming;
  audioNodeStateSet(mode == AudioPlayMode::Loop ? SHOWDUINO_AUDIO_ST_LOOPING
                                                : SHOWDUINO_AUDIO_ST_PLAYING);
  if (fadeMs < 0) fadeMs = (int)audioStorageConfig().fadeDefaultMs;
  if (fadeMs > 0) {
    audioCodecSetVolume(0);
    fadeStart(0, audibleVolume(), (uint32_t)fadeMs, false);
  } else {
    audioCodecSetVolume(audibleVolume());
  }
  char st[96];
  audioProtocolFormatStarted(st, sizeof(st), seq, sActiveRel);
  report(st, seq);
  return true;
}

static bool handleSoundCommand(const char *command, uint32_t sequence, bool fromShow) {
  const char *cmd = command;
  if (!strncmp(cmd, "AUDIO:NODE:SOUND:", 17)) cmd += 17;
  else if (!strncmp(cmd, "AUDIO:SOUND:", 12)) cmd += 12;
  else if (!strncmp(cmd, "SOUND:", 6)) cmd += 6;
  else return false;

  if (fromShow && !strcmp(cmd, "LOCAL_TEST_TRIGGER")) {
    char line[64];
    snprintf(line, sizeof(line), "SOUND:REJECTED:SHOW_MODE");
    Serial.print("[SOUND] ");
    Serial.println(line);
    audioEspNowSend(line, sequence);
    return true;
  }

  if (!strcmp(cmd, "STATUS") || !strcmp(cmd, "LEVEL") || !strcmp(cmd, "CONFIG")) {
    if (!strcmp(cmd, "CONFIG")) {
      char line[SHOWDUINO_NODE_COMMAND_MAX];
      snprintf(line, sizeof(line), "SOUND:CONFIG:%s,TH=%u,CD=%u,IN=%u",
               showduino_sound_mode_name(audioInputConfig().mode),
               (unsigned)audioInputConfig().threshold,
               (unsigned)audioInputConfig().cooldownMs,
               (unsigned)audioInputConfig().postPlaybackInhibitMs);
      Serial.print("[SOUND] ");
      Serial.println(line);
      audioEspNowSend(line, sequence);
    } else {
      audioInputPrintStatus();
      char line[SHOWDUINO_NODE_COMMAND_MAX];
      audioInputFormatStatus(line, sizeof(line));
      audioEspNowSend(line, sequence);
    }
    return true;
  }
  if (!strcmp(cmd, "ENABLE")) {
    audioInputEnable(true);
    char line[SHOWDUINO_NODE_COMMAND_MAX];
    audioInputFormatStatus(line, sizeof(line));
    audioEspNowSend(line, sequence);
    return true;
  }
  if (!strcmp(cmd, "DISABLE")) {
    audioInputEnable(false);
    char line[SHOWDUINO_NODE_COMMAND_MAX];
    audioInputFormatStatus(line, sizeof(line));
    audioEspNowSend(line, sequence);
    return true;
  }
  if (!strcmp(cmd, "CALIBRATE")) {
    audioInputCalibrate();
    return true;
  }
  if (!strcmp(cmd, "TRIGGER:TEST")) {
    audioInputTriggerTest();
    ShowduinoSoundEvent ev;
    if (soundTriggerTakeEvent(&ev)) {
      char line[SHOWDUINO_NODE_COMMAND_MAX];
      showduino_sound_format_trigger(line, sizeof(line), &ev);
      Serial.print("[SOUND] ");
      Serial.println(line);
      if (audioEspNowHaveComms() && audioNodeState() != SHOWDUINO_AUDIO_ST_EMERGENCY) {
        audioEspNowSend(line, sequence);
      }
    }
    return true;
  }
  if (!strcmp(cmd, "MONITOR")) {
    if (fromShow) return true;
    audioInputSetMonitor(true);
    Serial.println("[SOUND] MONITOR on");
    return true;
  }
  if (!strcmp(cmd, "MONITOR:STOP")) {
    audioInputSetMonitor(false);
    Serial.println("[SOUND] MONITOR off");
    return true;
  }
  if (!strcmp(cmd, "RECORD:TEST")) {
    audioInputStartRecordTest();
    return true;
  }
  if (!strcmp(cmd, "LOCAL_TEST_TRIGGER")) {
    audioInputSetLocalTest(true);
    return true;
  }
  if (!strncmp(cmd, "THRESHOLD:", 10)) {
    int v = atoi(cmd + 10);
    if (v < 0 || v > 100) {
      audioEspNowSend("SOUND:REJECTED:BAD_THRESHOLD", sequence);
      return true;
    }
    audioInputSetThreshold((uint8_t)v);
    char line[SHOWDUINO_NODE_COMMAND_MAX];
    audioInputFormatStatus(line, sizeof(line));
    audioEspNowSend(line, sequence);
    return true;
  }
  if (!strncmp(cmd, "COOLDOWN:", 9)) {
    int v = atoi(cmd + 9);
    if (v < 200 || v > 30000) {
      audioEspNowSend("SOUND:REJECTED:BAD_COOLDOWN", sequence);
      return true;
    }
    audioInputConfigSetCooldown((uint16_t)v, true);
    soundTriggerApplyConfig(&audioInputConfig());
    return true;
  }
  if (!strncmp(cmd, "INHIBIT:", 8)) {
    int v = atoi(cmd + 8);
    if (v < 0 || v > 10000) {
      audioEspNowSend("SOUND:REJECTED:BAD_INHIBIT", sequence);
      return true;
    }
    audioInputConfigSetInhibit((uint16_t)v, true);
    soundTriggerApplyConfig(&audioInputConfig());
    return true;
  }
  if (!strncmp(cmd, "MODE:", 5)) {
    uint8_t mode = 0;
    if (!showduino_sound_mode_from_name(cmd + 5, &mode)) {
      audioEspNowSend("SOUND:REJECTED:BAD_MODE", sequence);
      return true;
    }
    audioInputConfigSetMode(mode, true);
    soundTriggerApplyConfig(&audioInputConfig());
    return true;
  }
  return false;
}

void audioCommandApply(const char *command, uint32_t sequence, bool fromShow) {
  if (!command || !command[0]) return;
  if (fromShow) audioNodeStateNoteComms();
  if (handleSoundCommand(command, sequence, fromShow)) return;

  char arg[SHOWDUINO_AUDIO_REL_MAX + 1];
  int volume = -1;
  int fadeMs = -1;
  int pri = -1;
  const ShowduinoAudioCmd cmd =
      showduino_audio_parse_command_ex(command, arg, sizeof(arg), &volume, &fadeMs, &pri);
  const ShowduinoAudioFail gate = showduino_audio_can_accept(audioNodeState(), cmd);
  if (gate != SHOWDUINO_AUDIO_FAIL_NONE) {
    char line[96];
    audioProtocolFormatRejected(line, sizeof(line), sequence, gate);
    report(line, sequence);
    return;
  }

  if (cmd == SHOWDUINO_AUDIO_CMD_EMERGENCY_STOP) {
    stopPlaybackClear();
    audioCodecMute(true);
    audioInputSetEmergency(true);
    audioNodeStateSet(SHOWDUINO_AUDIO_ST_EMERGENCY);
    char line[48];
    snprintf(line, sizeof(line), "AUDIO:EMERGENCY:%lu", (unsigned long)sequence);
    report(line, sequence);
    return;
  }
  if (cmd == SHOWDUINO_AUDIO_CMD_EMERGENCY_CLEAR) {
    audioCodecMute(false);
    audioInputSetEmergency(false);
    applyMaster((uint8_t)sVolume, false);
    if (audioNodeState() != SHOWDUINO_AUDIO_ST_FAULT &&
        audioNodeState() != SHOWDUINO_AUDIO_ST_NO_STORAGE) {
      audioNodeStateSet(SHOWDUINO_AUDIO_ST_IDLE);
    }
    char line[48];
    snprintf(line, sizeof(line), "AUDIO:IDLE:%lu", (unsigned long)sequence);
    report(line, sequence);
    return;
  }
  if (cmd == SHOWDUINO_AUDIO_CMD_STOP) {
    if (fadeMs > 0 && audioNodeState() != SHOWDUINO_AUDIO_ST_EMERGENCY &&
        (audioPlaybackActive() || audioPlaybackPaused())) {
      char acc[48];
      audioProtocolFormatAccepted(acc, sizeof(acc), sequence);
      report(acc, sequence);
      audioNodeStateSet(SHOWDUINO_AUDIO_ST_STOPPING);
      fadeStart(audibleVolume(), 0, (uint32_t)fadeMs, true);
      return;
    }
    stopPlaybackClear();
    audioCodecSetVolume((uint8_t)sVolume);
    if (audioNodeState() != SHOWDUINO_AUDIO_ST_FAULT &&
        audioNodeState() != SHOWDUINO_AUDIO_ST_EMERGENCY &&
        audioNodeState() != SHOWDUINO_AUDIO_ST_NO_STORAGE) {
      audioNodeStateSet(SHOWDUINO_AUDIO_ST_IDLE);
    }
    char line[48];
    snprintf(line, sizeof(line), "AUDIO:IDLE:%lu", (unsigned long)sequence);
    report(line, sequence);
    return;
  }
  if (cmd == SHOWDUINO_AUDIO_CMD_PAUSE) {
    if (audioPlaybackPause()) {
      audioNodeStateSet(SHOWDUINO_AUDIO_ST_PAUSED);
      char line[48];
      snprintf(line, sizeof(line), "AUDIO:PAUSED:%lu", (unsigned long)sequence);
      report(line, sequence);
    }
    return;
  }
  if (cmd == SHOWDUINO_AUDIO_CMD_RESUME) {
    if (audioPlaybackResume()) {
      audioNodeStateSet(audioPlaybackLooping() ? SHOWDUINO_AUDIO_ST_LOOPING
                                               : SHOWDUINO_AUDIO_ST_PLAYING);
      char st[96];
      audioProtocolFormatStarted(st, sizeof(st), sequence, sActiveRel);
      report(st, sequence);
    }
    return;
  }
  if (cmd == SHOWDUINO_AUDIO_CMD_VOLUME && showduino_audio_volume_ok(volume)) {
    fadeCancel();
    sDuck = false;
    audioCodecMute(false);
    applyMaster((uint8_t)volume, true);
    char line[32];
    snprintf(line, sizeof(line), "AUDIO:VOLUME:%d", volume);
    report(line, sequence);
    return;
  }
  if (cmd == SHOWDUINO_AUDIO_CMD_DUCK) {
    const uint8_t from = sDuck ? audioStorageConfig().duckVolume : (uint8_t)sVolume;
    sDuck = true;
    fadeStart(from, audioStorageConfig().duckVolume, 250, false);
    char line[32];
    snprintf(line, sizeof(line), "AUDIO:VOLUME:%u", (unsigned)audioStorageConfig().duckVolume);
    report(line, sequence);
    return;
  }
  if (cmd == SHOWDUINO_AUDIO_CMD_UNDUCK) {
    const uint8_t from = sDuck ? audioStorageConfig().duckVolume : (uint8_t)sVolume;
    sDuck = false;
    fadeStart(from, (uint8_t)sVolume, 250, false);
    char line[32];
    snprintf(line, sizeof(line), "AUDIO:VOLUME:%u", (unsigned)sVolume);
    report(line, sequence);
    return;
  }
  if (cmd == SHOWDUINO_AUDIO_CMD_INVENTORY) {
    char names[SHOWDUINO_AUDIO_INV_MAX][40];
    const uint16_t total = audioStorageInventory(names, SHOWDUINO_AUDIO_INV_MAX);
    const uint16_t page = (uint16_t)atoi(arg);
    uint16_t start = 0, count = 0;
    showduino_audio_inventory_slice(total, page, &start, &count);
    char line[SHOWDUINO_NODE_COMMAND_MAX];
    size_t n = (size_t)snprintf(line, sizeof(line), "AUDIO:INVENTORY:%u:%u:",
                                (unsigned)page, (unsigned)total);
    for (uint16_t i = 0; i < count && n + 2 < sizeof(line); i++) {
      n += (size_t)snprintf(line + n, sizeof(line) - n, "%s%s",
                            i ? "," : "", names[start + i]);
    }
    report(line, sequence);
    return;
  }
  if (cmd == SHOWDUINO_AUDIO_CMD_STATUS) {
    char line[96];
    const char *fault = showduino_audio_fail_name(audioNodeStateFault());
    if (audioStorageConfigFault() && audioNodeStateFault() == SHOWDUINO_AUDIO_FAIL_NONE) {
      fault = "CONFIG_FAULT";
    }
    audioProtocolFormatStatus(line, sizeof(line), audioNodeStateName(),
                              (uint8_t)sVolume, sActiveRel, fault);
    report(line, sequence);
    char caps[SHOWDUINO_NODE_COMMAND_MAX];
    snprintf(caps, sizeof(caps), "AUDIO:CAPS:%s", SHOWDUINO_AUDIO_CAPS);
    report(caps, sequence);
    char meta[SHOWDUINO_NODE_COMMAND_MAX];
    snprintf(meta, sizeof(meta), "AUDIO:META:%s:%s:%s",
             SHOWDUINO_AUDIO_NODE_CODEC, audioStorageStateName(),
             audioCodecOutputName());
    report(meta, sequence);
    return;
  }
  if (cmd == SHOWDUINO_AUDIO_CMD_PLAY || cmd == SHOWDUINO_AUDIO_CMD_LOOP ||
      cmd == SHOWDUINO_AUDIO_CMD_TEST) {
    const char *rel = arg[0] ? arg : SHOWDUINO_AUDIO_TEST_FILE;
    audioNodeStateSetShowControlled(fromShow);
    startAsset(rel,
               cmd == SHOWDUINO_AUDIO_CMD_LOOP ? AudioPlayMode::Loop : AudioPlayMode::Once,
               sequence, fromShow, fadeMs, pri);
  }
}

void audioCommandLocalTestToggle() {
  if (audioPlaybackActive() || audioPlaybackPaused()) {
    Serial.println("[AUDIO] Local test: stop");
    audioCommandApply("AUDIO:NODE:STOP", 0, false);
    return;
  }
  char names[SHOWDUINO_AUDIO_INV_MAX][40];
  const uint16_t n = audioStorageInventory(names, SHOWDUINO_AUDIO_INV_MAX);
  const char *rel = SHOWDUINO_AUDIO_TEST_FILE;
  if (n > 0) {
    if (sLocalIndex < 0 || sLocalIndex >= n) sLocalIndex = 0;
    rel = names[sLocalIndex];
  }
  Serial.printf("[AUDIO] Local test: %s\n", rel);
  char cmd[96];
  snprintf(cmd, sizeof(cmd), "AUDIO:NODE:PLAY:%s", rel);
  audioCommandApply(cmd, 0, false);
}

void audioCommandLocalStop() {
  audioCommandApply("AUDIO:NODE:STOP", 0, false);
}

static void stepLocal(int dir) {
  char names[SHOWDUINO_AUDIO_INV_MAX][40];
  const uint16_t n = audioStorageInventory(names, SHOWDUINO_AUDIO_INV_MAX);
  if (n == 0) {
    Serial.println("[AUDIO] No local assets");
    return;
  }
  sLocalIndex = (int16_t)((sLocalIndex + dir + (int)n) % (int)n);
  Serial.printf("[AUDIO] Select %s\n", names[sLocalIndex]);
}

void audioCommandLocalPrev() { stepLocal(-1); }
void audioCommandLocalNext() { stepLocal(1); }

void audioCommandNudgeVolume(int delta) {
  fadeCancel();
  sDuck = false;
  applyMaster((uint8_t)showduino_audio_clamp_volume((int)sVolume + delta), true);
  Serial.printf("[AUDIO] Volume %u\n", (unsigned)sVolume);
}

void audioCommandAnnounce() {
  char mac[24];
  char line[96];
  audioEspNowMacString(mac, sizeof(mac));
  audioProtocolFormatAnnounce(line, sizeof(line), mac, SHOWDUINO_AUDIO_NODE_FW,
                              audioNodeStateName());
  audioEspNowSend(line, 0);
}

void audioCommandService() {
  if (sFadeOn) {
    const uint32_t elapsed = millis() - sFadeStart;
    const uint8_t lvl = showduino_audio_fade_level(sFadeFrom, sFadeTo, elapsed, sFadeDur);
    audioCodecSetVolume(lvl);
    if (elapsed >= sFadeDur) {
      const bool stop = sFadeStop;
      fadeCancel();
      if (stop) {
        stopPlaybackClear();
        audioCodecSetVolume((uint8_t)sVolume);
        if (audioNodeState() != SHOWDUINO_AUDIO_ST_FAULT &&
            audioNodeState() != SHOWDUINO_AUDIO_ST_EMERGENCY &&
            audioNodeState() != SHOWDUINO_AUDIO_ST_NO_STORAGE) {
          audioNodeStateSet(SHOWDUINO_AUDIO_ST_IDLE);
        }
        char line[48];
        snprintf(line, sizeof(line), "AUDIO:IDLE:%lu", (unsigned long)sActiveSeq);
        report(line, sActiveSeq);
      }
    }
  }

  if (audioPlaybackJustCompleted()) {
    fadeCancel();
    audioInputOnPlaybackStop();
    char line[96];
    audioProtocolFormatCompleted(line, sizeof(line), sActiveSeq, sActiveRel);
    report(line, sActiveSeq);
    audioNodeStateSet(SHOWDUINO_AUDIO_ST_IDLE);
    audioNodeStateSetShowControlled(false);
    sActiveRel[0] = '\0';
  }
  if (audioPlaybackJustFailed()) {
    fadeCancel();
    audioInputOnPlaybackStop();
    char line[96];
    audioProtocolFormatFailed(line, sizeof(line), sActiveSeq, SHOWDUINO_AUDIO_FAIL_UNSUPPORTED);
    report(line, sActiveSeq);
    audioNodeStateSet(SHOWDUINO_AUDIO_ST_IDLE);
    audioNodeStateSetShowControlled(false);
  }

  if (audioStorageJustRemoved()) {
    stopPlaybackClear();
    audioNodeStateSetFault(SHOWDUINO_AUDIO_FAIL_NO_STORAGE);
    audioNodeStateSet(SHOWDUINO_AUDIO_ST_NO_STORAGE);
    report("AUDIO:FAILED:0:NO_STORAGE", 0);
  }
  if (audioStorageJustInserted()) {
    if (audioNodeState() == SHOWDUINO_AUDIO_ST_NO_STORAGE ||
        audioNodeState() == SHOWDUINO_AUDIO_ST_FAULT) {
      audioNodeStateClearFault();
      audioNodeStateSet(SHOWDUINO_AUDIO_ST_IDLE);
    }
  }

  const uint32_t timeoutMs = audioStorageConfig().commsTimeoutMs;
  if (audioNodeStateShowControlled() &&
      !audioNodeStateAuthorityFresh(timeoutMs)) {
    const ShowduinoAudioNodeState next = showduino_audio_on_comms_timeout(audioNodeState());
    if (next != audioNodeState()) {
      Serial.println("[AUDIO] Comms timeout — stopping show playback");
      stopPlaybackClear();
      audioCodecMute(true);
      audioNodeStateSet(next);
      char line[64];
      audioProtocolFormatFailed(line, sizeof(line), sActiveSeq,
                                SHOWDUINO_AUDIO_FAIL_COMMS_TIMEOUT);
      report(line, sActiveSeq);
    }
  }

  if ((millis() - sLastAnnounce) >= SHOWDUINO_AUDIO_ANNOUNCE_MS) {
    sLastAnnounce = millis();
    audioCommandAnnounce();
  }
}

uint8_t audioCommandVolume() { return (uint8_t)sVolume; }
bool audioCommandDucking() { return sDuck; }
uint32_t audioCommandLastStartLatencyMs() { return sStartLatencyMs; }
