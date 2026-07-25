/**
 * Showduino Studio – Nodes (Device Inventory)
 *
 * Live node/device inventory driven by runtimeStore.
 * The store polls /api/devices every 5 s and receives WebSocket pushes —
 * this page subscribes and renders; it adds no independent polling.
 */

import { el, statRow, makeCleanupGroup } from '../utils.js';
import { subscribeRuntime } from '../state/runtimeStore.js';
import { DeviceCard } from '../components/DeviceCard.js';

export function NodesPage(container) {
  const cleanup = makeCleanupGroup();

  const status     = el('div', { className: 'live-status', text: 'Connecting…' });
  const summaryRow = el('div', { className: 'page-grid' });
  const grid       = el('div', { className: 'page-grid' });

  container.append(
    status,
    summaryRow,
    el('h2', { className: 'nav-section-label', style: 'margin: 1rem 0 0.5rem;', text: 'Nodes' }),
    grid
  );

  function paintSummary(state) {
    summaryRow.innerHTML = '';
    const { total = 0, online = 0, warning = 0, offline = 0 } = state.nodeCollection.counts;

    for (const c of [
      { label: 'Total',   value: total,   cls: '' },
      { label: 'Online',  value: online,  cls: 'card--teal' },
      { label: 'Warning', value: warning, cls: warning > 0 ? 'card--warn' : '' },
      { label: 'Offline', value: offline, cls: offline > 0 ? 'card--accent' : '' },
    ]) {
      summaryRow.append(el('div', { className: `card ${c.cls}` }, [
        el('h2', { text: c.label }),
        el('div', { className: 'value', text: String(c.value) }),
      ]));
    }

    const net = state.networkState;
    if (net.health && net.health !== 'unknown') {
      summaryRow.append(el('div', { className: 'card network-health-card' }, [
        el('h2', { text: 'Network Health' }),
        el('div', { className: 'value', text: net.health }),
        ...(net.averageRssi  != null ? [statRow('Avg Signal', `${net.averageRssi} dBm`)] : []),
        ...(net.heartbeatRate != null ? [statRow('Heartbeat', `${net.heartbeatRate}/min`)] : []),
      ]));
    }
  }

  function paintNodes(state) {
    grid.innerHTML = '';
    const nodes = state.nodeCollection.nodes;
    if (nodes.length === 0) {
      grid.append(el('div', { className: 'card' }, [
        el('p', { className: 'text-muted', text: 'No nodes discovered yet. Nodes announce themselves via ESP-NOW.' }),
      ]));
      return;
    }
    for (const d of nodes) grid.append(DeviceCard(d));
  }

  function paint(state) {
    const { online = 0, total = 0 } = state.nodeCollection.counts;
    status.textContent = `Live · ${online}/${total} node(s) online`;
    paintSummary(state);
    paintNodes(state);
  }

  cleanup.add(subscribeRuntime(paint));

  return () => cleanup.run();
}

NodesPage.title = 'Nodes';
