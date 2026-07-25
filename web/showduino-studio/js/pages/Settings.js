/**
 * Showduino Studio – Settings
 *
 * System information and firmware identity.
 * Reads entirely from runtimeStore — no independent REST calls.
 * The store polls /api/system every 6 s.
 */

import { el, statRow, formatBytes, formatUptime, makeCleanupGroup } from '../utils.js';
import { subscribeRuntime } from '../state/runtimeStore.js';

export function SettingsPage(container) {
  const cleanup = makeCleanupGroup();

  const sysCard = el('div', { className: 'card' });
  sysCard.append(el('h2', { text: 'System Info' }));
  const sysBody = el('div', {});
  sysCard.append(sysBody);

  const storageCard = el('div', { className: 'card' });
  storageCard.append(el('h2', { text: 'Storage' }));
  const storageBody = el('div', {});
  storageCard.append(storageBody);

  const wifiCard = el('div', { className: 'card' });
  wifiCard.append(el('h2', { text: 'Network Identity' }));
  const wifiBody = el('div', {});
  wifiCard.append(wifiBody);

  container.append(el('div', { className: 'page-grid' }, [sysCard, storageCard, wifiCard]));

  function paint(state) {
    const sys    = state.systemState;
    const assets = state.assetCollection;
    const cfg    = state.configurationState;
    const wifi   = cfg.wifi || {};

    sysBody.innerHTML = '';
    sysBody.append(statRow('Board', sys.boardName));
    sysBody.append(statRow('Firmware', sys.firmwareVersion));
    sysBody.append(statRow('Protocol', sys.protocolVersion));
    sysBody.append(statRow('Role', sys.role || '—'));
    sysBody.append(statRow('Uptime', formatUptime(sys.uptime)));
    if (sys.cpuMhz) sysBody.append(statRow('CPU', `${sys.cpuMhz} MHz`));
    if (sys.heapFree != null) sysBody.append(statRow('Heap Free', formatBytes(sys.heapFree)));
    if (sys.psramFree != null) sysBody.append(statRow('PSRAM Free', formatBytes(sys.psramFree)));

    storageBody.innerHTML = '';
    storageBody.append(statRow('SD Ready',  sys.storageReady    ? 'Yes' : 'No'));
    storageBody.append(statRow('Writable',  sys.storageWritable ? 'Yes' : 'No'));
    storageBody.append(statRow('WebUI on SD', sys.storageHasWww ? 'Yes' : 'No'));
    if (sys.storageCardType) storageBody.append(statRow('Card', sys.storageCardType));
    if (sys.storageTotalMb != null) {
      storageBody.append(statRow('Free / Total', `${sys.storageFreeMb || 0} / ${sys.storageTotalMb} MB`));
    }
    storageBody.append(statRow('Shows Path', assets.showsPath || '—'));
    storageBody.append(statRow('WebUI Path', assets.webuiPath || '—'));
    storageBody.append(statRow('Status', sys.storageMessage || assets.status || '—'));

    wifiBody.innerHTML = '';
    wifiBody.append(statRow('Mode', wifi.mode || '—'));
    wifiBody.append(statRow('SSID', wifi.ssid || '—'));
    wifiBody.append(statRow('IP Address', wifi.ip || '—'));
    wifiBody.append(statRow('Hostname', wifi.hostname || '—'));
    wifiBody.append(statRow('AP SSID', cfg.apSsid || '—'));
    wifiBody.append(statRow('mDNS', cfg.mdnsHost ? cfg.mdnsHost + '.local' : '—'));
    wifiBody.append(statRow('WebSocket', `ws://${location.hostname || '192.168.4.1'}:81/`));
    wifiBody.append(statRow('Connection Status', state.connectionStatus.lifecycle));
  }

  cleanup.add(subscribeRuntime(paint));

  return () => cleanup.run();
}

SettingsPage.title = 'Settings';
