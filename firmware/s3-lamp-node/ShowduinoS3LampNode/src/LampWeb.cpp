#include "LampWeb.h"

#include <WebServer.h>
#include <WiFi.h>

#include "LampEngine.h"
#include "LampNodeState.h"
#include "LampConfig.h"
#include "LampProtocol.h"
#include "LampSensors.h"
#include "LampAudio.h"
#include "LocalControls.h"
#include "EspNowLampTransport.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_lamp_node.h"
#include "../../../protocol/showduino_carbide_lamp.h"
#include "../../../protocol/showduino_version.h"
#include "../../../protocol/showduino_protocol_version.h"
#include "../../shared-node/NodeSoftAp.h"

static WebServer sServer(80);
static bool sRoutes = false;
static bool sBegun = false;

static const char kPage[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Showduino Lamp Node</title>
<style>
body{margin:0;background:#111;color:#ddd;font:15px/1.35 sans-serif}
header{padding:12px 16px;background:#1a1a1a;border-bottom:2px solid #c46b1a}
h1{margin:0;font-size:18px;letter-spacing:.04em}
.sub{font-size:12px;color:#aaa;margin-top:4px}
nav{display:flex;flex-wrap:wrap;gap:6px;padding:10px 12px;background:#181818}
nav button{background:#222;color:#ccc;border:1px solid #333;padding:8px 10px;border-radius:4px}
nav button.on{border-color:#c46b1a;color:#fff}
main{padding:12px 16px 24px}
.card{background:#1c1c1c;border:1px solid #2a2a2a;border-radius:8px;padding:12px;margin:0 0 12px}
.row{display:flex;gap:8px;flex-wrap:wrap;margin:8px 0}
button.act{background:#2a2a2a;color:#fff;border:1px solid #444;padding:10px 14px;border-radius:4px}
button.act:disabled{opacity:.45}
.banner{background:#3a2060;color:#fff;padding:10px;border-radius:6px;margin:0 0 12px}
.bad{background:#5a1020}
label{display:block;margin:8px 0 4px;color:#aaa}
input{background:#111;color:#fff;border:1px solid #444;padding:8px;width:100%;box-sizing:border-box}
.kv{display:grid;grid-template-columns:170px 1fr;gap:4px 10px;font-size:13px}
.ok{color:#6c6}.warn{color:#fc6}.err{color:#f66}
.live{display:inline-block;min-width:12px;min-height:12px;border-radius:50%;background:#333;margin-right:6px}
.live.on{background:#c46b1a}
</style></head><body>
<header>
<h1>SHOWDUINO LAMP NODE</h1>
<div class="sub">CARBIDE LAMP</div>
<div class="sub" id="owner">OPERATING MODE: …</div>
</header>
<nav>
<button data-t="status" class="on">STATUS</button>
<button data-t="lamp">LAMP</button>
<button data-t="aud">AUDIO</button>
<button data-t="blow">BLOW SENSOR</button>
<button data-t="light">LIGHT SENSOR</button>
<button data-t="volt">VOLTAGE</button>
<button data-t="btn">BUTTON</button>
<button data-t="sys">SYSTEM</button>
</nav>
<div id="banner" class="banner" hidden></div>
<main>
<section class="tab" id="status"><div class="card"><div class="kv" id="statuskv"></div></div></section>
<section class="tab" id="lamp" hidden><div class="card"><div class="kv" id="lampkv"></div>
<div class="row">
<button class="act ctrl" id="ignite">IGNITE</button>
<button class="act ctrl" id="ext">EXTINGUISH</button>
<button class="act ctrl" id="steady">STEADY FLAME</button>
<button class="act ctrl" id="low">LOW FLAME</button>
<button class="act ctrl" id="unst">UNSTABLE FLAME</button>
<button class="act ctrl" id="flare">FLARE</button>
<button class="act ctrl" id="dying">DYING FLAME</button>
</div>
<label>Brightness limit <span id="briv"></span></label>
<input id="bri" type="range" min="1" max="100" value="80">
<div class="row"><button class="act ctrl" id="setbri">SET BRIGHTNESS</button></div>
<p>Standalone WebUI uses the same carbide machine as the physical striker. This page cannot clear emergency.</p>
</div></section>
<section class="tab" id="aud" hidden><div class="card"><div class="kv" id="audkv"></div>
<label>Volume <span id="volv"></span></label>
<input id="vol" type="range" min="0" max="30" value="18">
<div class="row"><button class="act ctrl" id="setvol">SET VOLUME</button></div>
<div class="row">
<button class="act ctrl" id="strike">TEST STRIKE</button>
<button class="act ctrl" id="ignaud">TEST IGNITION</button>
<button class="act ctrl" id="burn">TEST BURN LOOP</button>
<button class="act ctrl" id="flareaud">TEST FLARE</button>
<button class="act ctrl" id="extaud">TEST EXTINGUISH</button>
<button class="act ctrl" id="astop">STOP AUDIO</button>
</div>
<p>Fermion is local lamp FX only. It is not the Showduino Audio Node.</p>
</div></section>
<section class="tab" id="blow" hidden><div class="card"><div class="kv" id="blowkv"></div>
<p><span id="blowdot" class="live"></span><span id="blowlive">BLOW IDLE</span></p>
<label>Threshold</label><input id="bth" type="number" min="8" max="2000">
<label>Puff maximum (ms)</label><input id="puff" type="number" min="20" max="2000">
<label>Minimum sustained duration (ms)</label><input id="blowms" type="number" min="60" max="4000">
<div class="row">
<button class="act" id="saveblow">SAVE BLOW SETTINGS</button>
<button class="act" id="calblow">CALIBRATE QUIET</button>
</div>
</div></section>
<section class="tab" id="light" hidden><div class="card"><div class="kv" id="lightkv"></div>
<div class="row"><button class="act" id="callight">CALIBRATE LIGHT (100%)</button></div>
<p>Normalized light is shown only after calibration. Raw ADC is always available.</p>
</div></section>
<section class="tab" id="volt" hidden><div class="card"><div class="kv" id="voltkv"></div>
<p>Millivolts appear only when a stored scale is valid. Firmware never invents 5.00 V.</p>
</div></section>
<section class="tab" id="btn" hidden><div class="card"><div class="kv" id="btnkv"></div>
<div class="row"><button class="act ctrl" id="btntest">IGNITION TEST</button></div>
</div></section>
<section class="tab" id="sys" hidden><div class="card"><div class="kv" id="syskv"></div>
<label>Node ID</label><input id="nid" maxlength="12">
<label>Friendly name</label><input id="nname" maxlength="20">
<div class="row">
<button class="act" id="saveid">SAVE IDENTITY</button>
<button class="act" id="reboot">REBOOT</button>
</div>
<p class="warn">Do not flash a production lamp until every GPIO is physically traced.</p>
</div></section>
</main>
<script>
const $=id=>document.getElementById(id);
let S=null;
function kv(el,rows){el.innerHTML=rows.map(r=>`<div>${r[0]}</div><div class="${r[2]||''}">${r[1]}</div>`).join('')}
function pin(v){return v<0?'UNCONFIRMED':'GPIO'+v}
async function load(){
  S=await (await fetch('/api/status')).json();
  const may=!!S.webMayControl;
  const em=!!S.emergency;
  const mode=S.operatingMode||'STANDALONE';
  $('owner').textContent='OPERATING MODE: '+mode;
  const ban=$('banner');
  if(em){ban.hidden=false;ban.className='banner bad';ban.textContent='EMERGENCY — jewel forced white. Webpage cannot clear this.';}
  else if(mode==='SHOWDUINO'){ban.hidden=false;ban.className='banner';ban.textContent='SHOWDUINO MODE — P4 authoritative. WebUI is status and commissioning only.';}
  else {ban.hidden=true;}
  document.querySelectorAll('button.ctrl').forEach(b=>b.disabled=!may);
  kv($('statuskv'),[
    ['Operating mode',mode,mode==='SHOWDUINO'?'ok':(em?'err':'warn')],
    ['Lamp Node ID',S.id],['Friendly name',S.name],
    ['Firmware',S.fw],['Showduino',S.product],['Protocol',S.protocol],
    ['Uptime',S.uptime+' s'],['Radio',S.radioStatus],
    ['Showduino connection',S.showduinoConnection],
    ['Ownership',S.owner],['Lamp state',S.carbide],
    ['SSID',S.ssid],['IP',S.ip],['MAC',S.mac]
  ]);
  kv($('lampkv'),[
    ['Lamp state',S.carbide],['Brightness',S.brightness],
    ['Jewel',S.jewelReady?'DRIVER READY':'STUB / UNCONFIRMED',S.jewelReady?'ok':'warn'],
    ['Local control',may?'ALLOWED':'LOCKED',may?'ok':'warn']
  ]);
  $('bri').value=S.brightness; $('briv').textContent=S.brightness;
  kv($('audkv'),[
    ['Fermion',S.audioStatus,S.audioStatus==='OK'?'ok':'warn'],
    ['Connected',S.audioPresent?'YES':'NO',S.audioPresent?'ok':'warn'],
    ['Role',S.audioRole],['Volume',S.audioVol]
  ]);
  $('vol').value=S.audioVol; $('volv').textContent=S.audioVol;
  kv($('blowkv'),[
    ['Raw',S.micRaw],['Filtered',S.micFilt],['Baseline',S.micBase],
    ['Threshold',S.blowThresh],['Minimum duration',S.blowMinMs+' ms'],
    ['Above ms',S.blowMs],['Class',S.blowClass],
    ['Live blow',S.blowYes?'YES':'NO',S.blowYes?'warn':'ok']
  ]);
  $('blowdot').className='live'+(S.blowYes?' on':'');
  $('blowlive').textContent=S.blowYes?(S.blowClass==='SUSTAINED'?'SUSTAINED BLOW':'PUFF / BLOW'):'BLOW IDLE';
  if(document.activeElement!==$('bth')) $('bth').value=S.blowThresh;
  if(document.activeElement!==$('puff')) $('puff').value=S.puffMs;
  if(document.activeElement!==$('blowms')) $('blowms').value=S.blowMinMs;
  kv($('lightkv'),[
    ['Raw',S.lightRaw],
    ['Normalized',S.lightNorm<0?'UNCALIBRATED':S.lightNorm,S.lightNorm<0?'warn':'ok'],
    ['Status',S.lightStatus]
  ]);
  kv($('voltkv'),[
    ['Raw ADC',S.voltRaw],
    ['Voltage mV',S.voltMv<0?'UNCALIBRATED':S.voltMv,S.voltMv<0?'warn':'ok'],
    ['Calibration',S.voltStatus],['Warning',S.voltWarn]
  ]);
  kv($('btnkv'),[
    ['Status',S.buttonStatus,S.buttonPressed?'warn':'ok'],
    ['GPIO',pin(S.pinBtn)],['Polarity',S.buttonPolarity]
  ]);
  kv($('syskv'),[
    ['Firmware',S.fw],['Uptime',S.uptime+' s'],
    ['Pin source',S.pinSource,S.pinsConfirmed?'ok':'warn'],
    ['Jewel DATA',pin(S.pinJewel)],['Ignition button',pin(S.pinBtn)],
    ['Mic ADC',pin(S.pinMic)],['Light ADC',pin(S.pinLight)],
    ['Voltage ADC',pin(S.pinVolt)],['Fermion TX',pin(S.pinAudTx)],
    ['Fermion RX',pin(S.pinAudRx)],['Last command',S.lastCommand],
    ['Last result',S.lastResult],['Fault',S.fault]
  ]);
  if(document.activeElement!==$('nid')) $('nid').value=S.id||'';
  if(document.activeElement!==$('nname')) $('nname').value=S.name||'';
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
async function cfg(body){
  const r=await (await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)})).json();
  if(!r.ok) alert(r.error||'config rejected');
  load();
}
$('saveid').onclick=()=>cfg({id:$('nid').value,name:$('nname').value});
$('saveblow').onclick=()=>cfg({blowThresh:+$('bth').value,puffMs:+$('puff').value,blowMs:+$('blowms').value});
$('calblow').onclick=()=>cfg({calibrateBlow:1});
$('callight').onclick=()=>cfg({calibrateLight:1});
$('setbri').onclick=()=>send('LAMP:BRIGHTNESS:'+$('bri').value);
$('setvol').onclick=()=>cfg({vol:+$('vol').value});
$('ignite').onclick=()=>send('LAMP:IGNITE');
$('ext').onclick=()=>send('LAMP:EXTINGUISH');
$('steady').onclick=()=>send('LAMP:FX:STEADY_FLAME');
$('low').onclick=()=>send('LAMP:FX:LOW_FLAME');
$('unst').onclick=()=>send('LAMP:FX:UNSTABLE');
$('flare').onclick=()=>send('LAMP:FX:FLARE');
$('dying').onclick=()=>send('LAMP:FX:DYING_FLAME');
$('strike').onclick=()=>send('LAMP:AUDIO:STRIKE');
$('ignaud').onclick=()=>send('LAMP:AUDIO:IGNITION');
$('burn').onclick=()=>send('LAMP:AUDIO:BURN_LOOP');
$('flareaud').onclick=()=>send('LAMP:AUDIO:FLARE');
$('extaud').onclick=()=>send('LAMP:AUDIO:EXTINGUISH');
$('astop').onclick=()=>send('LAMP:AUDIO:STOP');
$('btntest').onclick=()=>send('LAMP:IGNITE');
$('reboot').onclick=()=>fetch('/api/reboot',{method:'POST'});
$('bri').oninput=()=>$('briv').textContent=$('bri').value;
$('vol').oninput=()=>$('volv').textContent=$('vol').value;
load(); setInterval(load,1000);
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

static bool extractJsonLong(const String &body, const char *key, long *out) {
  if (!key || !out) return false;
  const String needle = String("\"") + key + "\"";
  int at = body.indexOf(needle);
  if (at < 0) return false;
  at = body.indexOf(':', at);
  if (at < 0) return false;
  at++;
  while (at < (int)body.length() && (body[at] == ' ' || body[at] == '\t')) at++;
  *out = body.substring(at).toInt();
  return true;
}

static void formatSsid(char *out, size_t n) {
  showduino_lamp_format_ssid(lampConfigId(), out, n);
}

static void handleRoot() { sServer.send_P(200, "text/html", kPage); }

static void handleStatus() {
  char mac[24];
  lampEspNowMacString(mac, sizeof(mac));
  const ShowduinoBlowDetector *blow = lampSensorsBlow();
  const ShowduinoLampProductMode product = lampNodeProductMode();
  const bool may = showduino_lamp_web_may_control(lampNodeState()) != 0;
  String json = "{";
  json += "\"id\":\"";
  jsonEsc(lampConfigId(), json);
  json += "\",\"name\":\"";
  jsonEsc(lampConfigName(), json);
  json += "\",\"product\":\"";
  json += SHOWDUINO_PLATFORM_VERSION;
  json += "\",\"fw\":\"";
  json += SHOWDUINO_LAMP_NODE_FW;
  json += "\",\"protocol\":\"";
  json += SHOWDUINO_LAMP_PROTOCOL;
  json += "\",\"operatingMode\":\"";
  json += showduino_lamp_product_mode_name(product);
  json += "\",\"owner\":\"";
  json += lampNodeStateName();
  json += "\",\"showduinoConnection\":\"";
  if (product == SHOWDUINO_LAMP_PRODUCT_EMERGENCY) json += "EMERGENCY";
  else if (product == SHOWDUINO_LAMP_PRODUCT_SHOWDUINO) json += "CONNECTED";
  else if (lampNodeState() == SHOWDUINO_LAMP_ST_SEARCHING) json += "SEARCHING";
  else json += "NONE";
  json += "\",\"radioStatus\":\"";
  if (!lampEspNowReady()) json += "FAULT";
  else if (lampEspNowHaveComms()) json += "ESP-NOW HEARD";
  else json += "ESP-NOW QUIET";
  json += "\",\"carbide\":\"";
  json += lampEngineCarbideName();
  json += "\",\"brightness\":";
  json += String((unsigned)lampEngineBrightness());
  json += ",\"emergency\":";
  json += lampEngineEmergency() ? "true" : "false";
  json += ",\"webMayControl\":";
  json += may ? "true" : "false";
  json += ",\"jewelReady\":";
  json += lampEngineJewelReady() ? "true" : "false";
  json += ",\"ssid\":\"";
  jsonEsc(nodeSoftApSsid(), json);
  json += "\",\"ip\":\"";
  json += nodeSoftApIp();
  json += "\",\"channel\":";
  json += String((unsigned)lampEspNowChannel());
  json += ",\"mac\":\"";
  json += mac;
  json += "\",\"micRaw\":";
  json += String((long)lampSensorsMicRaw());
  json += ",\"micFilt\":";
  json += String((long)lampSensorsMicFiltered());
  json += ",\"micBase\":";
  json += String((long)lampSensorsMicBaseline());
  json += ",\"blowThresh\":";
  json += String((long)lampConfigBlowThreshold());
  json += ",\"puffMs\":";
  json += String((unsigned)lampConfigPuffMs());
  json += ",\"blowMinMs\":";
  json += String((unsigned)lampConfigBlowMs());
  json += ",\"blowMs\":";
  json += String((unsigned long)(blow ? blow->aboveMs : 0));
  json += ",\"blowClass\":\"";
  json += (blow && blow->classified == SHOWDUINO_BLOW_SUSTAINED) ? "SUSTAINED" :
          (blow && blow->classified == SHOWDUINO_BLOW_PUFF) ? "PUFF" : "NONE";
  json += "\",\"blowYes\":";
  json += (blow && blow->classified != SHOWDUINO_BLOW_NONE) ? "true" : "false";
  json += ",\"lightRaw\":";
  json += String((long)lampSensorsLightRaw());
  json += ",\"lightNorm\":";
  json += String((long)lampSensorsLightNormalized());
  json += ",\"lightStatus\":\"";
  json += lampSensorsLightStatus();
  json += "\",\"voltRaw\":";
  json += String((long)lampSensorsVoltRaw());
  json += ",\"voltFilt\":";
  json += String((long)lampSensorsVoltFiltered());
  json += ",\"voltMv\":";
  json += String((long)lampSensorsVoltMv());
  json += ",\"voltStatus\":\"";
  json += lampSensorsVoltStatus();
  json += "\",\"voltWarn\":\"";
  json += lampSensorsVoltWarn();
  json += "\",\"audioStatus\":\"";
  json += lampAudioStatus();
  json += "\",\"audioPresent\":";
  json += lampAudioHardwarePresent() ? "true" : "false";
  json += ",\"audioRole\":\"";
  json += lampAudioCurrentRole();
  json += "\",\"audioVol\":";
  json += String((unsigned)lampAudioVolume());
  json += ",\"buttonStatus\":\"";
  json += lampLocalStatus();
  json += "\",\"buttonPressed\":";
  json += lampLocalPressed() ? "true" : "false";
  json += ",\"buttonPolarity\":\"";
  json += SHOWDUINO_LAMP_BTN_POLARITY_NOTE;
  json += "\",\"lastCommand\":\"";
  jsonEsc(lampNodeStateLastCommand(), json);
  json += "\",\"lastResult\":\"";
  jsonEsc(lampNodeStateLastResult(), json);
  json += "\",\"fault\":\"";
  jsonEsc(lampNodeStateFault(), json);
  json += "\",\"pinSource\":\"";
  json += SHOWDUINO_LAMP_PIN_SOURCE;
  json += "\",\"pinsConfirmed\":";
  json += SHOWDUINO_LAMP_PINS_CONFIRMED ? "true" : "false";
  json += ",\"pinJewel\":";
  json += String(SHOWDUINO_LAMP_PIXEL_PIN);
  json += ",\"pinBtn\":";
  json += String(SHOWDUINO_LAMP_BTN_IGNITE);
  json += ",\"pinMic\":";
  json += String(SHOWDUINO_LAMP_MIC_PIN);
  json += ",\"pinLight\":";
  json += String(SHOWDUINO_LAMP_LIGHT_PIN);
  json += ",\"pinVolt\":";
  json += String(SHOWDUINO_LAMP_VOLT_PIN);
  json += ",\"pinAudTx\":";
  json += String(SHOWDUINO_LAMP_FERMION_TX_PIN);
  json += ",\"pinAudRx\":";
  json += String(SHOWDUINO_LAMP_FERMION_RX_PIN);
  json += ",\"uptime\":";
  json += String((unsigned long)(millis() / 1000UL));
  json += "}";
  sServer.send(200, "application/json", json);
}

static void handleConfigPost() {
  const String body = sServer.hasArg("plain") ? sServer.arg("plain") : sServer.arg(0);
  char id[SHOWDUINO_LAMP_LOGICAL_MAX];
  char name[21];
  bool idChanged = false;
  if (extractJsonString(body, "id", id, sizeof(id))) {
    if (!lampConfigSetId(id)) {
      sServer.send(200, "application/json", "{\"ok\":false,\"error\":\"BAD_ID\"}");
      return;
    }
    idChanged = true;
  }
  if (extractJsonString(body, "name", name, sizeof(name))) {
    if (!lampConfigSetName(name)) {
      sServer.send(200, "application/json", "{\"ok\":false,\"error\":\"BAD_NAME\"}");
      return;
    }
  }

  const bool may = showduino_lamp_web_may_control(lampNodeState()) != 0;
  long v = 0;
  if (extractJsonLong(body, "vol", &v)) {
    if (!may) {
      sServer.send(200, "application/json", "{\"ok\":false,\"error\":\"SHOW_CONTROLLED\"}");
      return;
    }
    lampAudioSetVolume((uint8_t)(v < 0 ? 0 : v));
  }
  if (extractJsonLong(body, "brightness", &v)) {
    if (!may) {
      sServer.send(200, "application/json", "{\"ok\":false,\"error\":\"SHOW_CONTROLLED\"}");
      return;
    }
    lampEngineSetBrightness((uint8_t)v);
    lampConfigSetBrightness((uint8_t)v);
  }
  if (extractJsonLong(body, "blowThresh", &v)) {
    lampConfigSetBlowThreshold((int32_t)v);
  }
  long puff = 0, blow = 0;
  const bool hasPuff = extractJsonLong(body, "puffMs", &puff);
  const bool hasBlow = extractJsonLong(body, "blowMs", &blow);
  if (hasPuff || hasBlow) {
    lampConfigSetBlowWindows(hasPuff ? (uint16_t)puff : lampConfigPuffMs(),
                             hasBlow ? (uint16_t)blow : lampConfigBlowMs());
  }
  if (hasPuff || hasBlow || extractJsonLong(body, "blowThresh", &v)) {
    lampSensorsApplyConfig();
  }
  if (extractJsonLong(body, "calibrateBlow", &v) && v) {
    lampSensorsCalibrateQuiet();
  }
  if (extractJsonLong(body, "calibrateLight", &v) && v) {
    lampSensorsCalibrateLight();
  }
  if (idChanged && nodeSoftApStarted()) {
    char ssid[33];
    formatSsid(ssid, sizeof(ssid));
    nodeSoftApRetitle(ssid);
  }
  sServer.send(200, "application/json", "{\"ok\":true}");
}

static void handleCommand() {
  char cmd[SHOWDUINO_NODE_COMMAND_MAX];
  const String body = sServer.hasArg("plain") ? sServer.arg("plain") : sServer.arg(0);
  if (!extractJsonString(body, "command", cmd, sizeof(cmd))) {
    sServer.send(200, "application/json", "{\"ok\":false,\"error\":\"BAD_COMMAND\"}");
    return;
  }
  if (!strcmp(cmd, "EMERGENCY:CLEAR") || !strcmp(cmd, "LAMP:EMERGENCY:CLEAR")) {
    sServer.send(200, "application/json",
                 "{\"ok\":false,\"error\":\"EMERGENCY\",\"message\":\"Webpage cannot clear Showduino emergency\"}");
    return;
  }
  if (lampEngineEmergency() || lampNodeState() == SHOWDUINO_LAMP_ST_EMERGENCY) {
    sServer.send(200, "application/json",
                 "{\"ok\":false,\"error\":\"EMERGENCY\",\"message\":\"EMERGENCY ACTIVE\"}");
    return;
  }
  const bool may = showduino_lamp_web_may_control(lampNodeState()) != 0;
  if (!strncmp(cmd, "LAMP:AUDIO:", 11)) {
    if (!may) {
      sServer.send(200, "application/json",
                   "{\"ok\":false,\"error\":\"SHOW_CONTROLLED\",\"message\":\"CONTROLLED BY SHOWDUINO\"}");
      return;
    }
    if (!strcmp(cmd + 11, "STRIKE")) lampAudioPlay(SHOWDUINO_LAMP_SND_STRIKE);
    else if (!strcmp(cmd + 11, "IGNITION")) lampAudioPlay(SHOWDUINO_LAMP_SND_IGNITION);
    else if (!strcmp(cmd + 11, "BURN_LOOP")) lampAudioPlay(SHOWDUINO_LAMP_SND_BURN_LOOP);
    else if (!strcmp(cmd + 11, "FLARE")) lampAudioPlay(SHOWDUINO_LAMP_SND_FLARE);
    else if (!strcmp(cmd + 11, "EXTINGUISH")) lampAudioPlay(SHOWDUINO_LAMP_SND_EXTINGUISH);
    else if (!strcmp(cmd + 11, "STOP")) lampAudioStop();
    else {
      sServer.send(200, "application/json", "{\"ok\":false,\"error\":\"BAD_COMMAND\"}");
      return;
    }
    sServer.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  ShowduinoLampCommand parsed;
  const ShowduinoLampCmd parsedCmd = showduino_lamp_parse_command(cmd, &parsed);
  if (showduino_lamp_cmd_theatrical(parsedCmd) && !may) {
    sServer.send(200, "application/json",
                 "{\"ok\":false,\"error\":\"SHOW_CONTROLLED\",\"message\":\"CONTROLLED BY SHOWDUINO\"}");
    return;
  }
  lampProtocolApply(cmd, 0, SHOWDUINO_CMD_ORIGIN_WEB);
  sServer.send(200, "application/json", "{\"ok\":true}");
}

static void handleReboot() {
  sServer.send(200, "application/json", "{\"ok\":true}");
  delay(50);
  ESP.restart();
}

static void addRoutes() {
  if (sRoutes) return;
  sServer.on("/", handleRoot);
  sServer.on("/api/status", HTTP_GET, handleStatus);
  sServer.on("/api/config", HTTP_POST, handleConfigPost);
  sServer.on("/api/command", HTTP_POST, handleCommand);
  sServer.on("/api/reboot", HTTP_POST, handleReboot);
  sRoutes = true;
}

void lampWebEnsure() {
  nodeSoftApSetRadioHook(lampEspNowReassert);
  if (!nodeSoftApStarted()) {
    char ssid[33];
    formatSsid(ssid, sizeof(ssid));
    if (!nodeSoftApBeginNamed(ssid, lampEspNowChannel(),
                              SHOWDUINO_LAMP_AP_PASSWORD)) {
      return;
    }
  }
  addRoutes();
  if (!sBegun) {
    sServer.begin();
    sBegun = true;
    Serial.printf("[LAMP-WEB] http://%s  ssid=%s\n", nodeSoftApIp(),
                  nodeSoftApSsid());
  }
}

void lampWebService() {
  if (nodeSoftApStarted() && WiFi.scanComplete() != WIFI_SCAN_RUNNING) {
    nodeSoftApService();
  }
  if (sBegun) sServer.handleClient();
}

bool lampWebReady() { return sBegun && nodeSoftApReady(); }
