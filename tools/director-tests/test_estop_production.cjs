const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert');
const source=fs.readFileSync(path.resolve(__dirname,'../../firmware/stage-engine-p4/ShowduinoStageEngineP4/src/ProductionFormat.cpp'),'utf8');
const hardware=source.match(/const bool hardwareCue = ([\s\S]*?);/)[1];
const guard=source.match(/if \(\(strcmp\(cue->type, "PIXEL"\)[\s\S]*?\) \{/)[0].slice(4,-3);
const adapt=s=>s.replace(/cue->/g,'cue.');
const ctx=vm.createContext({strcmp:(a,b)=>a===b?0:1,strncmp:(a,b,n)=>a.slice(0,n)===b.slice(0,n)?0:1});
vm.runInContext('function check(cue){return ('+adapt(hardware)+') && !('+adapt(guard)+');}',ctx);
for(const command of ['ESTOP:NODE:PIXEL:SEGMENT:0:START','ESTOP:NODE:PIXEL:SEGMENT:0:STOP']){ctx.cue={type:'ESTOP',command};assert(vm.runInContext('check(cue)',ctx));}
for(const command of ['ESTOP:CLEAR','ESTOP:NODE:ESTOP-01:REARM','AUDIO:NODE:STOP','PIXEL:OFF']){ctx.cue={type:'ESTOP',command};assert(!vm.runInContext('check(cue)',ctx),command);}
console.log('Passed: Emergency pixel cues accepted; clear/rearm and unrelated commands rejected');
