const fs=require('fs'),vm=require('vm'),assert=require('assert'),path=require('path'),util=require('util');
const root=path.resolve(__dirname,'../../firmware/director-esp32-8048s050/ShowduinoDirector8048S050');
const source=fs.readFileSync(path.join(root,'page_outputs.cpp'),'utf8');
function body(name){const start=source.search(new RegExp('(?:void|bool) '+name+'\\('));const open=source.indexOf('{',start);let depth=1,end=open+1;for(;depth;end++){if(source[end]==='{')depth++;if(source[end]==='}')depth--;}return source.slice(open+1,end-1);}
function translate(text){return text.replace(/char (\w+)\[\d+\];/g,'let $1;')
 .replace(/snprintf\((\w+), sizeof\(\1\), ([\s\S]*?)\);/g,'$1 = format($2);')
 .replace(/\(unsigned\)\(uintptr_t\)/g,'').replace(/\(unsigned\)/g,'')
 .replace(/const (?:unsigned|bool|int) /g,'const ').replace(/unsigned i/g,'let i')
 .replace(/const char \*/g,'const ').replace(/ShowduinoOsTheme::setEnabled/g,'setEnabled');}
const initial={detail:{firstId:'',haveLevels:0,levels:[0,0,0,0]},linked:false,online:false,emergency:false,showRunning:false};
const slider=Array.from({length:4},()=>({value:0})),live=Array.from({length:4},()=>({})),wanted=Array.from({length:4},()=>({})),apply=Array.from({length:4},()=>({})),off=Array.from({length:4},()=>({})),requests=[];
const ctx=vm.createContext({sModel:initial,sSummary:{},sFeedback:{},sSlider:slider,sLive:live,sRequested:wanted,sApply:apply,sOff:off,sIdentify:{},sAllOff:{},sDirty:[false,false,false,false],
 lv_obj_set_style_bg_color(){},showduino_theme_get_accent:()=>0,LV_PART_INDICATOR:0,LV_PART_KNOB:0,
 sCallback:c=>requests.push(c),format:(fmt,...args)=>util.format(fmt.replace(/%u/g,'%d'),...args),strcmp:(a,b)=>a===b?0:1,
 showduino_mosfet_id_ok:id=>!!id&&id!=='-',setEnabled:(o,on)=>o.enabled=on,
 lv_slider_get_value:o=>o.value,lv_slider_set_value:(o,v)=>o.value=v,LV_ANIM_OFF:0,
 lv_label_set_text:(o,t)=>o.text=t,lv_event_get_user_data:e=>e.index,lv_event_get_target:e=>e.target});
for(const name of ['canReach','canSet','enabled','page_outputs_set_model','channelAction']){
 const args=name==='page_outputs_set_model'?'model':name==='enabled'?'obj,on':name==='channelAction'?'event':'';
 vm.runInContext('function '+name+'('+args+'){'+translate(body(name))+'}',ctx);
}
const online={...initial,linked:true,online:true,detail:{firstId:'MOSFET-01',haveLevels:1,levels:[15,30,0,100]}};
function model(value){ctx.model=value;vm.runInContext('page_outputs_set_model(model)',ctx);}
function action(index,isOff=false){ctx.event={index,target:isOff?off[index]:apply[index]};vm.runInContext('channelAction(event)',ctx);}
model(online);assert.equal(slider[0].value,15);assert.equal(live[0].text,'Actual: 15%');
slider[0].value=55;ctx.sDirty[0]=true;model(online);assert.equal(slider[0].value,55,'Status polling preserves unsubmitted edit');assert.equal(live[0].text,'Actual: 15%');assert.equal(requests.length,0,'Rendering never commands outputs');
action(0);assert.equal(requests.at(-1),'OUTPUTS:MOSFET:OUT:1:LEVEL:55');assert.equal(live[0].text,'Actual: 15%','Applying does not invent readback');
model({...online,showRunning:true});assert.equal(apply[0].enabled,false);let count=requests.length;action(0);assert.equal(requests.length,count);action(0,true);assert.equal(requests.at(-1),'OUTPUTS:MOSFET:OUT:1:OFF');
model({...online,emergency:true});assert.equal(apply[0].enabled,false);assert.equal(off[0].enabled,true);count=requests.length;action(0);assert.equal(requests.length,count);
model({...online,linked:false});assert.equal(off[0].enabled,false);assert.equal(live[0].text,'Actual: --');count=requests.length;action(0,true);assert.equal(requests.length,count);
model({...online,detail:{...online.detail,haveLevels:0}});assert.equal(live[0].text,'Actual: --','Old P4 state never appears as a zero level');
model({...online,detail:{...online.detail,firstId:'MOSFET-02',levels:[0,0,0,0]}});assert.equal(slider[0].value,0,'Changing node discards old edits');
const footer=vm.runInNewContext(source.match(/const int footerY = (.*);/)[1],{OS_DOCK_Y:402,OS_GAP:8});
const rows=vm.runInNewContext(source.match(/const int rowsY = (.*);/)[1],{footerY:footer});
const dock=402,gap=8;
assert.equal(rows,150);assert(rows+3*48+40<=footer);assert.equal(footer+44,dock-gap);
console.log('Passed: reported/requested level separation, dirty edits, explicit apply, emergency/show/link locks, OFF handling, node changes and dock containment');
