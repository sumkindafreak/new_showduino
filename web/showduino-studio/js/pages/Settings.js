import { subscribeStore } from '../store.js';
import { el, plannedNote, statRow } from '../utils.js';

const PREF_CUES = 'showduino.pref.liveCues';

function prefCues() {
  return localStorage.getItem(PREF_CUES) !== '0';
}

export async function SettingsPage(container) {
  container.append(el('p', {
    className: 'info-panel',
    text: 'Persistent configuration only. Live START / STOP / PANIC stay on Live. P4 Ethernet and E1.31 live on Network. SD health and backups live on System.'
  }));

  const host = el('div', { className: 'page-stack' });
  container.append(host);

  function paint(snap) {
    host.innerHTML = '';
    const c = snap.comms;

    const prefs = el('div', { className: 'card' });
    prefs.append(el('h2', { text: 'WebUI preferences' }));
    const cueLabel = el('label', { className: 'pref-row' });
    const cueBox = el('input', { type: 'checkbox' });
    cueBox.checked = prefCues();
    cueBox.addEventListener('change', () => {
      localStorage.setItem(PREF_CUES, cueBox.checked ? '1' : '0');
    });
    cueLabel.append(cueBox, document.createTextNode(' Keep Live cue table visible (local browser only)'));
    prefs.append(cueLabel);
    prefs.append(el('p', { className: 'sub', text: 'Preferences stay in this browser. They are not show state.' }));
    host.append(prefs);

    const ap = el('div', { className: 'card' });
    ap.append(el('h2', { text: 'Comms AP' }));
    if (c) {
      ap.append(statRow('SSID', c.ssid || 'Showduino'));
      ap.append(statRow('Channel', c.radioChannel ?? c.espnowChannel ?? '—'));
      ap.append(statRow('IP', c.ip || '—'));
      ap.append(statRow('mDNS', (c.mdnsHost || 'showduino') + '.local'));
      ap.append(statRow('MAC', c.mac || '—'));
    }
    ap.append(plannedNote('AP password and channel writes are not exposed. Default bench secret remains showduino until changed in firmware.'));
    host.append(ap);

    const eth = el('div', { className: 'card' });
    eth.append(el('h2', { text: 'P4 Ethernet' }));
    eth.append(el('p', { className: 'sub', text: 'DHCP, static IP, and the E1.31 test receiver are configured on the Network page. This page does not write them.' }));
    host.append(eth);

    const plugin = el('div', { className: 'card' });
    plugin.append(el('h2', { text: 'Plug-in Bus config' }));
    plugin.append(statRow('Role file', '/showduino/config/plugin-bus.json'));
    plugin.append(el('p', { className: 'sub', text: 'Roles are assigned on the P4 SD card. Identity is not role. This WebUI does not edit that file yet.' }));
    host.append(plugin);

    const system = el('div', { className: 'card' });
    system.append(el('h2', { text: 'System options' }));
    system.append(statRow('WebUI host', 'Communications S3 PROGMEM'));
    system.append(statRow('Authoritative engine', 'ESP32-P4'));
    system.append(statRow('Operator desk', 'Director'));
    system.append(statRow('Persistent storage', '/showduino on P4 SD'));
    host.append(system);
  }

  return subscribeStore(paint);
}
SettingsPage.title = 'Settings';
