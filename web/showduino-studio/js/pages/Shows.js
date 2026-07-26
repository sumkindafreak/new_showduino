import { dispatchOperatorCommand, initializeRuntimeStore, subscribeRuntime } from '../runtimeStore.js';
import { el, formatDurationMs, statRow } from '../utils.js';

async function issue(status, action, args = {}) {
  status.textContent = `Dispatching ${action}…`;
  try {
    await dispatchOperatorCommand(action, args);
    status.textContent = `${action} queued`;
  } catch (error) {
    status.textContent = error.message;
  }
}

export function ShowsPage(container) {
  initializeRuntimeStore();
  container.append(el('p', {
    className: 'info-panel',
    text: 'Timeline control backed by runtimeStore command dispatch. Uses existing firmware command endpoint only.'
  }));

  const status = el('div', { className: 'live-status', text: 'Loading timeline state…' });
  const stateCard = el('div', { className: 'card' });
  stateCard.append(el('h2', { text: 'Timeline Runtime' }));
  const stateBody = el('div');
  stateCard.append(stateBody);

  const controls = el('div', { className: 'card command-form' });
  controls.append(el('h2', { text: 'Playback Controls' }));
  controls.append(el('button', { className: 'btn-primary', text: 'Play', onClick: () => issue(status, 'timeline.play') }));
  controls.append(el('button', { className: 'btn-primary', text: 'Pause', onClick: () => issue(status, 'timeline.pause') }));
  controls.append(el('button', { className: 'btn-primary', text: 'Stop', onClick: () => issue(status, 'timeline.stop') }));
  controls.append(el('button', { className: 'btn-primary', text: 'Skip Next', onClick: () => issue(status, 'timeline.next') }));
  controls.append(el('button', { className: 'btn-primary', text: 'Previous', onClick: () => issue(status, 'timeline.previous') }));

  const jumpInput = el('input', { value: '', placeholder: 'Cue index or ID' });
  controls.append(el('label', { className: 'cmd-field' }, ['Jump To Cue ', jumpInput]));
  controls.append(el('button', {
    className: 'btn-primary',
    text: 'Jump',
    onClick: () => {
      const raw = jumpInput.value.trim();
      if (!raw) return;
      const cue = /^\d+$/.test(raw) ? Number(raw) : raw;
      issue(status, 'timeline.jump', { cue });
    }
  }));

  container.append(status, stateCard, controls);

  const unsub = subscribeRuntime((snap) => {
    const t = snap.modules.timeline;
    const sys = snap.system || {};
    status.textContent = snap.connection.connected
      ? `Live · playback ${t.playbackState || 'unknown'}`
      : `Disconnected · reconnect in ${snap.connection.reconnectInSec || 0}s`;

    stateBody.innerHTML = '';
    stateBody.append(statRow('Playback State', t.playbackState || sys.showState || 'unreported'));
    stateBody.append(statRow('Current Cue', t.currentCue ?? 'unreported'));
    stateBody.append(statRow('Next Cue', t.nextCue ?? 'unreported'));
    stateBody.append(statRow('Current Show', t.currentShow || 'unreported'));
    stateBody.append(statRow('Elapsed', formatDurationMs(t.elapsedMs)));
    stateBody.append(statRow('Remaining', formatDurationMs(t.remainingMs)));
    stateBody.append(statRow('Emergency State', sys.emergencyActive ? 'ACTIVE' : 'clear'));
    stateBody.append(statRow('Last Operator Action', t.lastAction || 'none'));
  });

  return () => unsub();
}
ShowsPage.title = 'Timeline';
