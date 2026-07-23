import { fetchSystem } from '../api.js';
import { el, archBlock } from '../utils.js';

export async function LightingPage(container) {
  container.append(el('p', { className: 'info-panel', text: 'Lighting and relays are executed on remote nodes (Relay Node ESP32, Stage Engine). The Director sends relay and LED commands; confirmed state returns via ESP-NOW.' }));

  const panel = el('div', { className: 'card' });
  panel.append(el('h2', { text: 'Lighting Architecture' }));

  try {
    const sys = await fetchSystem();
    panel.append(archBlock('Relay Command', '{"cmd":"relay","id":1-8,"state":0|1}'));
    panel.append(archBlock('LED Command', '{"cmd":"led","line":1-4,"pixel":N,"brightness":0-255}'));
    panel.append(archBlock('Fixture Profiles', sys.fixtureProfilesPath));
    panel.append(archBlock('Device Presets', sys.devicePresetsPath));
    panel.append(archBlock('Paired Devices', sys.pairedDevicesPath));
    panel.append(archBlock('Lighting Icons', sys.lightingIconsPath));
    panel.append(el('p', { className: 'sub', text: 'Relay grid supports 8 tactile toggles expandable via SX1509. LED lines use per-pixel brightness control on the Brain. Director never switches outputs directly.' }));
  } catch (err) {
    panel.append(el('p', { text: err.message }));
  }

  container.append(panel);
}
LightingPage.title = 'Lighting';
