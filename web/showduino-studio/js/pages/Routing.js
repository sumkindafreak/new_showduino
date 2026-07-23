import { fetchRoutes, postRouteTest } from '../api.js';
import { el } from '../utils.js';
import { connectLive, subscribeLive } from '../live.js';

export async function RoutingPage(container) {
  container.append(el('p', {
    className: 'info-panel',
    text: 'Device Router — resolve ShowCommands to capable devices. Test only; no hardware execution.'
  }));

  const form = el('div', { className: 'card command-form' });
  form.append(el('h2', { text: 'Route Test' }));
  const fields = {
    source: el('input', { value: 'web-studio' }),
    destination: el('input', { value: 'any', placeholder: 'any | ian | sue | broadcast' }),
    category: el('input', { value: 'relay' }),
    action: el('input', { value: 'Set' }),
    priority: el('input', { value: 'normal' }),
    payload: el('input', { value: '{}' })
  };
  for (const [k, input] of Object.entries(fields)) {
    form.append(el('label', { className: 'cmd-field' }, [k + ' ', input]));
  }
  const resultHost = el('div', { className: 'route-result' });
  const status = el('div', { className: 'live-status', text: 'Ready' });
  form.append(el('button', {
    className: 'btn-primary',
    text: 'POST /api/route-test',
    onClick: async () => {
      try {
        status.textContent = 'Resolving…';
        const data = await postRouteTest({
          source: fields.source.value,
          destination: fields.destination.value,
          category: fields.category.value,
          action: fields.action.value,
          priority: fields.priority.value,
          payload: fields.payload.value
        });
        paintResult(data);
        status.textContent = data.ok ? 'Resolved' : 'Failed';
      } catch (err) {
        status.textContent = err.message;
      }
    }
  }));
  container.append(form, status, resultHost);

  const rulesHost = el('div', { className: 'card' });
  container.append(rulesHost);

  function paintResult(data) {
    resultHost.innerHTML = '';
    const card = el('div', { className: 'card' });
    card.append(el('h2', { text: 'Routing Decision' }));
    const rows = [
      ['Resolved Device', data.resolvedDevice ? `${data.resolvedDevice.name || ''} (${data.resolvedDevice.id || ''})` : '—'],
      ['Board', data.resolvedDevice?.board || '—'],
      ['Decision', data.routingDecision || '—'],
      ['Capability', data.capability || '—'],
      ['Path', data.path || '—'],
      ['Fallback Used', data.fallbackUsed ? 'yes' : 'no'],
      ['Reason', data.reason || '—'],
      ['OK', data.ok ? 'yes' : 'no']
    ];
    const table = el('table', { className: 'log-table' });
    const tbody = el('tbody', {});
    for (const [k, v] of rows) {
      tbody.append(el('tr', {}, [el('th', { text: k }), el('td', { text: String(v) })]));
    }
    table.append(tbody);
    card.append(table);
    resultHost.append(card);
  }

  function paintRules(data) {
    rulesHost.innerHTML = '';
    rulesHost.append(el('h2', { text: 'Routing Table' }));
    const table = el('table', { className: 'log-table' });
    table.append(el('thead', {}, [
      el('tr', {}, [
        el('th', { text: 'Match' }), el('th', { text: 'Policy' }), el('th', { text: 'Capability' })
      ])
    ]));
    const tbody = el('tbody', {});
    for (const r of (data.rules || [])) {
      tbody.append(el('tr', {}, [
        el('td', { text: r.match || '—' }),
        el('td', { text: r.policy || '—' }),
        el('td', { text: r.capability || '—' })
      ]));
    }
    table.append(tbody);
    rulesHost.append(table);
    if (data.last && data.last.decision) {
      rulesHost.append(el('p', {
        className: 'live-status',
        text: `Last: ${data.last.decision} → ${data.last.deviceId || '—'} (${data.last.reason || ''})`
      }));
    }
  }

  connectLive();
  const unsub = subscribeLive((snap) => {
    if (snap.lastRoute) paintResult(snap.lastRoute);
  });

  try {
    paintRules(await fetchRoutes());
  } catch (err) {
    status.textContent = err.message;
  }

  return () => unsub();
}
RoutingPage.title = 'Routing';