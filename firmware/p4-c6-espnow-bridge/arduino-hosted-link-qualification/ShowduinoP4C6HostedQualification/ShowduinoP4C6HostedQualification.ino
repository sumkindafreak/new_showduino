/*
  Showduino P4 -> Onboard C6 Qualification (Arduino IDE)

  Purpose:
  - Runs on the ESP32-P4.
  - Leaves the factory ESP32-C6 firmware untouched.
  - Brings up the onboard C6 through the internal SDIO link.
  - Prints the C6 Wi-Fi MAC address.
  - Performs a real Wi-Fi scan and reports PASS / FAIL.

  Tested design target:
  - Waveshare ESP32-P4-Module-DEV-KIT
  - Arduino-ESP32 3.3.x (Showduino currently uses 3.3.11)

  No Wi-Fi password is required for this test.
  Do NOT put the C6 into download mode for this sketch.
*/

#include <Arduino.h>
#include <WiFi.h>

// ============================================================
// Internal P4 <-> C6 SDIO pin map
// ============================================================
#define C6_SDIO_CLK_PIN 18
#define C6_SDIO_CMD_PIN 19
#define C6_SDIO_D0_PIN  14
#define C6_SDIO_D1_PIN  15
#define C6_SDIO_D2_PIN  16
#define C6_SDIO_D3_PIN  17
#define C6_RESET_PIN     54

// ============================================================
// General settings
// ============================================================
#define SERIAL_BAUD       115200
#define RESCAN_INTERVAL_MS 15000UL
#define MAX_NETWORKS_PRINT 32

static bool gQualificationPassed = false;
static unsigned long gLastScanMs = 0;

// ============================================================
// Helper: readable encryption name
// ============================================================
const char *encryptionName(wifi_auth_mode_t mode) {
  switch (mode) {
    case WIFI_AUTH_OPEN:            return "OPEN";
    case WIFI_AUTH_WEP:             return "WEP";
    case WIFI_AUTH_WPA_PSK:         return "WPA";
    case WIFI_AUTH_WPA2_PSK:        return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA+WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-EAP";
    case WIFI_AUTH_WPA3_PSK:        return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2+WPA3";
#ifdef WIFI_AUTH_WAPI_PSK
    case WIFI_AUTH_WAPI_PSK:        return "WAPI";
#endif
    default:                        return "OTHER";
  }
}

// ============================================================
// Helper: print a hard failure clearly
// ============================================================
void failQualification(const char *reason) {
  gQualificationPassed = false;

  Serial.println();
  Serial.println("============================================================");
  Serial.println("SHOWDUINO ONBOARD C6 QUALIFICATION: FAIL");
  Serial.print("Reason: ");
  Serial.println(reason);
  Serial.println("Do NOT flash custom firmware to the C6 yet.");
  Serial.println("Copy the complete Serial Monitor output for diagnosis.");
  Serial.println("============================================================");
}

// ============================================================
// Scan through the C6 radio and print results
// ============================================================
bool scanNetworks() {
  Serial.println();
  Serial.println("[C6] Starting 2.4 GHz Wi-Fi scan...");

  int networkCount = WiFi.scanNetworks(false, true);

  if (networkCount < 0) {
    Serial.printf("[C6] Wi-Fi scan failed, code=%d\n", networkCount);
    return false;
  }

  Serial.printf("[C6] Scan complete: %d network(s) found\n", networkCount);

  if (networkCount == 0) {
    Serial.println("[C6] WARNING: scan completed but no APs were visible.");
    Serial.println("[C6] SDIO/Wi-Fi startup succeeded; check local 2.4 GHz coverage if unexpected.");
    WiFi.scanDelete();
    return true;
  }

  Serial.println();
  Serial.println("Nr | SSID                             | RSSI | CH | Security");
  Serial.println("---+----------------------------------+------+----+-------------");

  int printCount = networkCount;
  if (printCount > MAX_NETWORKS_PRINT) {
    printCount = MAX_NETWORKS_PRINT;
  }

  for (int i = 0; i < printCount; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.length() == 0) {
      ssid = "<hidden>";
    }

    Serial.printf("%2d | %-32.32s | %4ld | %2ld | %s\n",
                  i + 1,
                  ssid.c_str(),
                  (long)WiFi.RSSI(i),
                  (long)WiFi.channel(i),
                  encryptionName(WiFi.encryptionType(i)));
    delay(5);
  }

  if (networkCount > printCount) {
    Serial.printf("... %d additional network(s) not printed\n", networkCount - printCount);
  }

  WiFi.scanDelete();
  return true;
}

