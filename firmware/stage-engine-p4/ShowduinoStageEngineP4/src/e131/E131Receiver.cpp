#include "E131Receiver.h"

#include <NetworkUdp.h>
#include <string.h>
#include "../network/ShowNetwork.h"

static const uint32_t kStaleMs = 1500;
static const uint32_t kOfflineMs = 2500;
static const uint8_t kMaxPacketsPerLoop = 8;

static E131RxStatus sSt;
static uint8_t sSlots[SHOWDUINO_E131_MAX_SLOTS];
static NetworkUDP sUdp;
static uint16_t sBoundUniverse = 0;
static uint32_t sWindowStartMs = 0;
static uint16_t sWindowPackets = 0;
static uint32_t sLastRateMs = 0;

const char *e131RxStateName(E131RxState state) {
  switch (state) {
    case E131RxState::Online: return "ONLINE";
    case E131RxState::Stale: return "STALE";
    case E131RxState::Offline: return "OFFLINE";
    default: return "UNAVAILABLE";
  }
}

static void formatCid(const uint8_t cid[16], char *out, size_t n) {
  if (!out || n < 33) {
    if (out && n) out[0] = '\0';
    return;
  }
  size_t pos = 0;
  for (int i = 0; i < 16 && pos + 3 < n; i++) {
    pos += (size_t)snprintf(out + pos, n - pos, "%s%02X", i ? ":" : "", cid[i]);
  }
}

static void clearSource() {
  sSt.sourceName[0] = '\0';
  sSt.cid[0] = '\0';
  sSt.priority = 0;
  sSt.lastSequence = 0;
  sSt.haveSequence = false;
  sSt.lastPacketMs = 0;
  sSt.rateFps = 0;
  memset(sSlots, 0, sizeof(sSlots));
}

static void closeSocket() {
  sUdp.stop();
  sSt.socketOpen = false;
  sSt.multicastJoined = false;
  sBoundUniverse = 0;
}

static bool openSocket(uint16_t universe) {
  closeSocket();
  uint8_t m[4];
  showduino_e131_multicast_ipv4(universe, m);
  snprintf(sSt.multicast, sizeof(sSt.multicast), "%u.%u.%u.%u", m[0], m[1], m[2], m[3]);
  const IPAddress group(m[0], m[1], m[2], m[3]);
  if (!sUdp.beginMulticast(group, SHOWDUINO_E131_UDP_PORT)) {
    Serial.printf("[E131] Multicast join failed for %s — unicast listen only\n", sSt.multicast);
    if (!sUdp.begin(SHOWDUINO_E131_UDP_PORT)) {
      Serial.println("[E131] UDP 5568 bind failed");
      sSt.socketOpen = false;
      sSt.multicastJoined = false;
      return false;
    }
    sSt.socketOpen = true;
    sSt.multicastJoined = false;
    sBoundUniverse = universe;
    return true;
  }
  sSt.socketOpen = true;
  sSt.multicastJoined = true;
  sBoundUniverse = universe;
  Serial.printf("[E131] Listening UDP %u universe %u multicast %s\n",
                (unsigned)SHOWDUINO_E131_UDP_PORT, (unsigned)universe, sSt.multicast);
  return true;
}

static void updateState(uint32_t now) {
  const ShowNetConfig &cfg = showNetworkSavedConfig();
  sSt.enabled = cfg.e131Enabled;
  sSt.universe = cfg.e131Universe;
  if (!cfg.e131Enabled) {
    sSt.state = E131RxState::Unavailable;
    return;
  }
  if (!showNetworkHasAddress()) {
    sSt.state = E131RxState::Unavailable;
    return;
  }
  if (!sSt.lastPacketMs) {
    sSt.state = E131RxState::Offline;
    sSt.lastAgeMs = 0;
    return;
  }
  sSt.lastAgeMs = now - sSt.lastPacketMs;
  if (sSt.lastAgeMs > kOfflineMs) {
    if (sSt.state != E131RxState::Offline) {
      Serial.println("[E131] Source OFFLINE — local show continues");
    }
    sSt.state = E131RxState::Offline;
    sSt.rateFps = 0;
  } else if (sSt.lastAgeMs > kStaleMs) {
    if (sSt.state == E131RxState::Online) {
      Serial.println("[E131] Source STALE");
    }
    sSt.state = E131RxState::Stale;
  } else {
    sSt.state = E131RxState::Online;
  }
}

void e131ReceiverBegin() {
  memset(&sSt, 0, sizeof(sSt));
  memset(sSlots, 0, sizeof(sSlots));
  sSt.state = E131RxState::Unavailable;
  sSt.universe = showNetworkSavedConfig().e131Universe;
  sSt.enabled = showNetworkSavedConfig().e131Enabled;
}

void e131ReceiverStop() {
  closeSocket();
  sSt.state = E131RxState::Unavailable;
}

