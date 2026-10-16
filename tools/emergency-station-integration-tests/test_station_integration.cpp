/*
 * MISSION 001H integration tests: wireless Emergency Node <-> P4 report interpretation.
 *
 * Exercised, using the REAL protocol code:
 *   C3 side  : ShowduinoEmergencyMachine and its transition, transmit and
 *              command-observation functions; format/parse of ANNOUNCE, ASSERT
 *              and commands (protocol/showduino_emergency_node.h).
 *   P4 side  : showduino_station_interpret_line() (protocol/showduino_station_model.h),
 *              the same function the Stage Engine calls; blocker encode/decode
 *              and the clear gate (protocol/showduino_station_blockers.h).
 *
 * Simulated: radio delivery (messages passed in-process, in order, no loss unless
 * a test drops them), the P4 global latch and Director CLEAR, time (ms counter),
 * and station disconnection (timeout is driven explicitly).
 *
 * NOT covered: the C3 Arduino wrapper (EmergencyProtocol.cpp, ESP-NOW, GPIO
 * debounce), the P4 Arduino wrapper (EmergencyNodeLink.cpp, NVS, Serial). Those
 * need hardware acceptance. This is protocol behaviour only.
 */
#include "showduino_emergency_node.h"
#include "showduino_station_blockers.h"
#include "showduino_station_model.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int gFails = 0;
static void expect(bool ok, const char *msg) {
  if (ok) std::printf("PASS  %s\n", msg);
  else { std::printf("FAIL  %s\n", msg); gFails++; }
}

/* ---------------- C3 Emergency Node (real machine) ---------------- */
struct C3 {
  ShowduinoEmergencyMachine m{};
  char id[SHOWDUINO_EMERGENCY_ID_MAX + 1];
  char name[SHOWDUINO_EMERGENCY_NAME_MAX + 1];
  char mac[18];
  bool pressed = false;
  bool announced = false;
  uint32_t lastAnnounceMs = 0;
  uint32_t lastNow = 0;
  int assertsSent = 0;
  int announcesSent = 0;
  bool connected = true;

  C3(const char *idIn, const char *macIn, bool startPressed) {
    std::snprintf(id, sizeof(id), "%s", idIn);
    std::snprintf(name, sizeof(name), "%s", "TEST");
    std::snprintf(mac, sizeof(mac), "%s", macIn);
    pressed = startPressed;
    showduino_emergency_machine_init(&m, pressed ? 1 : 0);
    showduino_emergency_machine_set_radio(&m, 1);
  }

  std::string announce(uint32_t now) {
    ShowduinoEmergencyAnnounce an{};
    std::snprintf(an.mac, sizeof(an.mac), "%s", mac);
    std::snprintf(an.firmware, sizeof(an.firmware), "%s", "1.0.0");
    std::snprintf(an.state, sizeof(an.state), "%s", showduino_emergency_state_name(m.state));
    std::snprintf(an.id, sizeof(an.id), "%s", id);
    std::snprintf(an.name, sizeof(an.name), "%s", name);
    an.input_open = m.input_open;
    an.latched = m.latched;
    an.acked = m.acked;
    char buf[160];
    showduino_emergency_format_announce(&an, buf, sizeof(buf));
    lastAnnounceMs = now;
    announced = true;
    announcesSent++;
    return std::string(buf);
  }

  /* One service pass: mirrors EmergencyProtocol.cpp emergencyProtocolService(). */
  std::vector<std::string> tick(uint32_t now) {
    lastNow = now;
    std::vector<std::string> out;
    if (!connected) return out;
    showduino_emergency_machine_input(&m, pressed ? 1 : 0);
    showduino_emergency_machine_set_radio(&m, 1);
    if (showduino_emergency_machine_want_tx(&m, now)) {
      char cmd[48];
      showduino_emergency_format_assert(id, name, cmd, sizeof(cmd));
      out.push_back(cmd);
      assertsSent++;
      showduino_emergency_machine_sent(&m, now);
    }
    if (!announced || (now - lastAnnounceMs) >= SHOWDUINO_EMERGENCY_HEARTBEAT_MS) {
      out.push_back(announce(now));
    }
    return out;
  }

