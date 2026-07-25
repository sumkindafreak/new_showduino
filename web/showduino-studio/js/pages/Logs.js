import { el, formatTimestamp, severityClass } from '../utils.js';
import { subscribeRuntime } from '../state/runtimeStore.js';

export function LogsPage(container) {
  container.append(el('p', { className: 'info-panel', text: 'Unified runtime/event logs from the shared store. Page does not poll directly.' }));

  const wrap = el('div', { className: 'card' });
  const table = el('table', { className: 'log-table' });
  table.innerHTML = '<thead><tr><th>Time</th><th>Level</th><th>Source</th><th>Message</th></tr></thead>';
  const tbody = el('tbody');
  table.append(tbody);
  wrap.append(table);
  container.append(wrap);

  const unsub = subscribeRuntime((state) => {
    const entries = Array.isArray(state.logs) ? state.logs : [];
    tbody.innerHTML = '';
    for (const entry of entries.slice().reverse()) {
      const tr = el('tr', {}, [
        el('td', { text: formatTimestamp(entry.timestampMs || entry.at) }),
        el('td', { className: severityClass(entry.severity || entry.level), text: entry.severity || entry.level || 'info' }),
        el('td', { text: entry.source || 'runtime' }),
        el('td', { text: entry.message || '—' })
      ]);
      tbody.append(tr);
    }
    if (entries.length === 0) {
      tbody.append(el('tr', {}, [el('td', { colSpan: '4', text: 'No log entries yet.' })]));
    }
  });

  return () => unsub();
}
LogsPage.title = 'Logs';
LogsPage.subtitle = 'Centralised runtime and API logs.';
