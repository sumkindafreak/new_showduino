const fs=require('fs'), vm=require('vm'), assert=require('assert'), path=require('path');
const source=fs.readFileSync(path.resolve(__dirname,'../../firmware/audio-node-esp32-a1s/ShowduinoAudioNode/src/AudioWeb.cpp'),'utf8');
const script=source.split('<script>')[1].split('</script>')[0];
const elements=new Map();
function element(id){
  if(!elements.has(id)) elements.set(id,{id,value:'',textContent:'',innerHTML:'',disabled:false,hidden:false,dataset:{},classList:{toggle(){}}});
  return elements.get(id);
}
const buttons=['PIXEL:COUNT','PIXEL:INIT','PIXEL:TEST','STATUS:LED:TEST','AUDIO:NODE:PLAY'].map(c=>({dataset:{c},disabled:false}));
const status={owner:'SHOW_CONTROLLED',playback:'IDLE',pixel:{ready:false,busy:false,configured:0,count:0,statusGpio:5,statusCount:1,emergency:false},emergency:false,output:'SPEAKER',outputConfigured:'SPEAKER'};
let commandOk=true;
const configRequests=[];
const context=vm.createContext({
  document:{activeElement:null,getElementById:element,querySelectorAll(selector){
    if(selector==='button.act'||selector==='button.act[data-c]')return buttons;
    return [];
  }},
  fetch:async(url,options)=>{
    if(url==='/api/config'&&options&&options.method==='POST'){
      const body=JSON.parse(options.body); configRequests.push(body);
      if(commandOk&&body.output){status.outputConfigured=body.output; status.output=body.output;}
    }
    return {json:async()=>url==='/api/status'?status:url==='/api/library'?{files:[]}:{ok:commandOk}};
  },
  setInterval(){},alert(){},Math,Number,JSON
});
function run(code){return vm.runInContext(code,context);}
async function flush(){for(let i=0;i<10;i++)await Promise.resolve();}
(async()=>{
  run(script); await flush();
  assert.equal(buttons[0].disabled,false,'Count must be available while owned and uninitialised');
  assert.equal(buttons[1].disabled,false,'Initialise must be available while owned and uninitialised');
  assert.equal(buttons[2].disabled,true,'Show tests must remain locked');
  assert.equal(buttons[3].disabled,false,'Status test is independent of show ownership');
  assert.equal(buttons[4].disabled,true,'Audio playback remains locked');
  assert.equal(element('saveoutput').disabled,false,'Idle hardware output setup is available under P4 ownership');
  assert.equal(element('output').value,'SPEAKER','Output shows the saved configuration');
  element('output').value='HEADPHONE'; element('output').onchange();
  await run('load()'); assert.equal(element('output').value,'HEADPHONE','Polling preserves edited output');
  commandOk=false; await element('saveoutput').onclick();
  assert.equal(run('outputDirty'),true,'Failed output save preserves the edit');
  assert.equal(element('output').value,'HEADPHONE');
  commandOk=true; await element('saveoutput').onclick();
  assert.equal(configRequests.at(-1).output,'HEADPHONE','Output save sends the selected route');
  assert.equal(run('outputDirty'),false,'Successful output save clears the edit');
  assert.equal(element('output').value,'HEADPHONE');
  status.playback='PLAYING'; await run('load()');
  assert.equal(element('saveoutput').disabled,true,'Active playback blocks output reconfiguration');
  status.playback='PAUSED'; await run('load()');
  assert.equal(element('saveoutput').disabled,true,'Paused playback blocks output reconfiguration');
  status.playback='IDLE'; await run('load()');
  element('pixcount').value='25'; element('pixcount').oninput();
  await run('load()'); assert.equal(element('pixcount').value,'25','Polling must preserve edited count');
  commandOk=false; await run("send('PIXEL:COUNT:25')"); await flush();
  assert.equal(element('pixcount').value,'25','Failed save must preserve edited count');
  commandOk=true; status.pixel.configured=25;
  await run("send('PIXEL:COUNT:25')"); await flush();
  assert.equal(run('pixelCountDirty'),false,'Successful save clears dirty flag');
  status.pixel.ready=true; await run('load()');
  assert.equal(buttons[0].disabled,false,'Idle default strip can be commissioned under P4 control');
  status.pixel.busy=true; await run('load()');
  assert.equal(buttons[0].disabled,true,'Active effects block count changes under P4 control');
  assert.equal(buttons[1].disabled,true,'Active effects block initialisation');
  status.pixel.ready=false; status.emergency=true; await run('load()');
  assert.equal(element('saveoutput').disabled,true,'Emergency blocks output reconfiguration');
  assert.equal(buttons[0].disabled,true,'Emergency blocks setup');
  assert.equal(buttons[1].disabled,true,'Emergency blocks initialise');
  assert.equal(buttons[3].disabled,true,'Emergency indication takes priority over tests');
  status.emergency=false; status.owner='STANDALONE'; status.pixel.ready=true; status.pixel.busy=false; await run('load()');
  assert.equal(buttons[0].disabled,false,'Standalone permits reconfiguration');
  status.outputConfigured='AUTO'; status.output='HEADPHONE'; await run('load()');
  assert.equal(element('output').value,'AUTO','Polling preserves AUTO even when the active route is HEADPHONE');
  console.log('Passed: initial setup, ownership locks, emergency locks, edited-value persistence, headphone output setup and failed-save recovery');
})().catch(e=>{console.error(e);process.exitCode=1});