  /* P4 -> C3 command. Mirrors the C3 command handling for the commands this test uses. */
  std::vector<std::string> receive(const std::string &cmd) {
    std::vector<std::string> out;
    if (!connected) return out;
    const ShowduinoEmergencyNodeCmd c = showduino_emergency_parse_command(cmd.c_str());
    if (c == SHOWDUINO_ESTOP_CMD_GLOBAL_OBSERVED) {
      showduino_emergency_machine_global_observed(&m);
      out.push_back(announce(lastNow));
    } else if (c == SHOWDUINO_ESTOP_CMD_ACK_LATCHED) {
      showduino_emergency_machine_ack(&m);
    }
    return out;
  }

  void setPressed(bool p) { pressed = p; }
};

/* A bare ASSERT line as sent by a C3 (no button state). */
static std::string bareAssert(const C3 &c) {
  char buf[64];
  showduino_emergency_format_assert(c.id, c.name, buf, sizeof(buf));
  return std::string(buf);
}

/* ---------------- Stored blocker record (simulated NVS / SD) ---------------- */
struct StoreSim {
  std::string value;
  bool valid = false;
  int failWrites = 0; /* next N writes fail */
  bool save(const std::string &enc) {
    if (failWrites > 0) { failWrites--; return false; }
    value = enc;
    valid = true;
    return true;
  }
};

/* ---------------- P4 Stage Engine (real interpretation) ---------------- */
struct P4 {
  ShowduinoStationModel t[SHOWDUINO_EMERGENCY_NODE_MAX_NODES]{};
  bool globalLatched = false;
  int globalTriggers = 0;
  int clearsAccepted = 0;
  int clearsRefused = 0;
  std::vector<C3 *> stations;
  StoreSim store;
  bool persistDirty = false;   /* a write failed or was not verified: clear blocked */
  bool persistValid = false;
  bool bootUnknown = false;    /* stored record unreadable after a latched restart */
  std::string persistedEnc;

  C3 *byId(const char *id) {
    for (C3 *s : stations) if (!std::strcmp(s->id, id)) return s;
    return nullptr;
  }

  /* Applies one wireless line exactly as the P4 does. Returns the interpreted result. */
  ShowduinoStationResult deliver(const std::string &line, bool *handled = nullptr) {
    ShowduinoStationResult res{};
    const int h = showduino_station_interpret_line(t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES,
                                                   line.c_str(), &res);
    if (handled) *handled = h != 0;
    if (!h) return res;
    /* P4 storage runs after every handled station line; it writes only on change. */
    persist();
    if (res.globalLatch && !globalLatched) {
      globalLatched = true;
      globalTriggers++;
    }
    if (res.accepted && res.globalLatch && res.ackId[0]) {
      C3 *s = byId(res.ackId);
      if (s) {
        for (const std::string &r : s->receive("ESTOP:ACK:LATCHED")) deliver(r);
      }
    }
    return res;
  }

  /* Deliver every line a station produces. */
  void pump(C3 &s, uint32_t now) {
    for (const std::string &line : s.tick(now)) deliver(line);
  }

  /* Director CLEAR. Refused while any station blocks clear. */
  bool directorClear() {
    const int blocking = showduino_station_model_count_blocking(t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES);
    if (showduino_station_clear_blocked_full((unsigned)blocking, bootUnknown ? 1 : 0,
                                             persistDirty ? 1 : 0, 1)) {
      clearsRefused++;
      return false;
    }
    globalLatched = false;
    clearsAccepted++;
    /* P4 applyEmergencyClear: the episode ends, then the empty set is stored. */
    showduino_station_model_end_episode(t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES);
    persist();
    for (C3 *s : stations) {
      for (const std::string &r : s->receive("EMERGENCY:CLEAR")) deliver(r);
    }
    return true;
  }

