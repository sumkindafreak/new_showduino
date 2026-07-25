/**
 * Showduino Studio – Network
 *
 * Network configuration and mesh health view.
 * Uses shared runtime state for live network updates.
 */

import { el, statRow, makeCleanupGroup } from '../utils.js';
import { subscribe, getState, applyNetworkData, applySystemData } from '../state/runtime.js';
import { fetchNetwork, fetchSystem } from '../api.js';

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
  topo.append(el('p', {
    className: 'text-muted',
    text: 'Reserved for live link graph: Director ↔ SUE ↔ IAN ↔ Relay nodes.',
  }));
  topo.append(el('p', { className: 'sub', text: 'Phase 3 — ESP-NOW topology visualisation.' }));

  container.append(el('div', { className: 'page-grid' }, [healthCard, wifiCard, topo]));

  function paintNetwork(net) {
    if (!net) return;
    healthBody.innerHTML = '';
    if (net.networkHealth) {
      healthBody.append(el('div', { className: 'value', text: net.networkHealth }));
    }
    if (net.deviceCount != null) healthBody.append(statRow('Total Devices', net.deviceCount));
    if (net.onlineCount != null) healthBody.append(statRow('Online', net.onlineCount));
    if (net.warningCount != null && net.warningCount > 0) healthBody.append(statRow('Warning', net.warningCount));
    if (net.offlineCount != null) healthBody.append(statRow('Offline', net.offlineCount));
    if (net.averageRssi != null) healthBody.append(statRow('Avg Signal', `${net.averageRssi} dBm`));
    if (net.heartbeatRate != null) healthBody.append(statRow('Heartbeat', `${net.heartbeatRate}/min`));
    if (net.latencyMs != null) healthBody.append(statRow('Latency', `${net.latencyMs} ms`));
  }

  cleanup.add(subscribe((state) => paintNetwork(state.network)));

  // Seed from REST
  Promise.all([fetchNetwork().catch(() => null), fetchSystem().catch(() => null)]).then(([net, sys]) => {
    if (net) applyNetworkData(net);

    if (sys) {
      applySystemData(sys);
      const w = sys.wifi || {};
      wifiBody.innerHTML = '';
      wifiBody.append(statRow('Mode', w.mode));
      wifiBody.append(statRow('SSID', w.ssid));
      wifiBody.append(statRow('IP Address', w.ip));
      wifiBody.append(statRow('Hostname', w.hostname));
      wifiBody.append(statRow('mDNS', (sys.mdnsHost || 'showduino-studio') + '.local'));
      wifiBody.append(statRow('WebSocket', `ws://${location.hostname || '192.168.4.1'}:81/`));
    } else {
      wifiBody.append(el('p', { className: 'text-muted', text: 'System API unavailable — C3 bridge still serves live feed.' }));
    }
  });

  return () => cleanup.run();
}

NetworkPage.title = 'Network';
