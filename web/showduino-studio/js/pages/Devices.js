import { dispatchOperatorCommand, initializeRuntimeStore, refreshRuntime, subscribeRuntime } from '../runtimeStore.js';
import { el, formatAgeFromNow, statRow } from '../utils.js';

function qualityLabel(rssi) {
  if (rssi == null || isNaN(rssi)) return 'unreported';
  if (rssi >= -55) return `Excellent (${rssi} dBm)`;
  if (rssi >= -67) return `Good (${rssi} dBm)`;
  if (rssi >= -75) return `Fair (${rssi} dBm)`;
  return `Weak (${rssi} dBm)`;
}

function badge(device) {
  const status = (device.presence || (device.online ? 'online' : 'offline')).toLowerCase();
  const className = status === 'warning' ? 'warning' : (status === 'online' ? 'online' : 'offline');
  const label = className === 'warning' ? 'Warning' : (className === 'online' ? 'Online' : 'Offline');
  return el('span', { className: `badge ${className}`, text: label });
}

function controlButton(text, action) {
  return el('button', { className: 'btn-cancel', text, onClick: action });
}

async function invoke(statusEl, action, args) {
  statusEl.textContent = `Dispatching ${action}…`;
  try {
    await dispatchOperatorCommand(action, args);
    statusEl.textContent = `${action} queued`;
  } catch (error) {
    statusEl.textContent = error.message;
  }
}

export function DevicesPage(container) {
  initializeRuntimeStore();
  container.append(el('p', {
    className: 'info-panel',
    text: 'Node Manager from runtimeStore. Discovery, transitions, capabilities, heartbeat age, and operator commands run through the central dispatcher.'
  }));

  const status = el('div', { className: 'live-status', text: 'Loading node state…' });
  const toolbar = el('div', { className: 'card command-form' });
  toolbar.append(el('h2', { text: 'Node Actions' }));
  toolbar.append(controlButton('Refresh Discovery', () => invoke(status, 'node.refresh', { id: 'any' })));
  toolbar.append(controlButton('Reload Devices', async () => {
    status.textContent = 'Refreshing device snapshot…';
    await refreshRuntime('devices').catch(() => {});
  }));

  const grid = el('div', { className: 'page-grid' });
  container.append(status, toolbar, grid);

  const unsub = subscribeRuntime((snap) => {
    const devices = Array.isArray(snap.devices) ? snap.devices : [];
    status.textContent = snap.connection.connected
      ? `Live · ${devices.length} node(s) · ${snap.network?.networkHealth || 'health unreported'}`
      : `Disconnected · reconnect in ${snap.connection.reconnectInSec || 0}s`;

    grid.innerHTML = '';
    if (devices.length === 0) {
      grid.append(el('div', { className: 'card' }, [
        el('h2', { text: 'No Nodes' }),
        el('p', { text: 'No live nodes discovered yet.' })
      ]));
      return;
    }

    for (const node of devices) {
      const card = el('article', { className: 'device-card' });
      const top = el('div', { className: 'device-card-header' }, [
        el('div', {}, [
          el('div', { className: 'device-name', text: node.friendlyName || node.name || node.id || 'Unknown Node' }),
          el('div', { className: 'device-role', text: node.boardType || node.board || node.role || 'node' })
        ]),
        badge(node)
      ]);
      card.append(top);
      card.append(statRow('Node ID', node.id || '—'));
      card.append(statRow('Signal', qualityLabel(node.rssi)));
      card.append(statRow('Firmware', node.firmwareVersion || node.firmware || 'unreported'));
      card.append(statRow('Capabilities', node.capabilities || 'unreported'));
      card.append(statRow('Last Heartbeat', formatAgeFromNow(node._seenAtLocalMs)));
      card.append(statRow('Latency', node.latencyMs != null ? `${node.latencyMs} ms` : 'unreported'));
      card.append(statRow('Connection', node.connectionType || node.connectionStatus || 'unreported'));
      card.append(statRow('Protocol', node.protocolVersion || node.protocol || 'unreported'));
      card.append(statRow('MAC', node.mac || 'unreported'));

      const controls = el('div', { className: 'cap-chips' });
      controls.append(controlButton('Restart', () => invoke(status, 'node.restart', { id: node.id })));
      controls.append(controlButton('Identify', () => invoke(status, 'node.identify', { id: node.id })));
      controls.append(controlButton('Refresh', () => invoke(status, 'node.refresh', { id: node.id })));
      controls.append(controlButton('Rename', () => {
        const name = prompt('New node name', node.friendlyName || node.name || '');
        if (!name) return;
        invoke(status, 'node.rename', { id: node.id, name });
      }));
      card.append(controls);

      grid.append(card);
    }
  });

  return () => unsub();
}
DevicesPage.title = 'Node Manager';