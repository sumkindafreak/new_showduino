import { fetchNetwork, fetchSystem } from '../api.js';
import { el, statRow } from '../utils.js';
import { connectLive, subscribeLive } from '../live.js';

export async function NetworkPage(container) {
  container.append(el('p', {
    className: 'info-panel',
    text: 'Network health for the Showduino mesh. Topology graphics will attach here in a later stage.'
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
  topo.append(el('p', { className: 'sub', text: 'Reserved for future live link graph (Director ↔ SUE ↔ IAN ↔ Relays).' }));

  container.append(healthCard, wifiCard, topo);

  function renderNetwork(net) {
    if (!net) return;
    healthBody.innerHTML = '';
    healthBody.append(statRow('Total Devices', net.deviceCount));
    healthBody.append(statRow('Online', net.onlineCount));
    healthBody.append(statRow('Warning', net.warningCount));
    healthBody.append(statRow('Offline', net.offlineCount));
    healthBody.append(statRow('Average Signal', net.averageRssi != null ? `${net.averageRssi} dBm` : '—'));
    healthBody.append(statRow('Heartbeat Rate', net.heartbeatRate != null ? `${net.heartbeatRate} / min` : '—'));
    healthBody.append(statRow('Network Health', net.networkHealth || net.health || '—'));
  }

  connectLive();
  const unsub = subscribeLive((snap) => renderNetwork(snap.network));

  try {
    const net = await fetchNetwork();
    renderNetwork(net);
  } catch (err) {
    healthBody.append(el('p', { text: err.message }));
  }

  try {
    const sys = await fetchSystem();
    const w = sys.wifi || {};
    wifiBody.innerHTML = '';
    wifiBody.append(statRow('Mode', w.mode));
    wifiBody.append(statRow('SSID', w.ssid));
    wifiBody.append(statRow('IP Address', w.ip));
    wifiBody.append(statRow('Hostname', w.hostname));
    wifiBody.append(statRow('mDNS', (sys.mdnsHost || 'showduino-studio') + '.local'));
    wifiBody.append(statRow('WebSocket', `ws://${location.hostname || '192.168.4.1'}:81/`));
  } catch (err) {
    wifiBody.append(el('p', { text: 'Show Engine system API unavailable — front door still serves device live feed.' }));
  }

  return () => unsub();
}
NetworkPage.title = 'Network';