const fs=require('fs'),vm=require('vm'),assert=require('assert'),path=require('path');
const source=fs.readFileSync(path.resolve(__dirname,'../../protocol/showduino_state_wire.h'),'utf8');
const start=source.indexOf('  if (levels) {'),open=source.indexOf('{',start);let depth=1,end=open+1;
for(;depth;end++){if(source[end]==='{')depth++;if(source[end]==='}')depth--;}
// Execute the actual optional level decoder with C string/pointer operations
// adapted to JavaScript strings; malformed optional data must remain unknown.
const body=source.slice(open+1,end-1)
 .replace('const char *value = levels + 3;','let value = levels.slice(3);')
 .replace('uint8_t parsed[4];','let parsed = Array(4);')
 .replace('unsigned i','let i').replace('char *end;','let end;')
 .replace('unsigned long level = strtoul(value, &end, 10);','const read=readUnsigned(value); const level=read.level; end=read.end;')
 .replace(/\*value/g,'value[0]').replace(/\*end/g,"(end[0]||'\\0')")
 .replace('(uint8_t)level','level').replace('value = end + (i < 3 ? 1 : 0);','value = end.slice(i < 3 ? 1 : 0);')
 .replace('memcpy(out->levels, parsed, sizeof(parsed));','out.levels = parsed;').replace(/out->/g,'out.');
const ctx=vm.createContext({readUnsigned:s=>{const token=s.match(/^\d+/)?.[0]||'';return {level:Number(token),end:s.slice(token.length)};}});
vm.runInContext('function decode(levels){const out={haveLevels:0,levels:[0,0,0,0]};function parse(){'+body+'} if(levels)parse();return out;}',ctx);
function decode(s){ctx.input=s;return vm.runInContext('decode(input)',ctx);}
assert.equal(decode('').haveLevels,0,'Legacy reports leave levels unknown');
let good=decode(':O=0,25,50,100');assert.equal(good.haveLevels,1);assert.equal(JSON.stringify(good.levels),'[0,25,50,100]');
for(const bad of [':O=101,0,0,0',':O=-1,0,0,0',':O=0,0,0',':O=0,0,0,0,1',':O=0,0,0,0junk',':O=0,0,x,0',':O=999999999999999999999999,0,0,0'])assert.equal(decode(bad).haveLevels,0,bad);
const largest='STATE:NODE:MOSFET:D:255:255:'+'X'.repeat(15)+':'+'X'.repeat(19)+':O=100,100,100,100';assert(largest.length<96,'Detail report fits the radio command payload');
console.log('Passed: optional MOSFET readback, legacy fallback, malformed/out-of-range levels and wire length');
