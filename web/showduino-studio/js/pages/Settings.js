import { subscribeRuntime } from '../state/runtimeStore.js';
import { formatDurationMs } from '../utils.js';
import { infoBanner, keyValueTable } from './pagePrimitives.js';

export function SettingsPage(container) {
  container.append(infoBanner('System identity, connection ownership and runtime diagnostics for operators.'));
  const card = document.createElement('section');
  card.className = 'card';
  card.innerHTML = '<h2>System Identity</h2>';
  const host = document.createElement('div');
  card.append(host);
  container.append(card);

  const unsub = subscribeRuntime((state) => {
    host.innerHTML = '';
    host.append(keyValueTable([
      ['Board', state.systemState.boardName],
      ['Firmware', state.systemState.firmwareVersion],
      ['Protocol', state.systemState.protocolVersion],
      ['Runtime', state.runtimeStatus.runtime],
      ['Emergency', state.emergencyState.active ? 'ACTIVE' : 'clear'],
      ['Connection', state.connectionStatus.lifecycle],
      ['Uptime', formatDurationMs(state.systemState.uptime)],
      ['mDNS', `${state.configurationState.mdnsHost}.local`],
      ['AP SSID', state.configurationState.apSsid],
      ['WebUI Host', state.configurationState.webuiHost]
    ]));
  });

  return () => unsub();
}
SettingsPage.title = 'Settings';
SettingsPage.subtitle = 'System identity and runtime ownership.';
