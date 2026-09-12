#include "NodeDiagnostics.h"
#include "LampEngine.h"
#include "LampNodeState.h"
#include "LampProtocol.h"
#include "LampConfig.h"
#include "LampSensors.h"
#include "LampAudio.h"
#include "EspNowLampTransport.h"
#include "LocalControls.h"
#include "LampMotion.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_lamp_motion.h"
#include "../../../protocol/showduino_lamp_node.h"
#include "../../../protocol/showduino_log.h"
#include "../../../protocol/showduino_version.h"

static uint32_t sLoopUs = 0;
static uint32_t sLoopMaxUs = 0;
static uint32_t sMinHeap = 0;

void nodeDiagBegin() {
  sMinHeap = ESP.getFreeHeap();
}

void nodeDiagMarkLoop(uint32_t elapsedUs) {
  sLoopUs = elapsedUs;
  if (elapsedUs > sLoopMaxUs) sLoopMaxUs = elapsedUs;
  const uint32_t heap = ESP.getFreeHeap();
  if (heap < sMinHeap) sMinHeap = heap;
}

void nodeDiagPrintPins() {
  Serial.println("[PINS] ESP32-S3 Lamp Node — production physical map");
  Serial.printf("  source=%s confirmed=%s\n", SHOWDUINO_LAMP_PIN_SOURCE,
                SHOWDUINO_LAMP_PINS_CONFIRMED ? "YES" : "NO");
  Serial.printf("  Jewel DATA %s (%d)\n", showduino_lamp_gpio_label(SHOWDUINO_LAMP_PIXEL_PIN),
                SHOWDUINO_LAMP_PIXEL_PIN);
  Serial.printf("  Ignition button %s (%d) %s\n",
                showduino_lamp_gpio_label(SHOWDUINO_LAMP_BTN_IGNITE),
                SHOWDUINO_LAMP_BTN_IGNITE, SHOWDUINO_LAMP_BTN_POLARITY_NOTE);
  Serial.printf("  Mic ADC %s (%d)\n", showduino_lamp_gpio_label(SHOWDUINO_LAMP_MIC_PIN),
                SHOWDUINO_LAMP_MIC_PIN);
  Serial.printf("  Light ADC %s (%d)\n", showduino_lamp_gpio_label(SHOWDUINO_LAMP_LIGHT_PIN),
                SHOWDUINO_LAMP_LIGHT_PIN);
  Serial.printf("  Voltage ADC %s (%d)\n", showduino_lamp_gpio_label(SHOWDUINO_LAMP_VOLT_PIN),
                SHOWDUINO_LAMP_VOLT_PIN);
  Serial.printf("  Motion %s (%d) %s polarity=%s action=%s\n",
                showduino_lamp_gpio_label(SHOWDUINO_LAMP_MOTION_PIN),
                SHOWDUINO_LAMP_MOTION_PIN, lampMotionPhysicalStatus(),
                showduino_motion_polarity_name(lampConfigMotionActiveLow()),
                showduino_motion_action_name(lampConfigMotionAction()));
  Serial.printf("  Fermion TX %s (%d)  RX %s (%d) baud=%lu\n",
                showduino_lamp_gpio_label(SHOWDUINO_LAMP_FERMION_TX_PIN),
                SHOWDUINO_LAMP_FERMION_TX_PIN,
                showduino_lamp_gpio_label(SHOWDUINO_LAMP_FERMION_RX_PIN),
                SHOWDUINO_LAMP_FERMION_RX_PIN,
                (unsigned long)SHOWDUINO_LAMP_FERMION_BAUD);
  Serial.println("  Choose ADC1 (GPIO1-10 typical) for mic/light/voltage. Avoid ADC2 with Wi-Fi.");
}

