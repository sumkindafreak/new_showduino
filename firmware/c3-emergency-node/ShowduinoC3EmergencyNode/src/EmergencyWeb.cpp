#include "EmergencyWeb.h"

#include <WebServer.h>
#include <WiFi.h>

#include "EmergencyIdentity.h"
#include "EmergencyProtocol.h"
#include "EmergencyInput.h"
#include "EmergencyDisplay.h"
#include "EstopPixelEngine.h"
#include "EstopPixelProtocol.h"
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
<!DOCTYPE html><html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Showduino ESTOP+Pixel</title>
<style>
body{margin:0;background:#111;color:#ddd;font:14px/1.3 sans-serif}
header{padding:10px 14px;background:#1a1a1a;border-bottom:2px solid #c33}
h1{margin:0;font-size:17px}h2{margin:0 0 8px;font-size:14px;color:#aaa}
.card{background:#1c1c1c;border:1px solid #2a2a2a;border-radius:6px;padding:10px;margin:10px}
.kv{display:grid;grid-template-columns:140px 1fr;gap:3px 8px;font-size:12px}
label{display:block;margin:6px 0 2px;color:#aaa;font-size:12px}
input,select{background:#111;color:#fff;border:1px solid #444;padding:6px;width:100%;box-sizing:border-box}
button{background:#2a2a2a;color:#fff;border:1px solid #444;padding:8px 10px;border-radius:3px;margin:6px 6px 0 0}
.warn{color:#fc6}.err{color:#f66}.ok{color:#6c6}
.row{display:flex;flex-wrap:wrap;gap:6px;align-items:end}
.row>div{flex:1;min-width:90px}
</style></head><body>
<header><h1 id=title>Emergency + Pixel</h1></header>
<div class=card><div class=kv id=kv></div>
<label>Logical ID</label><input id=nid maxlength=12>
<label>Friendly location</label><input id=nname maxlength=20>
<button id=save>SAVE IDENTITY</button>
<button id=rearm>MAINTENANCE LOCAL RESET</button>
<p class=warn>ASSERT ONLY — cannot clear P4. Release never clears. Pixel = capability of this ESTOP peer (GPIO2), not a fake LED-xx.</p>
</div>
<div class=card><h2>PIXELS (GPIO2)</h2><div class=kv id=pkv></div>
<div class=row>
<div><label>Count</label><input id=pcnt type=number min=1 max=512></div>
<div><label>Brightness</label><input id=pbri type=number min=0 max=255></div>
</div>
<button id=pinit>INIT</button><button id=ptest>TEST</button><button id=pstop>TEST STOP</button>
<button id=poff>OFF</button><button id=ploc>LOCATE</button>
<div class=row>
<div><label>Seg</label><input id=sid type=number min=0 max=15 value=0></div>
<div><label>Start</label><input id=sst type=number min=0 value=0></div>
<div><label>Count</label><input id=scn type=number min=1 value=30></div>
</div>
<div class=row>
<div><label>FX</label><select id=sfx>
<option>OFF</option><option>SOLID</option><option>FADE_IN</option><option>FADE_OUT</option>
<option>PULSE</option><option>BREATHE</option><option>FLICKER</option><option>CANDLE</option>
<option>FIRE</option><option>LIGHTNING</option><option>STROBE</option><option>RANDOM_STROBE</option>
<option>CHASE</option><option>BOUNCE</option><option>COMET</option><option>WIPE</option>
<option>REVERSE_WIPE</option><option>BUILD</option><option>SPARKLE</option><option>TWINKLE</option>
<option>GLITCH</option><option>WARNING</option><option>PORTAL</option><option>RAINBOW</option>
<option>CUSTOM_SEQUENCE</option>
</select></div>
<div><label>R</label><input id=cr type=number min=0 max=255 value=255></div>
<div><label>G</label><input id=cg type=number min=0 max=255 value=80></div>
<div><label>B</label><input id=cb type=number min=0 max=255 value=0></div>
</div>
<div class=row>
<div><label>R2</label><input id=c2r type=number min=0 max=255 value=0></div>
<div><label>G2</label><input id=c2g type=number min=0 max=255 value=0></div>
<div><label>B2</label><input id=c2b type=number min=0 max=255 value=0></div>
<div><label>Speed</label><input id=spd type=number min=1 max=100 value=50></div>
<div><label>Int</label><input id=inten type=number min=0 max=100 value=80></div>
</div>
<button id=prange>RANGE</button><button id=pfx>SET FX</button><button id=pcol>COLOR</button>
<button id=pcol2>COLOR2</button><button id=psegstop>SEG STOP</button>
</div>
<script>
const $=id=>document.getElementById(id);
async function pix(cmd){await fetch('/api/pixel',{method:'POST',headers:{'Content-Type':'text/plain'},body:cmd});load()}
async function load(){
  const S=await(await fetch('/api/status')).json();
  $('title').textContent=(S.id||'ESTOP')+' — '+(S.name||'Emergency');
  $('nid').value=S.id||'';$('nname').value=S.name||'';
  $('pcnt').value=S.pixelCount||0;$('pbri').value=S.pixelBrightness||255;
  const rows=[
    ['Emergency pushbutton',S.input||'—',S.input==='PRESSED'?'err':'ok'],
    ['Local latch',S.latched?'YES':'NO',S.latched?'err':'ok'],
    ['Emergency',S.latched?'ASSERTED':'READY',S.latched?'err':'ok'],
    ['Radio',S.radio?'LINKED':'SEARCHING',S.radio?'ok':'warn'],
    ['OLED',S.oled||'—',S.oled==='READY'?'ok':'warn'],
    ['Firmware',S.firmware||'—','']
  ];
  $('kv').innerHTML=rows.map(r=>`<div>${r[0]}</div><div class="${r[2]||''}">${r[1]}</div>`).join('');
  const prow=[
    ['Pixel GPIO',String(S.pixelGpio||2),''],
    ['Configured count',String(S.pixelCount||0),''],
    ['Initialised',S.pixelReady?'YES':'NO',S.pixelReady?'ok':'warn'],
    ['Active segments',String(S.pixelSegments||0),''],
    ['Brightness',String(S.pixelBrightness||0),''],
    ['Emergency override',S.pixelEmergency?'YES':'NO',S.pixelEmergency?'err':'ok']
  ];
  $('pkv').innerHTML=prow.map(r=>`<div>${r[0]}</div><div class="${r[2]||''}">${r[1]}</div>`).join('');
}
$('save').onclick=async()=>{await fetch('/api/id',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({id:$('nid').value,name:$('nname').value})});load()};
$('rearm').onclick=async()=>{await fetch('/api/rearm',{method:'POST'});load()};
$('pinit').onclick=()=>pix('PIXEL:COUNT:'+$('pcnt').value).then(()=>pix('PIXEL:INIT'));
$('ptest').onclick=()=>pix('PIXEL:TEST');
$('pstop').onclick=()=>pix('PIXEL:TEST:STOP');
$('poff').onclick=()=>pix('PIXEL:OFF');
$('ploc').onclick=()=>pix('PIXEL:LOCATE');
$('pbri').onchange=()=>pix('PIXEL:BRIGHTNESS:'+$('pbri').value);
$('prange').onclick=()=>pix('PIXEL:SEGMENT:'+$('sid').value+':RANGE:'+$('sst').value+':'+$('scn').value);
$('pfx').onclick=()=>pix('PIXEL:SEGMENT:'+$('sid').value+':FX:'+$('sfx').value);
$('pcol').onclick=()=>pix('PIXEL:SEGMENT:'+$('sid').value+':COLOR:'+$('cr').value+':'+$('cg').value+':'+$('cb').value);
$('pcol2').onclick=()=>pix('PIXEL:SEGMENT:'+$('sid').value+':COLOR2:'+$('c2r').value+':'+$('c2g').value+':'+$('c2b').value);
$('psegstop').onclick=()=>pix('PIXEL:SEGMENT:'+$('sid').value+':STOP');
load();setInterval(load,2000);
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
  json += ",\n  \"canClearGlobal\": false";
  json += ",\n  \"pixelGpio\": ";
  json += String(SHOWDUINO_ESTOP_PIXEL_DATA_PIN);
  json += ",\n  \"pixelCount\": ";
  json += String((unsigned)pixelEngineConfiguredCount());
  json += ",\n  \"pixelReady\": ";
  json += pixelEngineReady() ? "true" : "false";
  json += ",\n  \"pixelSegments\": ";
  json += String((unsigned)pixelEngineActiveSegments());
  json += ",\n  \"pixelBrightness\": ";
  json += String((unsigned)pixelEngineGlobalBrightness());
  json += ",\n  \"pixelEmergency\": ";
  json += pixelEngineEmergency() ? "true" : "false";
  json += ",\n  \"capabilities\": \"" SHOWDUINO_EMERGENCY_CAPS "\"\n}\n";
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
  sServer.on("/api/pixel", HTTP_POST, []() {
    String body = sServer.arg("plain");
    body.trim();
    if (!body.length()) {
      sServer.send(400, "application/json", "{\"error\":\"EMPTY\"}\n");
      return;
    }
    emergencyProtocolApply(body.c_str(), 0);
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
