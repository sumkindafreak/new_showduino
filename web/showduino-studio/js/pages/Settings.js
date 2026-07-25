/**
 * Showduino Studio – Settings
 *
 * Director system configuration and firmware identity.
 * Uses shared runtime state for system info.
 */

import { el, statRow, formatBytes, formatUptime, makeCleanupGroup } from '../utils.js';
import { subscribe, getState, applySystemData } from '../state/runtime.js';
import { fetchSystem } from '../api.js';

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

  container.append(el('div', { className: 'page-grid' }, [sysCard, storageCard]));

  function paintSystem(sys) {
    if (!sys || !sys.boardName) return;
    sysBody.innerHTML = '';
    sysBody.append(statRow('Board', sys.boardName));
    sysBody.append(statRow('Firmware', sys.firmwareVersion));
    sysBody.append(statRow('Protocol', sys.protocolVersion));
    sysBody.append(statRow('Hostname', sys.wifi?.hostname));
    sysBody.append(statRow('AP SSID', sys.apSsid));
    sysBody.append(statRow('mDNS', sys.mdnsHost ? sys.mdnsHost + '.local' : '—'));
    sysBody.append(statRow('Uptime', formatUptime(sys.uptime)));
    sysBody.append(statRow('CPU', sys.cpuMhz ? sys.cpuMhz + ' MHz' : '—'));
    sysBody.append(statRow('Heap Free', formatBytes(sys.heapFree)));
    sysBody.append(statRow('PSRAM Free', formatBytes(sys.psramFree)));
    sysBody.append(statRow('WebUI Root', sys.webuiPath));

    storageBody.innerHTML = '';
    storageBody.append(statRow('SD Ready', sys.storageReady ? 'Yes' : 'No'));
    storageBody.append(statRow('Writable', sys.storageWritable ? 'Yes' : 'No'));
    storageBody.append(statRow('WebUI on SD', sys.storageHasWww ? 'Yes' : 'No'));
    storageBody.append(statRow('Card', sys.storageCardType || '—'));
    if (sys.storageTotalMb != null) {
      storageBody.append(statRow('Free / Total', `${sys.storageFreeMb || 0} / ${sys.storageTotalMb} MB`));
    }
    storageBody.append(statRow('Shows Path', sys.showsPath));
    storageBody.append(statRow('WebUI Path', sys.webuiPath || '/showduino/www'));
    storageBody.append(statRow('Status', sys.storageMessage || '—'));
  }

  // Paint from existing state immediately
  const s = getState();
  if (s.system?.boardName) {
    paintSystem(s.system);
  }

  // Subscribe for system updates
  cleanup.add(subscribe((state) => {
    if (state.system?.boardName) paintSystem(state.system);
  }));

  // Also poll if not already fresh
  fetchSystem().then((sys) => {
    applySystemData(sys);
  }).catch(() => {
    if (!getState().system?.boardName) {
      sysBody.innerHTML = '';
      sysBody.append(el('p', { className: 'text-muted', text: 'System API unavailable' }));
    }
  });

  return () => cleanup.run();
}

SettingsPage.title = 'Settings';
