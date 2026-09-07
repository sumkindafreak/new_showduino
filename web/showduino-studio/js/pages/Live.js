import { fetchShow, postCommand, isP4Offline } from '../api.js';
import { subscribeStore } from '../store.js';
import { el, formatPlayhead, p4OfflineBanner, statRow, tableWrap } from '../utils.js';
import { emergencyWord, loopWord, productionLabel, showDisplayState } from '../status.js';

export async function LivePage(container) {
  container.append(el('p', {
    className: 'info-panel',
    text: 'Authoritative P4 runtime. Buttons stay pending until the Show Engine confirms. Browser clear cannot bypass the physical 3s-hold window.'
  }));

  const host = el('div', { className: 'page-stack' });
  container.append(host);

  let show = null;
  let pending = null;
  let lastResult = 'No confirmed P4 result yet.';
  let lastSnap = { p4Online: false, system: null };

  function sys() { return lastSnap.system; }

  function confirmedState() {
    return showDisplayState(sys());
  }

  async function send(cmd, confirmText) {
    if (confirmText && !window.confirm(confirmText)) return;
    pending = cmd;
    lastResult = `${cmd} … waiting for P4`;
    paint();
    try {
      const data = await postCommand(cmd);
      if (isP4Offline(data)) {
        lastResult = 'P4 OFFLINE — command did not reach the Show Engine.';
      } else {
        lastResult = data.replies || JSON.stringify(data);
      }
    } catch (err) {
      lastResult = err.message;
    }
    pending = null;
    await pollShow();
  }

  function btn(label, cmd, kind) {
    const emergency = emergencyWord(sys()) === 'EMERGENCY';
    const waiting = pending === cmd;
    return el('button', {
      className: kind === 'primary' ? 'btn-primary' : 'btn-cancel',
      text: waiting ? `${label}…` : label,
      disabled: !lastSnap.p4Online || emergency || !!pending,
      onClick: () => send(cmd)
    });
  }

  function paint() {
    host.innerHTML = '';
    if (!lastSnap.p4Online) host.append(p4OfflineBanner());

    const state = confirmedState();
    const emergency = emergencyWord(sys());
    const s = sys();
    const runtime = el('div', { className: emergency === 'EMERGENCY' ? 'card danger-card' : 'card' });
    runtime.append(el('h2', { text: 'Live runtime' }));
    runtime.append(el('div', { className: 'value', text: state }));
    runtime.append(el('div', { className: 'sub', text: productionLabel(s) }));
    if (lastSnap.p4Online && s) {
      const elapsed = show && show.elapsedMs != null ? show.elapsedMs : s.showElapsedMs;
      const remain = show && show.remainingMs != null ? show.remainingMs : s.showRemainingMs;
      const duration = show && show.durationMs != null ? show.durationMs : s.showDurationMs;
      const pct = duration ? Math.min(100, Math.round((elapsed || 0) * 100 / duration)) : 0;
      const bar = el('div', { className: 'progress-track' });
      const fill = el('div', { className: 'progress-fill' });
      fill.style.width = pct + '%';
      bar.append(fill);
      runtime.append(bar);
      runtime.append(statRow('P4 state', s.showState || (show && show.state) || '—'));
      runtime.append(statRow('Cue', `${(show && show.currentCue) ?? s.currentCue ?? 0} / ${(show && show.totalCues) ?? s.totalCues ?? 0}`));
      runtime.append(statRow('Elapsed', formatPlayhead(elapsed)));
      runtime.append(statRow('Remaining', formatPlayhead(remain)));
      runtime.append(statRow('Pending operator action',
        pending ? `${pending} PENDING` : (s.emergencyPendingClear ? 'EMERGENCY CLEAR PENDING' : 'None')));
    }

    if (emergency === 'EMERGENCY') {
      runtime.append(el('div', { className: 'emergency-banner', text: 'EMERGENCY ACTIVE' }));
      runtime.append(statRow('Physical loop', loopWord(s)));
      runtime.append(statRow('Clear request', s && s.emergencyPendingClear ? 'PENDING' : 'Hold the physical button for 3s'));
      runtime.append(statRow('Latch source', (s && s.emergencySource) || '—'));
      const row = el('div', { className: 'transport-row' });
      row.append(el('button', {
        className: 'btn-cancel',
        text: pending === 'EMERGENCY:CLEAR_CONFIRM' ? 'CONFIRM…' : 'Confirm clear',
        disabled: !lastSnap.p4Online || !(s && s.emergencyPendingClear) || !!pending,
        onClick: () => send('EMERGENCY:CLEAR_CONFIRM')
      }));
      row.append(el('button', {
        className: 'btn-cancel',
        text: pending === 'EMERGENCY:CLEAR_CANCEL' ? 'CANCEL…' : 'Cancel clear',
        disabled: !lastSnap.p4Online || !(s && s.emergencyPendingClear) || !!pending,
        onClick: () => send('EMERGENCY:CLEAR_CANCEL')
      }));
      runtime.append(row);
      runtime.append(el('p', { className: 'sub', text: 'Web clear is no more permissive than Director clear. The P4 owns the latch.' }));
    }

    const transport = el('div', { className: 'transport-row' });
    const paused = !!(s && s.showPaused) || !!(show && show.paused);
    transport.append(btn(paused ? 'RESUME' : 'START', paused ? 'SHOW:RESUME' : 'SHOW:START', 'primary'));
    transport.append(btn('PAUSE', 'SHOW:PAUSE'));
    transport.append(btn('STOP', 'SHOW:STOP'));
    transport.append(el('button', {
      className: 'btn-panic',
      text: 'PANIC',
      disabled: !lastSnap.p4Online,
      onClick: () => send('EMERGENCY:STOP', 'PANIC = P4 EMERGENCY STOP. Latch the Show Engine now?')
    }));
    runtime.append(transport);
    host.append(runtime);

    const result = el('div', { className: 'card' });
    result.append(el('h2', { text: 'Confirmed P4 result' }));
    result.append(el('div', { className: 'reply-box', text: lastResult }));
    host.append(result);

    if (localStorage.getItem('showduino.pref.liveCues') === '0') return;

    const cues = el('div', { className: 'card' });
    cues.append(el('h2', { text: 'Cue stack' }));
    const list = (show && show.cues) || [];
    if (!lastSnap.p4Online) {
      cues.append(el('p', { className: 'sub', text: 'Cue list withheld while P4 is offline.' }));
    } else if (!list.length) {
      cues.append(el('p', { className: 'sub', text: 'Load a production to populate the P4 timeline.' }));
    } else {
      const table = el('table', { className: 'log-table' });
      table.append(el('thead', {}, [
        el('tr', {}, [el('th', { text: '#' }), el('th', { text: 'Time' }), el('th', { text: 'Command' })])
      ]));
      const tbody = el('tbody', {});
      const current = (show && show.currentCue) || 0;
      for (const cue of list) {
        tbody.append(el('tr', { className: cue.index === current ? 'cue-current' : '' }, [
          el('td', { text: String((cue.index ?? 0) + 1) }),
          el('td', { text: formatPlayhead(cue.timeMs) }),
          el('td', { text: cue.command || '—' })
        ]));
      }
      table.append(tbody);
      cues.append(tableWrap(table));
      if (show && show.cuesTruncated) {
        cues.append(el('p', { className: 'sub', text: 'List truncated at 64 cues.' }));
      }
    }
    host.append(cues);
  }

  async function pollShow() {
    if (!lastSnap.p4Online) {
      show = null;
      paint();
      return;
    }
    try {
      const data = await fetchShow();
      show = isP4Offline(data) ? null : data;
    } catch (_) {
      show = null;
    }
    paint();
  }

  const unsub = subscribeStore((snap) => {
    lastSnap = snap;
    paint();
  });
  await pollShow();
  const timer = setInterval(pollShow, 2000);
  return () => {
    unsub();
    clearInterval(timer);
  };
}
LivePage.title = 'Live';
