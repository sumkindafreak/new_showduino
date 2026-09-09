#include "AudioWeb.h"

#include <WebServer.h>
#include <WiFi.h>

#include "AudioCodec.h"
#include "AudioCommand.h"
#include "AudioNodeState.h"
#include "AudioPlayback.h"
#include "AudioStorage.h"
#include "EspNowNodeTransport.h"
#include "input/AudioInput.h"
#include "input/AudioInputConfig.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_audio_node.h"
#include "../../../protocol/showduino_protocol_version.h"
#include "../../shared-node/NodeConfig.h"
#include "../../shared-node/NodeSoftAp.h"

static WebServer sServer(80);
static bool sRoutes = false;
static bool sBegun = false;

static const char kPage[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Showduino Audio</title>
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
label{display:block;margin:8px 0 4px;color:#aaa}
input,select{background:#111;color:#fff;border:1px solid #444;padding:8px;width:100%;box-sizing:border-box}
.kv{display:grid;grid-template-columns:140px 1fr;gap:4px 10px;font-size:13px}
.ok{color:#6c6}.warn{color:#fc6}.bad{color:#f66}
</style></head><body>
<header><h1 id="title">Showduino Audio</h1><div id="owner">…</div></header>
<nav>
<button data-t="status" class="on">STATUS</button>
<button data-t="audio">AUDIO</button>
<button data-t="library">LIBRARY</button>
<button data-t="sound">SOUND INPUT</button>
<button data-t="conn">CONNECTION</button>
<button data-t="diag">DIAGNOSTICS</button>
<button data-t="set">SETTINGS</button>
</nav>
<div id="banner" class="banner" hidden>CONTROLLED BY SHOWDUINO</div>
<main>
<section class="tab" id="status">
<div class="card"><div class="kv" id="statuskv"></div></div>
</section>
<section class="tab" id="audio" hidden>
<div class="card">
<label>Asset</label><select id="asset"></select>
<label>Volume <span id="volv"></span></label>
<input id="vol" type="range" min="0" max="100">
<div class="row">
<button class="act" data-c="AUDIO:NODE:PLAY">PLAY</button>
<button class="act" data-c="AUDIO:NODE:LOOP">LOOP</button>
<button class="act" data-c="AUDIO:NODE:PAUSE">PAUSE</button>
<button class="act" data-c="AUDIO:NODE:RESUME">RESUME</button>
<button class="act" data-c="AUDIO:NODE:STOP">STOP</button>
<button class="act" data-c="AUDIO:NODE:TEST">TEST</button>
</div>
</div>
</section>
<section class="tab" id="library" hidden>
<div class="card" id="lib"></div>
</section>
<section class="tab" id="sound" hidden>
<div class="card"><div class="kv" id="soundkv"></div>
<div class="row">
<button class="act" data-c="AUDIO:NODE:SOUND:STATUS">STATUS</button>
<button class="act" data-c="AUDIO:NODE:SOUND:ENABLE">ENABLE</button>
<button class="act" data-c="AUDIO:NODE:SOUND:DISABLE">DISABLE</button>
<button class="act" data-c="AUDIO:NODE:SOUND:CALIBRATE">CALIBRATE</button>
</div></div>
</section>
<section class="tab" id="conn" hidden>
<div class="card"><div class="kv" id="connkv"></div></div>
</section>
<section class="tab" id="diag" hidden>
<div class="card"><div class="kv" id="diagkv"></div></div>
</section>
<section class="tab" id="set" hidden>
<div class="card">
<label>Node name</label><input id="name" maxlength="24">
<div class="row"><button class="act" id="save">SAVE NAME</button></div>
<p>Live playback is never saved as a boot state.</p>
</div>
</section>
</main>
<script>
const $=id=>document.getElementById(id);
let S=null;
function kv(el,rows){el.innerHTML=rows.map(r=>`<div>${r[0]}</div><div class="${r[2]||''}">${r[1]}</div>`).join('')}
async function load(){
  S=await (await fetch('/api/status')).json();
  const owned=S.owner==='SHOW_CONTROLLED';
  $('title').textContent=S.name||'Showduino Audio';
  $('owner').textContent=S.owner+' · '+S.playback+(S.asset&&S.asset!=='-'?' · '+S.asset:'');
  $('banner').hidden=!owned;
  document.querySelectorAll('button.act').forEach(b=>{if(b.id!=='save')b.disabled=owned});
  $('vol').disabled=owned; $('asset').disabled=owned;
  kv($('statuskv'),[
    ['Owner',S.owner,owned?'warn':''],
    ['Playback',S.playback],
    ['Volume',S.volume],
    ['Asset',S.asset||'-'],
    ['Storage',S.storage],
    ['Codec',S.codec],
    ['Firmware',S.fw],
    ['Emergency',S.emergency?'YES':'no',S.emergency?'bad':'']
  ]);
  kv($('soundkv'),[
    ['Ready',S.sound&&S.sound.ready?'YES':'no'],
    ['Level',S.sound?S.sound.level:0],
    ['Floor',S.sound?S.sound.floor:0],
    ['Threshold',S.sound?S.sound.threshold:0],
    ['Last',S.sound?S.sound.last:'NONE']
  ]);
  kv($('connkv'),[
    ['SSID',S.ssid],
    ['IP',S.ip],
    ['Channel',S.channel],
    ['ESP-NOW',S.espNow],
    ['Granted',S.granted?'YES':'no']
  ]);
  kv($('diagkv'),[
    ['MAC',S.mac],
    ['Output',S.output],
    ['Fault',S.fault],
    ['Pixel','GPIO22 status only']
  ]);
  $('vol').value=S.volume; $('volv').textContent=S.volume;
  $('name').value=S.name||'';
  const lib=await (await fetch('/api/library')).json();
  const sel=$('asset'); const cur=sel.value;
  sel.innerHTML=(lib.files||[]).map(f=>`<option>${f}</option>`).join('')||'<option>system-test.wav</option>';
  if(cur) sel.value=cur;
  $('lib').innerHTML='<ul>'+(lib.files||[]).map(f=>'<li>'+f+'</li>').join('')+'</ul>';
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
document.querySelectorAll('button.act[data-c]').forEach(b=>b.onclick=()=>{
  let c=b.dataset.c;
  if(c==='AUDIO:NODE:PLAY'||c==='AUDIO:NODE:LOOP') c=c+':'+$('asset').value;
  send(c);
});
$('vol').onchange=()=>send('AUDIO:NODE:VOLUME:'+$('vol').value);
$('save').onclick=async()=>{
  await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({name:$('name').value})});
  load();
};
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

static void handleRoot() {
  sServer.send_P(200, "text/html", kPage);
}

static void handleStatus() {
  char mac[24];
  char name[32];
  audioEspNowMacString(mac, sizeof(mac));
  nodeConfigGetName(name, sizeof(name), SHOWDUINO_AUDIO_NODE_NAME);
  String json = "{";
  json += "\"name\":\"";
  jsonEsc(name, json);
  json += "\",\"fw\":\"";
  json += SHOWDUINO_AUDIO_NODE_FW;
  json += "\",\"owner\":\"";
  json += audioOwnerModeName();
  json += "\",\"playback\":\"";
  json += audioNodeStateName();
  json += "\",\"volume\":";
  json += String((unsigned)audioCommandVolume());
  json += ",\"asset\":\"";
  jsonEsc(audioPlaybackRel()[0] ? audioPlaybackRel() : "-", json);
  json += "\",\"storage\":\"";
  json += audioStorageStateName();
  json += "\",\"codec\":\"";
  json += SHOWDUINO_AUDIO_NODE_CODEC;
  json += "\",\"output\":\"";
  json += audioCodecOutputName();
  json += "\",\"fault\":\"";
  json += showduino_audio_fail_name(audioNodeStateFault());
  json += "\",\"emergency\":";
  json += (audioNodeState() == SHOWDUINO_AUDIO_ST_EMERGENCY ||
           audioOwnerMode() == SHOWDUINO_OWNER_EMERGENCY)
              ? "true"
              : "false";
  json += ",\"ssid\":\"";
  jsonEsc(nodeSoftApSsid(), json);
  json += "\",\"ip\":\"";
  json += nodeSoftApIp();
  json += "\",\"channel\":";
  json += String((unsigned)nodeSoftApChannel());
  json += ",\"espNow\":\"";
  json += audioEspNowHaveComms() ? "heard" : "searching";
  json += "\",\"granted\":";
  json += audioOwnerGranted() ? "true" : "false";
  json += ",\"mac\":\"";
  json += mac;
  json += "\",\"sound\":{\"ready\":";
  json += audioInputReady() ? "true" : "false";
  json += ",\"level\":";
  json += String((unsigned)audioInputLevel());
  json += ",\"floor\":";
  json += String((unsigned)audioInputNoiseFloor());
  json += ",\"threshold\":";
  json += String((unsigned)audioInputConfig().threshold);
  json += ",\"last\":\"";
  jsonEsc(audioInputLastError()[0] ? audioInputLastError() : "NONE", json);
  json += "\"}}";
  sServer.send(200, "application/json", json);
}

static void handleLibrary() {
  char names[SHOWDUINO_AUDIO_INV_MAX][40];
  const uint16_t n = audioStorageInventory(names, SHOWDUINO_AUDIO_INV_MAX);
  String json = "{\"files\":[";
  for (uint16_t i = 0; i < n; i++) {
    if (i) json += ',';
    json += '"';
    jsonEsc(names[i], json);
    json += '"';
  }
  json += "]}";
  sServer.send(200, "application/json", json);
}

static void handleConnection() { handleStatus(); }

static void handleConfigGet() {
  char name[32];
  nodeConfigGetName(name, sizeof(name), SHOWDUINO_AUDIO_NODE_NAME);
  String json = "{\"name\":\"";
  jsonEsc(name, json);
  json += "\",\"volumeDefault\":";
  json += String((unsigned)audioStorageConfig().volume);
  json += "}";
  sServer.send(200, "application/json", json);
}

static void handleConfigPost() {
  char name[32];
  if (extractJsonString(sServer.arg("plain"), "name", name, sizeof(name)) ||
      extractJsonString(sServer.arg(0), "name", name, sizeof(name))) {
    nodeConfigSetName(name);
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
      sServer.send(200, "application/json",
                   "{\"ok\":false,\"error\":\"BAD_COMMAND\"}");
      return;
    }
  }
  audioCommandApply(cmd, 0, SHOWDUINO_CMD_ORIGIN_WEB);
  const bool locked = audioNodeStateShowControlled();
  const bool diagnostic =
      strstr(cmd, "STATUS") == cmd + 0 ||
      strstr(cmd, "AUDIO:NODE:STATUS") != NULL ||
      strstr(cmd, "AUDIO:NODE:INVENTORY") != NULL ||
      strstr(cmd, "SOUND:STATUS") != NULL ||
      strstr(cmd, "SOUND:LEVEL") != NULL ||
      strstr(cmd, "SOUND:CONFIG") != NULL;
  if (locked && !diagnostic) {
    sServer.send(200, "application/json",
                 "{\"ok\":false,\"error\":\"SHOW_CONTROLLED\","
                 "\"message\":\"CONTROLLED BY SHOWDUINO\"}");
    return;
  }
  sServer.send(200, "application/json", "{\"ok\":true}");
}

static void addRoutes() {
  if (sRoutes) return;
  sServer.on("/", handleRoot);
  sServer.on("/api/status", HTTP_GET, handleStatus);
  sServer.on("/api/library", HTTP_GET, handleLibrary);
  sServer.on("/api/connection", HTTP_GET, handleConnection);
  sServer.on("/api/config", HTTP_GET, handleConfigGet);
  sServer.on("/api/config", HTTP_POST, handleConfigPost);
  sServer.on("/api/command", HTTP_POST, handleCommand);
  sRoutes = true;
}

void audioWebEnsure() {
  nodeSoftApSetRadioHook(audioEspNowReassert);
  if (!nodeSoftApStarted()) {
    uint8_t mac[6];
    audioEspNowMacBytes(mac);
    if (!nodeSoftApBegin("Audio", mac, SHOWDUINO_ESPNOW_CHANNEL,
                         SHOWDUINO_AUDIO_AP_PASSWORD)) {
      return;
    }
  }
  addRoutes();
  if (!sBegun) {
    sServer.begin();
    sBegun = true;
    Serial.printf("[AUDIO-WEB] http://%s  ssid=%s\n", nodeSoftApIp(),
                  nodeSoftApSsid());
  }
}

void audioWebService() {
  if (nodeSoftApStarted()) nodeSoftApService();
  if (sBegun) sServer.handleClient();
}

bool audioWebReady() { return sBegun && nodeSoftApReady(); }
