const fs=require('fs'),vm=require('vm'),assert=require('assert'),path=require('path');
const root=path.resolve(__dirname,'../../firmware/director-esp32-8048s050/ShowduinoDirector8048S050');
function body(text,needle){let a=text.indexOf(needle),start=text.indexOf('{',a),end=start+1,d=1;assert(a>=0);for(;d;end++){if(text[end]==='{')d++;if(text[end]==='}')d--;}return text.slice(start+1,end-1);}
const transport=fs.readFileSync(path.join(root,'EspNowTransport.h'),'utf8');
let send=body(transport,'bool sendCommand(const String &command)')
 .replace('uint32_t waitFrom','let waitFrom').replaceAll('(uint32_t)(millis() - waitFrom)','((millis() - waitFrom) >>> 0)')
 .replaceAll('250UL','250').replace('ShowduinoEspNowPacket packet = {};','const packet = {};')
 .replace(/command\.substring\([^\n]+;/,'packet.command = command.value;')
 .replace('esp_err_t result = esp_now_send(stageBridgeMac, (uint8_t *)&packet, sizeof(packet));','const result = radioSend(packet);');
function radio(options={}){
 const c=vm.createContext({online:true,sendBusy:false,lastCallbackOk:false,callbackSeen:false,lastSendOk:false,lastCommand:null,lastSequence:0,nextSequence:1,now:0,SHOWDUINO_ESPNOW_COMMAND_MAX:96,SHOWDUINO_ESPNOW_MAGIC:1,SHOWDUINO_ESPNOW_VERSION:1,ESP_OK:0,...options});
 c.delivered=[];c.pending=null;c.millis=()=>c.now;
 c.delay=ms=>{c.now=(c.now+ms)>>>0;if(c.pending&&((c.now-c.pending.start)>>>0)>=c.pending.after){c.sendBusy=false;c.callbackSeen=true;c.lastCallbackOk=c.pending.ok;c.pending=null;}};
 c.radioSend=p=>{if(options.reject)return -1;c.delivered.push({...p});c.pending={start:c.now,after:options.after??4,ok:options.ok??true};return 0;};
 vm.runInContext('function send(command){'+send+'}',c);return c;
}
function sendOne(c,value){c.input={value,length:()=>value.length};return vm.runInContext('send(input)',c);}
let c=radio();for(let i=0;i<177;i++)assert(sendOne(c,'SHOW:TL:C:'+i+':PIXEL:OFF'));assert.equal(c.delivered.length,177);assert(c.delivered.every((p,i)=>p.command==='SHOW:TL:C:'+i+':PIXEL:OFF'),'No cues dropped or reordered during a full upload');
c=radio({sendBusy:true});c.pending={start:0,after:7,ok:true};assert(sendOne(c,'SHOW:TL:END'));assert.equal(c.delivered.length,1,'Prior callback is awaited');
c=radio({sendBusy:true});assert(!sendOne(c,'SHOW:TL:END'));assert.equal(c.delivered.length,0,'Stuck previous send cannot overwrite callback state');
c=radio({ok:false});assert(!sendOne(c,'SHOW:TL:END'),'Failed delivery must not report success');
c=radio({reject:true});assert(!sendOne(c,'SHOW:TL:END'));assert(!c.sendBusy);
c=radio({after:500});assert(!sendOne(c,'SHOW:TL:END'));assert(c.sendBusy,'Late callback stays owned by prior frame');
c=radio({now:0xfffffffe});assert(sendOne(c,'SHOW:TL:END'),'Deadline survives uptime wrap');
c=radio();assert(!sendOne(c,''));assert(!sendOne(c,'x'.repeat(96)));assert.equal(c.delivered.length,0,'Reject oversized commands rather than truncate');
const sketch=fs.readFileSync(path.join(root,'ShowduinoDirector8048S050.ino'),'utf8');
let run=body(sketch,'bool requestShowRun(const char *idOrName) {')
 .replace('ShowManager &sm','const sm').replace('const ShowIndexEntry *entry','const entry').replace('ShowDefinition def;','const def = {};').replaceAll('entry->','entry.').replace('const char *key','let key');
function runCase(o={}){
 const r=vm.createContext({emergencyLocked:false,linkState:1,LINK_READY:1,verifiedTimelineId:'show-a',gShowMirror:{state:2,totalCues:177},SHOW_STATE_SHOW_LOADED:2,SHOW_STATE_PAUSED:3,SHOW_STATE_FINISHED:4,strcmp:(a,b)=>a===b?0:1,...o});
 r.commands=[];r.uploads=[];r.logs=[];r.ui={appendLog:s=>r.logs.push(s)};
 r.gStorage={showManager:()=>({hasCurrentShow:()=>true,currentShow:()=>({id:'show-a'}),findByIdOrName:id=>({id}),loadShow:()=>true})};
 r.uploadShowTimelineToStage=id=>{r.uploads.push(id);return o.uploadOk??true;};r.sendToStage=cmd=>{r.commands.push(cmd);return o.sendOk??true;};
 vm.runInContext('function run(idOrName){'+run+'}',r);r.ok=vm.runInContext('run("show-a")',r);return r;
}
let r=runCase({verifiedTimelineId:'',uploadOk:false});assert(!r.ok);assert.deepEqual(r.commands,[],'Unverified/partial timeline cannot run');
r=runCase({verifiedTimelineId:'old-show',uploadOk:false});assert(!r.ok);assert.deepEqual(r.commands,[],'Prior loaded show cannot substitute for selected show');
r=runCase({verifiedTimelineId:'',uploadOk:true});assert(r.ok);assert.deepEqual(r.uploads,['show-a']);assert.deepEqual(r.commands,['SHOW:RUN']);
r=runCase();assert(r.ok);assert.deepEqual(r.uploads,[],'Already verified show can rerun');
for(const o of [{emergencyLocked:true},{linkState:0}]){r=runCase(o);assert(!r.ok);assert.deepEqual(r.commands,[]);assert.deepEqual(r.uploads,[]);}
console.log('Passed: 177-cue delivery, callback failures/timeouts, busy serialization, uptime wrap and verified-show run gates');
