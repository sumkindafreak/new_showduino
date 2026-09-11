#include "PixelWeb.h"

#include <WebServer.h>
#include <WiFi.h>

#include "PixelEngine.h"
#include "PixelNodeState.h"
#include "PixelIdentity.h"
#include "PixelProtocol.h"
#include "EspNowPixelTransport.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_pixel_node.h"
#include "../../../protocol/showduino_version.h"
#include "../../../protocol/showduino_protocol_version.h"
#include "../../shared-node/NodeConfig.h"
#include "../../shared-node/NodeSoftAp.h"

static WebServer sServer(80);
static bool sRoutes = false;
static bool sBegun = false;

static const char kPage[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Showduino Pixel Node</title>
<style>
body{margin:0;background:#111;color:#ddd;font:15px/1.35 sans-serif}
header{padding:12px 16px;background:#1a1a1a;border-bottom:2px solid #7a3cff}
h1{margin:0;font-size:18px;letter-spacing:.04em}
#owner{font-size:12px;color:#aaa;margin-top:4px}
nav{display:flex;flex-wrap:wrap;gap:6px;padding:10px 12px;background:#181818}
nav button{background:#222;color:#ccc;border:1px solid #333;padding:8px 10px;border-radius:4px}
nav button.on{border-color:#7a3cff;color:#fff}
main{padding:12px 16px 24px}
.card{background:#1c1c1c;border:1px solid #2a2a2a;border-radius:8px;padding:12px;margin:0 0 12px}
.row{display:flex;gap:8px;flex-wrap:wrap;margin:8px 0}
button.act{background:#2a2a2a;color:#fff;border:1px solid #444;padding:10px 14px;border-radius:4px}
button.act:disabled{opacity:.45}
.banner{background:#3a2060;color:#fff;padding:10px;border-radius:6px;margin:0 0 12px}
.bad{background:#5a1020}
label{display:block;margin:8px 0 4px;color:#aaa}
input,select{background:#111;color:#fff;border:1px solid #444;padding:8px;width:100%;box-sizing:border-box}
.kv{display:grid;grid-template-columns:140px 1fr;gap:4px 10px;font-size:13px}
.ok{color:#6c6}.warn{color:#fc6}.err{color:#f66}
</style></head><body>
<header><h1 id="title">Showduino Pixel Node</h1><div id="owner">…</div></header>
<nav>
<button data-t="node" class="on">NODE</button>
<button data-t="line">PIXEL LINE</button>
<button data-t="seg">SEGMENTS</button>
<button data-t="test">TEST</button>
<button data-t="conn">COMMS</button>
<button data-t="sys">SYSTEM</button>
</nav>
<div id="banner" class="banner" hidden></div>
<main>
<section class="tab" id="node"><div class="card"><div class="kv" id="nodekv"></div>
<label>Node ID</label><input id="nid" maxlength="12">
<label>Friendly name</label><input id="nname" maxlength="20">
<div class="row"><button class="act" id="saveid">SAVE IDENTITY</button></div>
</div></section>
<section class="tab" id="line" hidden><div class="card"><div class="kv" id="linekv"></div>
<label>Line pixels</label><input id="count" type="number" min="1" max="512">
<div class="row">
<button class="act" id="savecount">SAVE COUNT</button>
<button class="act" id="initline">INITIALISE LINE</button>
</div>
<p class="warn">Save Count persists length and does not light the strip. Initialise Line starts the driver.</p>
</div></section>
<section class="tab" id="seg" hidden><div class="card">
<label>Segment 0–15</label><input id="segid" type="number" min="0" max="15" value="0">
<label>Start</label><input id="segstart" type="number" min="0" value="0">
<label>Count</label><input id="segcount" type="number" min="1" value="10">
<div class="row"><button class="act" id="segrange">SET RANGE</button>
<button class="act" id="segstop">STOP SEGMENT</button></div>
<p>Overlap is allowed. Later slot IDs win, matching P4 GPIO23.</p>
</div></section>
<section class="tab" id="test" hidden><div class="card">
<label>FX</label>
<select id="fx"></select>
<label>RGB</label>
<div class="row">
<input id="r" type="number" min="0" max="255" value="255" style="width:30%">
<input id="g" type="number" min="0" max="255" value="0" style="width:30%">
<input id="b" type="number" min="0" max="255" value="0" style="width:30%">
</div>
<label>Brightness <span id="briv"></span></label>
<input id="bri" type="range" min="0" max="255">
<div class="row">
<button class="act" id="solid">SOLID</button>
<button class="act" id="runfx">RUN FX</button>
<button class="act" id="locate">LOCATE</button>
<button class="act" id="blackout">BLACKOUT</button>
<button class="act" id="linetest">LINE TEST</button>
</div>
</div></section>
<section class="tab" id="conn" hidden><div class="card"><div class="kv" id="connkv"></div></div></section>
<section class="tab" id="sys" hidden><div class="card"><div class="kv" id="syskv"></div>
<div class="row">
<button class="act" id="reboot">REBOOT</button>
<button class="act" id="defaults">SAFE DEFAULTS</button>
</div>
<p>This page cannot clear Showduino emergency. That remains a P4 authority action.</p>
</div></section>
</main>
<script>
const $=id=>document.getElementById(id);
const FX=['OFF','SOLID','FADE_IN','FADE_OUT','PULSE','BREATHE','FLICKER','CANDLE','FIRE','LIGHTNING','STROBE','RANDOM_STROBE','CHASE','BOUNCE','COMET','WIPE','REVERSE_WIPE','BUILD','SPARKLE','TWINKLE','GLITCH','WARNING','PORTAL','RAINBOW','CUSTOM_SEQUENCE'];
FX.forEach(n=>{const o=document.createElement('option');o.value=n;o.textContent=n;$('fx').appendChild(o);});
let S=null;
function kv(el,rows){el.innerHTML=rows.map(r=>`<div>${r[0]}</div><div class="${r[2]||''}">${r[1]}</div>`).join('')}
async function load(){
  S=await (await fetch('/api/status')).json();
  const owned=S.owner==='SHOW_CONTROLLED';
  const em=!!S.emergency;
  const ready=!!S.initialised;
  $('title').textContent=(S.id||'PIXEL')+' — '+(S.name||'Pixel Node');
  $('owner').textContent=S.owner+' · '+(ready?'READY':'NOT INITIALISED');
  const ban=$('banner');
  if(em){ban.hidden=false;ban.className='banner bad';ban.textContent='EMERGENCY — entire line bright white. Webpage cannot clear this.';}
  else if(owned){ban.hidden=false;ban.className='banner';ban.textContent='CONTROLLED BY SHOWDUINO';}
  else {ban.hidden=true;}
  document.querySelectorAll('button.act').forEach(b=>{
    const keep=b.id==='saveid'||b.id==='reboot'||b.id==='defaults'||b.id==='savecount'||b.id==='initline';
    b.disabled=em||(owned&&!keep)||((b.id==='solid'||b.id==='runfx'||b.id==='locate'||b.id==='blackout'||b.id==='linetest'||b.id==='segrange'||b.id==='segstop')&&!ready);
  });
  kv($('nodekv'),[
    ['Node ID',S.id],['Name',S.name],['Showduino',S.product],
    ['Pixel FW',S.fw],['Protocol',S.protocol],['MAC',S.mac],['Uptime',S.uptime+' s']
  ]);
  kv($('linekv'),[
    ['Line pixels',S.configured],['Active',S.count],
    ['Initialised',ready?'YES':'NOT INITIALISED',ready?'ok':'warn'],
    ['Max',S.max],['Brightness',S.brightness],['Segments',S.segments]
  ]);
  kv($('connkv'),[
    ['ESP-NOW',S.espNow],['Channel',S.channel],['RSSI',S.rssi],
    ['SSID',S.ssid],['IP',S.ip],['Granted',S.granted?'YES':'no']
  ]);
  kv($('syskv'),[
    ['State',S.state],['Fault',S.fault],['OLED','GPIO5/6 0x3C'],
    ['Pixel DATA','GPIO'+S.gpio]
  ]);
  $('nid').value=S.id||''; $('nname').value=S.name||'';
  $('count').value=S.configured||100; $('count').max=S.max||512;
  $('bri').value=S.brightness; $('briv').textContent=S.brightness;
}
document.querySelectorAll('nav button').forEach(b=>b.onclick=()=>{
  document.querySelectorAll('nav button').forEach(x=>x.classList.toggle('on',x===b));
  document.querySelectorAll('.tab').forEach(t=>t.hidden=t.id!==b.dataset.t);
});
async function send(cmd){
  const r=await (await fetch('/api/command',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({command:cmd})})).json();
  if(!r.ok) alert(r.message||r.error||'rejected');
  load();
}
$('saveid').onclick=async()=>{
  await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({id:$('nid').value,name:$('nname').value})});
  load();
};
$('savecount').onclick=()=>send('PIXEL:COUNT:'+$('count').value);
$('initline').onclick=()=>send('PIXEL:INIT');
$('segrange').onclick=()=>send('PIXEL:SEGMENT:'+$('segid').value+':RANGE:'+$('segstart').value+':'+$('segcount').value);
$('segstop').onclick=()=>send('PIXEL:SEGMENT:'+$('segid').value+':STOP');
$('solid').onclick=()=>send('PIXEL:SOLID:'+$('r').value+':'+$('g').value+':'+$('b').value);
$('runfx').onclick=()=>{
  const id=$('segid').value;
  send('PIXEL:SEGMENT:'+id+':RANGE:'+$('segstart').value+':'+$('segcount').value).then(()=>
  send('PIXEL:SEGMENT:'+id+':FX:'+$('fx').value)).then(()=>
  send('PIXEL:SEGMENT:'+id+':COLOR:'+$('r').value+':'+$('g').value+':'+$('b').value)).then(()=>
  send('PIXEL:SEGMENT:'+id+':START'));
};
$('locate').onclick=()=>send('PIXEL:LOCATE');
$('blackout').onclick=()=>send('PIXEL:BLACKOUT');
$('linetest').onclick=()=>send('PIXEL:TEST');
$('bri').onchange=()=>send('PIXEL:BRIGHTNESS:'+$('bri').value);
$('reboot').onclick=()=>fetch('/api/reboot',{method:'POST'});
$('defaults').onclick=()=>fetch('/api/defaults',{method:'POST'}).then(load);
load(); setInterval(load,1500);
</script></body></html>
)HTML";

static void jsonEsc(const char *in, String &out) {
  for (; in && *in; ++in) {
    if (*in == '"' || *in == '\\') {
      out += '\\';
      out += *in;
    } else if ((uint8_t)*in < 32) {
      continue;
    } else {
      out += *in;
    }
  }
}

static bool extractJsonString(const String &body, const char *key, char *out, size_t n) {
  if (!key || !out || n < 2) return false;
  out[0] = 0;
  const String needle = String("\"") + key + "\"";
  int at = body.indexOf(needle);
  if (at < 0) return false;
  at = body.indexOf(':', at);
  if (at < 0) return false;
  at = body.indexOf('"', at);
  if (at < 0) return false;
  int end = body.indexOf('"', at + 1);
  if (end < 0) return false;
  String v = body.substring(at + 1, end);
  v.replace("\\\"", "\"");
  strncpy(out, v.c_str(), n - 1);
  out[n - 1] = 0;
  return out[0] != 0;
}

static void handleRoot() { sServer.send_P(200, "text/html", kPage); }

static void handleStatus() {
  char mac[24];
  pixelEspNowMacString(mac, sizeof(mac));
  String json = "{";
  json += "\"id\":\"";
  jsonEsc(pixelIdentityId(), json);
  json += "\",\"name\":\"";
  jsonEsc(pixelIdentityName(), json);
  json += "\",\"product\":\"";
  json += SHOWDUINO_PLATFORM_VERSION;
  json += "\",\"fw\":\"";
  json += SHOWDUINO_PIXEL_NODE_FW;
  json += "\",\"protocol\":\"";
  json += SHOWDUINO_PIXEL_PROTOCOL;
  json += "\",\"owner\":\"";
  json += pixelOwnerModeName();
  json += "\",\"state\":\"";
  json += pixelNodeStateName();
  json += "\",\"configured\":";
  json += String((unsigned)pixelEngineConfiguredCount());
  json += ",\"count\":";
  json += String((unsigned)pixelEngineCount());
  json += ",\"max\":";
  json += String((unsigned)pixelEngineMax());
  json += ",\"initialised\":";
  json += pixelEngineReady() ? "true" : "false";
  json += ",\"brightness\":";
  json += String((unsigned)pixelEngineGlobalBrightness());
  json += ",\"segments\":";
  json += String((unsigned)pixelEngineActiveSegments());
  json += ",\"emergency\":";
  json += pixelEngineEmergency() ? "true" : "false";
  json += ",\"ssid\":\"";
  jsonEsc(nodeSoftApSsid(), json);
  json += "\",\"ip\":\"";
  json += nodeSoftApIp();
  json += "\",\"channel\":";
  json += String((unsigned)pixelEspNowChannel());
  json += ",\"rssi\":";
  json += String((int)pixelEspNowRssi());
  json += ",\"espNow\":\"";
  json += pixelEspNowHaveComms() ? "heard" : "searching";
  json += "\",\"granted\":";
  json += pixelOwnerGranted() ? "true" : "false";
  json += ",\"mac\":\"";
  json += mac;
  json += "\",\"gpio\":";
  json += String(pixelEnginePin());
  json += ",\"fault\":\"";
  jsonEsc(pixelNodeStateFault(), json);
  json += "\",\"uptime\":";
  json += String((unsigned long)(millis() / 1000UL));
  json += "}";
  sServer.send(200, "application/json", json);
}

static void handleConfigPost() {
  const String body = sServer.hasArg("plain") ? sServer.arg("plain") : sServer.arg(0);
  char id[SHOWDUINO_PIXEL_ID_MAX + 1];
  char name[SHOWDUINO_PIXEL_NAME_MAX + 1];
  if (extractJsonString(body, "id", id, sizeof(id))) {
    if (!pixelIdentitySetId(id)) {
      sServer.send(200, "application/json", "{\"ok\":false,\"error\":\"BAD_ID\"}");
      return;
    }
    Serial.println("[PIXEL] CONFIG SAVED");
  }
  if (extractJsonString(body, "name", name, sizeof(name))) {
    if (!pixelIdentitySetName(name)) {
      sServer.send(200, "application/json", "{\"ok\":false,\"error\":\"BAD_NAME\"}");
      return;
    }
    Serial.println("[PIXEL] CONFIG SAVED");
  }
  sServer.send(200, "application/json", "{\"ok\":true}");
}

static void handleCommand() {
  char cmd[SHOWDUINO_NODE_COMMAND_MAX];
  const String body = sServer.hasArg("plain") ? sServer.arg("plain") : sServer.arg(0);
  if (!extractJsonString(body, "command", cmd, sizeof(cmd))) {
    if (sServer.hasArg("command")) {
      strncpy(cmd, sServer.arg("command").c_str(), sizeof(cmd) - 1);
      cmd[sizeof(cmd) - 1] = 0;
    } else {
      sServer.send(200, "application/json", "{\"ok\":false,\"error\":\"BAD_COMMAND\"}");
      return;
    }
  }
  if (!strcmp(cmd, "EMERGENCY:CLEAR") || !strcmp(cmd, "PIXEL:EMERGENCY:CLEAR")) {
    sServer.send(200, "application/json",
                 "{\"ok\":false,\"error\":\"EMERGENCY\",\"message\":\"Webpage cannot clear Showduino emergency\"}");
    return;
  }
  pixelProtocolApply(cmd, 0, SHOWDUINO_CMD_ORIGIN_WEB);
  if (pixelEngineEmergency()) {
    sServer.send(200, "application/json",
                 "{\"ok\":false,\"error\":\"EMERGENCY\",\"message\":\"EMERGENCY ACTIVE\"}");
    return;
  }
  if (pixelNodeStateShowControlled() &&
      showduino_pixel_cmd_theatrical(showduino_pixel_classify_command(cmd))) {
    sServer.send(200, "application/json",
                 "{\"ok\":false,\"error\":\"SHOW_CONTROLLED\",\"message\":\"CONTROLLED BY SHOWDUINO\"}");
    return;
  }
  sServer.send(200, "application/json", "{\"ok\":true}");
}

static void handleReboot() {
  sServer.send(200, "application/json", "{\"ok\":true}");
  delay(50);
  ESP.restart();
}

static void handleDefaults() {
  if (pixelEngineEmergency() || pixelNodeStateShowControlled()) {
    sServer.send(200, "application/json", "{\"ok\":false,\"error\":\"LOCKED\"}");
    return;
  }
  nodeConfigSetU16("pix", 0);
  nodeConfigSetU8("bri", 255);
  pixelEngineRelease();
  Serial.println("[PIXEL] SAFE DEFAULTS — count cleared, NOT INITIALISED");
  sServer.send(200, "application/json", "{\"ok\":true}");
}

static void addRoutes() {
  if (sRoutes) return;
  sServer.on("/", handleRoot);
  sServer.on("/api/status", HTTP_GET, handleStatus);
  sServer.on("/api/config", HTTP_POST, handleConfigPost);
  sServer.on("/api/command", HTTP_POST, handleCommand);
  sServer.on("/api/reboot", HTTP_POST, handleReboot);
  sServer.on("/api/defaults", HTTP_POST, handleDefaults);
  sRoutes = true;
}

void pixelWebEnsure() {
  nodeSoftApSetRadioHook(pixelEspNowReassert);
  if (!nodeSoftApStarted()) {
    uint8_t mac[6];
    pixelEspNowMacBytes(mac);
    if (!nodeSoftApBegin("Pixel", mac, pixelEspNowChannel(),
                         SHOWDUINO_PIXEL_AP_PASSWORD)) {
      return;
    }
  }
  addRoutes();
  if (!sBegun) {
    sServer.begin();
    sBegun = true;
    Serial.printf("[PIXEL-WEB] http://%s  ssid=%s\n", nodeSoftApIp(),
                  nodeSoftApSsid());
  }
}

void pixelWebService() {
  if (nodeSoftApStarted() && WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
    nodeSoftApService();
  }
  if (sBegun) sServer.handleClient();
}

bool pixelWebReady() { return sBegun && nodeSoftApReady(); }