void nodeDiagPrintBootBanner() {
  char mac[24];
  lampEspNowMacString(mac, sizeof(mac));
  Serial.println("================================================");
  Serial.println(" SHOWDUINO S3 LAMP NODE — CARBIDE SIMULATOR");
  Serial.println("================================================");
  Serial.printf("Name: %s  id=%s\n", lampConfigName(), lampConfigId());
  Serial.printf("Board: %s\n", SHOWDUINO_LAMP_NODE_BOARD);
  Serial.printf("Firmware: %s  Product: %s  Protocol: %s\n",
                SHOWDUINO_LAMP_NODE_FW, SHOWDUINO_PLATFORM_VERSION,
                SHOWDUINO_LAMP_PROTOCOL);
  Serial.printf("ESP32: %s rev %u\n", ESP.getChipModel(),
                (unsigned)ESP.getChipRevision());
  Serial.printf("Flash: %lu MB  PSRAM: %s\n",
                (unsigned long)(ESP.getFlashChipSize() / (1024UL * 1024UL)),
                ESP.getPsramSize() ? "present" : "none");
  Serial.printf("MAC: %s\n", mac);
  Serial.printf("ESP-NOW: %s ch=%u comms=%s\n",
                lampEspNowReady() ? "ready" : "FAULT",
                (unsigned)lampEspNowChannel(),
                lampEspNowHaveComms() ? "ONLINE" : "SEARCHING");
  Serial.printf("Jewel: %s x%u  carbide=%s\n",
                lampEngineJewelReady() ? "driver" : "stub",
                (unsigned)lampEngineCount(), lampEngineCarbideName());
  Serial.printf("Fermion: %s (local FX, not Audio Node)\n", lampAudioStatus());
  Serial.printf("Mode: %s  owner=%s  emergency=%s  fault=%s\n",
                lampNodeProductModeName(),
                lampNodeStateName(),
                lampEngineEmergency() ? "ACTIVE" : "CLEAR",
                lampNodeStateFault());
  nodeDiagPrintPins();
  if (SHOWDUINO_LAMP_BTN_IGNITE >= 0) {
    Serial.printf("Button idle GPIO%d=%s (%s)\n", SHOWDUINO_LAMP_BTN_IGNITE,
                  digitalRead(SHOWDUINO_LAMP_BTN_IGNITE) ? "HIGH" : "LOW",
                  SHOWDUINO_LAMP_BTN_POLARITY_NOTE);
  }
  Serial.println("Physical lamp is live. SoftAP WebUI is local. Showduino is optional.");
}

void nodeDiagPrintHelp() {
  Serial.println("[CONSOLE] S3 Lamp Node commissioning commands:");
  Serial.println("  HELP  STATUS  MAC  PINS");
  Serial.println("  LAMP:STATUS  LAMP:IGNITE  LAMP:EXTINGUISH  LAMP:TEST");
  Serial.println("  LAMP:FX:LOW_FLAME | UNSTABLE | FLARE | STEADY_FLAME");
  Serial.println("  LAMP:OFF  MIC:STATUS  SENSORS  AUDIO:STATUS  MOTION:STATUS");
  Serial.println("Webpage cannot clear emergency. P4 remains authoritative.");
}

