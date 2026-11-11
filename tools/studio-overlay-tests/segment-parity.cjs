'use strict';

// Regression tests for the Studio source bundled with the Communications S3.
// These use the actual browser compiler and no third-party npm packages.
// They do NOT test physical P4 or node behaviour.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const source = fs.readFileSync(
  path.join(__dirname, '../../web/studio-v4-overlay/js/studio-deploy.js'), 'utf8'
);
const window = {
  location: { protocol: 'http:', origin: 'http://showduino.local' },
  ShowduinoPackage: { SCHEMA_NAME: 'showduino-shdo-v2' },
  ShowduinoPixelAuthoring: {
    commandPrefix: () => null,
    canonicalizeEffect: () => 'SOLID',
    validatePixelTarget: () => ({ warnings: [] })
  }
};
const document = { addEventListener() {} };
vm.runInNewContext(source, { window, document, console, URL, AbortController, setTimeout, clearTimeout }, {
  filename: 'web/studio-v4-overlay/js/studio-deploy.js'
});
assert.equal(typeof window.ShowduinoDeploy?.compileShdoForStage, 'function');

const devices = [
  { id: 'p4', binding: { route: 'p4-show-pixels', pixelStart: 0, pixelCount: 120 } },
  { id: 'node', binding: { route: 'pixel-node', nodeId: 'LED-01', pixelStart: 0, pixelCount: 60 } }
];

function cue(name, startMs, durationMs, segment, targetDeviceId = 'p4') {
  const params = { effect: 'SOLID', startPixel: 0, length: 5 };
  if (segment !== undefined) params.segment = segment;
  return {
    name, startMs, durationMs, enabled: true,
    action: { type: 'pixel', targetDeviceId, params }
  };
}

function plan(clips) {
  return window.ShowduinoDeploy.compileShdoForStage({
    schema: window.ShowduinoPackage.SCHEMA_NAME,
    devices, clips
  });
}

function starts(p) {
  return Array.from(p.commands)
    .filter(({ command }) => /:START$/.test(command))
    .map(({ command }) => ({
      prefix: command.split('SEGMENT:')[0],
      segment: Number(/SEGMENT:(\d+):START$/.exec(command)[1])
    }));
}

function segments(p) {
  return starts(p).map((entry) => entry.segment);
}

let checks = 0;
function check(condition, message) {
  assert.ok(condition, message); checks += 1;
}

// Sequential explicit zero reuses slot 0 rather than allocating a second one.
{
  const p = plan([cue('First', 0, 2000, 0), cue('Second', 4000, 2000, 0)]);
  check(p.ok, p.errors.join('; '));
  assert.deepEqual(segments(p), [0, 0]); checks += 1;
}

// Overlap must never replace a running effect on its segment.
{
  const p = plan([cue('A', 0, 6000, 0), cue('B', 2000, 4000, 0)]);
  check(p.ok, p.errors.join('; '));
  assert.deepEqual(segments(p), [0, 1]); checks += 1;
  check(p.warnings.some((message) => /A/.test(message) && /B/.test(message)), 'Warn who holds the segment');
}

// Reusing a timed segment for an indefinite hold must clear its old DURATION.
{
  const p = plan([cue('Timed', 0, 2000, 0), cue('Held', 4000, 0, 0)]);
  check(p.ok, p.errors.join('; '));
  assert.deepEqual(segments(p), [0, 0]); checks += 1;
  check(p.commands.some(({ timeMs, command }) => timeMs === 4000 &&
    command === 'PIXEL:SEGMENT:0:DURATION:0'), 'Held cue resets duration');
}

// An indefinite hold keeps its segment occupied for the remainder of the show.
{
  const p = plan([cue('Held', 0, 0, 0), cue('Later', 2000, 1000, 0)]);
  check(p.ok, p.errors.join('; '));
  assert.deepEqual(segments(p), [0, 1]); checks += 1;
}

// Older files that omit the segment retain the sequential counter rule.
{
  const p = plan([cue('Legacy A', 0, 1000), cue('Legacy B', 3000, 1000)]);
  check(p.ok, p.errors.join('; '));
  assert.deepEqual(segments(p), [0, 1]); checks += 1;
}

