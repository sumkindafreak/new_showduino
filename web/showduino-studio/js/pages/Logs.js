/**
 * Showduino Studio – Logs
 *
 * Displays the log ring buffer from the shared runtimeStore.
 * The store polls /api/logs every 5 s — no independent timer here.
 */

import { el, formatTimestamp, severityClass, makeCleanupGroup } from '../utils.js';
import { subscribeRuntime } from '../state/runtimeStore.js';

export function LogsPage(container) {
  const cleanup = makeCleanupGroup();

  container.append(el('p', { className: 'info-panel', text: 'Web API request log ring buffer (up to 250 entries in PSRAM). Refreshes every 5 s via the shared runtime store.' }));

  const wrap  = el('div', { className: 'card' });
  const table = el('table', { className: 'log-table' });
  table.append(el('thead', {}, [
    el('tr', {}, ['Time', 'Level', 'Source', 'Message'].map((t) => el('th', { text: t }))),
  ]));
  const tbody = el('tbody', {});
  table.append(tbody);
  wrap.append(table);
  container.append(wrap);

  function paint(state) {
    const list = Array.isArray(state.logs) ? state.logs.slice().reverse() : [];
    tbody.innerHTML = '';
    if (list.length === 0) {
      tbody.append(el('tr', {}, [el('td', { colSpan: '4', className: 'text-muted', text: 'No log entries yet.' })]));
      return;
    }
    for (const entry of list) {
      tbody.append(el('tr', {}, [
        el('td', { className: 'mono',              text: formatTimestamp(entry.timestampMs) }),
        el('td', { className: severityClass(entry.severity), text: entry.severity || '—' }),
        el('td', {                                 text: entry.source  || '—' }),
        el('td', {                                 text: entry.message || '—' }),
      ]));
    }
  }

  cleanup.add(subscribeRuntime(paint));

  return () => cleanup.run();
}

LogsPage.title = 'Logs';
