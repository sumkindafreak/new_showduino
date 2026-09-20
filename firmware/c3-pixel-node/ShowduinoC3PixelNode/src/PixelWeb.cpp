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
<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Showduino Pixel Node</title>
<style>
*{box-sizing:border-box}body{margin:0;background:#0d0e12;color:#e7e7ec;font:15px/1.45 system-ui,sans-serif}header{padding:16px 18px;background:#15161c;border-bottom:2px solid #7a3cff;position:sticky;top:0;z-index:2}h1{margin:0;font-size:19px}#owner{margin-top:5px;color:#aaa;font-size:12px}.wrap{max-width:820px;margin:auto}nav{display:flex;gap:6px;overflow:auto;padding:10px 12px;background:#111218;position:sticky;top:68px;z-index:2}nav button{white-space:nowrap;background:#1e2028;color:#aaa;border:1px solid #30323d;padding:9px 11px;border-radius:6px}nav button.on{color:#fff;border-color:#7a3cff;background:#272033}main{padding:12px}.card{background:#171820;border:1px solid #292b35;border-radius:10px;padding:14px;margin-bottom:12px}.card h2{font-size:15px;margin:0 0 10px}.grid{display:grid;grid-template-columns:140px 1fr;gap:5px 10px;font-size:13px}.grid div:nth-child(odd){color:#9295a3}.banner{padding:10px 14px;background:#342050;border-bottom:1px solid #5d3990}.banner.bad{background:#5a1020}.note{color:#aeb0ba;font-size:13px}.warn{color:#ffc66d}.ok{color:#78d98b}.err{color:#ff7777}label{display:block;color:#aaa;margin:11px 0 5px}input,select{width:100%;background:#0d0e12;color:#fff;border:1px solid #3a3d49;border-radius:6px;padding:10px;font:inherit}input:focus,select:focus{outline:2px solid #7a3cff;border-color:transparent}.row{display:flex;gap:8px;flex-wrap:wrap;margin-top:12px}.act{background:#292b35;color:#fff;border:1px solid #424552;border-radius:6px;padding:10px 13px}.primary{background:#6230b5;border-color:#8d5ee0}.act:disabled{opacity:.38}.saveState{min-height:20px;margin-top:8px;font-size:13px}.pill{display:inline-block;padding:2px 7px;border-radius:10px;background:#292b35;font-size:11px;margin-left:5px}
</style></head><body>
<header><div class="wrap"><h1 id="title">Showduino Pixel Node</h1><div id="owner">Loading…</div></div></header>
<div id="banner" class="banner" hidden></div>
<nav class="wrap"><button data-t="node" class="on">NODE</button><button data-t="line">PIXEL LINE</button><button data-t="seg">SEGMENTS</button><button data-t="test">TEST</button><button data-t="conn">COMMS</button><button data-t="sys">SYSTEM</button></nav>
<main class="wrap">
<section class="tab" id="node"><div class="card"><h2>Identity <span class="pill">safe while show-controlled</span></h2><div class="grid" id="nodekv"></div><label>Node ID</label><input id="nid" maxlength="12" autocomplete="off" autocapitalize="characters" spellcheck="false"><label>Friendly name</label><input id="nname" maxlength="20" autocomplete="off" spellcheck="false"><div class="row"><button class="act primary" id="saveid">SAVE IDENTITY</button></div><div id="idsave" class="saveState"></div></div></section>
<section class="tab" id="line" hidden><div class="card"><h2>Pixel line</h2><div class="grid" id="linekv"></div><label>Line pixels</label><input id="count" type="number" min="1" max="512" inputmode="numeric" placeholder="Not configured"><div class="row"><button class="act primary" id="savecount">SAVE COUNT</button><button class="act" id="initline">INITIALISE LINE</button></div><p class="note">Save Count only stores the physical line length. Initialise Line starts GPIO2. A fresh node stays at 0 until you explicitly enter a count.</p></div></section>
<section class="tab" id="seg" hidden><div class="card"><h2>Segments</h2><label>Segment 0–15</label><input id="segid" type="number" min="0" max="15" value="0"><label>Start pixel</label><input id="segstart" type="number" min="0" value="0"><label>Length</label><input id="segcount" type="number" min="1" value="10"><div class="row"><button class="act" id="segrange">SET RANGE</button><button class="act" id="segstop">STOP SEGMENT</button></div><p class="note">Show-controlled nodes lock theatrical changes. Identity and commissioning data remain visible.</p></div></section>
<section class="tab" id="test" hidden><div class="card"><h2>Local test</h2><label>FX</label><select id="fx"></select><label>RGB</label><div class="row"><input id="r" type="number" min="0" max="255" value="255" style="width:30%"><input id="g" type="number" min="0" max="255" value="0" style="width:30%"><input id="b" type="number" min="0" max="255" value="0" style="width:30%"></div><label>Brightness <span id="briv"></span></label><input id="bri" type="range" min="0" max="255"><div class="row"><button class="act" id="solid">SOLID</button><button class="act" id="runfx">RUN FX</button><button class="act" id="locate">LOCATE</button><button class="act" id="blackout">BLACKOUT</button><button class="act" id="linetest">LINE TEST</button></div></div></section>
<section class="tab" id="conn" hidden><div class="card"><h2>Communications</h2><div class="grid" id="connkv"></div></div></section>
<section class="tab" id="sys" hidden><div class="card"><h2>System</h2><div class="grid" id="syskv"></div><div class="row"><button class="act" id="reboot">REBOOT</button><button class="act" id="defaults">SAFE DEFAULTS</button></div><p class="note">Emergency can only be cleared by the authoritative P4.</p></div></section>
</main>
<script>
const $=id=>document.getElementById(id);
const FX=['OFF','SOLID','FADE_IN','FADE_OUT','PULSE','BREATHE','FLICKER','CANDLE','FIRE','LIGHTNING','STROBE','RANDOM_STROBE','CHASE','BOUNCE','COMET','WIPE','REVERSE_WIPE','BUILD','SPARKLE','TWINKLE','GLITCH','WARNING','PORTAL','RAINBOW','CUSTOM_SEQUENCE'];
FX.forEach(n=>{const o=document.createElement('option');o.value=n;o.textContent=n;$('fx').appendChild(o)});
let S=null,loading=false;
const dirty=new Set();
['nid','nname','count','bri'].forEach(id=>{$(id).addEventListener('input',()=>dirty.add(id));$(id).addEventListener('change',()=>dirty.add(id))});
function put(id,v){if(!dirty.has(id)&&document.activeElement!==$(id))$(id).value=v}
function kv(el,rows){el.innerHTML=rows.map(r=>'<div>'+r[0]+'</div><div class="'+(r[2]||'')+'">'+r[1]+'</div>').join('')}
async function load(force=false){
 if(loading)return; loading=true;
 try{
  S=await (await fetch('/api/status',{cache:'no-store'})).json();
  const owned=S.owner==='SHOW_CONTROLLED',em=!!S.emergency,ready=!!S.initialised;
  $('title').textContent=(S.id||'PIXEL')+' — '+(S.name||'Pixel Node');
  $('owner').textContent=S.owner+' · '+(ready?'READY':'NOT INITIALISED');
  const ban=$('banner');
  if(em){ban.hidden=false;ban.className='banner bad';ban.textContent='EMERGENCY — entire line bright white. WebUI cannot clear it.'}
  else if(owned){ban.hidden=false;ban.className='banner';ban.textContent='CONTROLLED BY SHOWDUINO — live theatrical controls are locked; commissioning fields are protected while you edit.'}
  else ban.hidden=true;
  document.querySelectorAll('button.act').forEach(b=>{const keep=['saveid','reboot','savecount','initline'].includes(b.id);const theatrical=['solid','runfx','locate','blackout','linetest','segrange','segstop'].includes(b.id);b.disabled=em||(owned&&!keep)||(theatrical&&!ready)});
  kv($('nodekv'),[['Node ID',S.id],['Name',S.name],['Showduino',S.product],['Pixel FW',S.fw],['Protocol',S.protocol],['MAC',S.mac],['Uptime',S.uptime+' s']]);
  kv($('linekv'),[['Configured',S.configured||'NOT SET'],['Active',S.count],['Initialised',ready?'YES':'NO',ready?'ok':'warn'],['Maximum',S.max],['Brightness',S.brightness],['Segments',S.segments]]);
  kv($('connkv'),[['ESP-NOW',S.espNow],['Channel',S.channel],['RSSI',S.rssi],['SSID',S.ssid],['IP',S.ip],['Granted',S.granted?'YES':'NO']]);
  kv($('syskv'),[['State',S.state],['Fault',S.fault||'NONE'],['OLED','GPIO5/6 · 0x3C'],['Pixel DATA','GPIO'+S.gpio]]);
  if(force){dirty.clear()}
  put('nid',S.id||'');put('nname',S.name||'');put('count',S.configured>0?S.configured:'');$('count').max=S.max||512;put('bri',S.brightness);$('briv').textContent=S.brightness;
 }catch(e){$('owner').textContent='Status unavailable'}finally{loading=false}
}
document.querySelectorAll('nav button').forEach(b=>b.onclick=()=>{document.querySelectorAll('nav button').forEach(x=>x.classList.toggle('on',x===b));document.querySelectorAll('.tab').forEach(t=>t.hidden=t.id!==b.dataset.t)});
async function send(cmd){const r=await (await fetch('/api/command',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({command:cmd})})).json();if(!r.ok)alert(r.message||r.error||'Rejected');await load();return r}
$('saveid').onclick=async()=>{const id=$('nid').value.trim(),name=$('nname').value.trim(),msg=$('idsave');msg.className='saveState';msg.textContent='Saving…';try{const r=await (await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({id:id,name:name})})).json();if(!r.ok)throw new Error(r.error||'Save failed');dirty.delete('nid');dirty.delete('nname');msg.className='saveState ok';msg.textContent='Identity saved';await load(true)}catch(e){msg.className='saveState err';msg.textContent=e.message}};
$('savecount').onclick=async()=>{const v=$('count').value;if(!v){alert('Enter the actual physical pixel count first.');return}await send('PIXEL:COUNT:'+v);dirty.delete('count');await load(true)};
$('initline').onclick=()=>send('PIXEL:INIT');$('segrange').onclick=()=>send('PIXEL:SEGMENT:'+$('segid').value+':RANGE:'+$('segstart').value+':'+$('segcount').value);$('segstop').onclick=()=>send('PIXEL:SEGMENT:'+$('segid').value+':STOP');$('solid').onclick=()=>send('PIXEL:SOLID:'+$('r').value+':'+$('g').value+':'+$('b').value);
$('runfx').onclick=async()=>{const id=$('segid').value;await send('PIXEL:SEGMENT:'+id+':RANGE:'+$('segstart').value+':'+$('segcount').value);await send('PIXEL:SEGMENT:'+id+':FX:'+$('fx').value);await send('PIXEL:SEGMENT:'+id+':COLOR:'+$('r').value+':'+$('g').value+':'+$('b').value);await send('PIXEL:SEGMENT:'+id+':START')};
$('locate').onclick=()=>send('PIXEL:LOCATE');$('blackout').onclick=()=>send('PIXEL:BLACKOUT');$('linetest').onclick=()=>send('PIXEL:TEST');$('bri').onchange=async()=>{await send('PIXEL:BRIGHTNESS:'+$('bri').value);dirty.delete('bri')};$('reboot').onclick=()=>fetch('/api/reboot',{method:'POST'});$('defaults').onclick=()=>fetch('/api/defaults',{method:'POST'}).then(()=>load(true));
load(true);setInterval(()=>load(false),2000);
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
  // Physical commissioning remains available while Showduino owns the node.
  // Theatrical commands stay locked so the local WebUI cannot fight the live show.
  const bool commissioningCommand =
      !strncmp(cmd, "PIXEL:COUNT:", 12) ||
      !strcmp(cmd, "PIXEL:INIT");

  if (pixelNodeStateShowControlled() &&
      !commissioningCommand &&
      showduino_pixel_cmd_theatrical(showduino_pixel_classify_command(cmd))) {
    sServer.send(200, "application/json",
                 "{\"ok\":false,\"error\":\"SHOW_CONTROLLED\",\"message\":\"CONTROLLED BY SHOWDUINO\"}");
    return;
  }

  pixelProtocolApply(cmd, 0, SHOWDUINO_CMD_ORIGIN_WEB);
  if (pixelEngineEmergency()) {
    sServer.send(200, "application/json",
                 "{\"ok\":false,\"error\":\"EMERGENCY\",\"message\":\"EMERGENCY ACTIVE\"}");
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
