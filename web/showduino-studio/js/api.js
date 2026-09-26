const BASE = '';

let sRequestSeq = 0;

export function isP4Offline(data) {
  return !data || data.p4Online === false || data.stageLink === 'offline' || data.error === 'p4_offline';
}

function controlHeaders(extra = {}) {
  const headers = { Accept: 'application/json', ...extra };
  try {
    const token = localStorage.getItem('showduino.controlToken');
    if (token) headers['X-Showduino-Token'] = token;
  } catch (_) {}
  return headers;
}

async function request(path, options = {}) {
  const res = await fetch(BASE + path, {
    headers: controlHeaders(options.body ? { 'Content-Type': 'application/json' } : {}),
    ...options
  });
  if (!res.ok) {
    let data = null;
    try { data = await res.json(); } catch (_) {}
    if (data && (data.error === 'p4_offline' || data.p4Online === false)) {
      return data;
    }
    throw new Error(`${path} → HTTP ${res.status}${data && data.error ? ' ' + data.error : ''}${data && data.reason ? ' ' + data.reason : ''}${data && data.lifecycle ? ' [' + data.lifecycle + ']' : ''}`);
  }
  if (res.status === 204) return null;
  return res.json();
}

export function fetchComms() { return request('/api/comms'); }
export function fetchSystem() { return request('/api/system'); }
export function fetchDevices() { return request('/api/devices'); }
export function fetchNetwork() { return request('/api/network'); }
export function fetchE131() { return request('/api/e131'); }
export function fetchE131Channels(from = 1, count = 64) {
  return request(`/api/e131/channels?from=${from}&count=${count}`);
}
export function fetchLogs() { return request('/api/logs'); }
export function fetchProductions() { return request('/api/productions'); }
export function fetchShow() { return request('/api/show'); }
export function fetchPlugins() { return request('/api/plugins'); }
export function fetchAudio() { return request('/api/audio'); }
export function fetchTime() { return request('/api/time'); }
export function fetchCapabilities() { return request('/api/capabilities'); }
export function fetchStorage() { return request('/api/storage'); }
export function fetchLighting() { return request('/api/lighting'); }

/**
 * Canonical browser → SoftAP → P4 command contract.
 * Wire verb is always a Showduino colon command in `cmd`.
 *
 * Accepted input shapes:
 *   "SHOW:START"
 *   { cmd: "SHOW:START", requestId?: string }
 *   { category, action, payload?, source?, destination?, priority? }
 *
 * Extra fields (source/destination/priority/payload) are accepted for UI
 * compatibility and ignored by transport unless folded into `cmd`.
 */
export function normalizeShowCommand(cmd) {
  if (typeof cmd === 'string') {
    return mapAliases(cmd.trim());
  }
  if (!cmd || typeof cmd !== 'object') return '';

  if (cmd.cmd && typeof cmd.cmd === 'string' && cmd.cmd.trim()) {
    return mapAliases(cmd.cmd.trim());
  }

  const category = String(cmd.category || '').toLowerCase();
  const action = String(cmd.action || '').toLowerCase();
  const payload = cmd.payload != null ? String(cmd.payload) : '';

  if (category === 'emergency' &&
      (action === 'stop' || action === 'panic' || action === 'activate' || action === 'estop')) {
    return 'EMERGENCY:STOP';
  }
  if (category === 'show' || category === 'runtime') {
    if (action === 'start' || action === 'run') return 'SHOW:START';
    if (action === 'pause') return 'SHOW:PAUSE';
    if (action === 'resume') return 'SHOW:RESUME';
    if (action === 'stop') return 'SHOW:STOP';
    if (action === 'status') return 'STATUS:REQUEST';
  }
  if (category === 'status' || action === 'status') return 'STATUS:REQUEST';
  if (payload && /^[A-Z0-9:_.=-]+$/i.test(payload)) return mapAliases(payload);
  return mapAliases(action);
}

function mapAliases(raw) {
  const upper = String(raw || '').toUpperCase();
  if (upper === 'PANIC' || upper === 'EMERGENCY:PANIC' || upper === 'ESTOP' ||
      upper === 'E-STOP' || upper === 'EMERGENCY:STOP') {
    return 'EMERGENCY:STOP';
  }
  return String(raw || '').trim();
}

