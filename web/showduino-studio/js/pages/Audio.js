import { dispatchOperatorCommand, initializeRuntimeStore, subscribeRuntime } from '../runtimeStore.js';
import { el, formatDurationMs, statRow } from '../utils.js';

async function invoke(status, action, payload = {}) {
  status.textContent = `Dispatching ${action}…`;
  try {
    await dispatchOperatorCommand(action, payload);
    status.textContent = `${action} queued`;
  } catch (error) {
    status.textContent = error.message;
  }
}

export function AudioPage(container) {
  initializeRuntimeStore();
  container.append(el('p', {
    className: 'info-panel',
    text: 'Audio transport + gain controls routed through runtimeStore command dispatch.'
  }));

  const status = el('div', { className: 'live-status', text: 'Loading audio state…' });
  const stateCard = el('div', { className: 'card' });
  stateCard.append(el('h2', { text: 'Audio Runtime' }));
  const body = el('div');
  stateCard.append(body);

  const controls = el('div', { className: 'card command-form' });
  controls.append(el('h2', { text: 'Deck Controls' }));
  controls.append(el('button', { className: 'btn-primary', text: 'Play', onClick: () => invoke(status, 'audio.play') }));
  controls.append(el('button', { className: 'btn-primary', text: 'Pause', onClick: () => invoke(status, 'audio.pause') }));
  controls.append(el('button', { className: 'btn-primary', text: 'Stop', onClick: () => invoke(status, 'audio.stop') }));
  controls.append(el('button', { className: 'btn-primary', text: 'Next', onClick: () => invoke(status, 'audio.next') }));
  controls.append(el('button', { className: 'btn-primary', text: 'Previous', onClick: () => invoke(status, 'audio.previous') }));

  const volumeSlider = el('input', { type: 'range', min: '0', max: '255', value: '128' });
  const muteToggle = el('button', { className: 'btn-primary', text: 'Mute / Unmute' });
  let currentMute = null;
  controls.append(el('label', { className: 'cmd-field' }, ['Volume ', volumeSlider]));
  controls.append(muteToggle);

  volumeSlider.addEventListener('change', () => {
    const volume = Number(volumeSlider.value);
    invoke(status, 'audio.volume', { volume });
  });
  muteToggle.addEventListener('click', () => {
    invoke(status, 'audio.mute', { mute: !currentMute });
  });

  container.append(status, stateCard, controls);

  const unsub = subscribeRuntime((snap) => {
    const audio = snap.modules.audio;
    status.textContent = snap.connection.connected
      ? `Live · ${audio.playbackStatus || 'unknown'}`
      : `Disconnected · reconnect in ${snap.connection.reconnectInSec || 0}s`;
    currentMute = audio.mute;
    muteToggle.textContent = audio.mute ? 'Unmute' : 'Mute';
    if (audio.volume != null && String(audio.volume) !== volumeSlider.value) {
      volumeSlider.value = String(audio.volume);
    }
    body.innerHTML = '';
    body.append(statRow('Track', audio.track || 'unreported'));
    body.append(statRow('Duration', formatDurationMs(audio.durationMs)));
    body.append(statRow('Elapsed', formatDurationMs(audio.elapsedMs)));
    body.append(statRow('Volume', audio.volume != null ? String(audio.volume) : 'unreported'));
    body.append(statRow('Mute', audio.mute == null ? 'unreported' : (audio.mute ? 'on' : 'off')));
    body.append(statRow('Playback', audio.playbackStatus || 'unreported'));
    body.append(statRow('Last Operator Action', audio.lastAction || 'none'));
  });

  return () => unsub();
}
AudioPage.title = 'Audio';
