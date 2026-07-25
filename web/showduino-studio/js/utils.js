/**
 * Showduino Studio – Utility helpers
 */

export function $(sel, root = document) {
  return root.querySelector(sel);
}

export function el(tag, attrs = {}, children = []) {
  const node = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) {
    if (k === 'className') node.className = v;
    else if (k === 'text') node.textContent = v;
    else if (k.startsWith('on') && typeof v === 'function') node.addEventListener(k.slice(2).toLowerCase(), v);
    else node.setAttribute(k, v);
  }
  for (const child of children) {
    if (child == null) continue;
    node.append(typeof child === 'string' ? document.createTextNode(child) : child);
  }
  return node;
}

export function formatBytes(n) {
  if (n == null || isNaN(n)) return '—';
  if (n >= 1048576) return (n / 1048576).toFixed(1) + ' MB';
  if (n >= 1024) return (n / 1024).toFixed(1) + ' KB';
  return n + ' B';
}

export function formatUptime(ms) {
  if (ms == null) return '—';
  const s = Math.floor(ms / 1000);
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  const sec = s % 60;
  if (h > 0) return `${h}h ${m}m ${sec}s`;
  if (m > 0) return `${m}m ${sec}s`;
  return `${sec}s`;
}

export function formatTimestamp(ms) {
  if (ms == null) return '—';
  const d = new Date(ms);
  return d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit', fractionalSecondDigits: 3 });
}

export function formatDuration(ms) {
  if (ms == null || isNaN(ms)) return '--:--';
  const totalSec = Math.floor(ms / 1000);
  const h = Math.floor(totalSec / 3600);
  const m = Math.floor((totalSec % 3600) / 60);
  const s = totalSec % 60;
  if (h > 0) return `${String(h).padStart(2,'0')}:${String(m).padStart(2,'0')}:${String(s).padStart(2,'0')}`;
  return `${String(m).padStart(2,'0')}:${String(s).padStart(2,'0')}`;
}

export function formatPercent(value, total) {
  if (total == null || total === 0) return 0;
  return Math.round((value / total) * 100);
}

export function severityClass(sev) {
  const map = { debug: 'sev-debug', info: 'sev-info', warn: 'sev-warn', error: 'sev-error' };
  return map[sev] || 'sev-info';
}

export function statRow(label, value) {
  return el('div', { className: 'stat-row' }, [
    el('span', { className: 'stat-label', text: label }),
    el('span', { className: 'stat-value', text: String(value ?? '—') })
  ]);
}

export function archBlock(label, value) {
  return el('dl', { className: 'arch-block' }, [
    el('dt', { text: label }),
    el('dd', { text: value ?? '—' })
  ]);
}

export function progressBar(value, total, variant = 'fill-teal') {
  const pct = total > 0 ? Math.min(100, Math.round((value / total) * 100)) : 0;
  const fill = el('div', { className: `progress-bar-fill ${variant}` });
  fill.style.width = `${pct}%`;
  return el('div', { className: 'progress-bar-track' }, [fill]);
}

export function healthDot(status) {
  const cls = { ok: 'ok', good: 'ok', online: 'ok', connected: 'ok',
                warn: 'warn', warning: 'warn', degraded: 'warn',
                error: 'err', offline: 'err', critical: 'err' }[status] || 'dim';
  return el('span', { className: `health-dot ${cls}` });
}

export function makeCleanupGroup() {
  const fns = [];
  return {
    add(fn) { fns.push(fn); },
    run() { for (const fn of fns) { try { fn(); } catch (_) {} } fns.length = 0; }
  };
}

// Alias so Timeline.js and others can import formatDurationMs
export const formatDurationMs = formatDuration;

export function formatRelativeMs(ms) {
  if (ms == null || isNaN(ms)) return '—';
  const s = Math.floor(Math.abs(ms) / 1000);
  if (s < 5) return 'just now';
  if (s < 60) return `${s}s ago`;
  const m = Math.floor(s / 60);
  if (m < 60) return `${m}m ago`;
  return `${Math.floor(m / 60)}h ago`;
}

export function severityRank(sev) {
  return { debug: 0, info: 1, warn: 2, error: 3, critical: 4 }[(sev || '').toLowerCase()] ?? 1;
}