  void timeout(const char *id) {
    const int slot = showduino_station_model_find_id(t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES, id);
    if (slot >= 0) showduino_station_model_on_timeout(&t[slot]);
  }

  int slotOf(const char *id) const {
    return showduino_station_model_find_id(t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES, id);
  }

  /* Mirrors emergencyNodeLinkPersistBlockers(): the required set is written on
   * change only. A failed or unbuildable write leaves persistDirty set. */
  bool persist() {
    ShowduinoBlocker list[SHOWDUINO_BLOCKER_MAX];
    int n = 0;
    if (!showduino_station_model_collect_required(t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES,
                                                  list, SHOWDUINO_BLOCKER_MAX, &n)) {
      persistDirty = true;
      return false;
    }
    char buf[SHOWDUINO_BLOCKER_BUF];
    if (showduino_blocker_encode(list, (uint8_t)n, buf, sizeof(buf)) < 0) {
      persistDirty = true;
      return false;
    }
    const std::string enc(buf);
    if (persistValid && !persistDirty && enc == persistedEnc) return true;
    if (!store.save(enc)) {
      persistDirty = true;
      return false;
    }
    persistedEnc = enc;
    persistValid = true;
    persistDirty = false;
    return true;
  }
};

/* Reboot: a new P4 that restores only the stored blockers. */
static void rebootFromStore(P4 &fresh, const std::string &stored, bool latched = true) {
  fresh.globalLatched = latched;
  ShowduinoBlocker list[SHOWDUINO_BLOCKER_MAX];
  uint8_t n = 0;
  const ShowduinoBlockerStatus st = showduino_blocker_decode(stored.c_str(), list, &n);
  if (st != SHOWDUINO_BLOCKER_OK) {
    /* Latched with an unreadable record: clear stays refused (fail closed). */
    fresh.bootUnknown = latched;
    return;
  }
  for (uint8_t i = 0; i < n; ++i) {
    const int slot = showduino_station_model_alloc(fresh.t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES);
    if (slot >= 0) showduino_station_model_restore(&fresh.t[slot], list[i].id, list[i].mac);
  }
}