function nextRequestId() {
  sRequestSeq += 1;
  return `web-${Date.now().toString(36)}-${sRequestSeq}`;
}

export function postCommand(cmd, options = {}) {
  const value = normalizeShowCommand(cmd);
  if (!value) {
    return Promise.reject(new Error('missing_cmd'));
  }
  const requestId = (options && options.requestId) ||
    (cmd && typeof cmd === 'object' && cmd.requestId) ||
    nextRequestId();
  const body = { cmd: value, requestId };
  /* Preserve optional metadata for logs/UI; P4 ignores unknown fields. */
  if (cmd && typeof cmd === 'object') {
    if (cmd.source) body.source = cmd.source;
    if (cmd.destination) body.destination = cmd.destination;
    if (cmd.category) body.category = cmd.category;
    if (cmd.action) body.action = cmd.action;
    if (cmd.priority != null) body.priority = cmd.priority;
  }
  return request('/api/command', { method: 'POST', body: JSON.stringify(body) });
}

export function postPanic() {
  return postCommand('EMERGENCY:STOP');
}

export function fetchGateway() { return request('/api/gateway'); }
export function fetchGatewayScan() { return request('/api/gateway/scan'); }
export function startGatewayScan() {
  return request('/api/gateway/scan', { method: 'POST', body: '{}' });
}
export function connectGateway(ssid, password) {
  return request('/api/gateway/connect', {
    method: 'POST',
    body: JSON.stringify({ ssid, password: password || '' })
  });
}
export function disconnectGateway() {
  return request('/api/gateway/disconnect', { method: 'POST', body: '{}' });
}
export function forgetGateway() {
  return request('/api/gateway/forget', { method: 'POST', body: '{}' });
}
export function setGatewayMode(mode) {
  return request('/api/gateway/mode', {
    method: 'POST',
    body: JSON.stringify({ mode })
  });
}
export function fetchUpdates() { return request('/api/updates'); }
export function fetchUpdateInventory() { return request('/api/updates/inventory'); }
export function fetchUpdateStatus() { return request('/api/updates/status'); }
export function checkUpdates() {
  return request('/api/updates/check', { method: 'POST', body: '{}' });
}
export function setUpdateMaintenance(on) {
  return request('/api/updates/maintenance', {
    method: 'POST',
    body: JSON.stringify({ on: !!on })
  });
}
export function applyCommsUpdate(payload) {
  return request('/api/updates/apply', {
    method: 'POST',
    body: JSON.stringify(payload)
  });
}

function crc32Ieee(bytes) {
  let crc = 0xFFFFFFFF;
  for (let i = 0; i < bytes.length; i++) {
    crc ^= bytes[i];
    for (let b = 0; b < 8; b++) {
      crc = (crc >>> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
    }
  }
  return (crc ^ 0xFFFFFFFF) >>> 0;
}

function toHex(bytes) {
  let out = '';
  for (let i = 0; i < bytes.length; i++) {
    out += bytes[i].toString(16).padStart(2, '0');
  }
  return out;
}

export async function persistShdo(text) {
  const bytes = new TextEncoder().encode(text);
  const crc32 = crc32Ieee(bytes);
  const begin = await request('/api/productions/deploy/begin', {
    method: 'POST',
    body: JSON.stringify({ bytes: bytes.length, crc32 })
  });
  if (begin && begin.ok === false) return begin;
  const chunk = 640;
  for (let i = 0; i < bytes.length; i += chunk) {
    const slice = bytes.subarray(i, i + chunk);
    const part = await request('/api/productions/deploy/chunk', {
      method: 'POST',
      body: JSON.stringify({ hex: toHex(slice) })
    });
    if (part && part.ok === false) return part;
  }
  return request('/api/productions/deploy/commit', {
    method: 'POST',
    body: JSON.stringify({ crc32 })
  });
}

export function fetchDeployStatus() {
  return request('/api/productions/deploy/status');
}
