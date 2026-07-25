/**
 * Showduino Studio – Network
 *
 * Network health and mesh topology view.
 * Driven entirely by runtimeStore — no independent REST calls.
 * The store polls /api/network every 4 s and /api/system every 6 s.
 */

import { el, statRow, makeCleanupGroup } from '../utils.js';
import { subscribeRuntime } from '../state/runtimeStore.js';

export function NetworkPage(container) {
  const cleanup = makeCleanupGroup();

  const healthCard = el('div', { className: 'card network-health-card' });
  healthCard.append(el('h2', { text: 'Connection Health' }));
  const healthBody = el('div', {});
  healthCard.append(healthBody);

  const wifiCard = el('div', { className: 'card' });
  wifiCard.append(el('h2', { text: 'Studio Wi-Fi (C3 Front Door)' }));
  const wifiBody = el('div', {});
  wifiCard.append(wifiBody);

  const topo = el('div', { className: 'card' });
  topo.append(el('h2', { text: 'Topology' }));
  topo.append(el('p', { className: 'text-muted', text: 'Reserved for live link graph: Director ↔ SUE ↔ IAN ↔ Relay nodes.' }));
  topo.append(el('p', { className: 'sub', text: 'Phase 3 — ESP-NOW topology visualisation.' }));

  container.append(el('div', { className: 'page-grid' }, [healthCard, wifiCard, topo]));

  function paint(state) {
    const net = state.networkState;
    healthBody.innerHTML = '';
    if (net.health && net.health !== 'unknown') {
      healthBody.append(el('div', { className: 'value', text: net.health }));
    }
    if (net.deviceCount  != null) healthBody.append(statRow('Total Devices', net.deviceCount));
    if (net.onlineCount  != null) healthBody.append(statRow('Online',  net.onlineCount));
    if (net.warningCount != null && net.warningCount > 0) healthBody.append(statRow('Warning', net.warningCount));
    if (net.offlineCount != null) healthBody.append(statRow('Offline', net.offlineCount));
    if (net.averageRssi  != null) healthBody.append(statRow('Avg Signal', `${net.averageRssi} dBm`));
    if (net.heartbeatRate != null) healthBody.append(statRow('Heartbeat', `${net.heartbeatRate}/min`));
    if (state.connectionStatus.latencyMs != null) healthBody.append(statRow('Latency', `${state.connectionStatus.latencyMs} ms`));

    const cfg  = state.configurationState;
    const wifi = cfg.wifi || {};
    wifiBody.innerHTML = '';
    wifiBody.append(statRow('Mode',     wifi.mode     || '—'));
    wifiBody.append(statRow('SSID',     wifi.ssid     || '—'));
    wifiBody.append(statRow('IP',       wifi.ip       || '—'));
    wifiBody.append(statRow('Hostname', wifi.hostname || '—'));
    wifiBody.append(statRow('mDNS',     cfg.mdnsHost ? cfg.mdnsHost + '.local' : '—'));
    wifiBody.append(statRow('WebSocket', `ws://${location.hostname || '192.168.4.1'}:81/`));
  }

  cleanup.add(subscribeRuntime(paint));

  return () => cleanup.run();
}

NetworkPage.title = 'Network';
