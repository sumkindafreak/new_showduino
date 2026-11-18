// Exercise the actual Outputs page against a small DOM/transport fixture.
const fs = require('node:fs');
const assert = require('node:assert/strict');
const path = require('node:path');
class Element {
  constructor(tag, props = {}) { this.tag = tag; this.children = []; this.events = {}; Object.assign(this, props); if (props.onClick) this.events.click = props.onClick; }
  append(...nodes) { this.children.push(...nodes); }
  set innerHTML(value) { this.children = []; }
  addEventListener(name, fn) { this.events[name] = fn; }
}
const source = fs.readFileSync(path.join(__dirname, '../../web/showduino-studio/js/pages/Outputs.js'), 'utf8')
  .replace(/^import .*;\s*$/gm, '').replace('export async function', 'async function');
const statusSource = fs.readFileSync(path.join(__dirname, '../../web/showduino-studio/js/status.js'), 'utf8');
// Use the real Emergency word mapping rather than changing its input contract.
const emergencyWord = new Function(statusSource.replace(/export function/g, 'function') + '\nreturn emergencyWord;')();
const el = (tag, props) => new Element(tag, props);
const calls = [];
let subscriber;
let snapshot = { p4Online: true, system: { emergencyActive: false } };
const pageFactory = new Function('fetchLighting', 'postCommand', 'isP4Offline', 'subscribeStore', 'el', 'p4OfflineBanner', 'plannedNote', 'statRow', 'emergencyWord', 'document', 'setInterval', 'clearInterval', source + '\nreturn OutputsPage;');
const OutputsPage = pageFactory(async () => ({ p4Online: true }), async cmd => {
  calls.push(cmd);
  return { p4Online: true, replies: cmd === 'AMBIENCE:FILE:0' ? 'AMBIENCE:FILE:0:room.wav' : 'OK:' + cmd };
}, data => !data || data.p4Online === false, callback => { subscriber = callback; callback(snapshot); return () => {}; }, el, () => el('p', { text: 'offline' }), text => el('p', { text }), (name, value) => el('p', { text: name + ': ' + value }), emergencyWord, { createTextNode: text => el('text', { text }) }, () => 1, () => {});
function flatten(element) { return [element, ...element.children.flatMap(flatten)]; }
function card(container, title) { return flatten(container).find(node => node.children.some(child => child.tag === 'h2' && child.text === title)); }
function button(card, text) { return flatten(card).find(node => node.tag === 'button' && node.text === text); }
async function click(card, text) { const node = button(card, text); assert(node && !node.disabled, text + ' enabled'); node.events.click(); await new Promise(resolve => setImmediate(resolve)); }
(async () => {
  const container = el('main');
  const dispose = await OutputsPage(container);
  const ambience = () => card(container, 'P4 ambience — PCM5102A');
  const io = () => card(container, 'P4 digital I/O — GPIO46 / GPIO47');
  await click(ambience(), 'Next file');
  await click(ambience(), 'Loop');
  assert.equal(calls.at(-1), 'AMBIENCE:LOOP:/showduino/audio/ambience/room.wav');
  await click(ambience(), 'Set volume');
  assert.equal(calls.at(-1), 'AMBIENCE:VOLUME:80');
  const file = flatten(ambience()).find(node => node.tag === 'input' && node.placeholder === 'ambience.wav');
  file.value = '../system/emergency.wav'; file.events.input();
  const before = calls.length; await click(ambience(), 'Play');
  assert.equal(calls.length, before, 'path traversal never reaches transport');
  await click(io(), 'Pulse'); assert.equal(calls.at(-1), 'IO:1:PULSE:1000');
  await click(io(), 'All off'); assert.equal(calls.at(-1), 'IO:ALL:OFF');
  subscriber({ ...snapshot, system: { emergencyActive: true } });
  for (const name of ['Play', 'Loop', 'Stop', 'Set volume']) assert(button(ambience(), name).disabled, name + ' locked during Emergency');
  assert(!button(ambience(), 'Status').disabled);
  assert(!button(ambience(), 'Next file').disabled);
  assert(button(io(), 'On').disabled); assert(button(io(), 'Output').disabled);
  assert(!button(io(), 'Off').disabled); assert(!button(io(), 'All off').disabled);
  subscriber({ p4Online: false, system: null });
  for (const node of flatten(ambience()).filter(node => node.tag === 'button')) assert(node.disabled, 'offline ambience action blocked');
  for (const node of flatten(io()).filter(node => node.tag === 'button')) assert(node.disabled, 'offline I/O action blocked');
  dispose();
  console.log('PASS actual Outputs page: WAV browsing, playback/volume commands, traversal rejection, I/O pulses, Emergency and offline locks');
})().catch(error => { console.error(error); process.exitCode = 1; });
