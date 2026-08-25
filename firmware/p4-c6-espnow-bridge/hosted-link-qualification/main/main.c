#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_hosted.h"
#include "esp_hosted_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

// ============================================================
// Showduino onboard C6 qualification
//
// This is intentionally a P4-host test. It leaves the factory C6
// firmware untouched and proves that the P4 can reach the onboard
// C6 over the module's internal SDIO link and use its Wi-Fi radio.
// ============================================================

static const char *TAG = "SHOWDUINO_C6_TEST";
static EventGroupHandle_t s_events;
static const EventBits_t TRANSPORT_UP_BIT = BIT0;
static const TickType_t TRANSPORT_TIMEOUT = pdMS_TO_TICKS(12000);

// Keep the output finite and readable in Serial Monitor.
#define MAX_APS_TO_PRINT 32

static void print_mac(const uint8_t mac[6])
{
    printf("%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static const char *auth_name(wifi_auth_mode_t auth)
{
    switch (auth) {
        case WIFI_AUTH_OPEN: return "OPEN";
        case WIFI_AUTH_WEP: return "WEP";
        case WIFI_AUTH_WPA_PSK: return "WPA-PSK";
        case WIFI_AUTH_WPA2_PSK: return "WPA2-PSK";
        case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2-PSK";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENTERPRISE";
        case WIFI_AUTH_WPA3_PSK: return "WPA3-PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3-PSK";
        default: return "OTHER";
    }
}

static void qualification_failed(const char *step, esp_err_t err)
{
    ESP_LOGE(TAG, "============================================================");
    ESP_LOGE(TAG, "SHOWDUINO ONBOARD C6 QUALIFICATION: FAIL");
    ESP_LOGE(TAG, "Failed step: %s", step);
    ESP_LOGE(TAG, "ESP error: %s (%d)", esp_err_to_name(err), (int)err);
    ESP_LOGE(TAG, "Do NOT flash custom firmware to the C6 yet.");
    ESP_LOGE(TAG, "============================================================");

    while (true) {
        ESP_LOGE(TAG, "C6 qualification remains FAILED at: %s", step);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void check_step(const char *step, esp_err_t err)
{
    if (err != ESP_OK) {
        qualification_failed(step, err);
    }
    ESP_LOGI(TAG, "PASS: %s", step);
}

static void hosted_event_handler(void *arg,
                                 esp_event_base_t event_base,
                                 int32_t event_id,
                                 void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base != ESP_HOSTED_EVENT) {
        return;
    }

    if (event_id == ESP_HOSTED_EVENT_TRANSPORT_UP) {
        ESP_LOGI(TAG, "ESP-Hosted event: SDIO transport UP");
        xEventGroupSetBits(s_events, TRANSPORT_UP_BIT);
    } else if (event_id == ESP_HOSTED_EVENT_CP_INIT) {
        ESP_LOGI(TAG, "ESP-Hosted event: C6 co-processor initialized");
    } else {
        ESP_LOGI(TAG, "ESP-Hosted event: id=%ld", (long)event_id);
    }
}

static void initialise_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS requires erase before initialization");
        check_step("erase NVS", nvs_flash_erase());
        err = nvs_flash_init();
    }
    check_step("initialize NVS", err);
}

static void run_wifi_scan(void)
{
    wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
    check_step("initialize remote Wi-Fi driver", esp_wifi_init(&wifi_cfg));
    check_step("set C6 Wi-Fi mode to STA", esp_wifi_set_mode(WIFI_MODE_STA));
    check_step("start C6 Wi-Fi", esp_wifi_start());

    uint8_t mac[6] = {0};
    check_step("read C6 STA MAC", esp_wifi_get_mac(WIFI_IF_STA, mac));

    printf("\n[C6] STA MAC: ");
    print_mac(mac);
    printf("\n\n");

    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time = {
            .active = {
                .min = 40,
                .max = 120,
            },
        },
    };

    ESP_LOGI(TAG, "Starting Wi-Fi scan through onboard C6...");
    check_step("complete Wi-Fi scan", esp_wifi_scan_start(&scan_cfg, true));

    uint16_t total_aps = 0;
    check_step("read Wi-Fi scan count", esp_wifi_scan_get_ap_num(&total_aps));

    uint16_t requested = total_aps;
    if (requested > MAX_APS_TO_PRINT) {
        requested = MAX_APS_TO_PRINT;
    }

    ESP_LOGI(TAG, "C6 reported %u access point(s); printing up to %u",
             (unsigned)total_aps,
             (unsigned)requested);

    if (requested == 0) {
        ESP_LOGW(TAG, "No APs found. SDIO and Wi-Fi RPC are working, but radio range/channel environment should be checked.");
        return;
    }

    wifi_ap_record_t *records = calloc(requested, sizeof(wifi_ap_record_t));
    if (records == NULL) {
        qualification_failed("allocate scan results", ESP_ERR_NO_MEM);
    }

    uint16_t returned = requested;
    esp_err_t err = esp_wifi_scan_get_ap_records(&returned, records);
    if (err != ESP_OK) {
        free(records);
        qualification_failed("read Wi-Fi scan records", err);
    }

    printf("\n--- Onboard C6 Wi-Fi scan ---\n");
    for (uint16_t i = 0; i < returned; ++i) {
        printf("%2u. %-32s RSSI=%4d dBm  CH=%2u  %s\n",
               (unsigned)(i + 1),
               (const char *)records[i].ssid,
               records[i].rssi,
               records[i].primary,
               auth_name(records[i].authmode));
    }
    printf("--- End scan ---\n\n");

    free(records);
}

