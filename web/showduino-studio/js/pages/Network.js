import { subscribeRuntime } from '../state/runtimeStore.js';
import { infoBanner, keyValueTable } from './pagePrimitives.js';

export function NetworkPage(container) {
  container.append(infoBanner('Network configuration and live fabric health from the shared runtime model.'));

  const healthCard = document.createElement('section');
  healthCard.className = 'card';
  healthCard.innerHTML = '<h2>Fabric Health</h2>';
  const healthHost = document.createElement('div');
  healthCard.append(healthHost);

  const wifiCard = document.createElement('section');
  wifiCard.className = 'card';
  wifiCard.innerHTML = '<h2>Wi-Fi Front Door</h2>';
  const wifiHost = document.createElement('div');
  wifiCard.append(wifiHost);

  const layout = document.createElement('div');
  layout.className = 'page-grid';
  layout.append(healthCard, wifiCard);
  container.append(layout);

  const unsub = subscribeRuntime((state) => {
    healthHost.innerHTML = '';
    healthHost.append(keyValueTable([
      ['Device Count', state.networkState.deviceCount],
      ['Online', state.networkState.onlineCount],
      ['Warning', state.networkState.warningCount],
      ['Offline', state.networkState.offlineCount],
      ['Average RSSI', state.networkState.averageRssi != null ? `${state.networkState.averageRssi} dBm` : 'Not reported'],
      ['Heartbeat Rate', state.networkState.heartbeatRate != null ? `${state.networkState.heartbeatRate} / min` : 'Not reported'],
      ['Network Health', state.networkState.health]
    ]));

    const wifi = state.configurationState.wifi || {};
    wifiHost.innerHTML = '';
    wifiHost.append(keyValueTable([
      ['Mode', wifi.mode || 'Not reported'],
      ['SSID', wifi.ssid || state.configurationState.apSsid],
      ['IP', wifi.ip || '192.168.4.1'],
      ['Hostname', wifi.hostname || state.configurationState.mdnsHost],
      ['mDNS', `${state.configurationState.mdnsHost}.local`],
      ['WebSocket', `ws://${location.hostname || '192.168.4.1'}:81/`]
    ]));
  });

  return () => unsub();
}
NetworkPage.title = 'Network';
NetworkPage.subtitle = 'Transport health and front-door configuration.';