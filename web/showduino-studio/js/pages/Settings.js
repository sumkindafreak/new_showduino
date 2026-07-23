import { fetchSystem } from '../api.js';
import { el, statRow, formatBytes, formatUptime } from '../utils.js';

export async function SettingsPage(container) {
  container.append(el('p', { className: 'info-panel', text: 'Director system configuration and firmware identity.' }));

  const card = el('div', { className: 'card' });
  card.append(el('h2', { text: 'System Info' }));
  container.append(card);

  try {
    const sys = await fetchSystem();
    card.append(statRow('Board', sys.boardName));
    card.append(statRow('Firmware', sys.firmwareVersion));
    card.append(statRow('Protocol', sys.protocolVersion));
    card.append(statRow('Hostname', sys.wifi?.hostname));
    card.append(statRow('AP SSID', sys.apSsid));
    card.append(statRow('mDNS', sys.mdnsHost + '.local'));
    card.append(statRow('Uptime', formatUptime(sys.uptime)));
    card.append(statRow('CPU', sys.cpuMhz + ' MHz'));
    card.append(statRow('Heap Free', formatBytes(sys.heapFree)));
    card.append(statRow('PSRAM Free', formatBytes(sys.psramFree)));
    card.append(statRow('Storage', sys.storageReady ? 'Ready' : 'Recovery'));
    card.append(statRow('WebUI Root', sys.webuiPath));
  } catch (err) {
    card.append(el('p', { text: err.message }));
  }
}
SettingsPage.title = 'Settings';
