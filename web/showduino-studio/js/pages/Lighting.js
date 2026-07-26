import { dispatchOperatorCommand, initializeRuntimeStore, subscribeRuntime } from '../runtimeStore.js';
import { el, statRow } from '../utils.js';

async function invoke(status, action, payload = {}) {
  status.textContent = `Dispatching ${action}…`;
  try {
    await dispatchOperatorCommand(action, payload);
    status.textContent = `${action} queued`;
  } catch (error) {
    status.textContent = error.message;
  }
}

export function LightingPage(container) {
  initializeRuntimeStore();
  container.append(el('p', {
    className: 'info-panel',
    text: 'Lighting operator controls using runtimeStore command dispatcher.'
  }));

  const status = el('div', { className: 'live-status', text: 'Loading lighting state…' });
  const stateCard = el('div', { className: 'card' });
  stateCard.append(el('h2', { text: 'Lighting Runtime' }));
  const stateBody = el('div');
  stateCard.append(stateBody);

  const controls = el('div', { className: 'card command-form' });
  controls.append(el('h2', { text: 'Lighting Controls' }));
  const sceneInput = el('input', { value: '', placeholder: 'scene name' });
  const brightness = el('input', { type: 'range', min: '0', max: '255', value: '128' });

  controls.append(el('label', { className: 'cmd-field' }, ['Scene ', sceneInput]));
  controls.append(el('button', {
    className: 'btn-primary',
    text: 'Trigger Scene',
    onClick: () => {
      const scene = sceneInput.value.trim();
      if (!scene) return;
      invoke(status, 'lighting.scene', { scene });
    }
  }));
  controls.append(el('button', {
    className: 'btn-primary',
    text: 'Blackout',
    onClick: () => invoke(status, 'lighting.blackout', { blackout: true })
  }));
  controls.append(el('button', {
    className: 'btn-primary',
    text: 'Release Blackout',
    onClick: () => invoke(status, 'lighting.releaseBlackout', { blackout: false })
  }));
  controls.append(el('label', { className: 'cmd-field' }, ['Master Brightness ', brightness]));
  controls.append(el('button', {
    className: 'btn-primary',
    text: 'Apply Brightness',
    onClick: () => invoke(status, 'lighting.brightness', { brightness: Number(brightness.value) })
  }));

  container.append(status, stateCard, controls);

  const unsub = subscribeRuntime((snap) => {
    const lighting = snap.modules.lighting;
    status.textContent = snap.connection.connected
      ? `Live · ${lighting.blackout ? 'blackout' : 'normal output'}`
      : `Disconnected · reconnect in ${snap.connection.reconnectInSec || 0}s`;
    if (lighting.brightness != null && String(lighting.brightness) !== brightness.value) {
      brightness.value = String(lighting.brightness);
    }

    stateBody.innerHTML = '';
    stateBody.append(statRow('Active Scene', lighting.activeScene || 'unreported'));
    stateBody.append(statRow('Brightness', lighting.brightness != null ? String(lighting.brightness) : 'unreported'));
    stateBody.append(statRow('Blackout', lighting.blackout == null ? 'unreported' : (lighting.blackout ? 'active' : 'released')));
    stateBody.append(statRow('Running Effects', Array.isArray(lighting.runningEffects) && lighting.runningEffects.length
      ? lighting.runningEffects.join(', ')
      : 'unreported'));
    stateBody.append(statRow('Last Operator Action', lighting.lastAction || 'none'));
  });

  return () => unsub();
}
LightingPage.title = 'Lighting';
