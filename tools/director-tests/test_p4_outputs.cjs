const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert'),util=require('util');
const source=fs.readFileSync(path.resolve(__dirname,'../../firmware/director-esp32-8048s050/ShowduinoDirector8048S050/page_p4_io.cpp'),'utf8');
function body(name){let start=source.indexOf(name+'('),open=source.indexOf('{',start),d=1,end=open+1;for(;d;end++){if(source[end]==='{')d++;if(source[end]==='}')d--;}return source.slice(open+1,end-1);}
function convert(s){return s.replace(/s\[end\]==0/g,'end===s.length').replace('i=n/3','i=Math.floor(n/3)')
 .replace(/sscanf\(s,"([^"]+)",([^)]*)\)/g,(_,fmt,vars)=>`(scan(s,${JSON.stringify(fmt)}), [${vars.replace(/&/g,'')}] = scanValues, scanCount)`)
 .replace(/char mode\[9\]=\{\},level\[5\]=\{\};/g,'let mode="",level="";')
 .replace(/char c\[16\]=\{\},r\[24\]=\{\},buf\[160\];/g,'let c="",r="",buf="";')
 .replace(/char (\w+)\[\d+\];/g,'let $1;')
 .replace(/snprintf\((\w+),sizeof\(\1\),([\s\S]*?)\);/g,'$1=format($2);')
 .replace(/(?:auto|const auto) &v=/g,'const v=')
 .replace(/\(uintptr_t\)/g,'').replace(/\(unsigned\)/g,'')
 .replace(/\bunsigned /g,'let ').replace(/\bint end=/g,'let end=')
 .replace(/\bbool changed=/g,'let changed=').replace(/ShowduinoOsTheme::setEnabled/g,'enable');}
function scan(s,fmt){let regex='^',tokens=[];for(let i=0;i<fmt.length;){if(fmt[i]==='%'){const token=fmt.slice(i).match(/^%(?:\d+\[\^:\]|\d+s|u|x|n)/)[0];i+=token.length;if(token==='%n'){tokens.push('n');continue;}tokens.push(token);regex+=token==='%u'?'([0-9]+)':token==='%x'?'([0-9A-Fa-f]+)':token.endsWith('s')?'(\\S+)':`([^:]{1,${token.match(/\d+/)[0]}})`;}else {regex+=fmt[i].replace(/[.*+?^${}()|[\]\\]/g,'\\$&');i++;}}
 const m=s.match(new RegExp(regex));ctx.scanCount=0;ctx.scanValues=[];if(!m)return;let j=1;for(const t of tokens){if(t==='n'){ctx.scanValues.push(m[0].length);continue;}ctx.scanCount++;ctx.scanValues.push(t==='%u'?Number(m[j++]):t==='%x'?parseInt(m[j++],16):m[j++]);}}
const requests=[],objs=()=>Array.from({length:2},()=>({}));
const ctx=vm.createContext({linked:true,locked:false,lines:Array.from({length:2},()=>({known:false,config:false,mode:0,high:1,pull:0,debounce:30,active:0,level:0})),states:objs(),on:objs(),off:objs(),configButtons:objs(),feedback:{},editor:{},fields:[{},{},{},{}],saveButton:{},editing:0,values:[],busState:{},busDevice:{},busFeedback:{},busSave:{},busScan:{},busNext:{},chipButton:{},roleButton:{},busIndex:0,busTotal:0,address:0,mux:0,chip:0,role:0,busKnown:false,busDirty:false,
 modes:['DISABLED','INPUT','OUTPUT'],pulls:['NONE','UP','DOWN'],chips:['SX1509','MCP23017','PCA9685','TCA9548A'],roles:['DIGITAL_INPUTS','DIGITAL_OUTPUTS','DIGITAL_IO','PWM_OUTPUTS','SERVO_OUTPUTS','I2C_MULTIPLEXER','NONE'],
 scan,scanValues:[],scanCount:0,format:(f,...a)=>util.format(f.replace(/%u/g,'%d'),...a),enable:(o,v)=>o.enabled=v,
 strcmp:(a,b)=>a===b?0:1,strncmp:(a,b,n)=>a.slice(0,n)===b.slice(0,n)?0:1,strstr:(a,b)=>a.includes(b),
 lv_label_set_text:(o,s)=>o.text=s,lv_event_get_user_data:e=>e.data,lv_obj_remove_flag(){},LV_OBJ_FLAG_HIDDEN:0,
 editLabels(){},busLabels(){},fetchBus(){requests.push('BUS REFRESH')},request:s=>requests.push(s),page_p4_io_feedback:s=>ctx.feedback.text=s});
