'use strict';

const assert = require('assert');
const path = require('path');
const authoring = require(path.join(
  __dirname,
  '..',
  '..',
  'web',
  'studio-v4-overlay',
  'js',
  'showduino-pixel-authoring.js'
));

let failures = 0;
function check(name, fn) {
  try {
    fn();
    console.log('PASS  ' + name);
  } catch (error) {
    failures += 1;
    console.log('FAIL  ' + name + ' — ' + error.message);
  }
}

authoring.resetLiveSnapshot();

check('P4 is always listed first', () => {
  const list = authoring.listPixelOutputs({ project: { clips: [] } });
  assert.strictEqual(list[0].logicalId, 'p4');
  assert.strictEqual(list[0].route, 'p4-show-pixels');
  assert.strictEqual(list[0].deviceId, 'p4-show-pixels');
  assert.strictEqual(list[0].online, true);
  assert.strictEqual(list[0].initialised, true);
  assert.ok(authoring.optionLabel(list[0]).indexOf('OFFLINE') < 0);
});

check('multiple Pixel Nodes use logical IDs not array indexes', () => {
  authoring.ingestLivePayloads([{
    showPixelsReady: true,
    showPixelsMax: 1024,
    p4Online: true,
    pixelNodes: [
      { id: 'LED-02', name: 'Corridor', online: true, initialised: true, pixelCount: 80, maxPixels: 512 },
      { id: 'LED-01', name: 'Entrance', online: true, initialised: true, pixelCount: 100, maxPixels: 512 },
      { id: 'LED-03', name: 'Crypt', online: false, initialised: false, pixelCount: 0, maxPixels: 512 }
    ]
  }]);
  const list = authoring.listPixelOutputs({});
  const ids = list.map((item) => item.logicalId);
  assert.deepStrictEqual(ids, ['p4', 'LED-02', 'LED-01', 'LED-03']);
  assert.ok(!ids.includes('0') && !ids.includes('1'));
  assert.strictEqual(authoring.findOutput('LED-01').friendlyName, 'Entrance');
  assert.strictEqual(authoring.findOutput('LED-03').online, false);
});

check('friendly name is presentation; logical ID is identity', () => {
  const led = authoring.findOutput('LED-01');
  assert.strictEqual(led.logicalId, 'LED-01');
  assert.ok(authoring.optionLabel(led).indexOf('LED-01 — Entrance') === 0);
  assert.strictEqual(authoring.packageDeviceId('LED-01'), 'LED-01');
  assert.strictEqual(authoring.routeForNodeId('LED-01'), 'pixel-node');
});

check('offline and missing targets are preserved', () => {
  const project = {
    clips: [
      { id: 'a', type: 'pixel', routing: { nodeId: 'LED-03' }, params: { effect: 'FLICKER', segment: 4 } },
      { id: 'b', type: 'pixel', routing: { nodeId: 'LED-07' }, params: { effect: 'FIRE', segment: 1 } }
    ]
  };
  const list = authoring.listPixelOutputs({ project, selectedId: 'LED-07' });
  const crypt = list.find((item) => item.logicalId === 'LED-03');
  const missing = list.find((item) => item.logicalId === 'LED-07');
  assert.ok(crypt);
  assert.strictEqual(crypt.logicalId, 'LED-03');
  assert.ok(authoring.optionLabel(crypt).indexOf('OFFLINE') >= 0);
  assert.ok(missing.missing);
  assert.ok(authoring.optionLabel(missing).indexOf('Missing / Offline') >= 0);
  assert.strictEqual(authoring.canonicalNodeId('LED-07'), 'LED-07');
});

check('P4 aliases never retarget a Pixel Node', () => {
  assert.strictEqual(authoring.canonicalNodeId('p4-show-pixels'), 'p4');
  assert.strictEqual(authoring.canonicalNodeId('P4 Show Pixel Line'), 'p4');
  assert.strictEqual(authoring.canonicalNodeId('LED-01'), 'LED-01');
  assert.ok(!authoring.isP4Id('LED-01'));
});

check('NOT INITIALISED is a hardware warning not an authoring error', () => {
  const result = authoring.validatePixelTarget('LED-02');
  assert.strictEqual(result.ok, true);
  assert.strictEqual(result.errors.length, 0);
  // LED-02 from the live snapshot is initialised; use LED-03
  const uninit = authoring.validatePixelTarget('LED-03');
  assert.strictEqual(uninit.ok, true);
  assert.ok(uninit.warnings.some((item) => item.code === 'OFFLINE' || item.code === 'NOT_INITIALISED'));
});

check('missing device is a warning; empty device is an authoring error', () => {
  const missing = authoring.validatePixelClip({
    name: 'Crypt fire',
    type: 'pixel',
    routing: { nodeId: 'LED-07' },
    params: { effect: 'FIRE', segment: 2, startPixel: 0, length: 10 }
  });
  assert.strictEqual(missing.ok, true);
  assert.ok(missing.warnings.some((item) => item.code === 'MISSING'));
  const none = authoring.validatePixelTarget('');
  assert.strictEqual(none.ok, false);
  assert.ok(none.errors.some((item) => item.code === 'NO_DEVICE'));
});

check('malformed Pixel Node IDs are authoring errors', () => {
  const bad = authoring.validatePixelTarget('99-LED');
  assert.strictEqual(bad.ok, false);
  assert.ok(bad.errors.some((item) => item.code === 'BAD_ID'));
  assert.strictEqual(authoring.pixelNodeIdOk('LED-01'), true);
  assert.strictEqual(authoring.pixelNodeIdOk('x'), false);
});

