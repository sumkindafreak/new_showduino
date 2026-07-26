import { el, formatBytes, formatDurationMs, formatUptime, statRow } from '../utils.js';
import { MemoryBar } from '../components/MemoryBar.js';
import { initializeRuntimeStore, refreshRuntime, subscribeRuntime } from '../runtimeStore.js';

export function HomePage(container) {
  initializeRuntimeStore();
  container.append(el('p', {
    className: 'info-panel',
    text: 'Runtime dashboard from firmware state + runtimeStore. Operator state stays available while reconnecting.'
  }));

  const connection = el('div', { className: 'live-status', text: 'Connecting runtime store…' });
  const grid = el('div', { className: 'page-grid' });
  container.append(connection, grid);

  const overview = el('div', { className: 'card' });
  overview.append(el('h2', { text: 'Runtime State' }));
  const runtimeValue = el('div', { className: 'value', text: '—' });
  const runtimeSub = el('div', { className: 'sub', text: 'Waiting for firmware…' });
  overview.append(runtimeValue, runtimeSub);

  const showCard = el('div', { className: 'card' });
  showCard.append(el('h2', { text: 'Show' }));
  const showBody = el('div');
  showCard.append(showBody);

  const uptimeCard = el('div', { className: 'card' });
  uptimeCard.append(el('h2', { text: 'System' }));
  const uptimeBody = el('div');
  uptimeCard.append(uptimeBody);

  const mem = el('div', { className: 'card' });
  mem.append(el('h2', { text: 'Memory' }));
  const memBody = el('div');
  mem.append(memBody);

  const networkCard = el('div', { className: 'card' });
  networkCard.append(el('h2', { text: 'Network Health' }));
  const networkBody = el('div');
  networkCard.append(networkBody);

  const storage = el('div', { className: 'card' });
  storage.append(el('h2', { text: 'Storage' }));
  const storageBody = el('div');
  storage.append(storageBody);

  grid.append(overview, showCard, uptimeCard, mem, networkCard, storage);

  const refreshBtn = el('button', {
    className: 'btn-primary',
    text: 'Refresh Runtime',
    onClick: async () => {
      connection.textContent = 'Refreshing runtime…';
      try {
        await refreshRuntime('all');
      } catch (_) {
        // Runtime store already tracks the error; UI status updates via subscription.
      }
    }
  });
  container.prepend(refreshBtn);

  const unsub = subscribeRuntime((snap) => {
    const sys = snap.system || {};
    const net = snap.network || {};
    const timeline = snap.modules.timeline;
    const time = snap.time || {};

    connection.textContent = snap.connection.connected
      ? `Connected · ws live${snap.connection.stale ? ' · stale data warning' : ''}`
      : `Disconnected · reconnect in ${snap.connection.reconnectInSec || 0}s${snap.connection.lastError ? ` · ${snap.connection.lastError}` : ''}`;

    runtimeValue.textContent = (sys.showState || timeline.playbackState || 'unknown').toUpperCase();
    runtimeSub.textContent = `${sys.boardName || 'Show Engine'} · firmware ${sys.firmwareVersion || 'unknown'}${sys.emergencyActive ? ' · EMERGENCY ACTIVE' : ''}`;

    showBody.innerHTML = '';
    showBody.append(statRow('Current Show', timeline.currentShow || 'unreported'));
    showBody.append(statRow('Active Cue', timeline.currentCue ?? 'unreported'));
    showBody.append(statRow('Next Cue', timeline.nextCue ?? 'unreported'));
    showBody.append(statRow('Playback', timeline.playbackState || 'unknown'));
    showBody.append(statRow('Current Time', time.time || 'unreported'));
    showBody.append(statRow('Elapsed', formatDurationMs(timeline.elapsedMs)));
    showBody.append(statRow('Remaining', formatDurationMs(timeline.remainingMs)));

    uptimeBody.innerHTML = '';
    uptimeBody.append(statRow('Uptime', formatUptime(sys.uptime)));
    uptimeBody.append(statRow('Connected Nodes', net.onlineCount ?? (snap.devices || []).filter((d) => d.online).length));
    uptimeBody.append(statRow('Emergency', sys.emergencyActive ? 'ACTIVE' : 'clear'));
    uptimeBody.append(statRow('CPU MHz', sys.cpuMhz != null ? `${sys.cpuMhz}` : 'unreported'));
    uptimeBody.append(statRow('CPU Load', sys.cpuLoad != null ? `${sys.cpuLoad}%` : 'not provided by firmware'));

    memBody.innerHTML = '';
    if (sys.heapTotal != null && sys.heapFree != null) {
      memBody.append(MemoryBar({ label: 'Heap', used: sys.heapTotal - sys.heapFree, total: sys.heapTotal, variant: 'heap' }));
      memBody.append(statRow('Heap Free', formatBytes(sys.heapFree)));
    } else {
      memBody.append(statRow('Heap', 'unreported'));
    }
    if (sys.psramTotal != null && sys.psramFree != null) {
      memBody.append(MemoryBar({ label: 'PSRAM', used: sys.psramTotal - sys.psramFree, total: sys.psramTotal, variant: 'psram' }));
      memBody.append(statRow('PSRAM Free', formatBytes(sys.psramFree)));
    } else {
      memBody.append(statRow('PSRAM', 'unreported'));
    }

    networkBody.innerHTML = '';
    networkBody.append(statRow('Health', net.networkHealth || net.health || 'unreported'));
    networkBody.append(statRow('Signal (avg)', net.averageRssi != null ? `${net.averageRssi} dBm` : 'unreported'));
    networkBody.append(statRow('Heartbeat', net.heartbeatRate != null ? `${net.heartbeatRate}/min` : 'unreported'));
    networkBody.append(statRow('Offline Nodes', net.offlineCount ?? 'unreported'));

    storageBody.innerHTML = '';
    storageBody.append(statRow('SD Ready', sys.storageReady ? 'Yes' : 'No'));
    storageBody.append(statRow('Writable', sys.storageWritable ? 'Yes' : 'No'));
    storageBody.append(statRow('WebUI on SD', sys.storageHasWww ? 'Yes' : 'No'));
    storageBody.append(statRow('Card', sys.storageCardType || '—'));
    storageBody.append(statRow('Storage', sys.storageTotalMb != null ? `${sys.storageFreeMb || 0}/${sys.storageTotalMb} MB` : 'unreported'));
    storageBody.append(statRow('Status', sys.storageMessage || '—'));
  });

  return () => unsub();
}
HomePage.title = 'Dashboard';
