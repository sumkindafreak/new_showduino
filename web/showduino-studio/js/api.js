const BASE = '';

async function request(path, options = {}) {
  const res = await fetch(BASE + path, {
    headers: { Accept: 'application/json', ...(options.body ? { 'Content-Type': 'application/json' } : {}) },
    ...options
  });
  if (!res.ok) {
    let detail = '';
    try { detail = await res.text(); } catch (_) {}
    throw new Error(`${path} → HTTP ${res.status}${detail ? ' ' + detail : ''}`);
  }
  if (res.status === 204) return null;
  return res.json();
}

export function fetchSystem() { return request('/api/system'); }
export function fetchDevices() { return request('/api/devices'); }
export function fetchDevice(id) { return request('/api/device/' + encodeURIComponent(id)); }
export function fetchNetwork() { return request('/api/network'); }
export function fetchLogs() { return request('/api/logs'); }
export function fetchCommands() { return request('/api/commands'); }
export function fetchCommand(id) { return request('/api/command/' + encodeURIComponent(id)); }
export function postCommand(body) {
  return request('/api/command', { method: 'POST', body: JSON.stringify(body) });
}
export function cancelCommand(id) {
  return request('/api/command/' + encodeURIComponent(id), { method: 'DELETE' });
}
export function fetchCapabilities() { return request('/api/capabilities'); }
export function fetchDeviceCapabilities() { return request('/api/device-capabilities'); }
export function fetchRoutes() { return request('/api/routes'); }
export function postRouteTest(body) {
  return request('/api/route-test', { method: 'POST', body: JSON.stringify(body) });
}
export function fetchTime() { return request('/api/time'); }
export function fetchTimeStatus() { return request('/api/time/status'); }