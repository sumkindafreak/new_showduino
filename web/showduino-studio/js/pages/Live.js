/**
 * Showduino Studio – Live Page
 *
 * Real-time show playback view driven by the shared runtimeStore.
 * No independent polling — all data arrives via the store's WebSocket and REST schedule.
 */

import { el, formatDuration, statRow, makeCleanupGroup } from '../utils.js';
import { subscribeRuntime } from '../state/runtimeStore.js';

export function LivePage(container) {
  const cleanup = makeCleanupGroup();

  // ── Connection pill ────────────────────────────────────────────────────────
  const connStatus = el('div', { className: 'live-status' });

  // ── Show progress card ─────────────────────────────────────────────────────
  const progressCard = el('div', { className: 'card live-progress-card' });
  progressCard.append(el('h2', { text: 'Show Progress' }));
  const showTitle   = el('div', { className: 'value', text: 'No show loaded' });
  const progTrack   = el('div', { className: 'live-progress-track' });
  const progFill    = el('div', { className: 'live-progress-fill' });
  progFill.style.width = '0%';
  progTrack.append(progFill);
  const timeRow     = el('div', { className: 'live-time-row' });
  const elapsedVal  = el('div', { className: 'live-t-val', text: '--:--' });
  const remainVal   = el('div', { className: 'live-t-val', text: '--:--' });
  const durVal      = el('div', { className: 'live-t-val', text: '--:--' });
  timeRow.append(
    el('div', { className: 'live-t-block' }, [el('div', { className: 'live-t-label', text: 'Elapsed' }), elapsedVal]),
    el('div', { className: 'live-t-block' }, [el('div', { className: 'live-t-label', text: 'Remaining' }), remainVal]),
    el('div', { className: 'live-t-block' }, [el('div', { className: 'live-t-label', text: 'Duration' }), durVal]),
  );
  progressCard.append(showTitle, progTrack, timeRow);

  // ── Cue card ───────────────────────────────────────────────────────────────
  const cueCard = el('div', { className: 'card cue-card' });
  cueCard.append(el('h2', { text: 'Current Cue' }));
  const cueDisplay = el('div', { className: 'cue-number', text: '—' });
  cueCard.append(cueDisplay);

  // ── Node activity card ─────────────────────────────────────────────────────
  const nodesCard = el('div', { className: 'card' });
  nodesCard.append(el('h2', { text: 'Node Activity' }));
  const nodesBody = el('div', {});
  nodesCard.append(nodesBody);

  // ── Transport health card ──────────────────────────────────────────────────
  const netCard = el('div', { className: 'card' });
  netCard.append(el('h2', { text: 'Transport Health' }));
  const netBody = el('div', {});
  netCard.append(netBody);

  // ── Executing actions card ─────────────────────────────────────────────────
  const cmdCard = el('div', { className: 'card' });
  cmdCard.append(el('h2', { text: 'Executing Actions' }));
  const cmdBody = el('div', {});
  cmdCard.append(cmdBody);

  // ── Warnings card ──────────────────────────────────────────────────────────
  const warnCard = el('div', { className: 'card' });
  warnCard.append(el('h2', { text: 'Warnings' }));
  const warnBody = el('div', {});
  warnCard.append(warnBody);

  container.append(
    connStatus,
    progressCard,
    el('div', { className: 'live-layout' }, [
      cueCard, nodesCard, netCard, cmdCard, warnCard,
    ])
  );

  // ── Paint ──────────────────────────────────────────────────────────────────

  function paintConnection(state) {
    const lifecycle = state.connectionStatus.lifecycle || 'connecting';
    const retries   = state.connectionStatus.retries;
    const labels    = {
      connecting:   'Connecting to Director…',
      connected:    'Live',
      degraded:     'Signal degraded',
      reconnecting: `Reconnecting (attempt ${retries})…`,
      offline:      'Offline',
    };
    connStatus.textContent = labels[lifecycle] || '';
    connStatus.className   = `live-status ls-${lifecycle}`;
  }

  function paintProgress(state) {
    const rs    = state.runtimeStatus;
    const rState = (rs.runtime || 'idle').toLowerCase();
    progressCard.className   = `card live-progress-card live-state-${rState}`;
    const showName = rs.currentShow;
    const hasShow  = showName && showName !== 'Not reported';
    showTitle.textContent    = hasShow ? showName : 'No show loaded';
    progFill.style.width     = `${(rs.progress || 0) * 100}%`;
    elapsedVal.textContent   = formatDuration(rs.elapsedMs);
    remainVal.textContent    = formatDuration(rs.remainingMs);
    durVal.textContent       = formatDuration(rs.timelineLengthMs);
  }

  function paintCue(state) {
    const cue = state.runtimeStatus.cue;
    cueDisplay.textContent = (cue && cue !== 'Not reported') ? `Cue ${cue}` : '—';
  }

  function paintNodes(state) {
    nodesBody.innerHTML = '';
    const nodes = state.nodeCollection.nodes;
    if (nodes.length === 0) {
      nodesBody.append(el('div', { className: 'text-muted sub', text: 'No nodes' }));
      return;
    }
    const table = el('table', { className: 'log-table' });
    table.append(el('thead', {}, [
      el('tr', {}, [el('th', { text: 'Node' }), el('th', { text: 'Status' }), el('th', { text: 'Signal' })]),
    ]));
    const tbody = el('tbody', {});
    for (const n of nodes) {
      const pres = (n.presence || (n.online ? 'online' : 'offline')).toLowerCase();
      tbody.append(el('tr', {}, [
        el('td', { text: n.friendlyName || n.name || n.id || '—' }),
        el('td', {}, [el('span', { className: `badge ${pres}`, text: pres })]),
        el('td', { className: 'mono', text: n.rssi != null ? `${n.rssi} dBm` : '—' }),
      ]));
    }
    table.append(tbody);
    nodesBody.append(table);
  }

  function paintNet(state) {
    netBody.innerHTML = '';
    const net = state.networkState;
    if (!net.health || net.health === 'unknown') {
      netBody.append(el('div', { className: 'sub text-muted', text: 'Awaiting telemetry…' }));
      return;
    }
    netBody.append(statRow('Health', net.health));
    if (state.connectionStatus.latencyMs != null) netBody.append(statRow('Latency', `${state.connectionStatus.latencyMs} ms`));
    if (net.averageRssi != null) netBody.append(statRow('Avg RSSI', `${net.averageRssi} dBm`));
    if (net.heartbeatRate != null) netBody.append(statRow('Heartbeat', `${net.heartbeatRate}/min`));
    const { online = 0, total = 0 } = state.nodeCollection.counts;
    if (total > 0) netBody.append(statRow('Online nodes', `${online}/${total}`));
  }

  function paintActions(state) {
    cmdBody.innerHTML = '';
    const actions = state.runtimeStatus.executingActions || [];
    if (actions.length === 0) {
      cmdBody.append(el('div', { className: 'sub text-muted', text: 'No active commands' }));
      return;
    }
    for (const a of actions) {
      cmdBody.append(el('div', { className: 'health-indicator' }, [
        el('span', { className: `badge ${a.status === 'started' ? 'running' : 'warning'}`, text: a.status || '?' }),
        el('span', { className: 'stat-label', text: `${a.action} → ${a.destination}` }),
      ]));
    }
  }

  function paintWarnings(state) {
    warnBody.innerHTML = '';
    const warnings = [];
    const lifecycle = state.connectionStatus.lifecycle;
    if (lifecycle === 'degraded')    warnings.push('Signal degraded');
    if (lifecycle === 'reconnecting') warnings.push(`Reconnecting (attempt ${state.connectionStatus.retries})`);
    if (state.emergencyState.active) warnings.push(`Emergency: ${state.emergencyState.status}`);
    for (const w of (state.runtimeStatus.warnings || [])) warnings.push(w);
    const warnNodes = state.nodeCollection.nodes.filter((n) => n.presence === 'warning');
    for (const n of warnNodes) warnings.push(`Node warning: ${n.friendlyName || n.name || n.id}`);

    if (warnings.length === 0) {
      warnBody.append(el('div', { className: 'sub text-success', text: 'No warnings' }));
      return;
    }
    for (const w of warnings) {
      warnBody.append(el('div', { className: 'warn-panel', text: w }));
    }
  }

  function paint(state) {
    paintConnection(state);
    paintProgress(state);
    paintCue(state);
    paintNodes(state);
    paintNet(state);
    paintActions(state);
    paintWarnings(state);
  }

  cleanup.add(subscribeRuntime(paint));

  return () => cleanup.run();
}

LivePage.title = 'Live';
