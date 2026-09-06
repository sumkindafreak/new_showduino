import { fetchSystem } from '../api.js';
import { el, archBlock } from '../utils.js';

export async function AudioPage(container) {
  container.append(el('p', {
    className: 'info-panel',
    text: 'Showduino has two deliberately separate audio roles: P4 local audio for system/safety notifications, and specialist Audio Nodes for theatrical show audio. The P4 remains authoritative; ESP-NOW carries commands and state, never PCM audio.'
  }));

  const systemPanel = el('div', { className: 'card' });
  systemPanel.append(el('h2', { text: 'System Audio — P4 Local' }));
  systemPanel.append(archBlock('Owner', 'ESP32-P4 Show Engine'));
  systemPanel.append(archBlock('Purpose', 'Boot, ready, operator notifications, warnings, faults and emergency/system audio'));
  systemPanel.append(archBlock('Execution', 'Local P4 audio hardware — does not depend on Director, browser, Wi-Fi, ESP-NOW or a remote Audio Node'));
  systemPanel.append(archBlock('Safety rule', 'Remote show-audio failure must never remove P4 system/emergency audio capability'));
  systemPanel.append(el('p', {
    className: 'sub',
    text: 'System audio belongs to Showduino itself. It remains available even when a production Audio Node is offline.'
  }));
  container.append(systemPanel);

  const showPanel = el('div', { className: 'card' });
  showPanel.append(el('h2', { text: 'Show Audio — Specialist Audio Node' }));
  showPanel.append(archBlock('Baseline hardware', 'Ai-Thinker ESP32-Audio-Kit / ESP32-A1S with ES8388 codec and local microSD'));
  showPanel.append(archBlock('Purpose', 'Music, ambience, dialogue, scare SFX, stingers and production audio'));
  showPanel.append(archBlock('Transport', 'P4 → S3 Comms Controller → ESP-NOW → Audio Node'));
  showPanel.append(archBlock('Assets', 'Stored locally on the Audio Node; Showduino sends intent/filenames, not audio streams'));
  showPanel.append(archBlock('Local controls', 'Board buttons are commissioning/maintenance controls for test playback, diagnostics and volume; show authority remains with the P4'));
  showPanel.append(el('p', {
    className: 'sub',
    text: 'Target command lifecycle: request → accepted/rejected → started → completed/failed → authoritative P4 state.'
  }));
  container.append(showPanel);

  const commandPanel = el('div', { className: 'card' });
  commandPanel.append(el('h2', { text: 'Audio Command Model' }));
  commandPanel.append(archBlock('P4 system audio', 'AUDIO:LOCAL:* / system-notification commands'));
  commandPanel.append(archBlock('Show Audio Node', 'AUDIO:NODE:PLAY, LOOP, STOP, PAUSE, RESUME, VOLUME'));
  commandPanel.append(archBlock('Node packet type', 'ShowduinoNodePacket · nodeType="AUDIO"'));
  commandPanel.append(archBlock('Emergency', 'Audio Node stops, clears playback and locks show-audio commands until EMERGENCY:CLEAR; it does not auto-resume'));
  container.append(commandPanel);

  const runtimePanel = el('div', { className: 'card' });
  runtimePanel.append(el('h2', { text: 'Runtime Status' }));
  try {
    const sys = await fetchSystem();
    runtimePanel.append(archBlock('Protocol', `Showduino wire protocol v${sys.protocolVersion || '1'}`));
    runtimePanel.append(archBlock('P4 system audio', sys.i2sStatus || 'Local system/emergency audio path'));
    runtimePanel.append(archBlock('Audio Node', sys.audioNodeStatus || 'Integration/commissioning pending — WebUI must not claim playback until P4 reports it'));
    runtimePanel.append(archBlock('System sound assets', sys.uiSoundsPath || '/showduino/audio/system/'));
  } catch (err) {
    runtimePanel.append(el('p', { text: err.message }));
  }
  container.append(runtimePanel);
}
AudioPage.title = 'Audio';
