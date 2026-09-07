const BASE = '';

export function isP4Offline(data) {
  return !data || data.p4Online === false || data.stageLink === 'offline' || data.error === 'p4_offline';
}

async function request(path, options = {}) {
  const res = await fetch(BASE + path, {
    headers: { Accept: 'application/json', ...(options.body ? { 'Content-Type': 'application/json' } : {}) },
    ...options
  });
  if (!res.ok) {
    let data = null;
    try { data = await res.json(); } catch (_) {}
    if (data && (data.error === 'p4_offline' || data.p4Online === false)) {
      return data;
    }
    throw new Error(`${path} → HTTP ${res.status}${data && data.error ? ' ' + data.error : ''}`);
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
export function normalizeShowCommand(cmd) {
  const raw = (typeof cmd === 'string' ? cmd : (cmd && (cmd.cmd || cmd.action)) || '').trim();
  const category = String((cmd && cmd.category) || '').toLowerCase();
  const action = String((cmd && cmd.action) || raw).toLowerCase();
  const upper = raw.toUpperCase();
  if (upper === 'PANIC' || upper === 'EMERGENCY:PANIC' || upper === 'ESTOP' ||
      upper === 'E-STOP' || upper === 'EMERGENCY:STOP' ||
      action === 'panic' ||
      (category === 'emergency' && (action === 'stop' || action === 'panic' || action === 'activate' || action === 'estop'))) {
    return 'EMERGENCY:STOP';
  }
  return raw;
}

export function postCommand(cmd) {
  const value = normalizeShowCommand(cmd);
  return request('/api/command', { method: 'POST', body: JSON.stringify({ cmd: value }) });
}

export function postPanic() {
  return postCommand('EMERGENCY:STOP');
}
