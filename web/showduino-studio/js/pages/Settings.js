import { dispatchOperatorCommand, initializeRuntimeStore, refreshRuntime, subscribeRuntime } from '../runtimeStore.js';
import { el, formatBytes, formatUptime, statRow } from '../utils.js';

async function invoke(status, action, payload = {}) {
  status.textContent = `Dispatching ${action}…`;
  try {
    await dispatchOperatorCommand(action, payload);
    status.textContent = `${action} queued`;
  } catch (error) {
    status.textContent = error.message;
  }
}

export function SettingsPage(container) {
  initializeRuntimeStore();
  container.append(el('p', {
    className: 'info-panel',
    text: 'Firmware settings loaded from runtimeStore. Save operations route through the central dispatcher.'
  }));

  const status = el('div', { className: 'live-status', text: 'Loading settings…' });
  const card = el('div', { className: 'card' });
  card.append(el('h2', { text: 'System Info' }));
  const info = el('div');
  card.append(info);

  const network = el('div', { className: 'card command-form' });
  network.append(el('h2', { text: 'Network / Hostname' }));
  const ssid = el('input', { value: '' });
  const hostname = el('input', { value: '' });
  network.append(el('label', { className: 'cmd-field' }, ['SSID ', ssid]));
  network.append(el('label', { className: 'cmd-field' }, ['Hostname ', hostname]));
  network.append(el('button', {
    className: 'btn-primary',
    text: 'Save Network',
    onClick: () => invoke(status, 'settings.network', { ssid: ssid.value.trim(), hostname: hostname.value.trim() })
  }));
  network.append(el('button', {
    className: 'btn-primary',
    text: 'Save Hostname',
    onClick: () => invoke(status, 'settings.hostname', { hostname: hostname.value.trim() })
  }));

  const time = el('div', { className: 'card command-form' });
  time.append(el('h2', { text: 'Time / Preferences' }));
  const timezone = el('input', { value: '' });
  const dst = el('input', { type: 'checkbox' });
  const preference = el('input', { value: '', placeholder: 'key=value' });
  time.append(el('label', { className: 'cmd-field' }, ['Timezone ', timezone]));
  time.append(el('label', { className: 'cmd-field' }, ['DST Enabled ', dst]));
  time.append(el('label', { className: 'cmd-field' }, ['Preference ', preference]));
  time.append(el('button', {
    className: 'btn-primary',
    text: 'Save Time',
    onClick: () => invoke(status, 'settings.time', { timezone: timezone.value.trim(), dstEnabled: dst.checked })
  }));
  time.append(el('button', {
    className: 'btn-primary',
    text: 'Save Preferences',
    onClick: () => {
      const raw = preference.value.trim();
      let prefs = {};
      if (raw.includes('=')) {
        const [key, value] = raw.split('=');
        prefs[key.trim()] = value.trim();
      }
      invoke(status, 'settings.preferences', prefs);
    }
  }));

  const danger = el('div', { className: 'card command-form' });
  danger.append(el('h2', { text: 'Device Control' }));
  danger.append(el('button', {
    className: 'btn-primary',
    text: 'Reboot Firmware',
    onClick: () => invoke(status, 'settings.reboot')
  }));
  danger.append(el('button', {
    className: 'btn-primary',
    text: 'Factory Reset',
    onClick: () => {
      if (!confirm('Factory reset firmware settings? This cannot be undone.')) return;
      invoke(status, 'settings.factoryReset');
    }
  }));
  danger.append(el('button', {
    className: 'btn-primary',
    text: 'Reload Settings',
    onClick: async () => {
      status.textContent = 'Refreshing settings…';
      await refreshRuntime('all').catch(() => {});
    }
  }));

  container.append(status, card, network, time, danger);

  const unsub = subscribeRuntime((snap) => {
    const sys = snap.system || {};
    const t = snap.time || {};
    status.textContent = snap.connection.connected
      ? 'Live settings linked to runtimeStore'
      : `Disconnected · reconnect in ${snap.connection.reconnectInSec || 0}s`;

    if (sys.wifi?.ssid && ssid.value !== sys.wifi.ssid) ssid.value = sys.wifi.ssid;
    const hostValue = sys.wifi?.hostname || sys.mdnsHost || '';
    if (hostValue && hostname.value !== hostValue) hostname.value = hostValue;
    if (t.timezone && timezone.value !== t.timezone) timezone.value = t.timezone;
    dst.checked = Boolean(t.dstEnabled);

    info.innerHTML = '';
    info.append(statRow('Board', sys.boardName || '—'));
    info.append(statRow('Firmware', sys.firmwareVersion || '—'));
    info.append(statRow('Protocol', sys.protocolVersion || '—'));
    info.append(statRow('Hostname', sys.wifi?.hostname || sys.mdnsHost || '—'));
    info.append(statRow('AP SSID', sys.apSsid || sys.wifi?.ssid || '—'));
    info.append(statRow('mDNS', sys.mdnsHost ? `${sys.mdnsHost}.local` : '—'));
    info.append(statRow('Uptime', formatUptime(sys.uptime)));
    info.append(statRow('CPU', sys.cpuMhz != null ? `${sys.cpuMhz} MHz` : '—'));
    info.append(statRow('Heap Free', formatBytes(sys.heapFree)));
    info.append(statRow('PSRAM Free', formatBytes(sys.psramFree)));
    info.append(statRow('Storage', sys.storageReady ? 'Ready' : 'Recovery'));
    info.append(statRow('WebUI Root', sys.webuiPath || '—'));
  });

  return () => unsub();
}
SettingsPage.title = 'Settings';
