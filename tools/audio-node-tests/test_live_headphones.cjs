const fs=require('fs'),vm=require('vm'),assert=require('assert'),path=require('path');
const source=fs.readFileSync(path.resolve(__dirname,'../../firmware/audio-node-esp32-a1s/ShowduinoAudioNode/src/AudioCodec.cpp'),'utf8');
function body(name){
 const start=source.search(new RegExp('(?:bool|void) '+name+'\\(')),open=source.indexOf('{',start); let depth=1,end=open+1;
 for(;depth;end++){if(source[end]==='{')depth++;if(source[end]==='}')depth--;}
 return source.slice(open+1,end-1);
}
// Execute the firmware's actual service and output-selection bodies. Translate
// only C++ type declarations and fixed-size string operations; hardware is mocked.
function translate(text){return text
 .replace(/\(uint32_t\)\(now - (\w+)\)/g,'((now - $1) >>> 0)')
 .replace(/\(uint32_t\)/g,'')
 .replace(/char previous\[sizeof\(sOutput\)\];/g,'let previous;')
 .replace(/memcpy\((\w+), (\w+), sizeof\(\w+\)\);/g,'$1 = $2;')
 .replace(/strncpy\(sOutput, use, sizeof\(sOutput\) - 1\);/g,'sOutput = use;')
 .replace(/snprintf\(sOutput, sizeof\(sOutput\), "%s", target\);/g,'sOutput = target;')
 .replace(/const char \*(\w+)/g,'let $1')
 .replace(/const (uint32_t|bool) /g,'const ');
}
let now=0,inserted=false,fail=false,volumeCalls=0,logs=[];
const context=vm.createContext({sReady:true,sAutoOutput:false,sOutputRetryPending:false,sHpCandidate:false,sHpChangedAt:0,
 sOutputAttemptAt:0,sOutput:'SPEAKER',sLastPercent:37,sMuted:false,kJackDebounceMs:150,kOutputRetryMs:1000,
 millis:()=>now,audioCodecHpInserted:()=>inserted,
 showduino_audio_output_ok:s=>['AUTO','SPEAKER','HEADPHONE','LINE'].includes(s),
 strcmp:(a,b)=>a===b?0:1,
 audioCodecSetPa:on=>{context.pa=on;},
 audioCodecSetVolume:percent=>{volumeCalls++;context.lastAppliedVolume=percent;if(fail)return false;
   context.pa=!context.sMuted&&context.sOutput==='SPEAKER'&&percent>0;return true;},
 Serial:{printf:(...args)=>logs.push(args)},pa:false});
vm.runInContext('function audioCodecApplyOutput(mode){'+translate(body('audioCodecApplyOutput'))+'}\nfunction audioCodecService(){'+translate(body('audioCodecService'))+'}',context);
function run(s){return vm.runInContext(s,context);}
function tick(t,hp){now=t;inserted=hp;run('audioCodecService()');}
assert.equal(run("audioCodecApplyOutput('AUTO')"),true);
tick(10,true);tick(100,false);tick(110,true);tick(259,true);
assert.equal(context.sOutput,'SPEAKER','Jack bounce must not switch the route');
tick(260,true);assert.equal(context.sOutput,'HEADPHONE');assert.equal(context.pa,false);
assert.equal(context.lastAppliedVolume,37,'Switch preserves current volume');
let calls=volumeCalls;tick(500,true);assert.equal(volumeCalls,calls,'Stable jack does not repeat codec writes');
tick(510,false);tick(660,false);assert.equal(context.sOutput,'SPEAKER','Successful routing does not throttle a quick unplug');
tick(670,true);tick(820,true);assert.equal(context.sOutput,'HEADPHONE');
context.sMuted=true;tick(1300,false);tick(1450,false);
assert.equal(context.sOutput,'SPEAKER');assert.equal(context.pa,false,'Unplug while muted must not enable amplifier');
assert.equal(context.sMuted,true,'Automatic routing preserves mute');
context.sMuted=false;tick(2500,true);tick(2650,true);assert.equal(context.sOutput,'HEADPHONE');
fail=true;tick(3800,false);tick(3950,false);assert.equal(context.sOutput,'HEADPHONE','Failed switch retains reported route');
assert.equal(context.pa,false);calls=volumeCalls;tick(4000,false);assert.equal(volumeCalls,calls,'Failed writes are rate limited');
fail=false;tick(4950,false);assert.equal(context.sOutput,'SPEAKER');assert.equal(context.pa,true);
run("audioCodecApplyOutput('HEADPHONE')");tick(6000,false);tick(6200,false);assert.equal(context.sOutput,'HEADPHONE','Explicit output overrides AUTO');
// Debounce elapsed-time arithmetic also works across millis() rollover.
now=0xffffff00;inserted=false;run("audioCodecApplyOutput('AUTO')");tick(0xfffffff0,true);tick(140,true);
assert.equal(context.sOutput,'HEADPHONE','Debounce works across timer wrap');
console.log('Passed: live insertion/removal, bounce rejection, volume/mute preservation, fixed modes, failed-write retry and timer wrap');
