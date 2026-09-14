import assert from 'assert';
import {
  createGatewayDraft,
  applySsidInput,
  applyPasswordInput,
  applyScanSelection,
  ssidForField,
  captureConnectCredentials,
  afterConnectSuccess,
  afterForget,
  statusSafeDraft,
  fieldsAfterRepaint
} from '../../web/showduino-studio/js/pages/networkGatewayDraft.js';

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

check('TYPE TEST: SSID and password survive a status repaint', () => {
  const d = createGatewayDraft();
  applySsidInput(d, 'ExampleNetwork');
  applyPasswordInput(d, 'testpassword');
  const painted = fieldsAfterRepaint(d, 'OldServerSsid');
  assert.strictEqual(painted.ssid, 'ExampleNetwork');
  assert.strictEqual(painted.password, 'testpassword');
});

check('SCAN TEST: clicked SSID survives a repaint', () => {
  const d = createGatewayDraft();
  assert.strictEqual(applyScanSelection(d, 'ExampleNetwork'), true);
  const painted = fieldsAfterRepaint(d, '');
  assert.strictEqual(painted.ssid, 'ExampleNetwork');
});

check('SCAN TEST: hidden network does not overwrite a manual SSID', () => {
  const d = createGatewayDraft();
  applySsidInput(d, 'ManualHidden');
  assert.strictEqual(applyScanSelection(d, ''), false);
  assert.strictEqual(ssidForField(d, ''), 'ManualHidden');
});

check('CONNECT ORDER TEST: capture happens from draft, before a destructive paint', () => {
  const d = createGatewayDraft();
  applySsidInput(d, 'ExampleNetwork');
  applyPasswordInput(d, 'testpassword');
  const creds = captureConnectCredentials(d);
  const paintedEmptyDom = { ssid: '', password: '' };
  assert.strictEqual(creds.ok, true);
  assert.strictEqual(creds.ssid, 'ExampleNetwork');
  assert.strictEqual(creds.password, 'testpassword');
  assert.notStrictEqual(creds.ssid, paintedEmptyDom.ssid);
  assert.notStrictEqual(creds.password, paintedEmptyDom.password);
});

check('CONNECT ORDER TEST: password is not trimmed', () => {
  const d = createGatewayDraft();
  applySsidInput(d, '  ExampleNetwork  ');
  applyPasswordInput(d, '  padded  ');
  const creds = captureConnectCredentials(d);
  assert.strictEqual(creds.ssid, 'ExampleNetwork');
  assert.strictEqual(creds.password, '  padded  ');
});

check('CONNECT ORDER TEST: empty SSID is rejected before connectGateway', () => {
  const d = createGatewayDraft();
  applyPasswordInput(d, 'testpassword');
  const creds = captureConnectCredentials(d);
  assert.strictEqual(creds.ok, false);
  assert.strictEqual(creds.error, 'missing_ssid');
});

check('POLLING TEST: multiple status polls keep the draft usable', () => {
  const d = createGatewayDraft();
  applySsidInput(d, 'Ex');
  let painted = fieldsAfterRepaint(d, 'Venue');
  applySsidInput(d, painted.ssid + 'ample');
  applyPasswordInput(d, 'test');
  painted = fieldsAfterRepaint(d, 'Venue');
  applyPasswordInput(d, painted.password + 'password');
  painted = fieldsAfterRepaint(d, 'OtherNet');
  assert.strictEqual(painted.ssid, 'Example');
  assert.strictEqual(painted.password, 'testpassword');
  const creds = captureConnectCredentials(d);
  assert.strictEqual(creds.ssid, 'Example');
  assert.strictEqual(creds.password, 'testpassword');
});

check('PASSWORD SAFETY TEST: status snapshot must not include the password', () => {
  const d = createGatewayDraft();
  applySsidInput(d, 'ExampleNetwork');
  applyPasswordInput(d, 'testpassword');
  const safe = statusSafeDraft(d);
  const encoded = JSON.stringify(safe);
  assert.strictEqual('password' in safe, false);
  assert.ok(encoded.indexOf('testpassword') < 0);
  assert.ok(!/"password"\s*:/.test(encoded));
});

check('SUCCESS TEST: password clears, SSID remains selected', () => {
  const d = createGatewayDraft();
  applySsidInput(d, 'ExampleNetwork');
  applyPasswordInput(d, 'testpassword');
  const creds = captureConnectCredentials(d);
  assert.strictEqual(creds.ok, true);
  afterConnectSuccess(d);
  const painted = fieldsAfterRepaint(d, 'ExampleNetwork');
  assert.strictEqual(painted.ssid, 'ExampleNetwork');
  assert.strictEqual(painted.password, '');
});

check('server SSID initialises only when there is no user draft', () => {
  const d = createGatewayDraft();
  assert.strictEqual(ssidForField(d, 'SavedNet'), 'SavedNet');
  applySsidInput(d, 'TypedNet');
  assert.strictEqual(ssidForField(d, 'SavedNet'), 'TypedNet');
});

check('forget clears transient credentials', () => {
  const d = createGatewayDraft();
  applySsidInput(d, 'ExampleNetwork');
  applyPasswordInput(d, 'testpassword');
  afterForget(d);
  const painted = fieldsAfterRepaint(d, '');
  assert.strictEqual(painted.ssid, '');
  assert.strictEqual(painted.password, '');
});

if (failures) {
  console.log('\n' + failures + ' FAILED');
  process.exit(1);
}
console.log('\nALL PASS');