// ============================================================
// Run the full qualification sequence
// ============================================================
bool runQualification() {
  Serial.println();
  Serial.println("============================================================");
  Serial.println("Showduino P4 -> onboard C6 Arduino qualification");
  Serial.println("Board: Waveshare ESP32-P4-Module-DEV-KIT");
  Serial.println("C6: factory firmware retained");
  Serial.println("Transport: internal SDIO");
  Serial.println("============================================================");

#if !defined(CONFIG_IDF_TARGET_ESP32P4)
  failQualification("This sketch must be compiled for ESP32-P4.");
  return false;
#endif

#if !defined(CONFIG_ESP_HOSTED_ENABLED) || !CONFIG_ESP_HOSTED_ENABLED
  failQualification("This Arduino-ESP32 build does not have ESP-Hosted enabled for the P4.");
  return false;
#else
  Serial.println("[C6] Configuring onboard SDIO pins...");

  // IMPORTANT: setPins() must happen before Wi-Fi is started.
  bool pinsOk = WiFi.setPins(
    C6_SDIO_CLK_PIN,
    C6_SDIO_CMD_PIN,
    C6_SDIO_D0_PIN,
    C6_SDIO_D1_PIN,
    C6_SDIO_D2_PIN,
    C6_SDIO_D3_PIN,
    C6_RESET_PIN
  );

  if (!pinsOk) {
    failQualification("WiFi.setPins() rejected the onboard C6 SDIO pin map.");
    return false;
  }

  Serial.println("PASS: onboard C6 SDIO pins accepted");
#endif

  Serial.println("[C6] Starting hosted Wi-Fi interface...");

  if (!WiFi.mode(WIFI_STA)) {
    failQualification("WiFi.mode(WIFI_STA) failed; hosted C6 did not initialize correctly.");
    return false;
  }

  Serial.println("PASS: hosted Wi-Fi interface started");

  delay(500);

  String mac = WiFi.STA.macAddress();
  Serial.print("[C6] STA MAC: ");
  Serial.println(mac);

  if (mac.length() < 17 || mac == "00:00:00:00:00:00") {
    failQualification("C6 returned an invalid STA MAC address.");
    return false;
  }

  Serial.println("PASS: valid C6 STA MAC received over hosted link");

  if (!scanNetworks()) {
    failQualification("C6 Wi-Fi scan failed.");
    return false;
  }

  Serial.println();
  Serial.println("============================================================");
  Serial.println("SHOWDUINO ONBOARD C6 QUALIFICATION: PASS");
  Serial.println("P4 -> SDIO -> onboard C6 -> Wi-Fi radio is operational.");
  Serial.println("Next gate: Showduino ESP-NOW service on the onboard C6.");
  Serial.println("============================================================");
  Serial.println();
  Serial.println("Type S in Serial Monitor to scan again.");
  Serial.println("Type I to print C6 link information.");

  return true;
}

// ============================================================
// Print current link information
// ============================================================
void printInfo() {
  Serial.println();
  Serial.println("--- Showduino onboard C6 status ---");
  Serial.print("Qualification: ");
  Serial.println(gQualificationPassed ? "PASS" : "FAIL");
  Serial.print("C6 STA MAC: ");
  Serial.println(WiFi.STA.macAddress());
  Serial.printf("SDIO pins: CLK=%d CMD=%d D0=%d D1=%d D2=%d D3=%d RST=%d\n",
                C6_SDIO_CLK_PIN,
                C6_SDIO_CMD_PIN,
                C6_SDIO_D0_PIN,
                C6_SDIO_D1_PIN,
                C6_SDIO_D2_PIN,
                C6_SDIO_D3_PIN,
                C6_RESET_PIN);
  Serial.println("-----------------------------------");
}

// ============================================================
// Arduino setup
// ============================================================
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1200);

  gQualificationPassed = runQualification();
  gLastScanMs = millis();
}

// ============================================================
// Arduino loop
// ============================================================
void loop() {
  // Manual Serial Monitor controls are useful while bench testing.
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (c == 'S' || c == 's') {
      if (gQualificationPassed) {
        scanNetworks();
        gLastScanMs = millis();
      } else {
        Serial.println("Qualification is failed; reset the P4 after correcting the problem.");
      }
    } else if (c == 'I' || c == 'i') {
      printInfo();
    }
  }

  // Keep the successful hosted path alive and visibly healthy.
  if (gQualificationPassed && (millis() - gLastScanMs >= RESCAN_INTERVAL_MS)) {
    Serial.println("[C6] Link alive. Type S to rescan or I for status.");
    gLastScanMs = millis();
  }

  delay(10);
}
