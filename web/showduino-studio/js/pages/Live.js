/**
 * Showduino Studio – Live Page
 *
 * Real-time show playback view using shared runtime state.
 * Shows exactly what the Director currently knows: progress, cues,
 * audio, lighting, node activity, latency and warnings.
 */

import { el, formatDuration, statRow, makeCleanupGroup } from '../utils.js';
import { subscribe, getState, ConnectionState } from '../state/runtime.js';

function progressPct(pos, dur) {
  if (!dur || dur === 0) return 0;
  return Math.min(100, (pos / dur) * 100);
}

export function LivePage(container) {
  const cleanup = makeCleanupGroup();

  // ── Connection status pill ─────────────────────────────────────────────
  const connStatus = el('div', { className: 'live-status' });

  // ── Show progress card ─────────────────────────────────────────────────
  const progressCard = el('div', { className: 'card live-progress-card' });
  progressCard.append(el('h2', { text: 'Show Progress' }));

  const showTitle = el('div', { className: 'value', text: 'No show loaded' });
  const progressTrack = el('div', { className: 'live-progress-track' });
  const progressFill = el('div', { className: 'live-progress-fill' });
  progressFill.style.width = '0%';
  progressTrack.append(progressFill);

  const timeRow = el('div', { className: 'live-time-row' });
  const elapsedBlock = el('div', { className: 'live-t-block' }, [
    el('div', { className: 'live-t-label', text: 'Elapsed' }),
    el('div', { className: 'live-t-val', text: '--:--' }),
  ]);
  const remainingBlock = el('div', { className: 'live-t-block' }, [
    el('div', { className: 'live-t-label', text: 'Remaining' }),
    el('div', { className: 'live-t-val', text: '--:--' }),
  ]);
  const durationBlock = el('div', { className: 'live-t-block' }, [
    el('div', { className: 'live-t-label', text: 'Duration' }),
    el('div', { className: 'live-t-val', text: '--:--' }),
  ]);
  timeRow.append(elapsedBlock, remainingBlock, durationBlock);

  progressCard.append(showTitle, progressTrack, timeRow);

  // ── Current cue card ───────────────────────────────────────────────────
  const cueCard = el('div', { className: 'card cue-card' });
  cueCard.append(el('h2', { text: 'Current Cue' }));
  const cueNumber = el('div', { className: 'cue-number', text: '—' });
  const cueName   = el('div', { className: 'cue-name', text: 'No cue' });
  const cueTime   = el('div', { className: 'cue-time', text: '' });
  cueCard.append(cueNumber, cueName, cueTime);

  // ── Next cue card ──────────────────────────────────────────────────────
  const nextCueCard = el('div', { className: 'card cue-card' });
  nextCueCard.append(el('h2', { text: 'Next Cue' }));
  const nextCueNum  = el('div', { className: 'cue-number', text: '—' });
  const nextCueName = el('div', { className: 'cue-name', text: '—' });
  const nextCueTime = el('div', { className: 'cue-time', text: '' });
  nextCueCard.append(nextCueNum, nextCueName, nextCueTime);

  // ── Node activity card ─────────────────────────────────────────────────
  const nodesCard = el('div', { className: 'card' });
  nodesCard.append(el('h2', { text: 'Node Activity' }));
  const nodesBody = el('div', {});
  nodesCard.append(nodesBody);

  // ── Network health card ────────────────────────────────────────────────
  const netCard = el('div', { className: 'card' });
  netCard.append(el('h2', { text: 'Transport Health' }));
  const netBody = el('div', {});
  netCard.append(netBody);

  // ── Command activity card ──────────────────────────────────────────────
  const cmdCard = el('div', { className: 'card' });
  cmdCard.append(el('h2', { text: 'Executing Actions' }));
  const cmdBody = el('div', {});
  cmdCard.append(cmdBody);

  // ── Warnings card ──────────────────────────────────────────────────────
  const warnCard = el('div', { className: 'card' });
  warnCard.append(el('h2', { text: 'Warnings' }));
  const warnBody = el('div', {});
  warnCard.append(warnBody);

  // ── Layout ─────────────────────────────────────────────────────────────
  container.append(
    connStatus,
    progressCard,
    el('div', { className: 'live-layout' }, [
      cueCard, nextCueCard,
      nodesCard, netCard,
      cmdCard, warnCard,
    ])
  );

  // ── Paint ──────────────────────────────────────────────────────────────

  function paintConnection(state) {
    const label = {
      [ConnectionState.CONNECTING]:  'Connecting to Director…',
      [ConnectionState.CONNECTED]:   'Live',
      [ConnectionState.DEGRADED]:    'Signal degraded',
      [ConnectionState.RECONNECTING]:`Reconnecting (attempt ${state.reconnectAttempts})…`,
      [ConnectionState.OFFLINE]:     'Offline',
    }[state.connection] || '';
    connStatus.textContent = label;
    connStatus.className = `live-status ls-${state.connection}`;
  }

  function paintProgress(state) {
    const r = state.runtime;
    const rState = r.state || 'idle';
    progressCard.className = `card live-progress-card live-state-${rState}`;

    const show = r.currentShow;
    showTitle.textContent = show ? (show.name || show.id || '—') : 'No show loaded';

    const pct = progressPct(r.position, r.duration);
    progressFill.style.width = `${pct}%`;

    elapsedBlock.querySelector('.live-t-val').textContent = formatDuration(r.elapsed || r.position);
    remainingBlock.querySelector('.live-t-val').textContent = formatDuration(r.remaining || Math.max(0, r.duration - r.position));
    durationBlock.querySelector('.live-t-val').textContent = formatDuration(r.duration);
  }

  function paintCue(state) {
    const cue = state.runtime?.cue;
    if (cue) {
      cueNumber.textContent = cue.number != null ? `Cue ${cue.number}` : 'Current Cue';
      cueName.textContent = cue.name || '—';
      cueTime.textContent = cue.timeMs != null ? `@ ${formatDuration(cue.timeMs)}` : '';
    } else {
      cueNumber.textContent = '—';
      cueName.textContent = 'No cue';
      cueTime.textContent = '';
    }

    const next = state.runtime?.nextCue;
    if (next) {
      nextCueNum.textContent = next.number != null ? `Cue ${next.number}` : 'Next';
      nextCueName.textContent = next.name || '—';
      nextCueTime.textContent = next.timeMs != null ? `@ ${formatDuration(next.timeMs)}` : '';
    } else {
      nextCueNum.textContent = '—';
      nextCueName.textContent = '—';
      nextCueTime.textContent = '';
    }
  }

  function paintNodes(state) {
    nodesBody.innerHTML = '';
    const nodes = state.nodes || [];
    if (nodes.length === 0) {
      nodesBody.append(el('div', { className: 'text-muted sub', text: 'No nodes' }));
      return;
    }
    const table = el('table', { className: 'log-table' });
    const thead = el('thead', {}, [
      el('tr', {}, [el('th', { text: 'Node' }), el('th', { text: 'Status' }), el('th', { text: 'Signal' })]),
    ]);
    const tbody = el('tbody', {});
    for (const n of nodes) {
      const online = n.online !== false && n.presence !== 'offline';
      tbody.append(el('tr', {}, [
        el('td', { text: n.name || n.id || '—' }),
        el('td', {}, [el('span', { className: `badge ${online ? 'online' : 'offline'}`, text: online ? 'Online' : 'Offline' })]),
        el('td', { className: 'mono', text: n.rssi != null ? `${n.rssi} dBm` : '—' }),
      ]));
    }
    table.append(thead, tbody);
    nodesBody.append(table);
  }

  function paintNet(state) {
    netBody.innerHTML = '';
    const net = state.network;
    if (!net) {
      netBody.append(el('div', { className: 'sub text-muted', text: 'Awaiting telemetry…' }));
      return;
    }
    if (net.networkHealth) netBody.append(statRow('Health', net.networkHealth));
    if (net.latencyMs != null) netBody.append(statRow('Latency', `${net.latencyMs} ms`));
    if (net.averageRssi != null) netBody.append(statRow('Avg RSSI', `${net.averageRssi} dBm`));
    if (net.heartbeatRate != null) netBody.append(statRow('Heartbeat', `${net.heartbeatRate}/min`));
    if (net.onlineCount != null && net.deviceCount != null) {
      netBody.append(statRow('Online nodes', `${net.onlineCount}/${net.deviceCount}`));
    }
  }

  function paintCommands(state) {
    cmdBody.innerHTML = '';
    const running = state.commands?.running || [];
    const queued  = state.commands?.queue || [];
    if (running.length === 0 && queued.length === 0) {
      cmdBody.append(el('div', { className: 'sub text-muted', text: 'No active commands' }));
      return;
    }
    for (const cmd of [...running, ...queued].slice(0, 6)) {
      cmdBody.append(el('div', { className: 'health-indicator' }, [
        el('span', { className: `badge ${cmd.status === 'started' ? 'running' : 'warning'}`, text: cmd.status || '?' }),
        el('span', { className: 'stat-label', text: `${cmd.category || '?'}/${cmd.action || '?'} → ${cmd.destination || '?'}` }),
      ]));
    }
  }

  function paintWarnings(state) {
    warnBody.innerHTML = '';
    const warnings = [];

    if (state.connection === ConnectionState.DEGRADED) warnings.push('Signal degraded');
    if (state.connection === ConnectionState.RECONNECTING) warnings.push(`Reconnecting (attempt ${state.reconnectAttempts})`);
    if (state.emergency?.active) warnings.push(`Emergency: ${state.emergency.reason || state.emergency.level || 'active'}`);

    const warnNodes = (state.nodes || []).filter((n) => n.presence === 'warning' || n.warning);
    for (const n of warnNodes) warnings.push(`Node warning: ${n.name || n.id}`);

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
    paintCommands(state);
    paintWarnings(state);
  }

  cleanup.add(subscribe(paint));

  return () => cleanup.run();
}

LivePage.title = 'Live';
