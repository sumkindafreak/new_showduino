import { fetchLogs } from '../api.js';
import { el, formatTimestamp, severityClass } from '../utils.js';

export async function LogsPage(container) {
  container.append(el('p', { className: 'info-panel', text: 'Web API request log ring buffer (250 entries in PSRAM). Auto-refreshes every 2 seconds.' }));

  const wrap = el('div', { className: 'card' });
  const table = el('table', { className: 'log-table' });
  table.innerHTML = '<thead><tr><th>Time</th><th>Level</th><th>Source</th><th>Message</th></tr></thead>';
  const tbody = el('tbody');
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
        const tr = el('tr', {}, [
          el('td', { text: formatTimestamp(entry.timestampMs) }),
          el('td', { className: severityClass(entry.severity), text: entry.severity }),
          el('td', { text: entry.source || '—' }),
          el('td', { text: entry.message || '—' })
        ]);
        tbody.append(tr);
      }
      if (list.length === 0) {
        tbody.append(el('tr', {}, [el('td', { colSpan: '4', text: 'No log entries yet.' })]));
      }
    } catch (err) {
      tbody.innerHTML = '';
      tbody.append(el('tr', {}, [el('td', { colSpan: '4', text: err.message })]));
    }
  }

  await refresh();
  const timer = setInterval(refresh, 2000);
  return () => clearInterval(timer);
}
LogsPage.title = 'Logs';
