#include "showduino_emergency_director_desk.h"
#include <cstdio>
#include <cstring>

static int gFails = 0;

static void expect(bool ok, const char *msg) {
  if (ok) std::printf("PASS  %s\n", msg);
  else {
    std::printf("FAIL  %s\n", msg);
    gFails++;
  }
}

int main() {
  ShowduinoEmergencyDirectorSheet s;
  showduino_emergency_desk_reset(&s);
  expect(s.has_clear_emergency == 0, "sheet has no CLEAR control");
  expect(s.has_simulate_emergency == 0, "sheet has no simulate control");
  expect(strcmp(s.refresh_cmd, "ESTOP:STATUS") == 0, "refresh is ESTOP:STATUS");
  expect(showduino_emergency_desk_layout_fits_800x480() == 1,
         "800x480 strip+cards stay above dock");

  ShowduinoEmergencyStationWire a{};
  a.slot = 0;
  strncpy(a.id, "ESTOP-01", sizeof(a.id) - 1);
  strncpy(a.name, "ENTRANCE", sizeof(a.name) - 1);
  a.online = 1;
  strncpy(a.state, "NORMAL", sizeof(a.state) - 1);
  showduino_emergency_desk_apply_station(&s, &a);

  ShowduinoEmergencyStationWire b{};
  b.slot = 1;
  strncpy(b.id, "ESTOP-02", sizeof(b.id) - 1);
  strncpy(b.name, "CONTROL ROOM", sizeof(b.name) - 1);
  b.online = 1;
  strncpy(b.state, "NORMAL", sizeof(b.state) - 1);
  showduino_emergency_desk_apply_station(&s, &b);

  ShowduinoEmergencyStationWire c{};
  c.slot = 2;
  strncpy(c.id, "ESTOP-03", sizeof(c.id) - 1);
  strncpy(c.name, "MAZE EXIT", sizeof(c.name) - 1);
  c.online = 0;
  strncpy(c.state, "OFFLINE", sizeof(c.state) - 1);
  showduino_emergency_desk_apply_station(&s, &c);

  expect(s.online == 2 && s.offline == 1, "multiple stations counted");
  expect(s.safety_fault == 1, "offline station is safety fault");
  expect(s.global_emergency == 0, "offline is not global emergency");
  expect(strstr(s.warning, "SAFETY NODE FAULT") != NULL, "warning text");
  expect(showduino_emergency_desk_has_clear_control(&s) == 0, "still no CLEAR");
  expect(strstr(s.strip, "2 / 3") != NULL, "strip shows 2 / 3 ONLINE");

  ShowduinoEmergencyUpdateGate g;
  showduino_emergency_update_gate(&g, "ESTOP-01", 1, 1);
  expect(g.allow_next == 1, "ESTOP-01 healthy+linked allows ESTOP-02 update");
  showduino_emergency_update_gate(&g, "ESTOP-01", 1, 0);
  expect(g.allow_next == 0, "not linked holds next update");

  if (gFails) {
    std::printf("%d test(s) failed.\n", gFails);
    return 1;
  }
  std::printf("ALL PASS\n");
  return 0;
}
