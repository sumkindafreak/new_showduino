import { fetchDevices } from '../api.js';
import { el } from '../utils.js';
import { DeviceCard } from '../components/DeviceCard.js';
import { connectLive, subscribeLive } from '../live.js';

export async function DevicesPage(container) {
  container.append(el('p', {
    className: 'info-panel',
    text: 'Live device inventory from SUE (C3). Updates push over WebSocket — no polling.'
  }));

  const status = el('div', { className: 'live-status', text: 'Connecting live feed…' });
  const grid = el('div', { className: 'page-grid' });
  container.append(status, grid);

  function render(devices) {
    grid.innerHTML = '';
    const list = Array.isArray(devices) ? devices : [];
    if (list.length === 0) {
      grid.append(el('div', { className: 'card' }, [el('p', { text: 'No devices discovered yet.' })]));
      return;
    }
    for (const d of list) grid.append(DeviceCard(d));
  }

  connectLive();
  const unsub = subscribeLive((snap) => {
    status.textContent = `Live · ${snap.devices.length} device(s)`;
    render(snap.devices);
  });

  try {
    const data = await fetchDevices();
    render(data.devices || []);
    status.textContent = `REST + Live · ${(data.devices || []).length} device(s)`;
  } catch (err) {
    status.textContent = err.message;
  }

  return () => unsub();
}
DevicesPage.title = 'Devices';