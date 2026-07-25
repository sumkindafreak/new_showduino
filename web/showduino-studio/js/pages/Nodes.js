/**
 * Showduino Studio – Nodes (Device Inventory)
 *
 * Live node/device inventory using shared runtime state.
 * Updates push over WebSocket — no independent polling.
 */

import { el, statRow, makeCleanupGroup } from '../utils.js';
import { subscribe, getState, applyNetworkData, setNodes } from '../state/runtime.js';
import { fetchDevices, fetchNetwork } from '../api.js';
import { DeviceCard } from '../components/DeviceCard.js';

export function NodesPage(container) {
  const cleanup = makeCleanupGroup();

  const status = el('div', { className: 'live-status', text: 'Connecting…' });
  const summaryRow = el('div', { className: 'page-grid' });
  const grid = el('div', { className: 'page-grid' });

  container.append(status, summaryRow, el('h2', { className: 'nav-section-label', style: 'margin: 1rem 0 0.5rem;', text: 'Nodes' }), grid);

  function paintSummary(state) {
    summaryRow.innerHTML = '';
    const nodes = state.nodes || [];
    const online  = nodes.filter((n) => n.online !== false && n.presence !== 'offline').length;
    const warning = nodes.filter((n) => n.presence === 'warning' || n.warning).length;
    const offline = nodes.length - online;

    const counts = [
      { label: 'Total', value: nodes.length, cls: '' },
      { label: 'Online', value: online, cls: 'card--teal' },
      { label: 'Warning', value: warning, cls: warning > 0 ? 'card--warn' : '' },
      { label: 'Offline', value: offline, cls: offline > 0 ? 'card--accent' : '' },
    ];

    for (const c of counts) {
      const card = el('div', { className: `card ${c.cls}` }, [
        el('h2', { text: c.label }),
        el('div', { className: 'value', text: String(c.value) }),
      ]);
      summaryRow.append(card);
    }

    // Network health summary
    const net = state.network;
    if (net && net.networkHealth) {
      const netCard = el('div', { className: 'card network-health-card' }, [
        el('h2', { text: 'Network Health' }),
        el('div', { className: 'value', text: net.networkHealth }),
        net.averageRssi != null ? statRow('Avg Signal', `${net.averageRssi} dBm`) : null,
        net.heartbeatRate != null ? statRow('Heartbeat', `${net.heartbeatRate}/min`) : null,
      ].filter(Boolean));
      summaryRow.append(netCard);
    }
  }

  function paintNodes(state) {
    grid.innerHTML = '';
    const nodes = state.nodes || [];
    if (nodes.length === 0) {
      grid.append(el('div', { className: 'card' }, [
        el('p', { className: 'text-muted', text: 'No nodes discovered yet. Nodes announce themselves via ESP-NOW.' }),
      ]));
      return;
    }
    for (const d of nodes) grid.append(DeviceCard(d));
  }

  function paint(state) {
    const nodes = state.nodes || [];
    const online = nodes.filter((n) => n.online !== false && n.presence !== 'offline').length;
    status.textContent = `Live · ${online}/${nodes.length} node(s) online`;
    paintSummary(state);
    paintNodes(state);
  }

  cleanup.add(subscribe(paint));

  // Seed from REST on first load
  Promise.all([
    fetchDevices().catch(() => null),
    fetchNetwork().catch(() => null),
  ]).then(([devData, netData]) => {
    if (devData?.devices) setNodes(devData.devices);
    if (netData) applyNetworkData(netData);
  });

  return () => cleanup.run();
}

NodesPage.title = 'Nodes';