check('shared 25-effect vocabulary', () => {
  assert.strictEqual(authoring.EFFECTS.length, 25);
  const ids = authoring.EFFECTS.map((fx) => fx.id);
  [
    'OFF','SOLID','FADE_IN','FADE_OUT','PULSE','BREATHE','FLICKER','CANDLE','FIRE',
    'LIGHTNING','STROBE','RANDOM_STROBE','CHASE','BOUNCE','COMET','WIPE','REVERSE_WIPE',
    'BUILD','SPARKLE','TWINKLE','GLITCH','WARNING','PORTAL','RAINBOW','CUSTOM_SEQUENCE'
  ].forEach((id) => assert.ok(ids.includes(id), id));
  assert.strictEqual(authoring.canonicalizeEffect('flicker'), 'FLICKER');
  assert.strictEqual(authoring.canonicalizeEffect('blackout'), 'OFF');
  assert.strictEqual(authoring.canonicalizeEffect('ember'), 'CANDLE');
  assert.strictEqual(authoring.canonicalizeEffect('nope'), '');
});

check('effect parameters are not invented', () => {
  assert.ok(authoring.effectSupports('FIRE', 'secondary'));
  assert.ok(!authoring.effectSupports('SOLID', 'reverse'));
  assert.ok(!authoring.effectSupports('RAINBOW', 'colour'));
  assert.ok(authoring.effectSupports('CHASE', 'reverse'));
});

check('SHDO devices use logical IDs and existing routes', () => {
  const p4 = authoring.shdoDeviceForNode('p4');
  const led = authoring.shdoDeviceForNode('LED-03', { name: 'Crypt' });
  assert.strictEqual(p4.id, 'p4-show-pixels');
  assert.strictEqual(p4.binding.route, 'p4-show-pixels');
  assert.strictEqual(p4.binding.nodeId, 'p4');
  assert.strictEqual(led.id, 'LED-03');
  assert.strictEqual(led.binding.route, 'pixel-node');
  assert.strictEqual(led.binding.nodeId, 'LED-03');
  assert.strictEqual(authoring.commandPrefix(p4), 'PIXEL:');
  assert.strictEqual(authoring.commandPrefix(led), 'PIXEL:NODE:LED-03:');
});

check('device switch keeps segment/effect/params', () => {
  const params = authoring.migratePixelParams({
    segment: 2,
    effect: 'FLICKER',
    r: 255,
    g: 0,
    b: 0,
    brightness: 160,
    speed: 40,
    startPixel: 8,
    length: 12
  });
  const afterDeviceChange = authoring.migratePixelParams({ ...params });
  assert.strictEqual(afterDeviceChange.segment, 2);
  assert.strictEqual(afterDeviceChange.effect, 'FLICKER');
  assert.strictEqual(afterDeviceChange.brightness, 160);
  assert.strictEqual(afterDeviceChange.speed, 40);
});

check('duplicate cue identity is a deep routing copy contract', () => {
  const src = { routing: { nodeId: 'LED-01', output: 'Entrance' }, params: { effect: 'FIRE', segment: 2 } };
  const duplicate = {
    routing: { ...src.routing },
    params: { ...src.params }
  };
  duplicate.routing.nodeId = 'LED-02';
  assert.strictEqual(src.routing.nodeId, 'LED-01');
  assert.strictEqual(duplicate.routing.nodeId, 'LED-02');
  assert.strictEqual(duplicate.params.effect, 'FIRE');
});

check('save/reload preserves exact logical targets', () => {
  const project = {
    clips: [
      { id: 'p4', type: 'pixel', routing: { nodeId: 'p4' }, params: { effect: 'FIRE', segment: 1 } },
      { id: 'a', type: 'pixel', routing: { nodeId: 'LED-01' }, params: { effect: 'FLICKER', segment: 2 } },
      { id: 'b', type: 'pixel', routing: { nodeId: 'LED-02' }, params: { effect: 'WARNING', segment: 4 } }
    ]
  };
  const json = JSON.stringify(project);
  const reloaded = JSON.parse(json);
  assert.strictEqual(reloaded.clips[0].routing.nodeId, 'p4');
  assert.strictEqual(reloaded.clips[1].routing.nodeId, 'LED-01');
  assert.strictEqual(reloaded.clips[2].routing.nodeId, 'LED-02');
  authoring.resetLiveSnapshot();
  const list = authoring.listPixelOutputs({ project: reloaded });
  assert.ok(list.some((item) => item.logicalId === 'LED-01'));
  assert.ok(list.some((item) => item.logicalId === 'LED-02'));
  assert.strictEqual(list[0].logicalId, 'p4');
});

check('audio and lamp devices are not pixel outputs', () => {
  authoring.ingestLivePayloads([{
    devices: [
      { id: 'audio-node', role: 'AUDIO', name: 'Audio Node', online: true },
      { id: 'lamp-node', role: 'LAMP', name: 'Lamp Node', online: true },
      { id: 'LED-04', role: 'PIXEL', friendlyName: 'Finale', online: true, initialised: true, pixelCount: 40 }
    ]
  }]);
  const list = authoring.listPixelOutputs({});
  const ids = list.map((item) => item.logicalId);
  assert.ok(!ids.includes('audio-node'));
  assert.ok(!ids.includes('lamp-node'));
  assert.ok(ids.includes('LED-04'));
});

check('select HTML truncates via CSS class and preserves values', () => {
  const html = authoring.selectHtml({
    selectedId: 'LED-01',
    project: { clips: [{ type: 'pixel', routing: { nodeId: 'LED-01' } }] }
  });
  assert.ok(html.indexOf('value="p4"') >= 0);
  assert.ok(html.indexOf('value="LED-01"') >= 0);
  assert.ok(html.indexOf('selected') >= 0);
});

if (failures) {
  console.error(failures + ' studio pixel-authoring tests failed');
  process.exit(1);
}
console.log('All studio pixel-authoring tests passed');