void app_main(void)
{
    printf("\n\n");
    ESP_LOGI(TAG, "============================================================");
    ESP_LOGI(TAG, "Showduino P4 -> onboard C6 qualification starting");
    ESP_LOGI(TAG, "Target board: Waveshare ESP32-P4-Module-DEV-KIT");
    ESP_LOGI(TAG, "C6 firmware: factory ESP-Hosted slave (leave untouched)");
    ESP_LOGI(TAG, "Expected bus: SDIO (P4 18/19/14/15/16/17 + reset 54)");
    ESP_LOGI(TAG, "============================================================");

    initialise_nvs();
    check_step("initialize esp-netif", esp_netif_init());
    check_step("create default event loop", esp_event_loop_create_default());

    s_events = xEventGroupCreate();
    if (s_events == NULL) {
        qualification_failed("create event group", ESP_ERR_NO_MEM);
    }

    check_step("register ESP-Hosted event handler",
               esp_event_handler_register(ESP_HOSTED_EVENT,
                                          ESP_EVENT_ANY_ID,
                                          hosted_event_handler,
                                          NULL));

    check_step("initialize ESP-Hosted host", esp_hosted_init());
    check_step("connect P4 to onboard C6", esp_hosted_connect_to_slave());

    ESP_LOGI(TAG, "Waiting up to 12 seconds for internal SDIO transport...");
    EventBits_t bits = xEventGroupWaitBits(s_events,
                                          TRANSPORT_UP_BIT,
                                          pdFALSE,
                                          pdTRUE,
                                          TRANSPORT_TIMEOUT);

    if ((bits & TRANSPORT_UP_BIT) == 0) {
        qualification_failed("wait for SDIO transport UP", ESP_ERR_TIMEOUT);
    }

    ESP_LOGI(TAG, "PASS: internal P4 <-> C6 SDIO transport is UP");

    run_wifi_scan();

    ESP_LOGI(TAG, "============================================================");
    ESP_LOGI(TAG, "SHOWDUINO ONBOARD C6 QUALIFICATION: PASS");
    ESP_LOGI(TAG, "P4 -> SDIO -> C6 -> Wi-Fi radio path is operational.");
    ESP_LOGI(TAG, "Next gate: add Showduino ESP-NOW service without losing SDIO.");
    ESP_LOGI(TAG, "============================================================");

    while (true) {
        ESP_LOGI(TAG, "C6 qualification PASS - hosted transport remains online");
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
