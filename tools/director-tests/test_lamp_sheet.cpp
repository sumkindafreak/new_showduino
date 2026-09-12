/*
 * Host tests for the Director Lamp settings sheet (no LVGL / ESP32 SDK).
 *
 * Build:
 *   g++ -std=c++17 -Wall -Wextra -I../../protocol -o lamp_sheet_tests.exe test_lamp_sheet.cpp
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include "showduino_lamp_director_desk.h"

static int g_fails = 0;

static void expect(bool ok, const char *name) {
  if (ok) {
    std::printf("PASS  %s\n", name);
  } else {
    std::printf("FAIL  %s\n", name);
    g_fails++;
  }
}

static void expect_str(const char *got, const char *want, const char *name) {
  if (got && want && strcmp(got, want) == 0) {
    std::printf("PASS  %s\n", name);
  } else {
    std::printf("FAIL  %s (got '%s' want '%s')\n", name,
                got ? got : "(null)", want ? want : "(null)");
    g_fails++;
  }
}

static ShowduinoLampDirectorInput online_burning(const char *id) {
  ShowduinoLampDirectorInput in;
  memset(&in, 0, sizeof(in));
  in.present = 1;
  in.offline = 0;
  in.emergency = 0;
  in.detail_valid = 1;
  strncpy(in.logical_id, id, sizeof(in.logical_id) - 1);
  strncpy(in.detail.state, "SHOW_CONTROLLED", sizeof(in.detail.state) - 1);
  strncpy(in.detail.fx, "BURNING", sizeof(in.detail.fx) - 1);
  in.detail.brightness = 80;
  return in;
}

int main() {
  std::printf("Director Lamp sheet host tests\n\n");

  /* ---- navigation: open / close / back ---- */
  {
    Page04Nav nav;
    page04_nav_reset(&nav);
    expect(nav.place == PAGE04_NAV_GRID && nav.role < 0, "lamp sheet starts as Nodes grid");
    page04_nav_open_sheet(&nav, PAGE04_NAV_ROLE_LAMP);
    expect(nav.place == PAGE04_NAV_SHEET && nav.role == PAGE04_NAV_ROLE_LAMP,
           "lamp sheet opens");
    expect(page04_nav_back(&nav) == 0, "BACK from lamp sheet stays on Nodes");
    expect(nav.place == PAGE04_NAV_GRID, "lamp sheet closes to Nodes grid");
    page04_nav_open_sheet(&nav, PAGE04_NAV_ROLE_AUDIO);
    page04_nav_close_sheet(&nav);
    expect(nav.place == PAGE04_NAV_GRID, "CLOSE returns to Nodes grid");
    expect(page04_nav_back(&nav) == 1, "BACK from Nodes grid returns Home");
    expect(nav.place == PAGE04_NAV_HOME, "Nodes BACK lands on Home");
  }

  /* ---- command syntax uses selected logical id ---- */
  {
    char cmd[48];
    ShowduinoLampCommand parsed;
    showduino_lamp_director_format_cmd("LAMP-01", SHOWDUINO_LAMP_DESK_CMD_IGNITE,
                                       cmd, sizeof(cmd));
    expect_str(cmd, "LAMP:NODE:LAMP-01:IGNITE", "IGNITE uses selected LAMP-01");
    expect(showduino_lamp_parse_command(cmd, &parsed) == SHOWDUINO_LAMP_CMD_IGNITE,
           "IGNITE parses on existing protocol");
    expect_str(parsed.logicalId, "LAMP-01", "IGNITE logical id extracted");

    showduino_lamp_director_format_cmd("LAMP-02", SHOWDUINO_LAMP_DESK_CMD_EXTINGUISH,
                                       cmd, sizeof(cmd));
    expect_str(cmd, "LAMP:NODE:LAMP-02:EXTINGUISH", "EXTINGUISH uses selected LAMP-02");
    expect(showduino_lamp_parse_command(cmd, &parsed) == SHOWDUINO_LAMP_CMD_EXTINGUISH,
           "EXTINGUISH parses on existing protocol");
    expect_str(parsed.logicalId, "LAMP-02", "EXTINGUISH logical id extracted");

    showduino_lamp_director_format_cmd("LAMP-03", SHOWDUINO_LAMP_DESK_CMD_FLARE,
                                       cmd, sizeof(cmd));
    expect_str(cmd, "LAMP:NODE:LAMP-03:FX:FLARE", "FLARE uses existing FX:FLARE syntax");
    expect(showduino_lamp_parse_command(cmd, &parsed) == SHOWDUINO_LAMP_CMD_FX,
           "FLARE parses as FX command");
    expect(parsed.fx == SHOWDUINO_LAMP_FX_FLARE, "FLARE token is FLARE");
    expect_str(parsed.logicalId, "LAMP-03", "FLARE logical id extracted");

    showduino_lamp_director_format_cmd("LAMP-01", SHOWDUINO_LAMP_DESK_CMD_STATUS,
                                       cmd, sizeof(cmd));
    expect_str(cmd, "LAMP:NODE:LAMP-01:STATUS", "REFRESH is STATUS not reboot");
    expect(showduino_lamp_parse_command(cmd, &parsed) == SHOWDUINO_LAMP_CMD_STATUS,
           "STATUS parses");

    showduino_lamp_director_format_cmd("", SHOWDUINO_LAMP_DESK_CMD_IGNITE,
                                       cmd, sizeof(cmd));
    expect_str(cmd, "LAMP:NODE:IGNITE", "empty id omits LAMP-01 bake-in");
  }

  /* ---- online presentation ---- */
  {
    ShowduinoLampDirectorInput in = online_burning("LAMP-01");
    ShowduinoLampDirectorSheet sh;
    showduino_lamp_director_build_sheet(&in, &sh);
    expect_str(sh.title, "LAMP NODE", "online header role title");
    expect_str(sh.logical_id, "LAMP-01", "online logical id");
    expect_str(sh.presence_line, "ONLINE • SHOWDUINO OWNED", "owned presence from detail");
    expect_str(sh.state, "BURNING", "STATE from carbide fx not owner token");
    expect_str(sh.motion, "--", "MOTION unavailable");
    expect_str(sh.blow, "--", "BLOW unavailable");
    expect_str(sh.audio, "--", "AUDIO play-state unavailable");
    expect(sh.ignite_enabled && sh.extinguish_enabled && sh.flare_enabled,
           "online burning enables IGNITE/EXTINGUISH/FLARE");
    expect(sh.refresh_enabled, "REFRESH remains available online");
    expect_str(sh.ignite_cmd, "LAMP:NODE:LAMP-01:IGNITE", "online IGNITE wire");
    expect_str(sh.flare_cmd, "LAMP:NODE:LAMP-01:FX:FLARE", "online FLARE wire");
  }

  /* ---- offline presentation ---- */
  {
    ShowduinoLampDirectorInput in;
    memset(&in, 0, sizeof(in));
    in.present = 0;
    in.offline = 1;
    strncpy(in.logical_id, "LAMP-01", sizeof(in.logical_id) - 1);
    ShowduinoLampDirectorSheet sh;
    showduino_lamp_director_build_sheet(&in, &sh);
    expect_str(sh.logical_id, "LAMP-01", "offline keeps logical id");
    expect_str(sh.presence_line, "OFFLINE", "offline presence");
    expect_str(sh.state, "OFFLINE", "offline state");
    expect(strstr(sh.banner, "No compatible Lamp Node detected.") != nullptr,
           "offline explains missing node");
    expect(!sh.ignite_enabled && !sh.extinguish_enabled && !sh.flare_enabled,
           "controls disabled offline");
    expect(sh.refresh_enabled, "REFRESH remains available offline");
  }

  /* ---- emergency locks theatrical controls; no clear ---- */
  {
    ShowduinoLampDirectorInput in = online_burning("LAMP-01");
    in.emergency = 1;
    ShowduinoLampDirectorSheet sh;
    showduino_lamp_director_build_sheet(&in, &sh);
    expect_str(sh.presence_line, "EMERGENCY", "emergency presence");
    expect_str(sh.state, "EMERGENCY", "emergency state");
    expect(!sh.ignite_enabled && !sh.extinguish_enabled && !sh.flare_enabled,
           "controls disabled during emergency");
    expect(sh.refresh_enabled, "REFRESH remains available during emergency");
    expect(!sh.has_clear_emergency, "no emergency-clear action exists");
    expect(strstr(sh.banner, "SYSTEM EMERGENCY ACTIVE") != nullptr,
           "emergency banner states lock");
    expect(strstr(sh.banner, "LAMP CONTROLS LOCKED BY SHOW ENGINE") != nullptr,
           "emergency names show-engine lock");
    expect(strstr(sh.banner, "JEWEL") == nullptr, "does not claim JEWEL WHITE");
    expect(strstr(sh.banner, "AUDIO       EMERGENCY") == nullptr,
           "does not claim AUDIO EMERGENCY telemetry");
  }

  /* ---- FLARE only when flame is known burning-class ---- */
  {
    ShowduinoLampDirectorInput in = online_burning("LAMP-01");
    ShowduinoLampDirectorSheet sh;
    strncpy(in.detail.fx, "OFF", sizeof(in.detail.fx) - 1);
    in.detail.fx[0] = 0;
    strncpy(in.detail.fx, "-", sizeof(in.detail.fx) - 1);
    showduino_lamp_director_build_sheet(&in, &sh);
    expect_str(sh.state, "OFF", "fx '-' is OFF not inferred BURNING");
    expect(!sh.flare_enabled, "FLARE disabled when flame is OFF");

    strncpy(in.detail.fx, "STRIKING", sizeof(in.detail.fx) - 1);
    showduino_lamp_director_build_sheet(&in, &sh);
    expect_str(sh.state, "STRIKING", "STRIKING from fx");
    expect(!sh.flare_enabled, "FLARE disabled while STRIKING");

    strncpy(in.detail.fx, "BURNING", sizeof(in.detail.fx) - 1);
    showduino_lamp_director_build_sheet(&in, &sh);
    expect(sh.flare_enabled, "FLARE enabled while BURNING");
  }

  /* ---- no fake OK health; no direct hardware controls ---- */
  {
    ShowduinoLampDirectorInput in = online_burning("LAMP-01");
    ShowduinoLampDirectorSheet sh;
    showduino_lamp_director_build_sheet(&in, &sh);
    expect(!showduino_lamp_desk_health_is_fake_ok(sh.jewel_health), "JEWEL not fake OK");
    expect(!showduino_lamp_desk_health_is_fake_ok(sh.audio_health), "AUDIO health not fake OK");
    expect(!showduino_lamp_desk_health_is_fake_ok(sh.motion_health), "MOTION health not fake OK");
    expect(!showduino_lamp_desk_health_is_fake_ok(sh.mic_health), "MIC not fake OK");
    expect(!showduino_lamp_desk_health_is_fake_ok(sh.voltage_health), "VOLTAGE not fake OK");
    expect_str(sh.jewel_health, "--", "JEWEL health unavailable");
    expect(!sh.has_jewel_control, "no direct Jewel control exists");
    expect(!sh.has_audio_control, "no direct lamp audio control exists");
    expect(!sh.has_espnow_direct, "no direct ESP-NOW lamp control exists");
  }

  /* ---- do not infer flameloop from BURNING ---- */
  {
    ShowduinoLampDirectorInput in = online_burning("LAMP-01");
    ShowduinoLampDirectorSheet sh;
    showduino_lamp_director_build_sheet(&in, &sh);
    expect(strcmp(sh.audio, "FLAME LOOP") != 0, "does not invent flameloop from BURNING");
    expect_str(sh.audio, "--", "AUDIO row stays unknown without telemetry");
  }

  /* ---- existing wire detail parser (no protocol expansion) ---- */
  {
    ShowduinoLampDetailWire d;
    expect(showduino_parse_state_node_lamp_detail(
               "STATE:NODE:LAMP:D:SHOW_CONTROLLED:BURNING:80:3C:DC:75:6B:CD:20:0.4.0",
               &d) == 1,
           "parses existing LAMP:D: wire");
    expect_str(d.state, "SHOW_CONTROLLED", "detail owner state");
    expect_str(d.fx, "BURNING", "detail carbide fx");
    expect(d.brightness == 80, "detail brightness");
  }

  /* ---- other node sheets unchanged ---- */
  {
    Page04SheetAction a[4];
    page04_sheet_fill_actions(PAGE04_NAV_ROLE_AUDIO, 1, 0, a);
    expect_str(a[0].label, "AUDIO DESK", "audio sheet AUDIO DESK");
    expect_str(a[1].label, "TEST", "audio sheet TEST");
    expect_str(a[2].label, "STOP", "audio sheet STOP");
    expect_str(a[3].label, "REFRESH", "audio sheet REFRESH");
    expect(a[1].enabled && a[2].enabled, "audio live actions enabled when online");

    page04_sheet_fill_actions(PAGE04_NAV_ROLE_AUDIO, 1, 1, a);
    expect(!a[1].enabled && !a[2].enabled && a[3].enabled,
           "audio TEST/STOP lock in emergency; REFRESH stays");

    page04_sheet_fill_actions(PAGE04_NAV_ROLE_NEOPIXEL, 1, 0, a);
    expect_str(a[0].label, "REFRESH", "pixel sheet refresh only");
    expect(!a[1].shown && !a[2].shown && !a[3].shown, "pixel has no FX / colour controls");

    page04_sheet_fill_actions(PAGE04_NAV_ROLE_MOSFET, 0, 0, a);
    expect(!a[0].shown && !a[1].shown, "MOSFET remains visible-unavailable");

    page04_sheet_fill_actions(PAGE04_NAV_ROLE_DMX, 0, 0, a);
    expect(!a[0].shown, "DMX remains visible-unavailable");

    page04_sheet_fill_actions(PAGE04_NAV_ROLE_STAGE, 1, 0, a);
    expect_str(a[0].label, "REFRESH", "stage/comms refresh");

    page04_sheet_fill_actions(PAGE04_NAV_ROLE_LAMP, 1, 0, a);
    expect(strcmp(a[0].label, "CLEAR") != 0 && strcmp(a[1].label, "CLEAR") != 0,
           "lamp actions are not emergency-clear");
    int i;
    for (i = 0; i < 4; i++) {
      expect(strstr(a[i].cmd, "EMERGENCY:CLEAR") == nullptr, "lamp UI cmd is not CLEAR");
      expect(strstr(a[i].cmd, "SOLID") == nullptr, "lamp UI cmd is not Jewel SOLID");
      expect(strstr(a[i].cmd, "AUDIO:NODE") == nullptr, "lamp UI cmd is not Audio Node");
    }
  }

  /* ---- default selected id is protocol default, not a second naming system ---- */
  {
    char id[16];
    showduino_lamp_director_select_id("", id, sizeof(id));
    expect_str(id, SHOWDUINO_CARBIDE_LOGICAL_DEFAULT, "empty discovery uses LAMP-01 default");
    showduino_lamp_director_select_id("LAMP-04", id, sizeof(id));
    expect_str(id, "LAMP-04", "discovered id wins over default");
    showduino_lamp_director_select_id("not-an-id", id, sizeof(id));
    expect_str(id, SHOWDUINO_CARBIDE_LOGICAL_DEFAULT, "invalid id rejected");
  }

  std::printf("\n");
  if (g_fails == 0) {
    std::printf("All Director lamp sheet tests passed.\n");
    return 0;
  }
  std::printf("%d test(s) failed.\n", g_fails);
  return 1;
}