// Allocators are independent for different physical pixel outputs.
{
  const p = plan([cue('P4', 0, 4000, 0), cue('Pixel Node', 0, 4000, 0, 'node')]);
  check(p.ok, p.errors.join('; '));
  assert.deepEqual(segments(p), [0, 0]); checks += 1;
  assert.deepEqual(starts(p).map((entry) => entry.prefix),
    ['PIXEL:', 'PIXEL:NODE:LED-01:']); checks += 1;
}

// More than sixteen simultaneous cues on one output must fail safely.
{
  const p = plan(Array.from({ length: 17 }, (_, i) => cue('Clip ' + i, 0, 5000, 0)));
  check(!p.ok, 'Seventeenth simultaneous segment must be refused');
  check(p.errors.some((message) => /16 pixel segment slots/.test(message)),
    'Capacity failure is explicit');
}

// The same slot can be reused many times if cues are sequential.
{
  const p = plan(Array.from({ length: 17 }, (_, i) => cue('Clip ' + i, i * 2000, 1000, 0)));
  check(p.ok, p.errors.join('; '));
  check(segments(p).every((n) => n === 0), 'All sequential cues reuse slot 0');
}


// The embedded S3 page must enforce the same host validation and RAM upload
// allow-list as the website, before sending SHOW:TL:BEGIN.
{
  const api = window.ShowduinoDeploy;
  assert.equal(api.normaliseTarget('not a host!!'), ''); checks += 1;
  assert.equal(api.normaliseTarget('192.168.4.1'), 'http://192.168.4.1'); checks += 1;
  check(api.uploadEnvelopeAllowed('SHOW:TL:BEGIN'), 'BEGIN allowed');
  check(api.uploadEnvelopeAllowed('SHOW:TL:C:200:PIXEL:SEGMENT:0:START'), 'Pixel allowed');
  check(api.uploadEnvelopeAllowed('SHOW:TL:C:200:AUDIO:NODE:STOP'), 'Audio Node allowed');
  check(!api.uploadEnvelopeAllowed('SHOW:TL:C:200:ESTOP:NODE:PIXEL:SEGMENT:0:START'),
    'Emergency Node pixel upload not in current RAM allow-list');
  check(!api.uploadEnvelopeAllowed('SHOW:TL:C:200:MOSFET:NODE:MOSFET-01:OUT:1:OFF'),
    'MOSFET upload rejected before any S3 request');
  check(!api.uploadEnvelopeAllowed('SHOW:TL:C:200:EMERGENCY:CLEAR'),
    'Safety commands forbidden in uploaded timelines');
  check(!api.uploadEnvelopeAllowed('SHOW:TL:C:200:PIXEL:' + 'A'.repeat(64)),
    'Commands over the P4 63-character limit forbidden');
  check(api.uploadPolicyErrors({ uploadCommands: [
    'SHOW:TL:BEGIN', 'SHOW:TL:C:0:MOSFET:NODE:MOSFET-01:OUT:1:OFF', 'SHOW:TL:END'
  ] }).length > 0, 'Unsupported timeline stopped before BEGIN');
}

// A deployment containing a refused emergency-node command must reject without
// reaching a network request (host reachability and device are not simulated).
(async () => {
  const clip = cue('Emergency Node ring', 0, 2000, 0, 'estop');
  const shdo = {
    schema: window.ShowduinoPackage.SCHEMA_NAME,
    devices: devices.concat([{
      id: 'estop', binding: { route: 'estop-node-pixels', pixelStart: 0, pixelCount: 10 }
    }]),
    clips: [clip]
  };
  window.ShowduinoProjects = {
    saveCurrentProject: async () => ({ project: { name: 'Test' } })
  };
  window.ShowduinoPackage.toShdo = () => shdo;
  await assert.rejects(window.ShowduinoDeploy.deployCurrentProject(),
    /does not accept ESTOP:NODE cues/, 'No partial RAM upload can begin');
  checks += 1;
  console.log('PASS: S3 Studio segment allocation and upload preflight (' +
    checks + ' assertions).');
})().catch((error) => {
  console.error(error);
  process.exitCode = 1;
});
