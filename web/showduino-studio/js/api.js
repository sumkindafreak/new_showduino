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

function provisionBase() {
  const host = location.hostname || '192.168.4.1';
  return `http://${host}:82`;
}

async function provisionRequest(path, options = {}) {
  const res = await fetch(provisionBase() + path, {
    headers: { Accept: 'application/json', ...(options.body ? { 'Content-Type': 'application/json' } : {}) },
    ...options
  });
  if (!res.ok) {
    let detail = '';
    try {
      const data = await res.json();
      detail = data.error || JSON.stringify(data);
    } catch (_) {
      try { detail = await res.text(); } catch (_) {}
    }
    throw new Error(detail || `${path} → HTTP ${res.status}`);
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

// C3 network provisioning service. This stays separate from the Stage API so
// Wi-Fi configuration can still be recovered even if the P4 is unavailable.
export function fetchLanConfig() { return provisionRequest('/api/network/config'); }
export function scanLanNetworks() { return provisionRequest('/api/network/scan'); }
export function saveLanConfig(ssid, password) {
  return provisionRequest('/api/network/config', {
    method: 'POST',
    body: JSON.stringify({ ssid, password })
  });
}
export function reconnectLan() {
  return provisionRequest('/api/network/reconnect', { method: 'POST' });
}
export function clearLanConfig() {
  return provisionRequest('/api/network/config', { method: 'DELETE' });
}
export function networkSetupUrl() { return provisionBase() + '/'; }
