import { fetchSystem } from '../api.js';
import { el, archBlock } from '../utils.js';

export async function AudioPage(container) {
  container.append(el('p', { className: 'info-panel', text: 'Audio playback is handled by the Stage Engine and MP3 modules on the Brain. The Director UI sends volume and transport commands via ESP-NOW.' }));

  const panel = el('div', { className: 'card' });
  panel.append(el('h2', { text: 'Audio Architecture' }));

  try {
    const sys = await fetchSystem();
    panel.append(archBlock('Command Protocol', 'JSON serial + ESP-NOW desk packets v' + (sys.protocolVersion || '1.0')));
    panel.append(archBlock('MP3 Control', '{"cmd":"mp3","player":"A|B","volume":0-255}'));
    panel.append(archBlock('UI Sounds', sys.uiSoundsPath));
    panel.append(archBlock('Director I2S', sys.i2sStatus || 'Deferred — GPIO17/18 reserved for Stage UART'));
    panel.append(el('p', { className: 'sub', text: 'Dual MP3 decks (A/B) with volume sliders and play/pause/stop are controlled from the Live Control screen. Audio hardware runs on the Brain, not the Director panel.' }));
  } catch (err) {
    panel.append(el('p', { text: err.message }));
  }

  container.append(panel);
}
AudioPage.title = 'Audio';
