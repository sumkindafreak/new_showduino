import { fetchCommands, postCommand, cancelCommand } from '../api.js';
import { el } from '../utils.js';
import { connectLive, subscribeLive } from '../live.js';

function cmdRow(cmd, { cancellable = false } = {}) {
  const row = el('tr', { 'data-id': cmd.id || '' }, [
    el('td', { text: cmd.priority || '—' }),
    el('td', { text: cmd.status || '—' }),
    el('td', { text: cmd.source || '—' }),
    el('td', { text: cmd.destination || '—' }),
    el('td', { text: cmd.category || '—' }),
    el('td', { text: cmd.action || '—' }),
    el('td', { text: cmd.executionTimeMs != null ? `${cmd.executionTimeMs} ms` : '—' }),
    el('td', { text: cmd.id || '—' })
  ]);
  if (cancellable && cmd.id) {
    const td = el('td', {});
    td.append(el('button', {
      className: 'btn-cancel',
      text: 'Cancel',
      onClick: async () => {
        try { await cancelCommand(cmd.id); } catch (err) { alert(err.message); }
      }
    }));
    row.append(td);
  } else {
    row.append(el('td', { text: '' }));
  }
  return row;
}

function section(title, rows, cancellable = false) {
  const card = el('div', { className: 'card' });
  card.append(el('h2', { text: title }));
  const table = el('table', { className: 'log-table cmd-table' });
  table.append(el('thead', {}, [
    el('tr', {}, [
      el('th', { text: 'Priority' }), el('th', { text: 'Status' }), el('th', { text: 'Source' }),
      el('th', { text: 'Dest' }), el('th', { text: 'Category' }), el('th', { text: 'Action' }),
      el('th', { text: 'Exec' }), el('th', { text: 'ID' }), el('th', { text: '' })
    ])
  ]));
  const tbody = el('tbody', {});
  if (!rows.length) tbody.append(el('tr', {}, [el('td', { colspan: '9', text: 'None' })]));
  else for (const c of rows) tbody.append(cmdRow(c, { cancellable }));
  table.append(tbody);
  card.append(table);
  return card;
}

export async function CommandsPage(container) {
  container.append(el('p', {
    className: 'info-panel',
    text: 'ShowCommand bus — validate, queue, dispatch (no hardware). Live via WebSocket.'
  }));

  const form = el('div', { className: 'card command-form' });
  form.append(el('h2', { text: 'Submit Command' }));
  const fields = {
    source: el('input', { value: 'web-studio' }),
    destination: el('input', { value: 'ian' }),
    category: el('input', { value: 'system' }),
    action: el('input', { value: 'ping' }),
    priority: el('input', { value: 'normal' }),
    payload: el('input', { value: '{}' })
  };
  for (const [k, input] of Object.entries(fields)) {
    form.append(el('label', { className: 'cmd-field' }, [k + ' ', input]));
  }
  const status = el('div', { className: 'live-status', text: 'Live feed…' });
  form.append(el('button', {
    className: 'btn-primary',
    text: 'POST /api/command',
    onClick: async () => {
      try {
        await postCommand({
          source: fields.source.value,
          destination: fields.destination.value,
          category: fields.category.value,
          action: fields.action.value,
          priority: fields.priority.value,
          payload: fields.payload.value
        });
      } catch (err) {
        alert(err.message);
      }
    }
  }));
  container.append(form, status);

  const queueHost = el('div', {});
  const runningHost = el('div', {});
  const doneHost = el('div', {});
  container.append(queueHost, runningHost, doneHost);

  function paint(cmds) {
    const hist = cmds.history || [];
    queueHost.innerHTML = '';
    runningHost.innerHTML = '';
    doneHost.innerHTML = '';
    queueHost.append(section(`Live Queue (${cmds.queueDepth ?? (cmds.queue || []).length})`, cmds.queue || [], true));
    runningHost.append(section('Running', cmds.running || []));
    doneHost.append(section('History (completed / failed / cancelled / rejected)', hist));
    status.textContent = `Live · queue ${cmds.queueDepth ?? 0} · emergency ${cmds.emergencyDepth ?? 0}`;
  }

  connectLive();
  const unsub = subscribeLive((snap) => paint(snap.commands || {}));

  try {
    const data = await fetchCommands();
    paint({
      queue: data.queue || [],
      running: data.running || [],
      history: data.history || [],
      queueDepth: data.queueDepth,
      emergencyDepth: data.emergencyDepth
    });
  } catch (err) {
    status.textContent = err.message;
  }

  return () => unsub();
}
CommandsPage.title = 'Commands';