void e131ReceiverOnNetworkChange() {
  closeSocket();
  clearSource();
  updateState(millis());
}

void e131ReceiverLoop() {
  const uint32_t t0 = micros();
  const uint32_t now = millis();
  const ShowNetConfig &cfg = showNetworkSavedConfig();
  sSt.enabled = cfg.e131Enabled;
  sSt.universe = cfg.e131Universe;

  if (!cfg.e131Enabled || !showNetworkHasAddress()) {
    if (sSt.socketOpen) closeSocket();
    updateState(now);
    sSt.lastLoopUs = micros() - t0;
    if (sSt.lastLoopUs > sSt.maxLoopUs) sSt.maxLoopUs = sSt.lastLoopUs;
    return;
  }

  if (!sSt.socketOpen || sBoundUniverse != cfg.e131Universe) {
    openSocket(cfg.e131Universe);
  }

  uint8_t processed = 0;
  uint8_t pkt[SHOWDUINO_E131_MAX_PACKET];
  while (processed < kMaxPacketsPerLoop) {
    const int n = sUdp.parsePacket();
    if (n <= 0) break;
    processed++;
    if (n > (int)SHOWDUINO_E131_MAX_PACKET) {
      sUdp.clear();
      sSt.packetsRejected++;
      sSt.lastRejectReason = SHOWDUINO_E131_TOO_LARGE;
      strncpy(sSt.lastRejectName, "TOO_LARGE", sizeof(sSt.lastRejectName) - 1);
      continue;
    }
    const int got = sUdp.read(pkt, (size_t)n);
    if (got <= 0) {
      sSt.packetsRejected++;
      continue;
    }

    ShowduinoE131View view;
    const int st = showduino_e131_parse(pkt, (size_t)got, cfg.e131Universe, &view);
    if (st == SHOWDUINO_E131_UNSUPPORTED_START_CODE) {
      sSt.packetsRejected++;
      sSt.lastRejectReason = (uint32_t)st;
      strncpy(sSt.lastRejectName, showduino_e131_status_name(st), sizeof(sSt.lastRejectName) - 1);
      continue;
    }
    if (st != SHOWDUINO_E131_OK) {
      sSt.packetsRejected++;
      sSt.lastRejectReason = (uint32_t)st;
      strncpy(sSt.lastRejectName, showduino_e131_status_name(st), sizeof(sSt.lastRejectName) - 1);
      continue;
    }

    const int seq = showduino_e131_sequence_check(sSt.lastSequence, view.sequence,
                                                  sSt.haveSequence ? 1 : 0);
    if (seq != SHOWDUINO_E131_OK) {
      sSt.packetsRejected++;
      sSt.lastRejectReason = (uint32_t)seq;
      strncpy(sSt.lastRejectName, showduino_e131_status_name(seq), sizeof(sSt.lastRejectName) - 1);
      continue;
    }

    if (view.slots && view.slotCount) {
      const uint16_t ncopy = view.slotCount > SHOWDUINO_E131_MAX_SLOTS
                                 ? SHOWDUINO_E131_MAX_SLOTS
                                 : view.slotCount;
      memcpy(sSlots, view.slots, ncopy);
      if (ncopy < SHOWDUINO_E131_MAX_SLOTS) {
        memset(sSlots + ncopy, 0, SHOWDUINO_E131_MAX_SLOTS - ncopy);
      }
    } else {
      memset(sSlots, 0, sizeof(sSlots));
    }

    strncpy(sSt.sourceName, view.sourceName, sizeof(sSt.sourceName) - 1);
    formatCid(view.cid, sSt.cid, sizeof(sSt.cid));
    sSt.priority = view.priority;
    sSt.lastSequence = view.sequence;
    sSt.haveSequence = true;
    sSt.packetsOk++;
    sSt.lastPacketMs = now;
    sWindowPackets++;
    if (sSt.state != E131RxState::Online) {
      Serial.printf("[E131] Source ONLINE name=%s pri=%u\n",
                    sSt.sourceName[0] ? sSt.sourceName : "?",
                    (unsigned)sSt.priority);
    }
    sSt.state = E131RxState::Online;
  }

  if (sWindowStartMs == 0) sWindowStartMs = now;
  if ((now - sWindowStartMs) >= 1000) {
    sSt.rateFps = (float)sWindowPackets * 1000.0f / (float)(now - sWindowStartMs);
    sWindowPackets = 0;
    sWindowStartMs = now;
    sLastRateMs = now;
  }
  (void)sLastRateMs;
  updateState(now);
  sSt.lastLoopUs = micros() - t0;
  if (sSt.lastLoopUs > sSt.maxLoopUs) sSt.maxLoopUs = sSt.lastLoopUs;
}

const E131RxStatus &e131ReceiverStatus() { return sSt; }
const uint8_t *e131ReceiverSlots() { return sSlots; }

