#include "MosfetWeb.h"
#include "MosfetIdentity.h"
#include "MosfetNodeState.h"
#include "MosfetOutputEngine.h"
#include "MosfetIdentifierPixels.h"
#include "MosfetProtocol.h"
#include "EspNowMosfetTransport.h"
#include "../BoardConfig.h"
#include "../../../protocol/showduino_mosfet_node.h"
#include "../../shared-node/NodeSoftAp.h"
#include "../../shared-node/NodeConfig.h"

#include <WebServer.h>
#include <WiFi.h>

static WebServer sServer(80);
static bool sBegun = false;
static uint32_t sLocalTestUntil = 0;

static const char kPage[] PROGMEM = R"HTML(
<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Showduino MOSFET Node</title>
<style>
body{margin:0;background:#111;color:#ddd;font:15px/1.35 sans-serif}
header{padding:12px 16px;background:#1a1a1a;border-bottom:2px solid #c45c26}
h1{margin:0;font-size:18px}
#owner{font-size:12px;color:#aaa;margin-top:4px}
main{padding:12px 16px 24px}
.card{background:#1c1c1c;border:1px solid #2a2a2a;border-radius:8px;padding:12px;margin:0 0 12px}
.row{display:flex;gap:8px;flex-wrap:wrap;margin:8px 0}
button.act{background:#2a2a2a;color:#fff;border:1px solid #444;padding:10px 14px;border-radius:4px}
button.act:disabled{opacity:.45}
.banner{background:#3a2060;color:#fff;padding:10px;border-radius:6px;margin:0 0 12px}
.bad{background:#5a1020}
.ctrl{background:#20402a}
label{display:block;margin:8px 0 4px;color:#aaa}
input{background:#111;color:#fff;border:1px solid #444;padding:8px;width:100%;box-sizing:border-box}
.kv{display:grid;grid-template-columns:140px 1fr;gap:4px 10px;font-size:13px}
.warn{color:#fc6}
.ch{display:flex;justify-content:space-between;align-items:center;padding:6px 0;border-bottom:1px solid #2a2a2a}
.dot{display:inline-block;width:10px;height:10px;border-radius:50%;background:#333;margin-right:8px;vertical-align:middle}
.dot.on{background:#2ecc71;box-shadow:0 0 6px #2ecc71}
</style></head><body>
<header><h1 id="title">Showduino MOSFET Node</h1><div id="owner">…</div></header>
<div id="banner" class="banner" hidden></div>
<main>
<div class="card"><div class="kv" id="nodekv"></div>
<label>Node ID</label><input id="nid" maxlength="12">
<label>Friendly name</label><input id="nname" maxlength="20">
<div class="row"><button class="act" id="saveid">SAVE IDENTITY</button></div>
</div>
<div class="card"><h3>Outputs</h3><div id="outs"></div>
<label>OUT name (1–4)</label><input id="och" type="number" min="1" max="4" value="1">
<input id="oname" maxlength="20" placeholder="Friendly name">
<div class="row"><button class="act" id="saveout">SAVE OUT NAME</button></div>
</div>
<div class="card"><h3>Commissioning</h3>
<p class="warn">Local tests auto-return OFF. Disabled while P4 owns this node or Emergency is active.</p>
<label>Channel</label><input id="tch" type="number" min="1" max="4" value="1">
<label>Level %</label><input id="tlv" type="range" min="0" max="100" value="50">
<div class="row">
<button class="act" id="test">TEST 5s</button>
<button class="act" id="alloff">ALL OFF</button>
</div>
</div>
<div class="card"><h3>Identifier pixels</h3>
<p class="warn">Local OUT1–OUT4 indicators only. Not a theatrical Pixel Line.</p>
<div class="kv" id="identkv"></div>
<div class="row"><button class="act" id="identify">IDENTIFY NODE</button></div>
</div>
<div class="card"><div class="kv" id="syskv"></div></div>
</main>
<script>
const $=id=>document.getElementById(id);
let S=null;
function kv(el,rows){el.innerHTML=rows.map(r=>`<div>${r[0]}</div><div>${r[1]}</div>`).join('')}
async function load(){
  S=await (await fetch('/api/status')).json();
  const owned=!!S.owned; const em=!!S.emergency;
  $('title').textContent=(S.id||'MOSFET')+' — '+(S.name||'MOSFET Node');
  $('owner').textContent=(S.state||'?')+' · '+(owned?'CONTROLLED BY SHOWDUINO':'LOCAL');
  const ban=$('banner');
  if(em){ban.hidden=false;ban.className='banner bad';ban.textContent='EMERGENCY — all outputs OFF. This page cannot clear Emergency.';}
  else if(owned){ban.hidden=false;ban.className='banner ctrl';ban.textContent='CONTROLLED BY SHOWDUINO — output tests disabled.';}
  else {ban.hidden=true;}
  $('nid').value=S.id||''; $('nname').value=S.name||'';
  kv($('nodekv'),[
    ['Firmware',S.fw||''],['Board',S.board||''],['GPIO map',S.gpioVerified?'VERIFIED':'SOFTWARE DEFINED / HARDWARE UNVERIFIED'],
    ['ESP-NOW',S.espnow?'linked':'searching'],['Owner',S.owned?'yes':'no']
  ]);
  const outs=S.outputs||[];
  $('outs').innerHTML=outs.map((o,i)=>`<div class="ch"><span><span class="dot ${o.level>0?'on':''}"></span>OUT${i+1} ${o.name||''}</span><span>${o.level||0}%</span></div>`).join('');
  kv($('identkv'),[
    ['GPIO',String(S.identifierGpio||25)],
    ['Count',String(S.identifierCount||4)],
    ['Mode','dual-duty: green=OUT; purple/cyan/turquoise/violet=status'],
    ['Ready',S.identifierReady?'yes':'no'],
    ['Verified',S.identifierVerified?'VERIFIED':'SOFTWARE DEFINED / HARDWARE UNVERIFIED']
  ]);
  kv($('syskv'),[
    ['PWM',(S.pwmHz||1000)+' Hz / '+(S.pwmBits||8)+'-bit'],
    ['Pins',(S.pins||[]).join(', ')],
    ['SoftAP',S.ssid||''],['IP',S.ip||'']
  ]);
  ['test','saveid','saveout'].forEach(id=>{$(id).disabled=owned||em;});
  $('identify').disabled=em;
}
$('saveid').onclick=async()=>{await fetch('/api/identity',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({id:$('nid').value,name:$('nname').value})});load();};
$('saveout').onclick=async()=>{await fetch('/api/outname',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ch:+$('och').value,name:$('oname').value})});load();};
$('test').onclick=async()=>{await fetch('/api/test',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ch:+$('tch').value,level:+$('tlv').value})});load();};
$('alloff').onclick=async()=>{await fetch('/api/alloff',{method:'POST'});load();};
$('identify').onclick=async()=>{await fetch('/api/identify',{method:'POST'});load();};
load(); setInterval(load,2000);
</script></body></html>
)HTML";

static void handleRoot() {
  sServer.send_P(200, "text/html", kPage);
}

static void handleStatus() {
  uint8_t lv[4] = {0, 0, 0, 0};
  mosfetOutputEngineLevels(lv);
  char mac[24];
  mosfetEspNowMacString(mac, sizeof(mac));
  String json = "{";
  json += "\"id\":\"" + String(mosfetIdentityId()) + "\",";
  json += "\"name\":\"" + String(mosfetIdentityName()) + "\",";
  json += "\"fw\":\"" + String(SHOWDUINO_MOSFET_NODE_FW) + "\",";
  json += "\"board\":\"" + String(SHOWDUINO_MOSFET_BOARD) + "\",";
  json += "\"state\":\"" + String(showduino_mosfet_state_name(mosfetNodeStateGet())) + "\",";
  json += "\"owned\":" + String(mosfetNodeStateOwned() ? "true" : "false") + ",";
  json += "\"emergency\":" + String(mosfetNodeStateEmergency() ? "true" : "false") + ",";
  json += "\"espnow\":" + String(mosfetEspNowHaveComms() ? "true" : "false") + ",";
  json += "\"gpioVerified\":" + String(SHOWDUINO_MOSFET_GPIO_VERIFIED ? "true" : "false") + ",";
  json += "\"identifierGpio\":" + String(SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_GPIO) + ",";
  json += "\"identifierCount\":" + String(SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_COUNT) + ",";
  json += "\"identifierReady\":" + String(mosfetIdentifierPixelsReady() ? "true" : "false") + ",";
  json += "\"identifierVerified\":" + String(SHOWDUINO_MOSFET_IDENTIFIER_PIXEL_VERIFIED ? "true" : "false") + ",";
  json += "\"pwmHz\":" + String(SHOWDUINO_MOSFET_PWM_HZ) + ",";
  json += "\"pwmBits\":" + String(SHOWDUINO_MOSFET_PWM_BITS) + ",";
  json += "\"pins\":[" + String(SHOWDUINO_MOSFET_OUT1_GPIO) + "," +
          String(SHOWDUINO_MOSFET_OUT2_GPIO) + "," +
          String(SHOWDUINO_MOSFET_OUT3_GPIO) + "," +
          String(SHOWDUINO_MOSFET_OUT4_GPIO) + "],";
  json += "\"ssid\":\"" + String(nodeSoftApSsid()) + "\",";
  json += "\"ip\":\"" + String(nodeSoftApIp()) + "\",";
  json += "\"outputs\":[";
  for (int i = 0; i < 4; ++i) {
    if (i) json += ",";
    json += "{\"name\":\"" + String(mosfetIdentityOutName((uint8_t)(i + 1))) +
            "\",\"level\":" + String(lv[i]) + "}";
  }
  json += "]}";
  sServer.send(200, "application/json", json);
}

static void handleIdentity() {
  if (mosfetNodeStateOwned() || mosfetNodeStateEmergency()) {
    sServer.send(403, "text/plain", "SHOW_CONTROLLED");
    return;
  }
  String body = sServer.arg("plain");
  // Minimal parse: look for "id":"..." and "name":"..."
  int idPos = body.indexOf("\"id\"");
  int namePos = body.indexOf("\"name\"");
  if (idPos >= 0) {
    int q1 = body.indexOf('"', body.indexOf(':', idPos) + 1);
    int q2 = body.indexOf('"', q1 + 1);
    if (q1 >= 0 && q2 > q1) {
      String id = body.substring(q1 + 1, q2);
      mosfetIdentitySetId(id.c_str());
    }
  }
  if (namePos >= 0) {
    int q1 = body.indexOf('"', body.indexOf(':', namePos) + 1);
    int q2 = body.indexOf('"', q1 + 1);
    if (q1 >= 0 && q2 > q1) {
      String name = body.substring(q1 + 1, q2);
      mosfetIdentitySetName(name.c_str());
    }
  }
  char ssid[40];
  snprintf(ssid, sizeof(ssid), "Showduino-%s", mosfetIdentityId());
  nodeSoftApRetitle(ssid);
  sServer.send(200, "text/plain", "OK");
}

static void handleOutName() {
  if (mosfetNodeStateOwned() || mosfetNodeStateEmergency()) {
    sServer.send(403, "text/plain", "SHOW_CONTROLLED");
    return;
  }
  String body = sServer.arg("plain");
  int ch = 1;
  int chPos = body.indexOf("\"ch\"");
  if (chPos >= 0) ch = body.substring(body.indexOf(':', chPos) + 1).toInt();
  int namePos = body.indexOf("\"name\"");
  if (namePos >= 0) {
    int q1 = body.indexOf('"', body.indexOf(':', namePos) + 1);
    int q2 = body.indexOf('"', q1 + 1);
    if (q1 >= 0 && q2 > q1) {
      String name = body.substring(q1 + 1, q2);
      mosfetIdentitySetOutName((uint8_t)ch, name.c_str());
    }
  }
  sServer.send(200, "text/plain", "OK");
}

static void handleTest() {
  if (mosfetNodeStateOwned() || mosfetNodeStateEmergency()) {
    sServer.send(403, "text/plain", "DENIED");
    return;
  }
  String body = sServer.arg("plain");
  int ch = 1;
  int level = 50;
  int chPos = body.indexOf("\"ch\"");
  if (chPos >= 0) ch = body.substring(body.indexOf(':', chPos) + 1).toInt();
  int lvPos = body.indexOf("\"level\"");
  if (lvPos >= 0) level = body.substring(body.indexOf(':', lvPos) + 1).toInt();
  if (ch < 1 || ch > 4) ch = 1;
  if (level < 0) level = 0;
  if (level > 100) level = 100;
  mosfetOutputEnginePulse((uint8_t)ch, (uint8_t)level, SHOWDUINO_MOSFET_LOCAL_TEST_MS);
  sLocalTestUntil = millis() + SHOWDUINO_MOSFET_LOCAL_TEST_MS;
  sServer.send(200, "text/plain", "OK");
}

static void handleAllOff() {
  mosfetOutputEngineAllOff("WEBUI");
  sLocalTestUntil = 0;
  sServer.send(200, "text/plain", "OK");
}

static void handleIdentify() {
  if (mosfetNodeStateEmergency()) {
    sServer.send(403, "text/plain", "EMERGENCY");
    return;
  }
  mosfetIdentifierPixelsIdentify();
  sServer.send(200, "text/plain", "OK");
}

void mosfetWebBegin() {
  if (sBegun) return;
  uint8_t mac[6];
  mosfetEspNowMacBytes(mac);
  char ssid[40];
  snprintf(ssid, sizeof(ssid), "Showduino-%s", mosfetIdentityId());
  nodeSoftApBeginNamed(ssid, mosfetEspNowChannel(), "showduino");
  nodeSoftApSetRadioHook(mosfetEspNowReassert);
  sServer.on("/", HTTP_GET, handleRoot);
  sServer.on("/api/status", HTTP_GET, handleStatus);
  sServer.on("/api/identity", HTTP_POST, handleIdentity);
  sServer.on("/api/outname", HTTP_POST, handleOutName);
  sServer.on("/api/test", HTTP_POST, handleTest);
  sServer.on("/api/alloff", HTTP_POST, handleAllOff);
  sServer.on("/api/identify", HTTP_POST, handleIdentify);
  sServer.begin();
  sBegun = true;
  Serial.printf("[MOSFET] SoftAP %s @ %s\n", nodeSoftApSsid(), nodeSoftApIp());
}

void mosfetWebLoop() {
  if (!sBegun) return;
  if (sLocalTestUntil && (int32_t)(millis() - sLocalTestUntil) >= 0) {
    mosfetOutputEngineAllOff("LOCAL_TEST_END");
    sLocalTestUntil = 0;
  }
  nodeSoftApService();
  sServer.handleClient();
}
