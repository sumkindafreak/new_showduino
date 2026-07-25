/**
 * Showduino Studio – API Client
 *
 * All requests use AbortController for timeout/cancellation support.
 * Default timeout: 8 seconds.
 */

const BASE = '';
const DEFAULT_TIMEOUT_MS = 8000;

export function request(path, options = {}) {
  const { timeoutMs = DEFAULT_TIMEOUT_MS, signal: externalSignal, ...fetchOptions } = options;

  const controller = new AbortController();
  let timer = null;

  if (timeoutMs > 0) {
    timer = setTimeout(() => controller.abort(), timeoutMs);
  }

  // If caller provided their own signal, abort when either fires
  if (externalSignal) {
    externalSignal.addEventListener('abort', () => controller.abort());
  }

  const signal = controller.signal;

  return fetch(BASE + path, {
    signal,
    headers: {
      Accept: 'application/json',
      ...(fetchOptions.body ? { 'Content-Type': 'application/json' } : {}),
    },
    ...fetchOptions,
  })
    .then((res) => {
      if (!res.ok) {
        return res.text().catch(() => '').then((detail) => {
          throw new Error(`${path} → HTTP ${res.status}${detail ? ' ' + detail : ''}`);
        });
      }
      if (res.status === 204) return null;
      return res.json();
    })
    .finally(() => {
      if (timer) clearTimeout(timer);
    });
}

// ─── System ───────────────────────────────────────────────────────────────────
export const fetchSystem       = (opts) => request('/api/system', opts);

// ─── Devices / Nodes ──────────────────────────────────────────────────────────
export const fetchDevices      = (opts) => request('/api/devices', opts);
export const fetchDevice       = (id, opts) => request('/api/device/' + encodeURIComponent(id), opts);

// ─── Network ──────────────────────────────────────────────────────────────────
export const fetchNetwork      = (opts) => request('/api/network', opts);

// ─── Logs ─────────────────────────────────────────────────────────────────────
export const fetchLogs         = (opts) => request('/api/logs', opts);

// ─── ShowCommands ─────────────────────────────────────────────────────────────
export const fetchCommands     = (opts) => request('/api/commands', opts);
export const fetchCommand      = (id, opts) => request('/api/command/' + encodeURIComponent(id), opts);
export const postCommand       = (body, opts) => request('/api/command', { method: 'POST', body: JSON.stringify(body), ...opts });
export const cancelCommand     = (id, opts) => request('/api/command/' + encodeURIComponent(id), { method: 'DELETE', ...opts });

// ─── Capabilities ─────────────────────────────────────────────────────────────
export const fetchCapabilities       = (opts) => request('/api/capabilities', opts);
export const fetchDeviceCapabilities = (opts) => request('/api/device-capabilities', opts);

// ─── Routing ──────────────────────────────────────────────────────────────────
export const fetchRoutes       = (opts) => request('/api/routes', opts);
export const postRouteTest     = (body, opts) => request('/api/route-test', { method: 'POST', body: JSON.stringify(body), ...opts });

// ─── Time / Alarms ────────────────────────────────────────────────────────────
export const fetchTime         = (opts) => request('/api/time', opts);
export const fetchTimeStatus   = (opts) => request('/api/time/status', opts);
export const fetchAlarms       = (opts) => request('/api/alarms', opts);
export const fetchAlarm        = (id, opts) => request('/api/alarm/' + encodeURIComponent(id), opts);
export const postAlarm         = (body, opts) => request('/api/alarm', { method: 'POST', body: JSON.stringify(body), ...opts });
export const putAlarm          = (id, body, opts) => request('/api/alarm/' + encodeURIComponent(id), { method: 'PUT', body: JSON.stringify(body), ...opts });
export const deleteAlarm       = (id, opts) => request('/api/alarm/' + encodeURIComponent(id), { method: 'DELETE', ...opts });

// ─── Shows ────────────────────────────────────────────────────────────────────
export const fetchShows        = (opts) => request('/api/shows', opts);
export const fetchShow         = (id, opts) => request('/api/show/' + encodeURIComponent(id), opts);
export const postLoadShow      = (id, opts) => request('/api/show/' + encodeURIComponent(id) + '/load', { method: 'POST', ...opts });

// ─── Runtime ──────────────────────────────────────────────────────────────────
export const fetchRuntime      = (opts) => request('/api/runtime', opts);
export const postTransport     = (action, opts) => request('/api/transport/' + encodeURIComponent(action), { method: 'POST', ...opts });

// ─── Emergency ────────────────────────────────────────────────────────────────
export const fetchEmergency    = (opts) => request('/api/emergency', opts);
export const postEmergencyStop = (opts) => request('/api/emergency/stop', { method: 'POST', ...opts });
export const postEmergencyClear = (opts) => request('/api/emergency/clear', { method: 'POST', ...opts });
