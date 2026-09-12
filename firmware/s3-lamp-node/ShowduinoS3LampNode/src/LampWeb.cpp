#include "LampWeb.h"

#include <WebServer.h>
#include <WiFi.h>

#include "LampEngine.h"
#include "LampNodeState.h"
#include "LampConfig.h"
#include "LampProtocol.h"
#include "LampSensors.h"
#include "LampAudio.h"
#include "EspNowLampTransport.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_lamp_node.h"
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
#owner{font-size:12px;color:#aaa;margin-top:4px}
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
.kv{display:grid;grid-template-columns:160px 1fr;gap:4px 10px;font-size:13px}
.ok{color:#6c6}.warn{color:#fc6}.err{color:#f66}
</style></head><body>
<header><h1 id="title">Showduino Lamp Node</h1><div id="owner">…</div></header>
<nav>
<button data-t="id" class="on">IDENTITY</button>
<button data-t="lamp">LAMP</button>
<button data-t="mic">MIC / BLOW</button>
<button data-t="sens">SENSORS</button>
<button data-t="aud">LOCAL AUDIO</button>
<button data-t="hw">HARDWARE</button>
</nav>
<div id="banner" class="banner" hidden></div>
<main>
<section class="tab" id="id"><div class="card"><div class="kv" id="idkv"></div>
<label>Node ID</label><input id="nid" maxlength="12">
<label>Friendly name</label><input id="nname" maxlength="20">
<div class="row"><button class="act" id="saveid">SAVE IDENTITY</button></div>
</div></section>
<section class="tab" id="lamp" hidden><div class="card"><div class="kv" id="lampkv"></div>
<div class="row">
<button class="act" id="ignite">TEST IGNITE</button>
<button class="act" id="ext">TEST EXTINGUISH</button>
<button class="act" id="low">LOW FLAME</button>
<button class="act" id="unst">UNSTABLE</button>
<button class="act" id="flare">FLARE</button>
</div>
<p>These local tests use the same carbide state machine as P4 IGNITE. This page cannot clear emergency.</p>
</div></section>
<section class="tab" id="mic" hidden><div class="card"><div class="kv" id="mickv"></div></div></section>
<section class="tab" id="sens" hidden><div class="card"><div class="kv" id="senskv"></div></div></section>
<section class="tab" id="aud" hidden><div class="card"><div class="kv" id="audkv"></div>
<div class="row">
<button class="act" id="strike">TEST STRIKE</button>
<button class="act" id="burn">TEST BURN LOOP</button>
<button class="act" id="astop">STOP AUDIO</button>
</div>
<p>Fermion is local lamp FX only. It is not the Showduino Audio Node.</p>
</div></section>
<section class="tab" id="hw" hidden><div class="card"><div class="kv" id="hwkv"></div>
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
  const owned=S.owner==='SHOW_CONTROLLED';
  const em=!!S.emergency;
  $('title').textContent=(S.id||'LAMP')+' — '+(S.name||'Lamp Node');
  $('owner').textContent=S.owner+' · '+S.carbide+' · '+(S.comms||'');
  const ban=$('banner');
  if(em){ban.hidden=false;ban.className='banner bad';ban.textContent='EMERGENCY — jewel forced white. Webpage cannot clear this.';}
  else if(owned){ban.hidden=false;ban.className='banner';ban.textContent='CONTROLLED BY SHOWDUINO';}
  else {ban.hidden=true;}
  document.querySelectorAll('button.act').forEach(b=>{
    const keep=b.id==='saveid';
    b.disabled=em||(owned&&!keep);
  });
  kv($('idkv'),[
    ['Node ID',S.id],['Name',S.name],['Showduino',S.product],
    ['Lamp FW',S.fw],['Protocol',S.protocol],['MAC',S.mac],
    ['Channel',S.channel],['Comms',S.espNow],['Uptime',S.uptime+' s']
  ]);
  kv($('lampkv'),[
    ['Carbide',S.carbide],['Ownership',S.owner],['Brightness',S.brightness],
    ['Jewel',S.jewelReady?'DRIVER READY':'STUB / UNCONFIRMED',S.jewelReady?'ok':'warn']
  ]);
  kv($('mickv'),[
    ['Raw',S.micRaw],['Filtered',S.micFilt],['Baseline',S.micBase],
    ['Threshold',S.blowThresh],['Above ms',S.blowMs],
    ['Class',S.blowClass],['Detected',S.blowYes?'YES':'NO',S.blowYes?'warn':'ok']
  ]);
  kv($('senskv'),[
    ['Light raw',S.lightRaw],['Light filtered',S.lightFilt],['Light',S.lightStatus],
    ['Volt raw',S.voltRaw],['Volt filtered',S.voltFilt],
    ['Volt mV',S.voltMv<0?'UNCALIBRATED':S.voltMv,S.voltMv<0?'warn':'ok'],
    ['Volt status',S.voltStatus],['Volt warn',S.voltWarn]
  ]);
  kv($('audkv'),[
    ['Fermion',S.audioStatus,S.audioStatus==='OK'?'ok':'warn'],
    ['Role',S.audioRole],['Volume',S.audioVol]
  ]);
  kv($('hwkv'),[
    ['Pin source',S.pinSource,S.pinsConfirmed?'ok':'warn'],
    ['Jewel DATA',pin(S.pinJewel)],['Ignition button',pin(S.pinBtn)],
    ['Mic ADC',pin(S.pinMic)],['Light ADC',pin(S.pinLight)],
    ['Voltage ADC',pin(S.pinVolt)],['Fermion TX',pin(S.pinAudTx)],
    ['Fermion RX',pin(S.pinAudRx)],['SSID',S.ssid],['IP',S.ip]
  ]);
  $('nid').value=S.id||''; $('nname').value=S.name||'';
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
$('ignite').onclick=()=>send('LAMP:IGNITE');
$('ext').onclick=()=>send('LAMP:EXTINGUISH');
$('low').onclick=()=>send('LAMP:FX:LOW_FLAME');
$('unst').onclick=()=>send('LAMP:FX:UNSTABLE');
$('flare').onclick=()=>send('LAMP:FX:FLARE');
$('strike').onclick=()=>send('LAMP:AUDIO:STRIKE');
$('burn').onclick=()=>send('LAMP:AUDIO:BURN_LOOP');
$('astop').onclick=()=>send('LAMP:AUDIO:STOP');
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

static void handleRoot() { sServer.send_P(200, "text/html", kPage); }

static void handleStatus() {
  char mac[24];
  lampEspNowMacString(mac, sizeof(mac));
  const ShowduinoBlowDetector *blow = lampSensorsBlow();
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
  json += "\",\"owner\":\"";
  json += lampNodeStateName();
  json += "\",\"carbide\":\"";
  json += lampEngineCarbideName();
  json += "\",\"brightness\":";
  json += String((unsigned)lampEngineBrightness());
  json += ",\"emergency\":";
  json += lampEngineEmergency() ? "true" : "false";
  json += ",\"jewelReady\":";
  json += lampEngineJewelReady() ? "true" : "false";
  json += ",\"ssid\":\"";
  jsonEsc(nodeSoftApSsid(), json);
  json += "\",\"ip\":\"";
  json += nodeSoftApIp();
  json += "\",\"channel\":";
  json += String((unsigned)lampEspNowChannel());
  json += ",\"espNow\":\"";
  json += lampEspNowHaveComms() ? "heard" : "searching";
  json += "\",\"mac\":\"";
  json += mac;
  json += "\",\"micRaw\":";
  json += String((long)lampSensorsMicRaw());
  json += ",\"micFilt\":";
  json += String((long)lampSensorsMicFiltered());
  json += ",\"micBase\":";
  json += String((long)lampSensorsMicBaseline());
  json += ",\"blowThresh\":";
  json += String((long)(blow ? blow->threshold : 0));
  json += ",\"blowMs\":";
  json += String((unsigned long)(blow ? blow->aboveMs : 0));
  json += ",\"blowClass\":\"";
  json += (blow && blow->classified == SHOWDUINO_BLOW_SUSTAINED) ? "SUSTAINED" :
          (blow && blow->classified == SHOWDUINO_BLOW_PUFF) ? "PUFF" : "NONE";
  json += "\",\"blowYes\":";
  json += showduino_blow_detected(blow) ? "true" : "false";
  json += ",\"lightRaw\":";
  json += String((long)lampSensorsLightRaw());
  json += ",\"lightFilt\":";
  json += String((long)lampSensorsLightFiltered());
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
  json += "\",\"audioRole\":\"";
  json += lampAudioCurrentRole();
  json += "\",\"audioVol\":";
  json += String((unsigned)lampAudioVolume());
  json += ",\"pinSource\":\"";
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
  if (extractJsonString(body, "id", id, sizeof(id))) {
    if (!lampConfigSetId(id)) {
      sServer.send(200, "application/json", "{\"ok\":false,\"error\":\"BAD_ID\"}");
      return;
    }
  }
  if (extractJsonString(body, "name", name, sizeof(name))) {
    if (!lampConfigSetName(name)) {
      sServer.send(200, "application/json", "{\"ok\":false,\"error\":\"BAD_NAME\"}");
      return;
    }
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
  if (!strncmp(cmd, "LAMP:AUDIO:", 11)) {
    if (lampEngineEmergency() || lampNodeStateShowControlled()) {
      sServer.send(200, "application/json", "{\"ok\":false,\"error\":\"LOCKED\"}");
      return;
    }
    if (!strcmp(cmd + 11, "STRIKE")) lampAudioPlay(SHOWDUINO_LAMP_SND_STRIKE);
    else if (!strcmp(cmd + 11, "BURN_LOOP")) lampAudioPlay(SHOWDUINO_LAMP_SND_BURN_LOOP);
    else if (!strcmp(cmd + 11, "STOP")) lampAudioStop();
    else {
      sServer.send(200, "application/json", "{\"ok\":false,\"error\":\"BAD_COMMAND\"}");
      return;
    }
    sServer.send(200, "application/json", "{\"ok\":true}");
    return;
  }
  lampProtocolApply(cmd, 0, SHOWDUINO_CMD_ORIGIN_WEB);
  if (lampEngineEmergency()) {
    sServer.send(200, "application/json",
                 "{\"ok\":false,\"error\":\"EMERGENCY\",\"message\":\"EMERGENCY ACTIVE\"}");
    return;
  }
  if (lampNodeStateShowControlled()) {
    sServer.send(200, "application/json",
                 "{\"ok\":false,\"error\":\"SHOW_CONTROLLED\",\"message\":\"CONTROLLED BY SHOWDUINO\"}");
    return;
  }
  sServer.send(200, "application/json", "{\"ok\":true}");
}

static void addRoutes() {
  if (sRoutes) return;
  sServer.on("/", handleRoot);
  sServer.on("/api/status", HTTP_GET, handleStatus);
  sServer.on("/api/config", HTTP_POST, handleConfigPost);
  sServer.on("/api/command", HTTP_POST, handleCommand);
  sRoutes = true;
}

void lampWebEnsure() {
  nodeSoftApSetRadioHook(lampEspNowReassert);
  if (!nodeSoftApStarted()) {
    uint8_t mac[6];
    lampEspNowMacBytes(mac);
    if (!nodeSoftApBegin("Lamp", mac, lampEspNowChannel(),
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
