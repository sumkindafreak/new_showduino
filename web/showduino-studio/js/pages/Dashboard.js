/**
 * Showduino Studio – Dashboard
 *
 * Operational overview using shared runtime state.
 * No duplicated polling — observes the singleton runtime model.
 */

import { el, formatBytes, formatUptime, formatDuration, statRow, healthDot, makeCleanupGroup } from '../utils.js';
import { subscribe, getState, applySystemData } from '../state/runtime.js';
import { fetchSystem } from '../api.js';
import { MemoryBar } from '../components/MemoryBar.js';

function msToProgress(pos, dur) {
  if (!dur || dur === 0) return 0;
  return Math.min(100, (pos / dur) * 100);
}

export async function DashboardPage(container) {
  const cleanup = makeCleanupGroup();

  // ── Runtime card ────────────────────────────────────────────────────────
  const runtimeCard = el('div', { className: 'card card--teal' });
  runtimeCard.append(el('h2', { text: 'Show Runtime' }));
  const runtimeBody = el('div', {});
  runtimeCard.append(runtimeBody);

  // ── Show info card ───────────────────────────────────────────────────────
  const showCard = el('div', { className: 'card' });
  showCard.append(el('h2', { text: 'Current Show' }));
  const showBody = el('div', {});
  showCard.append(showBody);

  // ── Emergency card ───────────────────────────────────────────────────────
  const emergencyCard = el('div', { className: 'card' });
  emergencyCard.append(el('h2', { text: 'Safety' }));
  const emergencyBody = el('div', {});
  emergencyCard.append(emergencyBody);

  // ── System card ──────────────────────────────────────────────────────────
  const systemCard = el('div', { className: 'card' });
  systemCard.append(el('h2', { text: 'System' }));
  const systemBody = el('div', {});
  systemCard.append(systemBody);

  // ── Memory card ──────────────────────────────────────────────────────────
  const memCard = el('div', { className: 'card' });
  memCard.append(el('h2', { text: 'Memory' }));
  const memBody = el('div', {});
  memCard.append(memBody);

  // ── Nodes card ───────────────────────────────────────────────────────────
  const nodesCard = el('div', { className: 'card' });
  nodesCard.append(el('h2', { text: 'Node Health' }));
  const nodesBody = el('div', {});
  nodesCard.append(nodesBody);

  // ── Network card ─────────────────────────────────────────────────────────
  const networkCard = el('div', { className: 'card' });
  networkCard.append(el('h2', { text: 'Network' }));
  const networkBody = el('div', {});
  networkCard.append(networkBody);

  // ── Recent events card ───────────────────────────────────────────────────
  const eventsCard = el('div', { className: 'card' });
  eventsCard.append(el('h2', { text: 'Recent Events' }));
  const eventsBody = el('ul', { className: 'events-list' });
  eventsCard.append(eventsBody);

  // ── Layout ───────────────────────────────────────────────────────────────
  const grid = el('div', { className: 'dashboard-grid' }, [
    runtimeCard, showCard, emergencyCard,
    systemCard, memCard, nodesCard,
    networkCard, eventsCard,
  ]);
  container.append(grid);

  // ── Paint functions ──────────────────────────────────────────────────────

  function paintRuntime(state) {
    const r = state.runtime;
    runtimeBody.innerHTML = '';
    const rState = r.state || 'idle';
    runtimeBody.append(el('div', { className: 'value', text: rState.toUpperCase() }));

    const pct = msToProgress(r.position, r.duration);
    const track = el('div', { className: 'show-progress-track' });
    const fill = el('div', { className: 'show-progress-fill' });
    fill.style.width = `${pct}%`;
    track.append(fill);
    runtimeBody.append(track);

    const timeRow = el('div', { className: 'time-display' });
    const elapsed = el('div', {}, [
      el('div', { className: 't-label', text: 'Elapsed' }),
      el('div', { className: 't-value', text: formatDuration(r.elapsed || r.position) }),
    ]);
    const remaining = el('div', {}, [
      el('div', { className: 't-label', text: 'Remaining' }),
      el('div', { className: 't-value', text: formatDuration(r.remaining || (r.duration - r.position)) }),
    ]);
    timeRow.append(elapsed, remaining);
    runtimeBody.append(timeRow);

    if (r.cue) {
      runtimeBody.append(el('div', {
        className: 'sub',
        text: `Cue ${r.cue.number ?? ''}: ${r.cue.name || '—'}`,
      }));
    }
  }

  function paintShow(state) {
    showBody.innerHTML = '';
    const show = state.runtime?.currentShow;
    if (!show) {
      showBody.append(el('div', { className: 'value text-muted', text: 'No show loaded' }));
      return;
    }
    showBody.append(el('div', { className: 'value', text: show.name || show.id || '—' }));
    if (show.description) {
      showBody.append(el('div', { className: 'sub', text: show.description }));
    }
    if (show.duration) {
      showBody.append(el('div', { className: 'sub', text: `Duration: ${formatDuration(show.duration)}` }));
    }
  }

  function paintEmergency(state) {
    emergencyBody.innerHTML = '';
    const em = state.emergency;
    if (em.active) {
      emergencyCard.className = 'card card--accent';
      emergencyBody.append(el('div', { className: 'value text-error', text: em.level === 'critical' ? 'EMERGENCY ACTIVE' : 'WARNING ACTIVE' }));
      if (em.reason) emergencyBody.append(el('div', { className: 'sub', text: em.reason }));
    } else {
      emergencyCard.className = 'card card--success';
      emergencyBody.append(el('div', { className: 'value text-success', text: 'SAFE' }));
      emergencyBody.append(el('div', { className: 'sub', text: 'No emergency conditions' }));
    }
  }

  function paintSystem(sys) {
    if (!sys || !sys.boardName) return;
    systemBody.innerHTML = '';
    systemBody.append(el('div', { className: 'value', text: sys.boardName || 'Show Engine' }));
    systemBody.append(el('div', { className: 'sub', text: `FW ${sys.firmwareVersion || '—'} · Protocol ${sys.protocolVersion || '—'}` }));
    systemBody.append(statRow('Uptime', formatUptime(sys.uptime)));
    systemBody.append(statRow('CPU', sys.cpuMhz ? `${sys.cpuMhz} MHz` : '—'));
    systemBody.append(statRow('Storage', sys.storageReady ? 'Ready' : 'Recovery'));
  }

  function paintMemory(sys) {
    if (!sys || sys.heapTotal == null) return;
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
    const nodes = state.nodes || [];
    if (nodes.length === 0) {
      nodesBody.append(el('div', { className: 'sub', text: 'No nodes discovered' }));
      return;
    }
    const online  = nodes.filter((n) => n.online !== false && n.presence !== 'offline').length;
    const warning = nodes.filter((n) => n.presence === 'warning' || n.warning).length;
    const offline = nodes.length - online;

    nodesBody.append(statRow('Total', nodes.length));
    nodesBody.append(statRow('Online', online));
    if (warning > 0) nodesBody.append(statRow('Warning', warning));
    if (offline > 0) nodesBody.append(statRow('Offline', offline));

    for (const node of nodes.slice(0, 5)) {
      const status = node.online !== false && node.presence !== 'offline' ? 'online' : 'offline';
      nodesBody.append(el('div', { className: 'health-indicator' }, [
        healthDot(status),
        el('span', { className: 'stat-label', text: node.name || node.id || '—' }),
      ]));
    }
    if (nodes.length > 5) {
      nodesBody.append(el('div', { className: 'sub', text: `+${nodes.length - 5} more — see Nodes page` }));
    }
  }

  function paintNetwork(state) {
    networkBody.innerHTML = '';
    const net = state.network;
    if (!net || (!net.deviceCount && !net.networkHealth)) {
      networkBody.append(el('div', { className: 'sub', text: 'Awaiting network telemetry…' }));
      return;
    }
    if (net.networkHealth) {
      networkBody.append(el('div', { className: 'value', text: net.networkHealth }));
    }
    if (net.averageRssi != null) networkBody.append(statRow('Avg Signal', `${net.averageRssi} dBm`));
    if (net.latencyMs != null) networkBody.append(statRow('Latency', `${net.latencyMs} ms`));
    if (net.heartbeatRate != null) networkBody.append(statRow('Heartbeat', `${net.heartbeatRate}/min`));
  }

  function paintEvents(state) {
    const commands = state.commands?.history || [];
    eventsBody.innerHTML = '';
    if (commands.length === 0) {
      eventsBody.append(el('li', {}, [
        el('span', { className: 'evt-msg text-muted', text: 'No events yet' }),
      ]));
      return;
    }
    for (const cmd of commands.slice(0, 8)) {
      const ts = cmd.createdAt ? new Date(cmd.createdAt).toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' }) : '—';
      eventsBody.append(el('li', {}, [
        el('span', { className: 'evt-time', text: ts }),
        el('span', { className: 'evt-msg', text: `${cmd.category || '?'}/${cmd.action || '?'} → ${cmd.status || '?'}` }),
      ]));
    }
  }

  // ── Combined state paint ─────────────────────────────────────────────────

  function paint(state) {
    paintRuntime(state);
    paintShow(state);
    paintEmergency(state);
    paintNodes(state);
    paintNetwork(state);
    paintEvents(state);

    const sys = state.system;
    if (sys?.boardName) {
      paintSystem(sys);
      paintMemory(sys);
    }
  }

  cleanup.add(subscribe(paint));

  // Bootstrap: fetch system info immediately
  try {
    const sys = await fetchSystem();
    applySystemData(sys);
  } catch (_) {
    // System API may not be reachable; runtime will reflect connection state
  }

  // Poll system info every 10 seconds (system data is relatively stable)
  const sysTimer = setInterval(async () => {
    try {
      const sys = await fetchSystem();
      applySystemData(sys);
    } catch (_) {}
  }, 10000);
  cleanup.add(() => clearInterval(sysTimer));

  return () => cleanup.run();
}

DashboardPage.title = 'Dashboard';
