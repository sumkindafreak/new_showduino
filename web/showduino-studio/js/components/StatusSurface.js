import { subscribeRuntime } from '../state/runtimeStore.js';

function toneForLifecycle(lifecycle) {
  if (lifecycle === 'connected') return 'ok';
  if (lifecycle === 'degraded') return 'warn';
  if (lifecycle === 'reconnecting' || lifecycle === 'connecting') return 'warn';
  return 'error';
}

function toneForEmergency(active) {
  return active ? 'error' : 'ok';
}

export function bindStatusSurface() {
  return subscribeRuntime((state) => {
    const lifecycle = state.connectionStatus.lifecycle;
    const runtime = state.runtimeStatus.runtime || 'unknown';
    const emergency = state.emergencyState.active ? 'ACTIVE' : 'clear';
    const notice = state.notifications[0]?.message || state.connectionStatus.reason || 'Ready';
    const nodes = state.nodeCollection.counts.total;
    const show = state.showCollection.currentShow || state.runtimeStatus.currentShow || 'Not reported';
    const clock = state.clock.label || '--:--:--';

    const connectionEl = document.getElementById('status-connection');
    if (connectionEl) {
      connectionEl.textContent = lifecycle;
      connectionEl.className = `status-value tone-${toneForLifecycle(lifecycle)}`;
    }
    const runtimeEl = document.getElementById('status-runtime');
    if (runtimeEl) runtimeEl.textContent = runtime;

    const emergencyEl = document.getElementById('status-emergency');
    if (emergencyEl) {
      emergencyEl.textContent = emergency;
      emergencyEl.className = `status-value tone-${toneForEmergency(state.emergencyState.active)}`;
    }

    const nodesEl = document.getElementById('status-nodes');
    if (nodesEl) nodesEl.textContent = String(nodes);

    const showEl = document.getElementById('status-show');
    if (showEl) showEl.textContent = show;

    const noticeEl = document.getElementById('status-notice');
    if (noticeEl) noticeEl.textContent = notice;

    const clockEl = document.getElementById('status-clock');
    if (clockEl) clockEl.textContent = clock;
  });
}
