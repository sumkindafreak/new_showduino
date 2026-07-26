const BASE = '';
const DEFAULT_TIMEOUT_MS = 3500;

export class ApiError extends Error {
  constructor(message, { status = 0, path = '', timeout = false, detail = '' } = {}) {
    super(message);
    this.name = 'ApiError';
    this.status = status;
    this.path = path;
    this.timeout = timeout;
    this.detail = detail;
  }
}

function delay(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

function parseJsonSafe(text) {
  if (!text) return null;
  try { return JSON.parse(text); } catch (_) { return null; }
}

function friendlyError(path, status, detail) {
  if (status === 502) return `${path} unavailable (firmware bridge offline)`;
  if (status === 503) return `${path} unavailable (service busy)`;
  if (status === 504) return `${path} timed out (gateway timeout)`;
  if (status >= 500) return `${path} failed (${status})`;
  if (status === 404) return `${path} not supported by this firmware`;
  return `${path} rejected (${status})${detail ? `: ${detail}` : ''}`;
}

export async function request(path, options = {}) {
  const {
    method = 'GET',
    body,
    timeoutMs = DEFAULT_TIMEOUT_MS,
    retries = method === 'GET' ? 1 : 0,
    retryDelayMs = 250,
    allowNotFound = false
  } = options;

  let attempt = 0;
  const normalizedBody = body == null ? undefined : (typeof body === 'string' ? body : JSON.stringify(body));
  const hasBody = normalizedBody != null;

  while (attempt <= retries) {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), timeoutMs);
    try {
      const res = await fetch(BASE + path, {
        method,
        signal: ctrl.signal,
        headers: {
          Accept: 'application/json',
          ...(hasBody ? { 'Content-Type': 'application/json' } : {})
        },
        body: normalizedBody
      });
      clearTimeout(timer);

      const text = res.status === 204 ? '' : await res.text();
      if (!res.ok) {
        if (allowNotFound && res.status === 404) {
          return null;
        }
        throw new ApiError(
          friendlyError(path, res.status, text),
          { status: res.status, path, detail: text }
        );
      }
      if (!text) return null;
      return parseJsonSafe(text) ?? text;
    } catch (err) {
      clearTimeout(timer);
      const timedOut = err?.name === 'AbortError';
      const wrapped = err instanceof ApiError
        ? err
        : new ApiError(
          timedOut ? `${path} request timed out` : `${path} request failed`,
          { path, timeout: timedOut }
        );
      if (attempt >= retries || wrapped.status >= 400 && wrapped.status < 500) {
        throw wrapped;
      }
      await delay(retryDelayMs * (attempt + 1));
      attempt += 1;
    }
  }

  throw new ApiError(`${path} request failed`, { path });
}

export function fetchSystem() { return request('/api/system'); }
export function fetchDevices() { return request('/api/devices'); }
export function fetchDevice(id) { return request('/api/device/' + encodeURIComponent(id)); }
export function fetchNetwork() { return request('/api/network'); }
export function fetchLogs() { return request('/api/logs'); }
export function fetchCommands() { return request('/api/commands'); }
export function fetchCommand(id) { return request('/api/command/' + encodeURIComponent(id)); }
export function postCommand(body) { return request('/api/command', { method: 'POST', body }); }
export function cancelCommand(id) { return request('/api/command/' + encodeURIComponent(id), { method: 'DELETE' }); }
export function fetchCapabilities() { return request('/api/capabilities'); }
export function fetchDeviceCapabilities() { return request('/api/device-capabilities'); }
export function fetchRoutes() { return request('/api/routes'); }
export function postRouteTest(body) { return request('/api/route-test', { method: 'POST', body }); }
export function fetchTime() { return request('/api/time'); }
export function fetchTimeStatus() { return request('/api/time/status'); }
export function postTimeAlarm(body) { return request('/api/time/alarm', { method: 'POST', body }); }
export function clearTimeAlarm() { return request('/api/time/alarm', { method: 'DELETE' }); }
export function fetchAssets() { return request('/api/assets', { allowNotFound: true }); }
export function deleteAsset(path) { return request('/api/assets/' + encodeURIComponent(path), { method: 'DELETE' }); }
export function uploadAsset(payload) { return request('/api/assets/upload', { method: 'POST', body: payload, timeoutMs: 12000 }); }