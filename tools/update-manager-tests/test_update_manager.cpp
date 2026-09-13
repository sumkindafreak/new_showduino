#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "showduino_update_manager.h"

static int g_failures = 0;

static void expect(bool ok, const char *name) {
  if (ok) std::printf("PASS  %s\n", name);
  else {
    std::printf("FAIL  %s\n", name);
    g_failures++;
  }
}

static void expect_str(const char *got, const char *want, const char *name) {
  expect(got && want && strcmp(got, want) == 0, name);
}

int main() {
  std::printf("Showduino update manager host tests\n\n");

  expect(showduino_update_role_ok("emergency"), "emergency role ok");
  expect(!showduino_update_role_ok("dmx"), "dmx role rejected");
  expect_str(showduino_update_policy_for_role("emergency"),
             SHOWDUINO_EMERGENCY_UPDATE_POLICY, "emergency uses existing ONE_AT_A_TIME");
  expect_str(showduino_update_policy_for_role("lamp"),
             SHOWDUINO_UPDATE_POLICY_INDEPENDENT, "lamp is independent");

  ShowduinoReleaseManifest man;
  showduino_release_manifest_clear(&man);
  showduino_release_manifest_add(&man, "p4", "P4", "0.6.2", 0);
  showduino_release_manifest_add(&man, "emergency", "ESTOP-01", "0.1.0", 1);
  expect(showduino_release_manifest_valid(&man), "rc1-style manifest valid");
  expect(man.ota_install == 0, "manifest ota_install is false");
  expect(man.components[1].ota_capable == 0, "emergency ota_capable is false");
  expect(man.shdo == 2, "manifest SHDO v2");

  man.ota_install = 1;
  expect(!showduino_release_manifest_valid(&man), "claiming OTA install invalidates Phase 1 manifest");
  man.ota_install = 0;
  man.components[0].ota_capable = 1;
  expect(!showduino_release_manifest_valid(&man), "ota_capable component invalid in Phase 1");

  expect(showduino_update_apply_allowed(0, 0) == 0, "apply blocked when idle");
  expect(showduino_update_apply_allowed(1, 0) == 0, "apply blocked when show running");
  expect(showduino_update_apply_allowed(0, 1) == 0, "apply blocked when emergency");
  expect_str(showduino_update_apply_block_reason(0, 1), SHOWDUINO_UPDATE_BLOCK_EMERGENCY,
             "emergency reason surfaced");
  expect_str(showduino_update_apply_block_reason(1, 0), SHOWDUINO_UPDATE_BLOCK_SHOW,
             "show-running reason surfaced");
  expect_str(showduino_update_apply_block_reason(0, 0), SHOWDUINO_UPDATE_BLOCK_OTA,
             "idle reason is OTA_NOT_IMPLEMENTED");

  ShowduinoEmergencyUpdateGate g;
  char fault[40];
  expect(showduino_update_emergency_advance(&g, "ESTOP-01", 1, 1, 0, fault, sizeof(fault)) == 1,
         "healthy+linked advances via existing gate");
  expect(g.allow_next == 1, "gate allow_next set");
  expect_str(g.next_id, "ESTOP-02", "next station is ESTOP-02");

  expect(showduino_update_emergency_advance(&g, "ESTOP-01", 1, 0, 0, fault, sizeof(fault)) == 0,
         "healthy but not linked holds");
  expect(fault[0] == 0, "hold is not a safety fault");

  expect(showduino_update_emergency_advance(&g, "ESTOP-01", 0, 0, 1, fault, sizeof(fault)) == 0,
         "timeout without healthy+linked fails");
  expect_str(fault, SHOWDUINO_UPDATE_FAULT_SAFETY, "timeout reports SAFETY_NODE_UPDATE_FAILED");

  ShowduinoUpdateInventory inv;
  showduino_update_inventory_clear(&inv);
  showduino_update_inventory_add(&inv, "p4", "P4", "P4", "0.6.2", 1, 1, 1, 1, 0);
  showduino_update_inventory_add(&inv, "emergency", "ESTOP-02", "Exit", "0.1.0", 1, 1, 0, 0, 1);
  showduino_update_inventory_add(&inv, "emergency", "ESTOP-01", "Entrance", "0.1.0", 1, 1, 1, 1, 1);
  showduino_update_inventory_add(&inv, "emergency", "ESTOP-03", "Maze", "0.1.0", 1, 1, 1, 1, 1);

  ShowduinoUpdatePlan plan;
  showduino_update_plan_from_inventory(&plan, &inv, 0, 0, 0);
  expect(plan.apply_implemented == 0, "plan apply_implemented is false");
  expect(plan.blocked == 1, "plan blocked");
  expect(plan.count >= 4, "plan has core + stations");
  expect_str(plan.steps[0].role, "p4", "non-safety first");
  expect_str(plan.steps[1].id, "ESTOP-01", "ESTOP-01 first in sequence");
  expect_str(plan.steps[2].id, "ESTOP-02", "ESTOP-02 second");
  expect(plan.steps[1].allowed == 1, "ESTOP-01 may start");
  expect(plan.steps[2].allowed == 1, "ESTOP-01 healthy+linked allows ESTOP-02");
  expect(plan.steps[3].allowed == 0, "ESTOP-03 held until ESTOP-02 is healthy+linked");
  expect_str(plan.steps[3].state, SHOWDUINO_UPDATE_STEP_HOLD, "ESTOP-03 is HOLD");
  expect(plan.safety_failed == 0, "hold is not a safety failure");

  showduino_update_plan_from_inventory(&plan, &inv, 0, 0, 1);
  expect(plan.safety_failed == 1, "timeout while ESTOP-02 is not linked fails the sequence");
  expect_str(plan.blocked_reason, SHOWDUINO_UPDATE_FAULT_SAFETY,
             "plan reason SAFETY_NODE_UPDATE_FAILED");
  expect_str(plan.steps[3].fault, SHOWDUINO_UPDATE_FAULT_SAFETY,
             "ESTOP-03 step carries safety fault");
  expect_str(plan.steps[3].state, SHOWDUINO_UPDATE_STEP_FAILED,
             "later stations do not continue after safety fault");

  expect(strcmp(SHOWDUINO_UPDATE_POLICY_ONE_AT_TIME, SHOWDUINO_EMERGENCY_UPDATE_POLICY) == 0,
         "update policy token is the Emergency Node token");

  if (g_failures) {
    std::printf("\n%d FAILED\n", g_failures);
    return 1;
  }
  std::printf("\nALL PASS\n");
  return 0;
}
