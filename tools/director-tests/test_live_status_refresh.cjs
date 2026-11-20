const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const root = path.resolve(__dirname, '../../firmware/director-esp32-8048s050/ShowduinoDirector8048S050');
const header = fs.readFileSync(path.join(root, 'page_10_live.h'), 'utf8');
const page = fs.readFileSync(path.join(root, 'page_10_live.cpp'), 'utf8');
const sketch = fs.readFileSync(path.join(root, 'ShowduinoDirector8048S050.ino'), 'utf8');
assert.match(header, /#define PAGE10_CMD_STATUS "UI:LIVE:STATUS"/);
assert.match(page, /"STATUS"[^\n]*PAGE10_CMD_STATUS/);

const start = sketch.indexOf('if (command == PAGE10_CMD_STATUS) {');
assert.ok(start >= 0, 'The normal UI dispatcher handles the Live button');
const open = sketch.indexOf('{', start);
let end = open + 1;
let depth = 1;
while (depth > 0 && end < sketch.length) {
  if (sketch[end] === '{') depth++;
  if (sketch[end] === '}') depth--;
  end++;
}
assert.equal(depth, 0);
const body = sketch.slice(open + 1, end - 1);

function run({ linked = true, accepted = true, alreadyWaiting = false } = {}) {
  const calls = [];
  const context = vm.createContext({
    linkState: linked ? 1 : 0,
    LINK_READY: 1,
    SHOW_STATE_QUERY: 'SHOW:STATE?',
    ui: {
      beginLiveStatusRefresh() { calls.push('begin'); return !alreadyWaiting; },
      failLiveStatusRefresh() { calls.push('failed'); },
      appendLog(message) { calls.push(message); },
    },
    sendToStage(command) { calls.push(command); return accepted; },
  });
  vm.runInContext('function pressStatus() {' + body + '} pressStatus();', context);
  return calls;
}

let calls = run();
assert.equal(calls.filter(x => x === 'SHOW:STATE?').length, 1);
assert.ok(!calls.includes('failed'));
assert.deepEqual(run({ alreadyWaiting: true }), ['begin']);
calls = run({ linked: false });
assert.ok(calls.includes('failed'));
assert.ok(!calls.includes('SHOW:STATE?'));
calls = run({ accepted: false });
assert.ok(calls.includes('failed'));
assert.equal(calls.filter(x => x === 'SHOW:STATE?').length, 1);
console.log('PASS Live Status button: dispatch, playback query, duplicate press and offline/rejected send');
