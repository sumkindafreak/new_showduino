import { fetchCapabilities, fetchDeviceCapabilities } from '../api.js';
import { el } from '../utils.js';
import { connectLive, subscribeLive } from '../live.js';

export async function CapabilitiesPage(container) {
  container.append(el('p', {
    className: 'info-panel',
    text: 'Capability inventory — what each device can do. No hardware execution.'
  }));

  const status = el('div', { className: 'live-status', text: 'Loading…' });
  const catalogHost = el('div', { className: 'card' });
  const groupsHost = el('div', {});
  container.append(status, catalogHost, groupsHost);

  function paint(catalog, grouped) {
    catalogHost.innerHTML = '';
    catalogHost.append(el('h2', { text: 'Capability Types' }));
    const chips = el('div', { className: 'cap-chips' });
    for (const c of (catalog.capabilities || [])) {
      chips.append(el('span', { className: 'cap-chip', text: c.name }));
    }
    catalogHost.append(chips);

    groupsHost.innerHTML = '';
    const by = grouped.byCapability || {};
    const names = Object.keys(by).sort();
    if (!names.length) {
      groupsHost.append(el('div', { className: 'card', text: 'No capability providers online yet.' }));
      return;
    }
    for (const name of names) {
      const card = el('div', { className: 'card' });
      card.append(el('h2', { text: name }));
      const table = el('table', { className: 'log-table' });
      table.append(el('thead', {}, [
        el('tr', {}, [
          el('th', { text: 'Device' }), el('th', { text: 'Board' }), el('th', { text: 'Online' }),
          el('th', { text: 'Firmware' }), el('th', { text: 'Priority' }), el('th', { text: 'Preferred' }),
          el('th', { text: 'Owner' })
        ])
      ]));
      const tbody = el('tbody', {});
      for (const d of by[name]) {
        tbody.append(el('tr', {}, [
          el('td', { text: d.name || d.id }),
          el('td', { text: d.board || '—' }),
          el('td', { text: d.online ? 'online' : (d.presence || 'offline') }),
          el('td', { text: d.firmware || '—' }),
          el('td', { text: String(d.priority ?? '—') }),
          el('td', { text: d.preferred ? 'yes' : 'no' }),
          el('td', { text: d.owner || d.name || '—' })
        ]));
      }
      table.append(tbody);
      card.append(table);
      groupsHost.append(card);
    }
    status.textContent = `Live · ${names.length} capabilities with providers`;
  }

  async function reload() {
    try {
      const [catalog, grouped] = await Promise.all([fetchCapabilities(), fetchDeviceCapabilities()]);
      paint(catalog, grouped);
    } catch (err) {
      status.textContent = err.message;
    }
  }

  connectLive();
  const unsub = subscribeLive((snap) => {
    if (snap.capabilityTick != null) reload();
  });
  await reload();
  return () => unsub();
}
CapabilitiesPage.title = 'Capabilities';