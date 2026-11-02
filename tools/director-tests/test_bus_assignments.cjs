const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert');
const source=fs.readFileSync(path.resolve(__dirname,'../../firmware/stage-engine-p4/ShowduinoStageEngineP4/src/plugin/PluginBus.cpp'),'utf8');
const start=source.indexOf('bool pluginBusAssignPreset('),open=source.indexOf('{',start);let end=open+1,depth=1;for(;depth;end++){if(source[end]==='{')depth++;if(source[end]==='}')depth--;}
let body=source.slice(open+1,end-1)
 .replace('PluginChip chip;PluginRole role=PluginRole::None;','let chip;let role=0;')
 .replace('pluginChipFromName(chipName,&chip)','(chip=chipIds[chipName]??0,chip!==0)')
 .replace('pluginRoleFromName(roleName,&role)','(role=roleIds[roleName]??0,role!==0)')
 .replace(/PluginChip::(\w+)/g,'Chip.$1').replace(/PluginConfigLoadResult::(\w+)/g,'Load.$1')
 .replace('const PluginInstance *found=nullptr;','let found=null;').replace('found=&sInst[i]','found=sInst[i]')
 .replace('PluginRoleFile next=sRoles;','let next=structuredClone(sRoles);')
 .replace('PluginRoleFile verified;PluginConfigLoadResult validation;','')
 .replace('pluginParseRoleFile(json.c_str(),json.length(),&verified,&validation)','validate(json)')
 .replace(/const bool /g,'const ').replace(/unsigned /g,'let ').replace('String json=','let json=')
 .replace(/char row\[160\];/g,'let row;').replace(/const auto &v=/g,'const v=')
 .replace(/snprintf\(reply,replyLen,([\s\S]*?)\);/g,'reply.text=format($1);')
 .replace(/snprintf\(row,sizeof\(row\),([\s\S]*?)\);/g,'row=format($1);')
 .replace(/json\.c_str\(\)/g,'json').replace(/json\.length\(\)/g,'json.length');
const Chip={Unknown:0,ES8311:1,SX1509:2,MCP23017:3,PCA9685:4,TCA9548A:5},Load={Ok:0,Missing:1,InvalidJson:2};
const chipIds={...Chip},roleIds={DIGITAL_INPUTS:2,DIGITAL_OUTPUTS:3,DIGITAL_IO:4,PWM_OUTPUTS:5,SERVO_OUTPUTS:6,I2C_MULTIPLEXER:7};
const reverse=(obj,v)=>Object.keys(obj).find(k=>obj[k]===v);
const entry=(address,chip,role)=>({busId:0,address,chip,role});
const base={formatVersion:1,deviceCount:2,devices:Array.from({length:20},(_,i)=>i===0?entry(0x3e,2,2):i===1?entry(0x40,4,5):entry(0,0,0))};
let stored='',canWrite=true,scans=0;
const ctx=vm.createContext({Chip,Load,chipIds,roleIds,structuredClone,sRoles:structuredClone(base),sRoleLoad:0,sCount:3,sInst:[{loc:{address:0x3e,muxAddr:0}},{loc:{address:0x40,muxAddr:0}},{loc:{address:0x3f,muxAddr:0x70}}],PLUGIN_ROLE_ADDR_MIN:8,PLUGIN_ROLE_ADDR_MAX:0x77,PLUGIN_BUILTIN_ES8311_ADDR:0x18,PLUGIN_MUX_NONE:0,PLUGIN_ROLE_FILE_FORMAT:1,PLUGIN_ROLE_MAX_DEVICES:20,PATH_PLUGIN_BUS_CONFIG:'roles',
 strcmp:(a,b)=>a===b?0:1,pluginRoleCompatible:(c,r)=>(c===2||c===3)?[2,3,4].includes(r):c===4?[5,6].includes(r):c===5&&r===7,
 pluginChipName:c=>reverse(chipIds,c),pluginRoleName:r=>reverse(roleIds,r),
 format:(fmt,...args)=>{let i=0;return fmt.replace(/%02X|%u|%s/g,t=>{const v=args[i++];return t==='%02X'?v.toString(16).toUpperCase().padStart(2,'0'):String(v);});},
 validate:json=>{const j=JSON.parse(json);return j.formatVersion===1&&j.devices.length<=20&&j.devices.every(v=>v.chip&&v.role);},
 stageStoreAtomicWrite:(p,json)=>{if(!canWrite)return false;stored=json;return true;},loadRoleFile:()=>{const j=JSON.parse(stored);ctx.sRoles={formatVersion:1,deviceCount:j.devices.length,devices:j.devices.map(v=>entry(parseInt(v.address,16),chipIds[v.chip],roleIds[v.role]))};},pluginBusScan:()=>scans++});
vm.runInContext(`function assign(address,chipName,roleName,reply,replyLen){${body}}`,ctx);
function assign(addr,c,r){ctx.args=[addr,c,r,{},72];const ok=vm.runInContext('assign(...args)',ctx);return {ok,message:ctx.args[3].text};}
assert.equal(assign(0x18,'SX1509','DIGITAL_OUTPUTS').ok,false);assert.equal(assign(0x3e,'SX1509','PWM_OUTPUTS').ok,false);assert.equal(assign(0x3f,'SX1509','DIGITAL_INPUTS').ok,false,'Mux device cannot overwrite root assignment');assert.equal(stored,'');
canWrite=false;assert.equal(assign(0x3e,'SX1509','DIGITAL_OUTPUTS').ok,false);assert.equal(ctx.sRoles.devices[0].role,2,'Failed write leaves active assignments unchanged');assert.equal(scans,0);
canWrite=true;assert.equal(assign(0x3e,'SX1509','DIGITAL_OUTPUTS').ok,true);let file=JSON.parse(stored);assert.equal(file.devices[0].role,'DIGITAL_OUTPUTS');assert.equal(file.devices[1].role,'PWM_OUTPUTS','Other assignment preserved');assert.equal(scans,1);
assert.equal(assign(0x3e,'SX1509','NONE').ok,true);assert.equal(JSON.parse(stored).devices.length,1);assert.equal(JSON.parse(stored).devices[0].address,'0x40');
ctx.sRoleLoad=Load.InvalidJson;const before=stored;assert.equal(assign(0x40,'PCA9685','SERVO_OUTPUTS').ok,false);assert.equal(stored,before,'Invalid existing file is not overwritten');
console.log('Passed: actual preset assignment logic, compatibility, protected/mux addresses, atomic save failure, preserved entries and removal');
