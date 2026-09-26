#include "EmergencyWeb.h"

#include <WebServer.h>
#include <WiFi.h>

#include "EmergencyIdentity.h"
#include "EmergencyProtocol.h"
#include "EmergencyInput.h"
#include "EmergencyDisplay.h"
#include "EspNowEmergencyTransport.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_emergency_node.h"
#include "../../../protocol/showduino_version.h"
#include "../../../protocol/showduino_protocol_version.h"
#include "../../shared-node/NodeSoftAp.h"

static WebServer sServer(80);
static bool sRoutes = false;
static bool sBegun = false;

static const char kPage[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Showduino Emergency Node</title>
<style>
body{margin:0;background:#111;color:#ddd;font:15px/1.35 sans-serif}
header{padding:12px 16px;background:#1a1a1a;border-bottom:2px solid #c33}
h1{margin:0;font-size:18px}
.card{background:#1c1c1c;border:1px solid #2a2a2a;border-radius:8px;padding:12px;margin:12px}
.kv{display:grid;grid-template-columns:160px 1fr;gap:4px 10px;font-size:13px}
label{display:block;margin:8px 0 4px;color:#aaa}
input{background:#111;color:#fff;border:1px solid #444;padding:8px;width:100%;box-sizing:border-box}
button{background:#2a2a2a;color:#fff;border:1px solid #444;padding:10px 14px;border-radius:4px;margin:8px 8px 0 0}
.warn{color:#fc6}.err{color:#f66}.ok{color:#6c6}
</style></head><body>
<header><h1 id="title">Wireless Emergency Button</h1></header>
<div class="card"><div class="kv" id="kv"></div>
<label>Logical ID</label><input id="nid" maxlength="12">
<label>Friendly location</label><input id="nname" maxlength="20">
<button id="save">SAVE IDENTITY</button>
<button id="rearm">MAINTENANCE LOCAL RESET</button>
<p class="warn">ASSERT ONLY — this page cannot clear the P4 global emergency.</p>
<p class="warn">Momentary pushbutton: press asserts and latches. Release never clears. After an authoritative P4 clear with the button released, this station returns to READY automatically.</p>
<p class="warn">Maintenance local reset never clears P4. Normal operators do not need it.</p>
<p class="warn">Update one station at a time: ESTOP-01 update → reboot → healthy+linked → ESTOP-02.</p>
<p>Emergency operation does not require this page, Wi-Fi, OLED, or a browser.</p>
</div>
<script>
const $=id=>document.getElementById(id);
async function load(){
  const S=await (await fetch('/api/status')).json();
  $('title').textContent=(S.id||'ESTOP')+' — '+(S.name||'Emergency');
  $('nid').value=S.id||''; $('nname').value=S.name||'';
  const rows=[
    ['Emergency pushbutton',S.input||'—',S.input==='PRESSED'?'err':'ok'],
    ['Local latch',S.latched?'YES':'NO',S.latched?'err':'ok'],
    ['Emergency',S.latched?'ASSERTED':'READY',S.latched?'err':'ok'],
    ['Global observed',S.state||'—',''],
    ['Radio',S.radio?'LINKED':'SEARCHING',S.radio?'ok':'warn'],
    ['OLED',S.oled||'—',S.oled==='READY'?'ok':'warn'],
    ['Channel',String(S.channel||'—'),''],
    ['RSSI',String(S.rssi||'—'),''],
    ['Firmware',S.firmware||'—',''],
    ['GPIO verified',S.gpioVerified?'YES':'NO (UNCONFIRMED)','warn']
  ];
  $('kv').innerHTML=rows.map(r=>`<div>${r[0]}</div><div class="${r[2]||''}">${r[1]}</div>`).join('');
}
$('save').onclick=async()=>{
  await fetch('/api/id',{method:'POST',headers:{'Content-Type':'application/json'},
    body:JSON.stringify({id:$('nid').value,name:$('nname').value})});
  load();
};
$('rearm').onclick=async()=>{ await fetch('/api/rearm',{method:'POST'}); load(); };
load(); setInterval(load,2000);
</script></body></html>
)HTML";

static void sendStatus() {
  char mac[24];
  emergencyEspNowMacString(mac, sizeof(mac));
  String json = "{\n";
  json += "  \"id\": \"";
  json += emergencyIdentityId();
  json += "\",\n  \"name\": \"";
  json += emergencyIdentityName();
  json += "\",\n  \"input\": \"";
  json += showduino_emergency_button_name(gEmergencyMachine.input_open);
  json += "\",\n  \"latched\": ";
  json += gEmergencyMachine.latched ? "true" : "false";
  json += ",\n  \"acked\": ";
  json += gEmergencyMachine.acked ? "true" : "false";
  json += ",\n  \"state\": \"";
  json += showduino_emergency_state_name(gEmergencyMachine.state);
  json += "\",\n  \"radio\": ";
  json += emergencyEspNowHaveComms() ? "true" : "false";
  json += ",\n  \"oled\": \"";
  json += emergencyDisplayReady() ? "READY" : "FAIL";
  json += "\",\n  \"channel\": ";
  json += String((unsigned)emergencyEspNowChannel());
  json += ",\n  \"rssi\": ";
  json += String((int)emergencyEspNowRssi());
  json += ",\n  \"firmware\": \"" SHOWDUINO_EMERGENCY_NODE_FW "\"";
  json += ",\n  \"mac\": \"";
  json += mac;
  json += "\",\n  \"gpioVerified\": ";
  json += SHOWDUINO_EMERGENCY_NODE_GPIO_VERIFIED ? "true" : "false";
  json += ",\n  \"canClearGlobal\": false\n}\n";
  sServer.send(200, "application/json", json);
}

static void addRoutes() {
  if (sRoutes) return;
  sRoutes = true;
  sServer.on("/", HTTP_GET, []() {
    sServer.send_P(200, "text/html", kPage);
  });
  sServer.on("/api/status", HTTP_GET, sendStatus);
  sServer.on("/api/id", HTTP_POST, []() {
    const String body = sServer.arg("plain");
    auto take = [&](const char *key, char *out, size_t n) {
      const int i = body.indexOf(key);
      if (i < 0) return;
      int q = body.indexOf('"', i + (int)strlen(key));
      if (q < 0) return;
      int q2 = body.indexOf('"', q + 1);
      if (q2 < 0) return;
      String v = body.substring(q + 1, q2);
      strncpy(out, v.c_str(), n - 1);
      out[n - 1] = 0;
    };
    char id[16] = "";
    char name[24] = "";
    take("\"id\"", id, sizeof(id));
    take("\"name\"", name, sizeof(name));
    if (id[0]) emergencyIdentitySetId(id);
    if (name[0]) emergencyIdentitySetName(name);
    emergencyProtocolAnnounce();
    sendStatus();
  });
  sServer.on("/api/rearm", HTTP_POST, []() {
    emergencyProtocolTryLocalRearm();
    sendStatus();
  });
  sServer.on("/api/clear", HTTP_ANY, []() {
    sServer.send(403, "application/json",
                 "{\"error\":\"NO_CLEAR\",\"note\":\"Emergency Node cannot clear P4 emergency\"}\n");
  });
}

void emergencyWebEnsure() {
  nodeSoftApSetRadioHook(emergencyEspNowReassert);
  if (!nodeSoftApStarted()) {
    char ssid[33];
    snprintf(ssid, sizeof(ssid), "Showduino-EStop-%s", emergencyIdentityId());
    if (!nodeSoftApBeginNamed(ssid, emergencyEspNowChannel(),
                              SHOWDUINO_ESTOP_NODE_AP_PASSWORD)) {
      return;
    }
  }
  addRoutes();
  if (!sBegun) {
    sServer.begin();
    sBegun = true;
    Serial.printf("[ESTOP-WEB] http://%s  ssid=%s\n", nodeSoftApIp(),
                  nodeSoftApSsid());
  }
}

void emergencyWebService() {
  if (nodeSoftApStarted() && WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
    nodeSoftApService();
  }
  if (sBegun) sServer.handleClient();
}

bool emergencyWebReady() { return sBegun && nodeSoftApReady(); }
