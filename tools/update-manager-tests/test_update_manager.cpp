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

  man.components[0].ota_capable = 0;
  ShowduinoReleaseManifest commsMan;
  showduino_release_manifest_clear(&commsMan);
  showduino_release_manifest_add(&commsMan, "comms", "COMMS", "0.5.0", 0);
  showduino_release_manifest_add(&commsMan, "emergency", "ESTOP-01", "0.1.0", 1);
  commsMan.components[0].ota_capable = 1;
  expect(showduino_release_manifest_valid(&commsMan), "comms ota_capable is valid");
  expect(showduino_update_component_ota_available("comms"), "comms OTA available");
  expect(!showduino_update_component_ota_available("p4"), "p4 OTA unavailable");
  expect(!showduino_update_component_ota_available("director"), "director OTA unavailable");
  expect(!showduino_update_component_ota_available("lamp"), "lamp OTA unavailable");
  expect(!showduino_update_component_ota_available("audio"), "audio OTA unavailable");
  expect(!showduino_update_component_ota_available("pixel"), "pixel OTA unavailable");
  expect(!showduino_update_component_ota_available("emergency"), "emergency OTA unavailable");
  commsMan.components[1].ota_capable = 1;
  expect(!showduino_release_manifest_valid(&commsMan), "emergency ota_capable still invalid");

  static const char kSha[] =
      "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
  ShowduinoOtaCandidate cand;
  memset(&cand, 0, sizeof(cand));
  showduino_update_copy(cand.role, sizeof(cand.role), "comms");
  showduino_update_copy(cand.hardware_id, sizeof(cand.hardware_id), SHOWDUINO_COMMS_HARDWARE_ID);
  showduino_update_copy(cand.firmware, sizeof(cand.firmware), "0.5.1");
  showduino_update_copy(cand.sha256, sizeof(cand.sha256), kSha);
  cand.size = 1200000;
  cand.ota_capable = 1;
  cand.confirm = 1;
  expect(showduino_comms_ota_reject_reason(&cand, "0.5.0", SHOWDUINO_COMMS_HARDWARE_ID, 0, 0, 1, 1) == NULL,
         "valid comms candidate eligible");

  showduino_update_copy(cand.hardware_id, sizeof(cand.hardware_id), "SHOWDUINO-S3-WRONG");
  expect_str(showduino_comms_ota_reject_reason(&cand, "0.5.0", SHOWDUINO_COMMS_HARDWARE_ID, 0, 0, 1, 1),
             SHOWDUINO_UPDATE_BLOCK_HARDWARE, "wrong hardware rejected");
  showduino_update_copy(cand.hardware_id, sizeof(cand.hardware_id), SHOWDUINO_COMMS_HARDWARE_ID);

  showduino_update_copy(cand.firmware, sizeof(cand.firmware), "0.5.0");
  expect_str(showduino_comms_ota_reject_reason(&cand, "0.5.0", SHOWDUINO_COMMS_HARDWARE_ID, 0, 0, 1, 1),
             SHOWDUINO_UPDATE_BLOCK_SAME, "same version rejected");
  cand.force = 1;
  expect(showduino_comms_ota_reject_reason(&cand, "0.5.0", SHOWDUINO_COMMS_HARDWARE_ID, 0, 0, 1, 1) == NULL,
         "force same version allowed in helper");
  cand.force = 0;

  showduino_update_copy(cand.firmware, sizeof(cand.firmware), "0.4.9");
  expect_str(showduino_comms_ota_reject_reason(&cand, "0.5.0", SHOWDUINO_COMMS_HARDWARE_ID, 0, 0, 1, 1),
             SHOWDUINO_UPDATE_BLOCK_DOWNGRADE, "downgrade rejected");
  showduino_update_copy(cand.firmware, sizeof(cand.firmware), "0.5.1");

  cand.sha256[0] = 0;
  expect_str(showduino_comms_ota_reject_reason(&cand, "0.5.0", SHOWDUINO_COMMS_HARDWARE_ID, 0, 0, 1, 1),
             SHOWDUINO_UPDATE_BLOCK_SHA, "bad SHA-256 rejected");
  showduino_update_copy(cand.sha256, sizeof(cand.sha256), kSha);

  cand.size = SHOWDUINO_COMMS_OTA_SLOT_BYTES + 1;
  expect_str(showduino_comms_ota_reject_reason(&cand, "0.5.0", SHOWDUINO_COMMS_HARDWARE_ID, 0, 0, 1, 1),
             SHOWDUINO_UPDATE_BLOCK_SIZE, "oversize rejected");
  cand.size = 1200000;

  expect_str(showduino_comms_ota_reject_reason(&cand, "0.5.0", SHOWDUINO_COMMS_HARDWARE_ID, 0, 0, 0, 1),
             SHOWDUINO_UPDATE_BLOCK_MAINT, "maintenance required");
  expect_str(showduino_comms_ota_reject_reason(&cand, "0.5.0", SHOWDUINO_COMMS_HARDWARE_ID, 1, 0, 1, 1),
             SHOWDUINO_UPDATE_BLOCK_SHOW, "show running rejected");
  expect_str(showduino_comms_ota_reject_reason(&cand, "0.5.0", SHOWDUINO_COMMS_HARDWARE_ID, 0, 1, 1, 1),
             SHOWDUINO_UPDATE_BLOCK_EMERGENCY, "emergency rejected");
  expect_str(showduino_comms_ota_reject_reason(&cand, "0.5.0", SHOWDUINO_COMMS_HARDWARE_ID, 0, 0, 1, 0),
             SHOWDUINO_UPDATE_BLOCK_P4, "P4 offline rejected");
  cand.confirm = 0;
  expect_str(showduino_comms_ota_reject_reason(&cand, "0.5.0", SHOWDUINO_COMMS_HARDWARE_ID, 0, 0, 1, 1),
             SHOWDUINO_UPDATE_BLOCK_CONFIRM, "confirm required");
  cand.confirm = 1;

  showduino_update_copy(cand.role, sizeof(cand.role), "p4");
  expect_str(showduino_comms_ota_reject_reason(&cand, "0.5.0", SHOWDUINO_COMMS_HARDWARE_ID, 0, 0, 1, 1),
             SHOWDUINO_UPDATE_BLOCK_COMPONENT, "generic apply cannot update P4");
  showduino_update_copy(cand.role, sizeof(cand.role), "emergency");
  expect_str(showduino_comms_ota_reject_reason(&cand, "0.5.0", SHOWDUINO_COMMS_HARDWARE_ID, 0, 0, 1, 1),
             SHOWDUINO_UPDATE_BLOCK_COMPONENT, "generic apply cannot update Emergency");

  expect(!showduino_ota_sha256_hex_ok("not-a-hash"), "short hash rejected");
  expect(showduino_ota_sha256_hex_ok(kSha), "64 hex hash ok");
  expect(!showduino_ota_hardware_match("A", "B"), "hardware mismatch");
  expect(showduino_ota_hardware_match(SHOWDUINO_COMMS_HARDWARE_ID, SHOWDUINO_COMMS_HARDWARE_ID),
         "hardware match");

  ShowduinoCommsHealth health;
  memset(&health, 0, sizeof(health));
  health.booted = 1;
  health.uart = 1;
  health.p4_link = 1;
  health.espnow = 1;
  health.network = 1;
  health.webui = 1;
  expect(showduino_comms_health_pass(&health), "health gate pass");
  health.p4_link = 0;
  expect(!showduino_comms_health_pass(&health), "missing P4 link fails health");
  health.p4_link = 1;
  health.fatal = 1;
  expect(!showduino_comms_health_pass(&health), "fatal fails health");

  expect_str(showduino_ota_state_after(SHOWDUINO_OTA_STATE_IDLE, SHOWDUINO_OTA_EVT_START),
             SHOWDUINO_OTA_STATE_DOWNLOADING, "IDLE -> DOWNLOADING");
  expect_str(showduino_ota_state_after(SHOWDUINO_OTA_STATE_DOWNLOADING, SHOWDUINO_OTA_EVT_DOWNLOAD_OK),
             SHOWDUINO_OTA_STATE_VERIFYING, "DOWNLOADING -> VERIFYING");
  expect_str(showduino_ota_state_after(SHOWDUINO_OTA_STATE_VERIFYING, SHOWDUINO_OTA_EVT_HASH_OK),
             SHOWDUINO_OTA_STATE_INSTALLING, "VERIFYING -> INSTALLING");
  expect_str(showduino_ota_state_after(SHOWDUINO_OTA_STATE_INSTALLING, SHOWDUINO_OTA_EVT_WRITE_OK),
             SHOWDUINO_OTA_STATE_REBOOT, "INSTALLING -> REBOOT_REQUIRED");
  expect_str(showduino_ota_state_after(SHOWDUINO_OTA_STATE_REBOOT, SHOWDUINO_OTA_EVT_REBOOTED),
             SHOWDUINO_OTA_STATE_PENDING, "REBOOT -> PENDING_VALIDATION");
  expect_str(showduino_ota_state_after(SHOWDUINO_OTA_STATE_PENDING, SHOWDUINO_OTA_EVT_HEALTH_OK),
             SHOWDUINO_OTA_STATE_COMPLETE, "PENDING -> COMPLETE");
  expect_str(showduino_ota_state_after(SHOWDUINO_OTA_STATE_DOWNLOADING, SHOWDUINO_OTA_EVT_DOWNLOAD_FAIL),
             SHOWDUINO_OTA_STATE_FAILED, "DOWNLOAD FAILED");
  expect_str(showduino_ota_state_after(SHOWDUINO_OTA_STATE_VERIFYING, SHOWDUINO_OTA_EVT_HASH_FAIL),
             SHOWDUINO_OTA_STATE_FAILED, "HASH FAILED");
  expect_str(showduino_ota_state_after(SHOWDUINO_OTA_STATE_INSTALLING, SHOWDUINO_OTA_EVT_WRITE_FAIL),
             SHOWDUINO_OTA_STATE_FAILED, "WRITE FAILED");
  expect_str(showduino_ota_state_after(SHOWDUINO_OTA_STATE_DOWNLOADING, SHOWDUINO_OTA_EVT_EMERGENCY),
             SHOWDUINO_OTA_STATE_EMERGENCY, "EMERGENCY INTERRUPT");
  expect_str(showduino_ota_state_after(SHOWDUINO_OTA_STATE_PENDING, SHOWDUINO_OTA_EVT_HEALTH_FAIL),
             SHOWDUINO_OTA_STATE_FAILED, "HEALTH CHECK FAILED");
  expect_str(showduino_ota_state_after(SHOWDUINO_OTA_STATE_FAILED, SHOWDUINO_OTA_EVT_ROLLBACK_DONE),
             SHOWDUINO_OTA_STATE_ROLLED_BACK, "ROLLBACK");

  if (g_failures) {
    std::printf("\n%d FAILED\n", g_failures);
    return 1;
  }
  std::printf("\nALL PASS\n");
  return 0;
}
