import { el, statRow } from '../utils.js';
import { initializeRuntimeStore, subscribeRuntime } from '../runtimeStore.js';

export function NetworkPage(container) {
  initializeRuntimeStore();
  container.append(el('p', {
    className: 'info-panel',
    text: 'Network health from runtimeStore (single backend feed, no page-level polling).'
  }));

  const healthCard = el('div', { className: 'card network-health-card' });
  healthCard.append(el('h2', { text: 'Connection Health' }));
  const healthBody = el('div', { className: 'network-body' });
  healthCard.append(healthBody);

  const wifiCard = el('div', { className: 'card' });
  wifiCard.append(el('h2', { text: 'Studio Wi-Fi Front Door' }));
  const wifiBody = el('div', { className: 'wifi-body' });
  wifiCard.append(wifiBody);

  const topo = el('div', { className: 'card topology-placeholder' });
  topo.append(el('h2', { text: 'Topology' }));
  topo.append(el('p', { className: 'sub', text: 'Topology endpoint not yet exposed by firmware.' }));

  container.append(healthCard, wifiCard, topo);

  function renderNetwork(net) {
    healthBody.innerHTML = '';
    if (!net) {
      healthBody.append(statRow('Network Health', 'unreported'));
      return;
    }
    healthBody.append(statRow('Total Devices', net.deviceCount ?? '—'));
    healthBody.append(statRow('Online', net.onlineCount ?? '—'));
    healthBody.append(statRow('Warning', net.warningCount ?? '—'));
    healthBody.append(statRow('Offline', net.offlineCount ?? '—'));
    healthBody.append(statRow('Average Signal', net.averageRssi != null ? `${net.averageRssi} dBm` : '—'));
    healthBody.append(statRow('Heartbeat Rate', net.heartbeatRate != null ? `${net.heartbeatRate} / min` : '—'));
    healthBody.append(statRow('Network Health', net.networkHealth || net.health || '—'));
  }

  const unsub = subscribeRuntime((snap) => {
    renderNetwork(snap.network);
    const sys = snap.system || {};
    const w = sys.wifi || {};
    wifiBody.innerHTML = '';
    wifiBody.append(statRow('Mode', w.mode || '—'));
    wifiBody.append(statRow('SSID', w.ssid || '—'));
    wifiBody.append(statRow('IP Address', w.ip || '—'));
    wifiBody.append(statRow('Hostname', w.hostname || '—'));
    wifiBody.append(statRow('mDNS', (sys.mdnsHost || 'showduino-studio') + '.local'));
    wifiBody.append(statRow('WebSocket', `ws://${location.hostname || '192.168.4.1'}:81/`));
    wifiBody.append(statRow('Connection', snap.connection.connected ? 'connected' : `reconnect ${snap.connection.reconnectInSec || 0}s`));
  });

  return () => unsub();
}
NetworkPage.title = 'Network';