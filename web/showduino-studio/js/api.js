const BASE = '';
const DEFAULT_TIMEOUT_MS = 3000;

function compactDetail(detail) {
  if (!detail) return '';
  const singleLine = String(detail).replace(/<[^>]+>/g, ' ').replace(/\s+/g, ' ').trim();
  if (!singleLine) return '';
  return singleLine.length > 180 ? `${singleLine.slice(0, 177)}...` : singleLine;
}

function mergeAbortSignals(...signals) {
  const active = signals.filter(Boolean);
  if (active.length === 0) return null;
  if (active.length === 1) return active[0];
  const ctrl = new AbortController();
  const onAbort = () => ctrl.abort();
  for (const signal of active) {
    if (signal.aborted) {
      ctrl.abort();
      return ctrl.signal;
    }
    signal.addEventListener('abort', onAbort, { once: true });
  }
  return ctrl.signal;
}

async function request(path, options = {}) {
  const timeoutMs = options.timeoutMs ?? DEFAULT_TIMEOUT_MS;
  const timeoutController = new AbortController();
  const timeout = setTimeout(() => timeoutController.abort(), timeoutMs);

  const signal = mergeAbortSignals(timeoutController.signal, options.signal);
  const fetchOptions = {
    ...options,
    signal,
    headers: {
      Accept: 'application/json',
      ...(options.body ? { 'Content-Type': 'application/json' } : {}),
      ...(options.headers || {})
    }
  };
  delete fetchOptions.timeoutMs;

  try {
    const res = await fetch(BASE + path, fetchOptions);
    if (!res.ok) {
      let detail = '';
      try { detail = await res.text(); } catch (_) {}
      const safeDetail = compactDetail(detail);
      throw new Error(`${path} → HTTP ${res.status}${safeDetail ? ` ${safeDetail}` : ''}`);
    }
    if (res.status === 204) return null;
    return res.json();
  } catch (err) {
    if (err?.name === 'AbortError') {
      throw new Error(`${path} → request timeout after ${timeoutMs}ms`);
    }
    throw err;
  } finally {
    clearTimeout(timeout);
  }
}

export function fetchSystem(options) { return request('/api/system', options); }
export function fetchDevices(options) { return request('/api/devices', options); }
export function fetchDevice(id, options) { return request('/api/device/' + encodeURIComponent(id), options); }
export function fetchNetwork(options) { return request('/api/network', options); }
export function fetchLogs(options) { return request('/api/logs', options); }
export function fetchCommands(options) { return request('/api/commands', options); }
export function fetchCommand(id, options) { return request('/api/command/' + encodeURIComponent(id), options); }
export function postCommand(body, options) {
  return request('/api/command', { method: 'POST', body: JSON.stringify(body), ...options });
}
export function cancelCommand(id, options) {
  return request('/api/command/' + encodeURIComponent(id), { method: 'DELETE', ...options });
}
export function fetchCapabilities(options) { return request('/api/capabilities', options); }
export function fetchDeviceCapabilities(options) { return request('/api/device-capabilities', options); }
export function fetchRoutes(options) { return request('/api/routes', options); }
export function postRouteTest(body, options) {
  return request('/api/route-test', { method: 'POST', body: JSON.stringify(body), ...options });
}
export function fetchTime(options) { return request('/api/time', options); }
export function fetchTimeStatus(options) { return request('/api/time/status', options); }