void nodeDiagPrintStatus() {
  char mac[24];
  lampEspNowMacString(mac, sizeof(mac));
  Serial.printf("ID %s  NAME %s\n", lampConfigId(), lampConfigName());
  Serial.printf("MODE %s  OWNER %s  CARBIDE %s\n",
                lampNodeProductModeName(), lampNodeStateName(),
                lampEngineCarbideName());
  Serial.printf("FAULT %s  SHOW %s  EMERGENCY %s\n",
                lampNodeStateFault(),
                lampNodeStateShowControlled() ? "YES" : "NO",
                lampEngineEmergency() ? "ACTIVE" : "CLEAR");
  Serial.printf("MAC %s  CH %u  COMMS %s\n", mac,
                (unsigned)lampEspNowChannel(),
                lampEspNowHaveComms() ? "YES" : "NO");
  Serial.printf("MIC raw=%ld filt=%ld base=%ld blowMs=%lu class=%s\n",
                (long)lampSensorsMicRaw(), (long)lampSensorsMicFiltered(),
                (long)lampSensorsMicBaseline(),
                (unsigned long)lampSensorsBlow()->aboveMs,
                showduino_blow_detected(lampSensorsBlow()) ? "YES" : "NO");
  Serial.printf("LIGHT raw=%ld  VOLT raw=%ld mv=%ld (%s %s)\n",
                (long)lampSensorsLightRaw(), (long)lampSensorsVoltRaw(),
                (long)lampSensorsVoltMv(), lampSensorsVoltStatus(),
                lampSensorsVoltWarn());
  Serial.printf("AUDIO %s heard=%s rx=%s role=%s file=%s query=%s\n",
                lampAudioStatus(),
                lampAudioHeardReply() ? "YES" : "NO",
                lampAudioLastRx(),
                lampAudioCurrentRole(),
                lampAudioCurrentFile()[0] ? lampAudioCurrentFile() : "-",
                lampAudioFileQueryStatus());
  Serial.printf("MOTION gpio=%d %s raw=%s state=%s pol=%s en=%s act=%s cd=%s\n",
                lampMotionPin(), lampMotionPhysicalStatus(),
                lampMotionRawName(), lampMotionStateName(),
                showduino_motion_polarity_name(lampConfigMotionActiveLow()),
                lampConfigMotionEnabled() ? "YES" : "NO",
                showduino_motion_action_name(lampConfigMotionAction()),
                showduino_motion_cooldown_hold(lampMotionDetector()) ? "HOLD" : "READY");
  Serial.printf("UPTIME %lu ms heap=%lu min=%lu loop_us=%lu max=%lu\n",
                (unsigned long)millis(),
                (unsigned long)ESP.getFreeHeap(),
                (unsigned long)sMinHeap,
                (unsigned long)sLoopUs,
                (unsigned long)sLoopMaxUs);
  nodeDiagPrintPins();
}

bool nodeDiagHandleLine(const char *line) {
  if (!line || !line[0]) return false;
  if (showduino_log_handle_command(line)) return true;
  if (!strcmp(line, "HELP")) {
    nodeDiagPrintHelp();
    return true;
  }
  if (!strcmp(line, "STATUS")) {
    nodeDiagPrintStatus();
    return true;
  }
  if (!strcmp(line, "MAC")) {
    char mac[24];
    lampEspNowMacString(mac, sizeof(mac));
    Serial.println(mac);
    return true;
  }
  if (!strcmp(line, "PINS")) {
    nodeDiagPrintPins();
    return true;
  }
  if (!strcmp(line, "MIC:STATUS") || !strcmp(line, "SENSORS")) {
    nodeDiagPrintStatus();
    return true;
  }
  if (!strcmp(line, "MOTION:STATUS") || !strcmp(line, "MOTION")) {
    nodeDiagPrintStatus();
    return true;
  }
  if (!strcmp(line, "AUDIO:STATUS")) {
    Serial.printf("AUDIO %s role=%s file=%s vol=%u query=%s expected=%s\n",
                  lampAudioStatus(), lampAudioCurrentRole(),
                  lampAudioCurrentFile()[0] ? lampAudioCurrentFile() : "-",
                  (unsigned)lampAudioVolume(), lampAudioFileQueryStatus(),
                  lampAudioExpectedFiles());
    return true;
  }
  if (!strcmp(line, "LAMP:STATUS")) {
    char st[96];
    lampProtocolFormatStatus(st, sizeof(st));
    Serial.println(st);
    return true;
  }
  if (!strcmp(line, "PIXEL:IDENTIFY")) {
    if (lampEngineEmergency() || !showduino_lamp_web_may_control(lampNodeState())) {
      Serial.println("[LAMP] PIXEL IDENTIFY locked");
      return true;
    }
    if (!lampEngineStartIdentify()) {
      Serial.println("[LAMP] PIXEL IDENTIFY rejected");
      return true;
    }
    Serial.println("[LAMP] PIXEL IDENTIFY started");
    return true;
  }
  return false;
}
