/**
 * Showduino Studio – Logs
 *
 * Web API request log ring buffer (250 entries in PSRAM).
 * Auto-refreshes every 2 seconds.
 */

import { el, formatTimestamp, severityClass, makeCleanupGroup } from '../utils.js';
import { fetchLogs } from '../api.js';

export function LogsPage(container) {
  const cleanup = makeCleanupGroup();

  container.append(el('p', { className: 'info-panel', text: 'Web API request log ring buffer (250 entries in PSRAM). Auto-refreshes every 2 seconds.' }));

  const wrap  = el('div', { className: 'card' });
  const table = el('table', { className: 'log-table' });
  const thead = el('thead', {}, [
    el('tr', {}, ['Time', 'Level', 'Source', 'Message'].map((t) => el('th', { text: t }))),
  ]);
  table.append(thead);
  const tbody = el('tbody', {});
  table.append(tbody);
  wrap.append(table);
  container.append(wrap);

  async function refresh() {
    try {
      const data = await fetchLogs();
      const entries = data.logs || data;
      tbody.innerHTML = '';
      const list = Array.isArray(entries) ? entries : [];
      for (const entry of list.slice().reverse()) {
        tbody.append(el('tr', {}, [
          el('td', { className: 'mono', text: formatTimestamp(entry.timestampMs) }),
          el('td', { className: severityClass(entry.severity), text: entry.severity }),
          el('td', { text: entry.source || '—' }),
          el('td', { text: entry.message || '—' }),
        ]));
      }
      if (list.length === 0) {
        tbody.append(el('tr', {}, [el('td', { colSpan: '4', className: 'text-muted', text: 'No log entries yet.' })]));
      }
    } catch (err) {
      tbody.innerHTML = '';
      tbody.append(el('tr', {}, [el('td', { colSpan: '4', text: err.message })]));
    }
  }

  refresh();
  const timer = setInterval(refresh, 2000);
  cleanup.add(() => clearInterval(timer));

  return () => cleanup.run();
}

LogsPage.title = 'Logs';
