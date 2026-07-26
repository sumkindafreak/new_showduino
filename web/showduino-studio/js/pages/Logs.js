import { dispatchOperatorCommand, initializeRuntimeStore, subscribeRuntime } from '../runtimeStore.js';
import { el, formatTimestamp, severityClass } from '../utils.js';

export function LogsPage(container) {
  initializeRuntimeStore();
  container.append(el('p', {
    className: 'info-panel',
    text: 'Event log streamed and refreshed exclusively through runtimeStore.'
  }));

  const status = el('div', { className: 'live-status', text: 'Loading logs…' });
  const controls = el('div', { className: 'card command-form' });
  controls.append(el('h2', { text: 'Filters' }));

  const severity = el('select');
  for (const level of ['all', 'debug', 'info', 'warn', 'error']) {
    severity.append(el('option', { value: level, text: level.toUpperCase() }));
  }
  const search = el('input', { placeholder: 'search logs' });
  const autoScroll = el('input', { type: 'checkbox', checked: 'checked' });
  controls.append(el('label', { className: 'cmd-field' }, ['Severity ', severity]));
  controls.append(el('label', { className: 'cmd-field' }, ['Search ', search]));
  controls.append(el('label', { className: 'cmd-field' }, ['Auto-scroll ', autoScroll]));
  controls.append(el('button', {
    className: 'btn-primary',
    text: 'Clear Log',
    onClick: async () => {
      status.textContent = 'Clearing log…';
      try {
        await dispatchOperatorCommand('log.clear');
      } catch (error) {
        status.textContent = error.message;
      }
    }
  }));

  const wrap = el('div', { className: 'card' });
  const table = el('table', { className: 'log-table' });
  table.innerHTML = '<thead><tr><th>Time</th><th>Level</th><th>Source</th><th>Message</th></tr></thead>';
  const tbody = el('tbody');
  table.append(tbody);
  wrap.append(table);
  container.append(status, controls, wrap);

  function render(entries, connected) {
    const severityValue = severity.value;
    const term = search.value.trim().toLowerCase();
    const filtered = entries.filter((entry) => {
      const sev = (entry.severity || 'info').toLowerCase();
      if (severityValue !== 'all' && sev !== severityValue) return false;
      if (!term) return true;
      return `${entry.source || ''} ${entry.message || ''}`.toLowerCase().includes(term);
    });

    status.textContent = `${connected ? 'Live' : 'Disconnected'} · ${filtered.length} entries`;
    tbody.innerHTML = '';
    if (!filtered.length) {
      tbody.append(el('tr', {}, [el('td', { colSpan: '4', text: 'No matching entries.' })]));
      return;
    }
    for (const entry of filtered) {
      const tr = el('tr', {}, [
        el('td', { text: formatTimestamp(entry.timestampMs) }),
        el('td', { className: severityClass(entry.severity), text: String(entry.severity || 'info').toUpperCase() }),
        el('td', { text: entry.source || '—' }),
        el('td', { text: entry.message || '—' })
      ]);
      tbody.append(tr);
    }
    if (autoScroll.checked) {
      wrap.scrollTop = wrap.scrollHeight;
    }
  }

  let currentEntries = [];
  let currentConnected = false;
  const rerender = () => render(currentEntries, currentConnected);
  severity.addEventListener('change', rerender);
  search.addEventListener('input', rerender);

  const unsub = subscribeRuntime((snap) => {
    currentEntries = Array.isArray(snap.logs) ? snap.logs : [];
    currentConnected = snap.connection.connected;
    render(currentEntries, currentConnected);
  });

  return () => unsub();
}
LogsPage.title = 'Event Log';
