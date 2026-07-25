/**
 * Showduino Studio – Dashboard
 *
 * Operational overview driven entirely by the shared runtimeStore.
 * No independent polling timers — the store owns all REST and WebSocket traffic.
 */

import { el, formatBytes, formatUptime, formatDuration, statRow, healthDot, makeCleanupGroup } from '../utils.js';
import { subscribeRuntime } from '../state/runtimeStore.js';
import { MemoryBar } from '../components/MemoryBar.js';

export function DashboardPage(container) {
  const cleanup = makeCleanupGroup();

  // ── Cards ─────────────────────────────────────────────────────────────────
  const runtimeCard = el('div', { className: 'card card--teal' });
  runtimeCard.append(el('h2', { text: 'Show Runtime' }));
  const runtimeBody = el('div', {});
  runtimeCard.append(runtimeBody);

  const showCard = el('div', { className: 'card' });
  showCard.append(el('h2', { text: 'Current Show' }));
  const showBody = el('div', {});
  showCard.append(showBody);

  const emergencyCard = el('div', { className: 'card' });
  emergencyCard.append(el('h2', { text: 'Safety' }));
  const emergencyBody = el('div', {});
  emergencyCard.append(emergencyBody);

  const systemCard = el('div', { className: 'card' });
  systemCard.append(el('h2', { text: 'System' }));
  const systemBody = el('div', {});
  systemCard.append(systemBody);

  const memCard = el('div', { className: 'card' });
  memCard.append(el('h2', { text: 'Memory' }));
  const memBody = el('div', {});
  memCard.append(memBody);

  const nodesCard = el('div', { className: 'card' });
  nodesCard.append(el('h2', { text: 'Node Health' }));
  const nodesBody = el('div', {});
  nodesCard.append(nodesBody);

  const networkCard = el('div', { className: 'card' });
  networkCard.append(el('h2', { text: 'Network' }));
  const networkBody = el('div', {});
  networkCard.append(networkBody);

  const eventsCard = el('div', { className: 'card' });
  eventsCard.append(el('h2', { text: 'Recent Events' }));
  const eventsBody = el('ul', { className: 'events-list' });
  eventsCard.append(eventsBody);

  const grid = el('div', { className: 'dashboard-grid' }, [
    runtimeCard, showCard, emergencyCard,
    systemCard, memCard, nodesCard,
    networkCard, eventsCard,
  ]);
  container.append(grid);

  // ── Paint ─────────────────────────────────────────────────────────────────

  function paintRuntime(state) {
    const rs = state.runtimeStatus;
    runtimeBody.innerHTML = '';
    const rState = (rs.runtime || 'idle').toLowerCase();
    runtimeBody.append(el('div', { className: 'value', text: rState.toUpperCase() }));

    const pct = rs.progress != null ? rs.progress * 100 : 0;
    const track = el('div', { className: 'show-progress-track' });
    const fill  = el('div', { className: 'show-progress-fill' });
    fill.style.width = `${pct}%`;
    track.append(fill);
    runtimeBody.append(track);

    const timeRow = el('div', { className: 'time-display' });
    timeRow.append(el('div', {}, [
      el('div', { className: 't-label', text: 'Elapsed' }),
      el('div', { className: 't-value', text: formatDuration(rs.elapsedMs) }),
    ]));
    timeRow.append(el('div', {}, [
      el('div', { className: 't-label', text: 'Remaining' }),
      el('div', { className: 't-value', text: formatDuration(rs.remainingMs) }),
    ]));
    runtimeBody.append(timeRow);

    if (rs.cue && rs.cue !== 'Not reported') {
      runtimeBody.append(el('div', { className: 'sub', text: `Cue ${rs.cue}` }));
    }
  }

  function paintShow(state) {
    showBody.innerHTML = '';
    const showName = state.runtimeStatus.currentShow;
    const hasShow  = showName && showName !== 'Not reported';
    if (!hasShow) {
      showBody.append(el('div', { className: 'value text-muted', text: 'No show loaded' }));
      return;
    }
    showBody.append(el('div', { className: 'value', text: showName }));
    const dur = state.runtimeStatus.timelineLengthMs;
    if (dur) showBody.append(el('div', { className: 'sub', text: `Duration: ${formatDuration(dur)}` }));
  }

  function paintEmergency(state) {
    emergencyBody.innerHTML = '';
    if (state.emergencyState.active) {
      emergencyCard.className = 'card card--accent';
      emergencyBody.append(el('div', { className: 'value text-error', text: 'EMERGENCY ACTIVE' }));
      if (state.emergencyState.status !== 'active') {
        emergencyBody.append(el('div', { className: 'sub', text: state.emergencyState.status }));
      }
    } else {
      emergencyCard.className = 'card card--success';
      emergencyBody.append(el('div', { className: 'value text-success', text: 'SAFE' }));
      emergencyBody.append(el('div', { className: 'sub', text: 'No emergency conditions' }));
    }
  }

  function paintSystem(state) {
    const sys = state.systemState;
    if (!sys.boardName || sys.boardName === 'Show Engine') return;
    systemBody.innerHTML = '';
    systemBody.append(el('div', { className: 'value', text: sys.boardName }));
    systemBody.append(el('div', { className: 'sub', text: `FW ${sys.firmwareVersion || '—'} · Protocol ${sys.protocolVersion || '—'}` }));
    systemBody.append(statRow('Uptime', formatUptime(sys.uptime)));
    if (sys.cpuMhz) systemBody.append(statRow('CPU', `${sys.cpuMhz} MHz`));
    systemBody.append(statRow('Storage', sys.storageReady ? 'Ready' : 'Recovery'));
  }

  function paintMemory(state) {
    const sys = state.systemState;
    if (sys.heapTotal == null) return;
    memBody.innerHTML = '';
    memBody.append(MemoryBar({ label: 'Heap', used: sys.heapTotal - sys.heapFree, total: sys.heapTotal, variant: 'heap' }));
    if (sys.psramTotal) {
      memBody.append(MemoryBar({ label: 'PSRAM', used: sys.psramTotal - sys.psramFree, total: sys.psramTotal, variant: 'psram' }));
    }
    memBody.append(statRow('Heap Free', formatBytes(sys.heapFree)));
    if (sys.psramFree) memBody.append(statRow('PSRAM Free', formatBytes(sys.psramFree)));
  }

  function paintNodes(state) {
    nodesBody.innerHTML = '';
    const nodes = state.nodeCollection.nodes;
    if (nodes.length === 0) {
      nodesBody.append(el('div', { className: 'sub', text: 'No nodes discovered' }));
      return;
    }
    const { online = 0, warning = 0, offline = 0 } = state.nodeCollection.counts;
    nodesBody.append(statRow('Total', nodes.length));
    nodesBody.append(statRow('Online', online));
    if (warning > 0) nodesBody.append(statRow('Warning', warning));
    if (offline > 0) nodesBody.append(statRow('Offline', offline));
    for (const node of nodes.slice(0, 5)) {
      const status = (node.presence || (node.online ? 'online' : 'offline')).toLowerCase();
      nodesBody.append(el('div', { className: 'health-indicator' }, [
        healthDot(status),
        el('span', { className: 'stat-label', text: node.friendlyName || node.name || node.id || '—' }),
      ]));
    }
    if (nodes.length > 5) {
      nodesBody.append(el('div', { className: 'sub', text: `+${nodes.length - 5} more — see Nodes page` }));
    }
  }

  function paintNetwork(state) {
    networkBody.innerHTML = '';
    const net = state.networkState;
    if (!net.health || net.health === 'unknown') {
      networkBody.append(el('div', { className: 'sub', text: 'Awaiting network telemetry…' }));
      return;
    }
    networkBody.append(el('div', { className: 'value', text: net.health }));
    if (net.averageRssi != null) networkBody.append(statRow('Avg Signal', `${net.averageRssi} dBm`));
    if (state.connectionStatus.latencyMs != null) networkBody.append(statRow('Latency', `${state.connectionStatus.latencyMs} ms`));
    if (net.heartbeatRate != null) networkBody.append(statRow('Heartbeat', `${net.heartbeatRate}/min`));
  }

  function paintEvents(state) {
    eventsBody.innerHTML = '';
    const events = state.latestEvents;
    if (events.length === 0) {
      eventsBody.append(el('li', {}, [el('span', { className: 'evt-msg text-muted', text: 'No events yet' })]));
      return;
    }
    for (const evt of events.slice(0, 8)) {
      const ts = evt.at ? new Date(evt.at).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' }) : '—';
      eventsBody.append(el('li', {}, [
        el('span', { className: 'evt-time', text: ts }),
        el('span', { className: `evt-msg sev-${evt.level || 'info'}`, text: `[${evt.source || '?'}] ${evt.message || ''}` }),
      ]));
    }
  }

  function paint(state) {
    paintRuntime(state);
    paintShow(state);
    paintEmergency(state);
    paintSystem(state);
    paintMemory(state);
    paintNodes(state);
    paintNetwork(state);
    paintEvents(state);
  }

  cleanup.add(subscribeRuntime(paint));

  return () => cleanup.run();
}

DashboardPage.title = 'Dashboard';
