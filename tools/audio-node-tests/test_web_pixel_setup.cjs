const fs=require('fs'), vm=require('vm'), assert=require('assert'), path=require('path');
const source=fs.readFileSync(path.resolve(__dirname,'../../firmware/audio-node-esp32-a1s/ShowduinoAudioNode/src/AudioWeb.cpp'),'utf8');
const script=source.split('<script>')[1].split('</script>')[0];
const elements=new Map();
function element(id){
  if(!elements.has(id)) elements.set(id,{id,value:'',textContent:'',innerHTML:'',disabled:false,hidden:false,dataset:{},classList:{toggle(){}}});
  return elements.get(id);
}
const buttons=['PIXEL:COUNT','PIXEL:INIT','PIXEL:TEST','STATUS:LED:TEST','AUDIO:NODE:PLAY'].map(c=>({dataset:{c},disabled:false}));
const status={owner:'SHOW_CONTROLLED',playback:'IDLE',pixel:{ready:false,busy:false,configured:0,count:0,statusGpio:5,statusCount:1,emergency:false},emergency:false};
let commandOk=true;
const context=vm.createContext({
  document:{activeElement:null,getElementById:element,querySelectorAll(selector){
    if(selector==='button.act'||selector==='button.act[data-c]')return buttons;
    return [];
  }},
  fetch:async(url)=>({json:async()=>url==='/api/status'?status:url==='/api/library'?{files:[]}:{ok:commandOk}}),
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
  assert.equal(buttons[0].disabled,true,'Emergency blocks setup');
  assert.equal(buttons[1].disabled,true,'Emergency blocks initialise');
  assert.equal(buttons[3].disabled,true,'Emergency indication takes priority over tests');
  status.emergency=false; status.owner='STANDALONE'; status.pixel.ready=true; status.pixel.busy=false; await run('load()');
  assert.equal(buttons[0].disabled,false,'Standalone permits reconfiguration');
  console.log('Passed: initial setup, ownership locks, emergency locks, edited-value persistence and failed-save recovery');
})().catch(e=>{console.error(e);process.exitCode=1});