int main() {
  const char *MAC_A = "AA:BB:CC:DD:EE:01";
  const char *MAC_B = "AA:BB:CC:DD:EE:02";
  const char *MAC_C = "AA:BB:CC:DD:EE:03";
  const char *FORGED = "66:55:44:33:22:11";

  /* ============ Scenario 1-6: the legitimate press -> release -> confirm -> clear ============ */
  {
    C3 a("ESTOP-01", MAC_A, false);
    P4 p4;
    p4.stations.push_back(&a);
    uint32_t now = 0;

    /* 1. Press and emergency assertion. */
    a.setPressed(true);
    p4.pump(a, now);
    {
      const int slot = p4.slotOf("ESTOP-01");
      expect(p4.globalLatched && p4.globalTriggers == 1,
             "S1a press: ASSERT latches the P4 globally");
      expect(slot >= 0 && p4.t[slot].inputOpen == 1 && p4.t[slot].reportKnown == 1,
             "S1b press: station recorded PRESSED from a verified ANNOUNCE");
      expect(!p4.directorClear() && p4.clearsRefused == 1,
             "S1c clear refused while the button is pressed");
    }

    /* 2. Release while the C3 stays locally latched. The C3 keeps asserting. */
    a.setPressed(false);
    now = 2000;
    p4.pump(a, now);
    now = 4000;
    p4.pump(a, now);
    {
      const int slot = p4.slotOf("ESTOP-01");
      expect(a.m.latched == 1 && a.m.input_open == 0,
             "S2a C3 after release: still locally latched, input released");
      expect(slot >= 0 && p4.t[slot].inputOpen == 0 && p4.t[slot].announcedLatched == 1,
             "S2b released-but-latched ANNOUNCE: P4 records RELEASED, not PRESSED");
    }

    /* 3. Released-input ANNOUNCE while latched: the P4 must not mark it pressed. */
    const int trigBefore = p4.globalTriggers;
    now = 6000;
    {
      const std::string an = a.announce(now);
      p4.deliver(an);
      const int slot = p4.slotOf("ESTOP-01");
      expect(slot >= 0 && p4.t[slot].inputOpen == 0,
             "S3a released-input ANNOUNCE keeps RELEASED");
      expect(p4.globalTriggers == trigBefore,
             "S3b released-but-latched ANNOUNCE does not re-trigger the global latch");
    }

    /* 4. Further periodic ASSERT messages (the C3 keeps sending while latched).
     * Each ASSERT is delivered ON ITS OWN and the state checked immediately, so a
     * following ANNOUNCE cannot mask an ASSERT that wrongly set PRESSED. */
    const int assertsBefore = a.assertsSent;
    for (uint32_t t = 8000; t <= 16000; t += 2000) {
      for (const std::string &line : a.tick(t)) {
        if (showduino_emergency_is_assert(line.c_str())) {
          p4.deliver(line);
          const int slot = p4.slotOf("ESTOP-01");
          expect(slot >= 0 && p4.t[slot].inputOpen == 0,
                 "S4b periodic ASSERT alone does not override a confirmed release");
        } else {
          p4.deliver(line);
        }
      }
    }
    expect(a.assertsSent > assertsBefore,
           "S4a periodic ASSERT messages were actually sent by the C3");

    /* 5. Additional released-input ANNOUNCE messages. */
    now = 16000;
    for (int k = 0; k < 3; ++k) { now += 2000; p4.deliver(a.announce(now)); }
    {
      const int slot = p4.slotOf("ESTOP-01");
      expect(slot >= 0 && p4.t[slot].inputOpen == 0 && p4.directorClear() == true,
             "S5 repeated released ANNOUNCE keeps the release; clear allowed");
    }

    /* 6. Deliberate Director clear was accepted above. Global clear observed by C3. */
    expect(p4.clearsAccepted == 1 && !p4.globalLatched,
           "S6 Director clear accepted and P4 global latch released");

    /* 7. Global clear acknowledgement: the C3 resets to NORMAL when released. */
    expect(a.m.latched == 0 && a.m.state == SHOWDUINO_ESTOP_ST_NORMAL && a.m.acked == 0,
           "S7 C3 observes global clear: latched 0, state NORMAL, acked cleared");

    /* 8. Repeated reports after normal recovery: no re-latch, no false pressed. */
    const int trigAfterRecovery = p4.globalTriggers;
    const int assertsAfterRecovery = a.assertsSent;
    for (uint32_t t = 18000; t <= 26000; t += 2000) p4.pump(a, t);
    {
      const int slot = p4.slotOf("ESTOP-01");
      expect(p4.globalTriggers == trigAfterRecovery && !p4.globalLatched,
             "S8a repeated reports after recovery do not re-latch the P4");
      expect(a.assertsSent == assertsAfterRecovery,
             "S8b a recovered C3 (NORMAL) sends no ASSERT");
      expect(slot >= 0 && p4.t[slot].inputOpen == 0 && p4.t[slot].announcedLatched == 0,
             "S8c recovered station stays RELEASED and NORMAL");
    }
    /* 8d. Conservative: a late ASSERT from a NORMAL station is treated as PRESSED
     * until a released ANNOUNCE confirms otherwise, so clear is not allowed on it. */
    {
      bool handled = false;
      ShowduinoStationResult r = p4.deliver("ESTOP:ASSERT:ESTOP-01:TEST", &handled);
      const int slot = p4.slotOf("ESTOP-01");
      expect(handled && r.accepted && slot >= 0 && p4.t[slot].inputOpen == 1 && !p4.directorClear(),
             "S8d late ASSERT from a NORMAL station is conservative: clear refused");
      p4.deliver(a.announce(30000));
      expect(slot >= 0 && p4.t[slot].inputOpen == 0 && p4.directorClear(),
             "S8e next released ANNOUNCE confirms release; clear then allowed");
    }
  }

  /* ============ Scenario 9: disconnection before a verified release ============ */
  {
    C3 b("ESTOP-02", MAC_B, false);
    P4 p4;
    p4.stations.push_back(&b);
    b.setPressed(true);
    p4.pump(b, 0);
    b.connected = false;                /* the station drops off before any release */
    p4.timeout("ESTOP-02");
    expect(!p4.directorClear(),
           "S9a pressed station that disconnects keeps blocking clear");

    b.connected = true;
    b.setPressed(false);                /* released, but no verified report yet */
    p4.timeout("ESTOP-02");
    expect(!p4.directorClear(),
           "S9b offline station with no verified release keeps blocking clear");

    p4.pump(b, 1000);                   /* reconnects and reports released */
    p4.pump(b, 3000);
    expect(p4.directorClear(),
           "S9c reconnect with a verified release allows clear");
  }

  /* ============ Scenario 9b: released station goes offline before clear ============ */
  {
    C3 b("ESTOP-05", MAC_B, false);
    P4 p4;
    p4.stations.push_back(&b);
    b.setPressed(true);  p4.pump(b, 0);
    b.setPressed(false); p4.pump(b, 2000); p4.pump(b, 4000);
    p4.timeout("ESTOP-05");
    expect(!p4.directorClear(),
           "S9d station released then offline (not re-verified) keeps blocking clear");
    b.connected = true;
    p4.pump(b, 6000);
    expect(p4.directorClear(),
           "S9e re-verified release after reconnect allows clear");
  }

  /* ============ Scenario 10: multiple stations, different physical states ============ */
  {
    C3 a("ESTOP-10", MAC_A, false);
    C3 b("ESTOP-11", MAC_B, false);
    C3 c("ESTOP-12", MAC_C, false);
    P4 p4;
    p4.stations = {&a, &b, &c};
    a.setPressed(true);  p4.pump(a, 0);       /* A: pressed, verified */
    b.setPressed(true);  p4.pump(b, 0);       /* B: pressed, verified */
    /* C: bare ASSERT only, no verified report at all: unknown -> conservative. */
    p4.deliver("ESTOP:ASSERT:ESTOP-12:TEST");
    {
      const int blocking = showduino_station_model_count_blocking(p4.t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES);
      expect(blocking == 3, "S10a pressed A, pressed B and unverified C all block clear");
    }
    /* A releases and confirms. C is still unverified. */
    a.setPressed(false); p4.pump(a, 2000); p4.pump(a, 4000);
    {
      const int blocking = showduino_station_model_count_blocking(p4.t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES);
      expect(blocking == 2, "S10b A confirmed released: B and unverified C still block");
    }
    /* B releases and confirms. C reports its real state (released). */
    b.setPressed(false); p4.pump(b, 2000); p4.pump(b, 4000);
    p4.deliver(c.announce(4000));
    {
      const int blocking = showduino_station_model_count_blocking(p4.t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES);
      expect(blocking == 0, "S10c all confirmed released: nothing blocks clear");
      expect(p4.directorClear(), "S10d multi-station press/release/confirm sequence clears");
    }
  }

  /* ============ Scenario 11: P4 reboot with stored wireless blockers ============ */
  {
    C3 a("ESTOP-20", MAC_A, false);
    C3 b("ESTOP-21", MAC_B, false);
    P4 before;
    before.stations = {&a, &b};
    a.setPressed(true);  before.pump(a, 0);
    b.setPressed(true);  before.pump(b, 0);
    before.persist();
    const std::string stored = before.store.value;

    /* P4 reboots. Only the stored set survives. */
    P4 after;
    after.stations = {&a, &b};
    rebootFromStore(after, stored);
    {
      const int slotA = after.slotOf("ESTOP-20");
      const int slotB = after.slotOf("ESTOP-21");
      expect(slotA >= 0 && slotB >= 0 && after.t[slotA].inputOpen && after.t[slotB].inputOpen,
             "S11a restored blockers come back PRESSED");
      expect(slotA >= 0 && after.t[slotA].reportKnown == 0,
             "S11b restored blocker has no verified report yet");
      expect(!after.directorClear(), "S11c clear refused after reboot with stored blockers");
    }
    /* Bare ASSERT after reboot must not release. */
    after.deliver("ESTOP:ASSERT:ESTOP-20:TEST");
    expect(!after.directorClear(), "S11d bare ASSERT after reboot does not release");

    /* Forged released report with the stored ID and another MAC: rejected. */
    ShowduinoEmergencyAnnounce forged{};
    std::snprintf(forged.mac, sizeof(forged.mac), "%s", FORGED);
    std::snprintf(forged.id, sizeof(forged.id), "%s", "ESTOP-20");
    std::snprintf(forged.name, sizeof(forged.name), "%s", "X");
    std::snprintf(forged.state, sizeof(forged.state), "%s", "NORMAL");
    forged.input_open = 0; forged.latched = 0;
    char fbuf[160];
    showduino_emergency_format_announce(&forged, fbuf, sizeof(fbuf));
    after.deliver(fbuf);
    {
      const int slotA = after.slotOf("ESTOP-20");
      expect(slotA >= 0 && after.t[slotA].inputOpen == 1 && !after.directorClear(),
             "S11e forged released ANNOUNCE after reboot is rejected");
    }

    /* Different ID presenting the stored MAC (the MAC is public): rejected. */
    ShowduinoEmergencyAnnounce idForge{};
    std::snprintf(idForge.mac, sizeof(idForge.mac), "%s", MAC_A);
    std::snprintf(idForge.id, sizeof(idForge.id), "%s", "ESTOP-99");
    std::snprintf(idForge.name, sizeof(idForge.name), "%s", "X");
    std::snprintf(idForge.state, sizeof(idForge.state), "%s", "NORMAL");
    idForge.input_open = 0; idForge.latched = 0;
    char ibuf[160];
    showduino_emergency_format_announce(&idForge, ibuf, sizeof(ibuf));
    after.deliver(ibuf);
    {
      const int slotA = after.slotOf("ESTOP-20");
      expect(slotA >= 0 && after.t[slotA].inputOpen == 1 && after.slotOf("ESTOP-99") < 0 &&
             !after.directorClear(),
             "S11g different ID with the stored MAC: rejected, no release, no rename");
    }

    /* The original stations report their real state. A releases, B releases. */
    a.setPressed(false); after.pump(a, 2000); after.pump(a, 4000);
    b.setPressed(false); after.pump(b, 2000); after.pump(b, 4000);
    expect(after.directorClear(), "S11f verified releases after reboot allow clear");
  }

  /* ============ Scenario 12: press re-engages a released station after clear ============ */
  {
    C3 a("ESTOP-30", MAC_A, false);
    P4 p4;
    p4.stations.push_back(&a);
    a.setPressed(true);  p4.pump(a, 0);
    a.setPressed(false); p4.pump(a, 2000); p4.pump(a, 4000);
    expect(p4.directorClear(), "S12a clear after release succeeds");
    a.setPressed(true);  p4.pump(a, 6000);
    expect(p4.globalLatched && !p4.directorClear(),
           "S12b a new press after clear re-latches the P4 and blocks clear again");
  }

  /* ============ Scenario 13 (MISSION 001I): bare ASSERT never restores freshness ============ */
  {
    C3 a("ESTOP-40", MAC_A, false);
    P4 p4;
    p4.stations.push_back(&a);
    a.setPressed(true);  p4.pump(a, 0);
    a.setPressed(false); p4.pump(a, 2000); p4.pump(a, 4000);
    const int slot = p4.slotOf("ESTOP-40");
    expect(slot >= 0 && p4.t[slot].inputOpen == 0 && p4.t[slot].online == 1,
           "S13a release confirmed by a verified ANNOUNCE while online");

    /* 1. Disconnection before the clear. */
    a.connected = false;
    p4.timeout("ESTOP-40");
    expect(slot >= 0 && p4.t[slot].online == 0 &&
           showduino_station_model_count_blocking(p4.t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES) == 1,
           "S13b disconnected station blocks clear, even though its last report was released");

    /* 2. A bare ASSERT arrives. It must not restore online or establish release. */
    bool handled = false;
    p4.deliver(bareAssert(a), &handled);
    expect(handled && p4.globalLatched && p4.t[slot].online == 0,
           "S13c bare ASSERT keeps the global latch and does NOT restore ONLINE");
    expect(!p4.directorClear() && p4.clearsRefused >= 1,
           "S13d attempted clear refused after a bare ASSERT from an offline station");

    /* 3. Fresh, identity-validated released ANNOUNCE after reconnection. */
    a.connected = true;
    p4.deliver(a.announce(8000));
    expect(p4.t[slot].online == 1 && p4.t[slot].inputOpen == 0 && p4.t[slot].reportKnown == 1,
           "S13e fresh released ANNOUNCE restores ONLINE and RELEASED");

    /* 4. Authorised clear. */
    expect(p4.directorClear() && !p4.globalLatched && p4.clearsAccepted == 1,
           "S13f authorised clear accepted after the fresh released ANNOUNCE");
  }

  /* ============ Scenario 14 (MISSION 001I): reboot after release-then-disconnect ============ */
  {
    C3 a("ESTOP-50", MAC_A, false);
    P4 p4;
    p4.stations.push_back(&a);
    a.setPressed(true);  p4.pump(a, 0);
    a.setPressed(false); p4.pump(a, 2000); p4.pump(a, 4000);
    a.connected = false;
    p4.timeout("ESTOP-50");
    expect(p4.persist() && p4.store.value.find("ESTOP-50") != std::string::npos,
           "S14a released-then-disconnected station stays in the stored blocker set");

    P4 after;
    after.stations.push_back(&a);
    rebootFromStore(after, p4.store.value, true);
    expect(showduino_station_model_count_blocking(after.t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES) == 1
           && !after.bootUnknown,
           "S14b reboot restores the station as a clear blocker");
    expect(!after.directorClear(),
           "S14c clear refused after reboot while no announcement has been received");

    after.deliver(bareAssert(a));
    expect(!after.directorClear(),
           "S14d bare ASSERT after reboot does not satisfy the clear requirement");

    a.connected = true;
    after.deliver(a.announce(6000));
    expect(after.directorClear(),
           "S14e fresh released ANNOUNCE after reboot allows the authorised clear");
  }

  /* ============ Scenario 15 (MISSION 001I): reboot with several stations in different states ============ */
  {
    C3 a("ESTOP-60", MAC_A, false);
    C3 b("ESTOP-61", MAC_B, false);
    P4 p4;
    p4.stations.push_back(&a);
    p4.stations.push_back(&b);
    a.setPressed(true);  p4.pump(a, 0);
    b.setPressed(true);  p4.pump(b, 0);
    a.setPressed(false); p4.pump(a, 2000); p4.pump(a, 4000);
    a.connected = false;
    p4.timeout("ESTOP-60");            /* A: released, then offline. B: still pressed. */
    p4.persist();

    P4 after;
    after.stations.push_back(&a);
    after.stations.push_back(&b);
    rebootFromStore(after, p4.store.value, true);
    expect(showduino_station_model_count_blocking(after.t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES) == 2,
           "S15a after reboot both the released-then-offline A and the pressed B block clear");

    b.setPressed(false); after.pump(b, 6000); after.pump(b, 8000);
    expect(showduino_station_model_count_blocking(after.t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES) == 1
           && !after.directorClear(),
           "S15b B confirmed released; A still required, clear refused");

    a.connected = true;
    after.deliver(a.announce(10000));
    expect(after.directorClear(),
           "S15c A fresh released ANNOUNCE and B released: authorised clear accepted");
  }

  /* ============ Scenario 16 (MISSION 001I): storage faults stay fail-closed ============ */
  {
    C3 a("ESTOP-70", MAC_A, false);
    C3 b("ESTOP-71", MAC_B, false);
    P4 p4;
    p4.stations.push_back(&a);
    p4.stations.push_back(&b);
    a.setPressed(true);  p4.pump(a, 0);
    a.setPressed(false); p4.pump(a, 2000); p4.pump(a, 4000);

    /* Storage becomes unavailable, then B asserts. The required set changes and
     * the write fails. */
    p4.store.failWrites = 1000;
    b.setPressed(true);  p4.pump(b, 6000);
    b.setPressed(false); p4.pump(b, 8000); p4.pump(b, 10000);
    expect(p4.persistDirty && showduino_station_model_count_blocking(p4.t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES) == 0,
           "S16a failed write: every station is released, yet the unverified record blocks clear");
    expect(!p4.directorClear(),
           "S16b clear refused while the blocker record is unverified");

    /* Storage recovers. The next station line retries and verifies the write. */
    p4.store.failWrites = 0;
    p4.deliver(b.announce(12000));
    expect(!p4.persistDirty && p4.store.valid,
           "S16c retry after recovery verifies the blocker record");
    expect(p4.directorClear(),
           "S16d clear allowed once the record is verified and all stations are released");
  }
  {
    /* Unreadable record after a latched restart: fail closed, even with no stations. */
    C3 a("ESTOP-80", MAC_A, false);
    P4 corrupt;
    corrupt.stations.push_back(&a);
    rebootFromStore(corrupt, "B1;corrupted;#0000", true);
    expect(corrupt.bootUnknown && showduino_station_model_count_blocking(corrupt.t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES) == 0,
           "S16e unreadable record after latched reboot: no stations, yet boot state is unknown");
    a.connected = true;
    corrupt.deliver(a.announce(2000));
    expect(!corrupt.directorClear(),
           "S16f unknown boot state stays fail-closed, even after a released ANNOUNCE");
  }

  /* ============ Scenario 17 (MISSION 001I): an accepted clear ends the episode ============ */
  {
    C3 a("ESTOP-90", MAC_A, false);
    P4 p4;
    p4.stations.push_back(&a);
    a.setPressed(true);  p4.pump(a, 0);
    a.setPressed(false); p4.pump(a, 2000); p4.pump(a, 4000);
    expect(p4.store.value.find("ESTOP-90") != std::string::npos,
           "S17a before clear the released station is still a stored blocker");
    expect(p4.directorClear(), "S17b authorised clear accepted");
    expect(p4.store.value.find("ESTOP-90") == std::string::npos,
           "S17c after the accepted clear the stored record holds no stale blocker");

    P4 after;
    after.stations.push_back(&a);
    rebootFromStore(after, p4.store.value, false);
    expect(showduino_station_model_count_blocking(after.t, SHOWDUINO_EMERGENCY_NODE_MAX_NODES) == 0,
           "S17d reboot after a completed clear restores no blockers");
  }

  /* ============ Scenario 18 (MISSION 001I): the stored set is never dropped silently ============ */
  {
    ShowduinoStationModel t[3]{};
    t[0].used = 1; t[0].asserted = 1; std::snprintf(t[0].id, sizeof(t[0].id), "%s", "ESTOP-A");
    t[1].used = 1; t[1].asserted = 1; std::snprintf(t[1].id, sizeof(t[1].id), "%s", "ESTOP-B");
    ShowduinoBlocker out[SHOWDUINO_BLOCKER_MAX];
    int n = -1;
    expect(showduino_station_model_collect_required(t, 3, out, 1, &n) == 0,
           "S18a more required stations than the record can hold: collection fails");
    ShowduinoStationModel noId[1]{};
    noId[0].used = 1; noId[0].asserted = 1;
    expect(showduino_station_model_collect_required(noId, 1, out, SHOWDUINO_BLOCKER_MAX, &n) == 0,
           "S18b required station without an identity: collection fails");
    expect(showduino_station_model_collect_required(t, 3, out, SHOWDUINO_BLOCKER_MAX, &n) == 1 && n == 2,
           "S18c two required stations collect in full");
  }

  std::printf(gFails == 0 ? "ALL PASS\n" : "FAILURES: %d\n", gFails);
  return gFails == 0 ? 0 : 1;
}