void e131ReceiverCopySlots(uint8_t *out, uint16_t from, uint16_t count) {
  if (!out || count == 0) return;
  if (from < 1) from = 1;
  if (from > SHOWDUINO_E131_MAX_SLOTS) {
    memset(out, 0, count);
    return;
  }
  const uint16_t idx = (uint16_t)(from - 1);
  uint16_t avail = SHOWDUINO_E131_MAX_SLOTS - idx;
  if (count > avail) {
    memcpy(out, sSlots + idx, avail);
    memset(out + avail, 0, count - avail);
  } else {
    memcpy(out, sSlots + idx, count);
  }
}

void e131ReceiverPrintStatus() {
  Serial.println("[E131]");
  Serial.printf("Receiver: %s\n", e131RxStateName(sSt.state));
  Serial.printf("Enabled: %s\n", sSt.enabled ? "YES" : "NO");
  Serial.printf("Universe: %u\n", (unsigned)sSt.universe);
  Serial.printf("Multicast: %s %s\n",
                sSt.multicast[0] ? sSt.multicast : "--",
                sSt.multicastJoined ? "JOINED" : "NOT_JOINED");
  Serial.printf("Source: %s\n", sSt.sourceName[0] ? sSt.sourceName : "--");
  Serial.printf("Priority: %u\n", (unsigned)sSt.priority);
  Serial.printf("Rate: %.1f fps\n", (double)sSt.rateFps);
  Serial.printf("Packets: %lu\n", (unsigned long)sSt.packetsOk);
  Serial.printf("Rejected: %lu\n", (unsigned long)sSt.packetsRejected);
  if (sSt.lastPacketMs) {
    Serial.printf("Last packet: %lu ms\n", (unsigned long)sSt.lastAgeMs);
  }
  if (sSt.lastRejectName[0]) {
    Serial.printf("Last reject: %s\n", sSt.lastRejectName);
  }
  Serial.printf("Loop: last=%lu us max=%lu us\n",
                (unsigned long)sSt.lastLoopUs, (unsigned long)sSt.maxLoopUs);
  Serial.printf("Heap: free=%lu min=%lu\n",
                (unsigned long)ESP.getFreeHeap(),
                (unsigned long)ESP.getMinFreeHeap());
}

void e131ReceiverPrintChannels(uint16_t from, uint16_t count) {
  if (from < 1) from = 1;
  if (count == 0) count = 32;
  if (from > SHOWDUINO_E131_MAX_SLOTS) return;
  if ((uint32_t)from + count - 1 > SHOWDUINO_E131_MAX_SLOTS) {
    count = (uint16_t)(SHOWDUINO_E131_MAX_SLOTS - from + 1);
  }
  Serial.printf("[E131] Channels %u-%u\n", (unsigned)from, (unsigned)(from + count - 1));
  for (uint16_t i = 0; i < count; i++) {
    const uint16_t ch = (uint16_t)(from + i);
    Serial.printf("%03u  %03u\n", (unsigned)ch, (unsigned)sSlots[ch - 1]);
  }
}

bool e131ReceiverHandleCommand(const String &command) {
  if (command == "E131:STATUS") {
    e131ReceiverPrintStatus();
    return true;
  }
  if (command == "E131:CHANNELS") {
    e131ReceiverPrintChannels(1, 32);
    return true;
  }
  if (command.startsWith("E131:CHANNELS:")) {
    String rest = command.substring(14);
    int c = rest.indexOf(':');
    uint16_t from = (uint16_t)rest.toInt();
    uint16_t count = 32;
    if (c >= 0) count = (uint16_t)rest.substring(c + 1).toInt();
    if (from < 1) from = 1;
    if (count < 1) count = 1;
    if (count > 64) count = 64;
    e131ReceiverPrintChannels(from, count);
    return true;
  }
  if (command.startsWith("E131:ENABLE:")) {
    String rest = command.substring(12);
    rest.toUpperCase();
    ShowNetConfig next = showNetworkSavedConfig();
    if (rest == "1" || rest == "ON" || rest == "YES" || rest == "TRUE") next.e131Enabled = true;
    else if (rest == "0" || rest == "OFF" || rest == "NO" || rest == "FALSE") next.e131Enabled = false;
    else return false;
    char err[48] = "";
    return showNetworkSaveConfig(next, err, sizeof(err));
  }
  if (command.startsWith("E131:UNIVERSE:")) {
    const long u = command.substring(14).toInt();
    if (u < SHOWDUINO_E131_UNIVERSE_MIN || u > SHOWDUINO_E131_UNIVERSE_MAX) {
      Serial.println("[E131] Universe out of range");
      return false;
    }
    ShowNetConfig next = showNetworkSavedConfig();
    next.e131Universe = (uint16_t)u;
    char err[48] = "";
    return showNetworkSaveConfig(next, err, sizeof(err));
  }
  return false;
}
