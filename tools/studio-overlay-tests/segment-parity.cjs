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

console.log('PASS: S3 Studio segment allocation, overlaps, holds, compatibility and capacity (' +
  checks + ' assertions).');
