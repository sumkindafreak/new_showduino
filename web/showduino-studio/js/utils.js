export function $(sel, root = document) {
  return root.querySelector(sel);
}

export function el(tag, attrs = {}, children = []) {
  const node = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) {
    if (k === 'className') node.className = v;
    else if (k === 'text') node.textContent = v;
    else if (k === 'disabled') node.disabled = !!v;
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

export function formatPlayhead(ms) {
  const total = Math.max(0, Number(ms) || 0);
  const minutes = Math.floor(total / 60000).toString().padStart(2, '0');
  const seconds = Math.floor((total % 60000) / 1000).toString().padStart(2, '0');
  const frac = Math.floor(total % 1000).toString().padStart(3, '0');
  return `${minutes}:${seconds}.${frac}`;
}

export function p4OfflineBanner(message) {
  return el('div', { className: 'offline-banner', role: 'status' }, [
    el('strong', { text: 'P4 OFFLINE' }),
    el('span', { text: message || 'Show Engine is not reachable. P4-owned controls are disabled. No show state is invented.' })
  ]);
}

export function plannedNote(text) {
  return el('p', { className: 'planned-note' }, [
    el('span', { className: 'status-chip warn', text: 'PLANNED' }),
    el('span', { text: text })
  ]);
}

export function emptyState(title, text) {
  return el('div', { className: 'empty-state' }, [
    el('h3', { text: title }),
    el('p', { text })
  ]);
}

export function tableWrap(table) {
  return el('div', { className: 'table-wrap' }, [table]);
}

export function resultBox(text) {
  return el('div', { className: 'reply-box', text: text || 'No confirmed P4 result yet.' });
}
