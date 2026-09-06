import { fetchSystem } from '../api.js';
import { el, archBlock } from '../utils.js';

export async function LightingPage(container) {
  container.append(el('p', {
    className: 'info-panel',
    text: 'Show pixels are separate from the P4 emergency pixel line. Normal show effects may run locally on the P4 or on remote Pixel Nodes, but both use the same output/segment/effect model. Emergency pixels remain isolated and non-editable by show timelines.'
  }));

  const emergencyPanel = el('div', { className: 'card' });
  emergencyPanel.append(el('h2', { text: 'Emergency Pixels — P4 Local' }));
  emergencyPanel.append(archBlock('Data line', 'P4 GPIO24'));
  emergencyPanel.append(archBlock('Purpose', 'Safety/emergency indication only'));
  emergencyPanel.append(archBlock('Ownership', 'Local P4 emergency subsystem'));
  emergencyPanel.append(archBlock('Show editing', 'Never exposed as a normal Studio pixel line or production effect lane'));
  container.append(emergencyPanel);

  const showPanel = el('div', { className: 'card' });
  showPanel.append(el('h2', { text: 'Show Pixel Engine' }));
  showPanel.append(archBlock('Execution targets', 'P4 local show-pixel outputs + remote ESP32 Pixel Nodes'));
  showPanel.append(archBlock('Shared model', 'Output → named segments → independent effects → final strip render'));
  showPanel.append(archBlock('Example', 'Pixels 0–7 LIGHTNING · 8–10 SOLID BLUE · 11+ CANDLE/WARM GLOW simultaneously'));
  showPanel.append(archBlock('Renderer location', 'Effects render locally at the output device; P4 sends intent, not per-frame pixel data'));
  showPanel.append(archBlock('Colour model', 'RGBW-capable internal model so RGB and RGBW strips can share the same cue language'));
  container.append(showPanel);

  const effectPanel = el('div', { className: 'card' });
  effectPanel.append(el('h2', { text: 'Segments & Effects' }));
  effectPanel.append(archBlock('Core effects', 'OFF, SOLID, FADE, PULSE, FLICKER, CANDLE, FIRE, STROBE, LIGHTNING, CHASE, SPARKLE, BREATH, COLOR_CYCLE'));
  effectPanel.append(archBlock('Segment parameters', 'output, start, count, effect, RGBW colour, brightness, speed, direction, duration'));
  effectPanel.append(archBlock('Brightness hierarchy', 'Node master × output brightness × segment brightness'));
  effectPanel.append(archBlock('Power policy', 'Configurable current/brightness limiting; strip power comes from an external supply, not the ESP32'));
  container.append(effectPanel);

  const routingPanel = el('div', { className: 'card' });
  routingPanel.append(el('h2', { text: 'Pixel Routing & Safety' }));
  routingPanel.append(archBlock('Local P4 pixels', 'P4 executes the shared Pixel Engine directly on a dedicated show-pixel output'));
  routingPanel.append(archBlock('Remote pixels', 'P4 → ROUTE:PIXEL:* → S3 Comms Controller → ESP-NOW → Pixel Node'));
  routingPanel.append(archBlock('Node packet type', 'ShowduinoNodePacket · nodeType="PIXEL"'));
  routingPanel.append(archBlock('Emergency behaviour', 'Show pixels blackout, effects clear and commands lock until EMERGENCY:CLEAR; no automatic effect resume'));
  routingPanel.append(el('p', {
    className: 'sub',
    text: 'The P4 remains authoritative for show state. Remote nodes execute requested effects and report start/completion/failure; they do not make show decisions.'
  }));
  container.append(routingPanel);

  const runtimePanel = el('div', { className: 'card' });
  runtimePanel.append(el('h2', { text: 'Runtime Status' }));
  try {
    const sys = await fetchSystem();
    runtimePanel.append(archBlock('Emergency pixel line', sys.emergencyPixelStatus || 'P4 local GPIO24'));
    runtimePanel.append(archBlock('P4 show pixels', sys.localPixelStatus || 'Shared Pixel Engine integration pending'));
    runtimePanel.append(archBlock('Remote Pixel Nodes', sys.pixelNodeStatus || 'Integration/commissioning pending — WebUI must not claim effects until P4 reports them'));
    runtimePanel.append(archBlock('Fixture profiles', sys.fixtureProfilesPath || '—'));
    runtimePanel.append(archBlock('Device presets', sys.devicePresetsPath || '—'));
  } catch (err) {
    runtimePanel.append(el('p', { text: err.message }));
  }
  container.append(runtimePanel);
}
LightingPage.title = 'Lighting';
