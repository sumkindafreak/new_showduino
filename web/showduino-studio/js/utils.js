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

export function formatDurationMs(ms) {
  if (ms == null || isNaN(ms)) return '—';
  if (ms < 1000) return `${Math.max(0, Math.floor(ms))} ms`;
  const total = Math.floor(ms / 1000);
  const h = Math.floor(total / 3600);
  const m = Math.floor((total % 3600) / 60);
  const s = total % 60;
  if (h > 0) return `${h}:${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
  return `${m}:${String(s).padStart(2, '0')}`;
}

export function formatAgeFromNow(seenAtMs) {
  if (!seenAtMs) return '—';
  const delta = Math.max(0, Date.now() - seenAtMs);
  if (delta < 1000) return '<1s ago';
  if (delta < 60000) return `${Math.floor(delta / 1000)}s ago`;
  if (delta < 3600000) return `${Math.floor(delta / 60000)}m ago`;
  return `${Math.floor(delta / 3600000)}h ago`;
}

export function formatTimestamp(ms) {
  if (ms == null) return '—';
  const d = new Date(ms);
  return d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit', fractionalSecondDigits: 3 });
}

export function severityClass(sev) {
  const map = { debug: 'sev-debug', info: 'sev-info', warn: 'sev-warn', warning: 'sev-warn', error: 'sev-error' };
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
