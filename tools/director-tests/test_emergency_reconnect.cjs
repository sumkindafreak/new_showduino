const fs=require('fs'),path=require('path'),vm=require('vm'),assert=require('assert');
const root=path.resolve(__dirname,'../..');
const source=fs.readFileSync(path.join(root,'firmware/c3-emergency-node/ShowduinoC3EmergencyNode/src/EspNowEmergencyTransport.cpp'),'utf8');
const fresh=source.match(/bool emergencyEspNowLinkFresh\(\)\s*\{([\s\S]*?)\n\}/)[1]
 .replace('const uint32_t last','const last').replace('(uint32_t)(millis() - last)','((millis() - last) >>> 0)');
const route=source.match(/const uint8_t \*dest = ([^;]+);/)[1];
const ctx=vm.createContext({sHaveComms:true,sLastRx:100,now:200,SHOWDUINO_RADIO_LINK_FRESH_MS:8000,sCommsMac:'unicast',kBroadcast:'broadcast'});
vm.runInContext('function millis(){return now;}function emergencyEspNowLinkFresh(){'+fresh+'}function destination(){return '+route+';}',ctx);
function target(now,last=100,known=true){ctx.now=now;ctx.sLastRx=last;ctx.sHaveComms=known;return vm.runInContext('destination()',ctx);}
assert.equal(target(200),'unicast','Fresh replies permit unicast');
assert.equal(target(8099),'unicast','Just before expiry');
assert.equal(target(8100),'broadcast','Stale peer must not strand discovery');
assert.equal(target(10000),'broadcast','Queued sends do not renew receive freshness');
assert.equal(target(10000,9999),'unicast','Fresh reply restores unicast');
assert.equal(target(10000,9999,false),'broadcast','Unknown gateway uses discovery');
assert.equal(target(10000,0),'broadcast','No reply yet');
assert.equal(target(0x100,0xffffff00),'unicast','Freshness survives uptime wrap');
assert.equal(target(0x3000,0xffffff00),'broadcast','Expiry survives uptime wrap');
console.log('Passed: fresh, stale, startup, rediscovery and uptime-wrap Emergency Node routing');