for(const [name,args] of [['render',''],['action','e'],['page_p4_io_receive','s']])vm.runInContext(`function ${name}(${args}){${convert(body(name))}}`,ctx);
function receive(s){ctx.s=s;vm.runInContext('page_p4_io_receive(s)',ctx);}
function action(data){ctx.e={data};vm.runInContext('action(e)',ctx);}
receive('STATE:IO:1:MODE:OUTPUT:GPIO:46:LEVEL:LOW:ACTIVE:0');receive('STATE:IO:1:CFG:2,1,0,30');assert.equal(ctx.lines[0].known,true);assert.equal(ctx.lines[0].config,true);assert.equal(ctx.on[0].enabled,true);assert.equal(requests.length,0);
action(0);assert.equal(requests.pop(),'OUTPUTS:IO:1:ON');assert.equal(ctx.lines[0].active,0,'Command send never invents readback');
ctx.locked=true;vm.runInContext('render()',ctx);assert.equal(ctx.on[0].enabled,false);action(0);assert.equal(requests.length,0);action(1);assert.equal(requests.pop(),'OUTPUTS:IO:1:OFF');action(2);assert.equal(ctx.editing,0,'Configuration locked');
ctx.locked=false;receive('STATE:IO:2:MODE:INPUT:GPIO:47:LEVEL:HIGH:ACTIVE:1');action(3);assert.equal(requests.length,0,'Inputs cannot be activated');assert.equal(ctx.on[1].enabled,false);
receive('STATE:IO:1:CFG:9,1,0,30');assert.equal(ctx.lines[0].mode,2);receive('STATE:IO:1:CFG:2,1,0,5001');assert.equal(ctx.lines[0].debounce,30);receive('STATE:IO:1:MODE:OUTPUT:GPIO:47:LEVEL:HIGH:ACTIVE:1');assert.equal(ctx.lines[0].active,0,'Wrong physical pin is rejected');
receive('STATE:BUS:0:2:3E:00:1:SX1509:DIGITAL_INPUTS:1');assert.equal(ctx.busKnown,true);assert.equal(ctx.busSave.enabled,true);assert.equal(ctx.role,0);
ctx.busDirty=true;ctx.role=2;receive('STATE:BUS:0:2:3E:00:1:SX1509:DIGITAL_INPUTS:1');assert.equal(ctx.role,2,'Polling preserves preset edits');receive('STATE:BUS:RESULT:SD save failed; assignment unchanged.');assert.equal(ctx.busDirty,true);assert(ctx.busFeedback.text.includes('SD save failed'));
receive('STATE:BUS:0:2:18:00:1:ES8311:P4_INTERNAL_AUDIO:1');assert.equal(ctx.busSave.enabled,false,'Internal audio protected');receive('STATE:BUS:0:2:3E:70:1:SX1509:DIGITAL_INPUTS:1');assert.equal(ctx.busSave.enabled,false,'Mux device assignment not treated as root');
ctx.linked=false;vm.runInContext('render()',ctx);assert.equal(ctx.on[0].enabled,false);assert.equal(ctx.off[0].enabled,false);assert(ctx.states[0].text.includes('Awaiting'));assert.equal(ctx.busSave.enabled,false);
console.log('Passed: P4 readback validation, explicit activation, input/read-only behavior, safety locks, bus protection and preserved preset edits/errors');
