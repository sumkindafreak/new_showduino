import { fetchLighting, postCommand, isP4Offline } from '../api.js';
import { subscribeStore } from '../store.js';
import { el, p4OfflineBanner, plannedNote, statRow } from '../utils.js';
import { emergencyWord } from '../status.js';

const PIXEL_FX = [
  'OFF', 'SOLID', 'FADE_IN', 'FADE_OUT', 'PULSE', 'BREATHE', 'FLICKER',
  'CANDLE', 'FIRE', 'LIGHTNING', 'STROBE', 'RANDOM_STROBE', 'CHASE',
  'BOUNCE', 'COMET', 'WIPE', 'REVERSE_WIPE', 'BUILD', 'SPARKLE',
  'TWINKLE', 'GLITCH', 'WARNING', 'PORTAL', 'RAINBOW', 'CUSTOM_SEQUENCE'
];

function hexRgb(value) {
  const v = String(value || '#000000').replace('#', '').padStart(6, '0').slice(0, 6);
  return [0, 2, 4].map((i) => parseInt(v.slice(i, i + 2), 16) || 0);
}

function defaultSegEditor() {
  return {
    id: 0,
    start: 0,
    count: 10,
    fx: 'SOLID',
    color: '#ffffff',
    color2: '#000000',
    brightness: 255,
    speed: 50,
    intensity: 80,
    randomness: 70,
    duration: 0,
    reverse: false
  };
}

function numberField(label, value, min, max, onChange) {
  const wrap = el('label', { className: 'sub' });
  wrap.append(document.createTextNode(label + ' '));
  const input = el('input', {
    className: 'text-input',
    type: 'number',
    value: String(value),
    min: String(min),
    max: String(max)
  });
  input.addEventListener('input', () => {
    const n = parseInt(input.value, 10);
    if (Number.isFinite(n)) onChange(n);
  });
  input.addEventListener('change', () => {
    const n = Math.max(min, Math.min(max, Number(input.value) || 0));
    input.value = String(n);
    onChange(n);
  });
  wrap.append(input);
  return wrap;
}

export async function OutputsPage(container) {
  container.append(el('p', {
    className: 'info-panel',
    text: 'P4 GPIO23 is the local segmented Show Pixel Line. Remote C3 Pixel Nodes use the same PIXEL LINE → SEGMENTS → FX model. GPIO24 is safety-owned emergency signage. Programme audio is the Audio Node.'
  }));

  const host = el('div', { className: 'page-stack' });
  container.append(host);

  let lighting = null;
  let pending = null;
  let lastResult = '';
  let lastSnap = { p4Online: false, system: null };
  let assetPath = 'system-test.wav';
  let lineCount = 100;
  let lineCountSeeded = false;
  const nodeLineCounts = {};
  const nodeEditors = {};
  const px = {
    id: 0,
    start: 0,
    count: 10,
    fx: 'SOLID',
    color: '#ffffff',
    color2: '#000000',
    brightness: 255,
    speed: 50,
    intensity: 80,
    randomness: 70,
    duration: 0,
    reverse: false
  };

  async function send(cmd) {
    pending = cmd;
    paint();
    try {
      const data = await postCommand(cmd);
      lastResult = isP4Offline(data) ? 'P4 OFFLINE' : (data.replies || JSON.stringify(data));
      if (String(cmd).startsWith('PIXEL:')) {
        try {
          const light = await fetchLighting();
          lighting = isP4Offline(light) ? null : light;
        } catch (_) {}
      }
    } catch (err) {
      lastResult = err.message;
    }
    pending = null;
    paint();
  }

  async function sendMany(commands, label = 'PIXEL:SEGMENT') {
    pending = label;
    lastResult = '';
    paint();
    const replies = [];
    try {
      for (const cmd of commands) {
        const data = await postCommand(cmd);
        if (isP4Offline(data)) {
          replies.push('P4 OFFLINE');
          break;
        }
        replies.push(data.replies || cmd + ': OK');
      }
      lastResult = replies.join(' | ');
      try {
        const data = await fetchLighting();
        lighting = isP4Offline(data) ? null : data;
      } catch (_) {}
    } catch (err) {
      lastResult = err.message;
    }
    pending = null;
    paint();
  }

  function segmentCommands(includeStart) {
    const c1 = hexRgb(px.color);
    const c2 = hexRgb(px.color2);
    const base = `PIXEL:SEGMENT:${px.id}`;
    const commands = [
      `${base}:RANGE:${px.start}:${px.count}`,
      `${base}:FX:${px.fx}`,
      `${base}:COLOR:${c1[0]}:${c1[1]}:${c1[2]}`,
      `${base}:COLOR2:${c2[0]}:${c2[1]}:${c2[2]}`,
      `${base}:BRIGHTNESS:${px.brightness}`,
      `${base}:SPEED:${px.speed}`,
      `${base}:INTENSITY:${px.intensity}`,
      `${base}:RANDOMNESS:${px.randomness}`,
      `${base}:DURATION:${px.duration}`,
      `${base}:REVERSE:${px.reverse ? 1 : 0}`
    ];
    if (includeStart) commands.push(`${base}:START`);
    return commands;
  }

  function paint() {
    host.innerHTML = '';
    if (!lastSnap.p4Online) host.append(p4OfflineBanner());
    const s = lastSnap.system;
    const emergency = emergencyWord(s);

    const pixels = el('div', { className: emergency === 'EMERGENCY' ? 'card danger-card' : 'card' });
    pixels.append(el('h2', { text: 'Emergency signage — GPIO24' }));
    if (lastSnap.p4Online && lighting) {
      pixels.append(statRow('Line', lighting.emergencyPixelsReady ? 'READY' : 'FAULT'));
      pixels.append(statRow('Emergency white', lighting.emergencyPixelsWhite ? 'ACTIVE' : 'OFF'));
      pixels.append(statRow('Emergency latch', emergency === 'EMERGENCY' ? 'EMERGENCY' : 'CLEAR'));
      pixels.append(el('p', {
        className: 'sub',
        text: 'Safety-owned 10-pixel sign groups. Normal state is one green locator pixel per group; emergency changes the complete line to synchronized bright white.'
      }));
    } else {
      pixels.append(el('p', { className: 'sub', text: lastSnap.p4Online ? 'Waiting for P4 lighting status…' : 'Unavailable while P4 is offline.' }));
    }
    host.append(pixels);

    if (lighting && !lineCountSeeded) {
      const seeded = Number(lighting.showPixelsConfiguredCount || lighting.showPixelsCount || 0);
      if (seeded > 0) lineCount = seeded;
      lineCountSeeded = true;
    }

    const showPx = el('div', { className: emergency === 'EMERGENCY' ? 'card danger-card' : 'card' });
    showPx.append(el('h2', { text: 'P4 Show Pixel Line — GPIO23' }));
    if (lastSnap.p4Online && lighting) {
      const engine = lighting.showPixelsReady ? 'READY' : 'NOT INITIALISED';
      const maxPx = lighting.showPixelsMax != null ? lighting.showPixelsMax : 1024;
      showPx.append(statRow('Engine', engine));
      showPx.append(statRow('Configured length', lighting.showPixelsConfiguredCount != null ? String(lighting.showPixelsConfiguredCount) : '—'));
      showPx.append(statRow('Active pixels', lighting.showPixelsReady ? String(lighting.showPixelsCount) : '—'));
      showPx.append(statRow('Global brightness', lighting.showPixelsBrightness != null ? String(lighting.showPixelsBrightness) : '—'));
      showPx.append(statRow('Emergency override', lighting.showPixelsEmergencyWhite ? 'ALL WHITE' : 'CLEAR'));
      showPx.append(statRow('Segment slots', lighting.showPixelMaxSegments != null ? String(lighting.showPixelMaxSegments) : '16'));
      showPx.append(statRow('FX library', lighting.showPixelFxCount != null ? String(lighting.showPixelFxCount) : '25'));
      showPx.append(el('p', {
        className: 'sub',
        text: 'Set the physical line length, then Initialise. The driver does not start until that count is applied. Max ' + maxPx + ' pixels. Default suggestion is 100.'
      }));
      const clampedCount = () => Math.max(1, Math.min(maxPx, Number(lineCount) || 0));
      const countRow = el('div', { className: 'filter-row' });
      countRow.append(numberField('Line pixels', lineCount, 1, maxPx, (v) => { lineCount = v; }));
      const countBusy = !!pending || !lastSnap.p4Online || emergency === 'EMERGENCY';
      countRow.append(el('button', {
        className: 'btn-cancel',
        text: String(pending || '').startsWith('PIXEL:COUNT') ? 'SAVING…' : 'SAVE COUNT',
        disabled: countBusy,
        onClick: () => send('PIXEL:COUNT:' + clampedCount())
      }));
      countRow.append(el('button', {
        className: 'btn-primary',
        text: pending === 'PIXEL:INIT' ? 'INITIALISING…' : 'INITIALISE LINE',
        disabled: countBusy,
        onClick: () => sendMany(['PIXEL:COUNT:' + clampedCount(), 'PIXEL:INIT'], 'PIXEL:INIT')
      }));
      showPx.append(countRow);
    } else {
      showPx.append(el('p', { className: 'sub', text: lastSnap.p4Online ? 'Waiting for P4 pixel status…' : 'Unavailable while P4 is offline.' }));
    }
    showPx.append(el('p', {
      className: 'sub',
      text: 'Segments are the Studio authoring primitive: define a region once, then apply FX to that region. Emergency policy is fixed and cannot be edited here.'
    }));

    const globalRow = el('div', { className: 'filter-row' });
    const pixelBusy = !!pending || !lastSnap.p4Online || emergency === 'EMERGENCY' || !lighting || !lighting.showPixelsReady;
    globalRow.append(el('button', {
      className: 'btn-primary',
      text: pending === 'PIXEL:TEST' ? 'TEST…' : 'RUN PIXEL TEST',
      disabled: pixelBusy,
      onClick: () => send('PIXEL:TEST')
    }));
    globalRow.append(el('button', {
      className: 'btn-cancel',
      text: 'STOP TEST',
      disabled: pixelBusy,
      onClick: () => send('PIXEL:TEST:STOP')
    }));
    globalRow.append(el('button', {
      className: 'btn-cancel',
      text: 'BLACKOUT',
      disabled: pixelBusy,
      onClick: () => send('PIXEL:BLACKOUT')
    }));
    showPx.append(globalRow);

    const globalBrightness = el('input', {
      type: 'range',
      min: '0',
      max: '255',
      value: String(lighting && lighting.showPixelsBrightness != null ? lighting.showPixelsBrightness : 255),
      disabled: pixelBusy
    });
    globalBrightness.addEventListener('change', () => send('PIXEL:BRIGHTNESS:' + globalBrightness.value));
    showPx.append(el('p', { className: 'sub', text: 'Global line brightness' }));
    showPx.append(globalBrightness);

    showPx.append(el('h3', { text: 'Segment editor / commissioning' }));
    const pickRow = el('div', { className: 'filter-row' });
    const segSelect = el('select', { className: 'text-input' });
    for (let i = 0; i < 16; i++) {
      const opt = el('option', { value: String(i), text: `Segment ${i}` });
      if (i === px.id) opt.selected = true;
      segSelect.append(opt);
    }
    segSelect.addEventListener('change', () => { px.id = Number(segSelect.value); });
    pickRow.append(segSelect);

    const fxSelect = el('select', { className: 'text-input' });
    for (const name of PIXEL_FX) {
      const opt = el('option', { value: name, text: name });
      if (name === px.fx) opt.selected = true;
      fxSelect.append(opt);
    }
    fxSelect.addEventListener('change', () => { px.fx = fxSelect.value; });
    pickRow.append(fxSelect);
    showPx.append(pickRow);

    const rangeRow = el('div', { className: 'filter-row' });
    rangeRow.append(numberField('Start', px.start, 0, 999, (v) => { px.start = v; }));
    rangeRow.append(numberField('Segment length', px.count, 1, lighting && lighting.showPixelsMax != null ? lighting.showPixelsMax : 1024, (v) => { px.count = v; }));
    rangeRow.append(numberField('Brightness', px.brightness, 0, 255, (v) => { px.brightness = v; }));
    rangeRow.append(numberField('Speed', px.speed, 1, 100, (v) => { px.speed = v; }));
    showPx.append(rangeRow);

    const fxRow = el('div', { className: 'filter-row' });
    fxRow.append(numberField('Intensity', px.intensity, 0, 100, (v) => { px.intensity = v; }));
    fxRow.append(numberField('Randomness', px.randomness, 0, 100, (v) => { px.randomness = v; }));
    fxRow.append(numberField('Duration ms', px.duration, 0, 4294967295, (v) => { px.duration = v; }));
    const reverse = el('label', { className: 'sub' });
    const reverseBox = el('input', { type: 'checkbox', checked: px.reverse });
    reverseBox.addEventListener('change', () => { px.reverse = reverseBox.checked; });
    reverse.append(reverseBox, document.createTextNode(' Reverse'));
    fxRow.append(reverse);
    showPx.append(fxRow);

    const colorRow = el('div', { className: 'filter-row' });
    const c1Wrap = el('label', { className: 'sub' });
    c1Wrap.append(document.createTextNode('Primary '));
    const c1 = el('input', { type: 'color', value: px.color });
    c1.addEventListener('change', () => { px.color = c1.value; });
    c1Wrap.append(c1);
    colorRow.append(c1Wrap);
    const c2Wrap = el('label', { className: 'sub' });
    c2Wrap.append(document.createTextNode('Secondary '));
    const c2 = el('input', { type: 'color', value: px.color2 });
    c2.addEventListener('change', () => { px.color2 = c2.value; });
    c2Wrap.append(c2);
    colorRow.append(c2Wrap);
    showPx.append(colorRow);

    const segRow = el('div', { className: 'filter-row' });
    segRow.append(el('button', {
      className: 'btn-primary',
      text: pending === 'PIXEL:SEGMENT:APPLY_START' ? 'APPLYING…' : 'APPLY + START',
      disabled: pixelBusy,
      onClick: () => sendMany(segmentCommands(true), 'PIXEL:SEGMENT:APPLY_START')
    }));
    segRow.append(el('button', {
      className: 'btn-cancel',
      text: 'APPLY ONLY',
      disabled: pixelBusy,
      onClick: () => sendMany(segmentCommands(false), 'PIXEL:SEGMENT:APPLY')
    }));
    segRow.append(el('button', {
      className: 'btn-cancel',
      text: 'START',
      disabled: pixelBusy,
      onClick: () => send(`PIXEL:SEGMENT:${px.id}:START`)
    }));
    segRow.append(el('button', {
      className: 'btn-cancel',
      text: 'STOP',
      disabled: pixelBusy,
      onClick: () => send(`PIXEL:SEGMENT:${px.id}:STOP`)
    }));
    segRow.append(el('button', {
      className: 'btn-cancel',
      text: 'STATUS',
      disabled: !lastSnap.p4Online || !!pending,
      onClick: () => send(`PIXEL:SEGMENT:${px.id}:STATUS`)
    }));
    showPx.append(segRow);

    if (emergency === 'EMERGENCY') {
      showPx.append(el('p', {
        className: 'sub',
        text: 'EMERGENCY ACTIVE — every pixel-capable output is forced bright white. Segment/show controls are locked.'
      }));
    }
    if (lastResult && String(pending || lastResult).includes('PIXEL')) {
      showPx.append(el('p', { className: 'sub', text: lastResult }));
    }
    host.append(showPx);

    const remoteNodes = (lighting && Array.isArray(lighting.pixelNodes)) ? lighting.pixelNodes : [];
    if (!remoteNodes.length) {
      const waiting = el('div', { className: 'card' });
      waiting.append(el('h2', { text: 'Pixel Nodes' }));
      waiting.append(el('p', {
        className: 'sub',
        text: lastSnap.p4Online
          ? 'No C3 Pixel Nodes discovered yet. Flash the same firmware, then commission LED-01 / LED-02… on the node webpage.'
          : 'Unavailable while P4 is offline.'
      }));
      host.append(waiting);
    }
    for (const node of remoteNodes) {
      const nid = String(node.id || '').trim();
      if (!nid) continue;
      const prefix = `PIXEL:NODE:${nid}:`;
      if (nodeLineCounts[nid] == null) {
        const seeded = Number(node.pixelCount || 0);
        nodeLineCounts[nid] = seeded > 0 ? seeded : 100;
      }
      if (!nodeEditors[nid]) nodeEditors[nid] = defaultSegEditor();
      const npx = nodeEditors[nid];
      const maxPx = node.maxPixels != null ? Number(node.maxPixels) : 512;
      const ready = !!node.initialised && !!node.online;
      const card = el('div', { className: emergency === 'EMERGENCY' ? 'card danger-card' : 'card' });
      card.append(el('h2', { text: `Pixel Node — ${nid}${node.name && node.name !== nid ? ' — ' + node.name : ''}` }));
      card.append(statRow('Presence', node.online ? 'ONLINE' : 'OFFLINE'));
      card.append(statRow('Engine', ready ? 'READY' : 'NOT INITIALISED'));
      card.append(statRow('Configured length', node.pixelCount != null ? String(node.pixelCount) : '—'));
      card.append(statRow('Segments', node.segments != null ? String(node.segments) : '—'));
      card.append(statRow('State', node.state || '—'));
      card.append(statRow('Firmware', node.firmware || '—'));
      if (node.lastError) card.append(statRow('Fault', node.lastError));
      card.append(el('p', {
        className: 'sub',
        text: 'Same commissioning model as P4 GPIO23: Save Count (does not light) → Initialise Line. Max ' + maxPx + ' pixels on C3. Locate is time-limited and below emergency.'
      }));
      const nodeBusy = !!pending || !lastSnap.p4Online || emergency === 'EMERGENCY' || !node.online;
      const clamped = () => Math.max(1, Math.min(maxPx, Number(nodeLineCounts[nid]) || 0));
      const countRow = el('div', { className: 'filter-row' });
      countRow.append(numberField('Line pixels', nodeLineCounts[nid], 1, maxPx, (v) => { nodeLineCounts[nid] = v; }));
      countRow.append(el('button', {
        className: 'btn-cancel',
        text: String(pending || '') === prefix + 'COUNT:' + clamped() ? 'SAVING…' : 'SAVE COUNT',
        disabled: nodeBusy,
        onClick: () => send(prefix + 'COUNT:' + clamped())
      }));
      countRow.append(el('button', {
        className: 'btn-primary',
        text: pending === prefix + 'INIT' ? 'INITIALISING…' : 'INITIALISE LINE',
        disabled: nodeBusy,
        onClick: () => sendMany([prefix + 'COUNT:' + clamped(), prefix + 'INIT'], prefix + 'INIT')
      }));
      countRow.append(el('button', {
        className: 'btn-cancel',
        text: 'LOCATE',
        disabled: nodeBusy || !ready,
        onClick: () => send(prefix + 'LOCATE')
      }));
      card.append(countRow);

      const fxBusy = nodeBusy || !ready;
      const globalRow = el('div', { className: 'filter-row' });
      globalRow.append(el('button', {
        className: 'btn-primary',
        text: 'RUN PIXEL TEST',
        disabled: fxBusy,
        onClick: () => send(prefix + 'TEST')
      }));
      globalRow.append(el('button', {
        className: 'btn-cancel',
        text: 'BLACKOUT',
        disabled: fxBusy,
        onClick: () => send(prefix + 'BLACKOUT')
      }));
      card.append(globalRow);

      const pickRow = el('div', { className: 'filter-row' });
      const segSelect = el('select', { className: 'text-input' });
      for (let i = 0; i < 16; i++) {
        const opt = el('option', { value: String(i), text: `Segment ${i}` });
        if (i === npx.id) opt.selected = true;
        segSelect.append(opt);
      }
      segSelect.addEventListener('change', () => { npx.id = Number(segSelect.value); });
      pickRow.append(segSelect);
      const fxSelect = el('select', { className: 'text-input' });
      for (const name of PIXEL_FX) {
        const opt = el('option', { value: name, text: name });
        if (name === npx.fx) opt.selected = true;
        fxSelect.append(opt);
      }
      fxSelect.addEventListener('change', () => { npx.fx = fxSelect.value; });
      pickRow.append(fxSelect);
      card.append(pickRow);

      const rangeRow = el('div', { className: 'filter-row' });
      rangeRow.append(numberField('Start', npx.start, 0, Math.max(0, maxPx - 1), (v) => { npx.start = v; }));
      rangeRow.append(numberField('Count', npx.count, 1, maxPx, (v) => { npx.count = v; }));
      rangeRow.append(numberField('Brightness', npx.brightness, 0, 255, (v) => { npx.brightness = v; }));
      rangeRow.append(numberField('Speed', npx.speed, 1, 100, (v) => { npx.speed = v; }));
      card.append(rangeRow);

      const colorRow = el('div', { className: 'filter-row' });
      const c1Wrap = el('label', { className: 'sub' });
      c1Wrap.append(document.createTextNode('Primary '));
      const c1 = el('input', { type: 'color', value: npx.color });
      c1.addEventListener('change', () => { npx.color = c1.value; });
      c1Wrap.append(c1);
      colorRow.append(c1Wrap);
      card.append(colorRow);

      const nodeSegCommands = (includeStart) => {
        const rgb = hexRgb(npx.color);
        const base = `${prefix}SEGMENT:${npx.id}`;
        const commands = [
          `${base}:RANGE:${npx.start}:${npx.count}`,
          `${base}:FX:${npx.fx}`,
          `${base}:COLOR:${rgb[0]}:${rgb[1]}:${rgb[2]}`,
          `${base}:BRIGHTNESS:${npx.brightness}`,
          `${base}:SPEED:${npx.speed}`
        ];
        if (includeStart) commands.push(`${base}:START`);
        return commands;
      };
      const segRow = el('div', { className: 'filter-row' });
      segRow.append(el('button', {
        className: 'btn-primary',
        text: 'APPLY + START',
        disabled: fxBusy,
        onClick: () => sendMany(nodeSegCommands(true), prefix + 'SEGMENT:APPLY_START')
      }));
      segRow.append(el('button', {
        className: 'btn-cancel',
        text: 'STOP',
        disabled: fxBusy,
        onClick: () => send(`${prefix}SEGMENT:${npx.id}:STOP`)
      }));
      card.append(segRow);
      if (emergency === 'EMERGENCY') {
        card.append(el('p', {
          className: 'sub',
          text: 'EMERGENCY ACTIVE — this Pixel Node line is forced bright white. Studio cannot clear emergency from here.'
        }));
      }
      host.append(card);
    }

    const audio = el('div', { className: 'card' });
    audio.append(el('h2', { text: 'P4 speaker is not a show output' }));
    audio.append(el('p', {
      className: 'sub',
      text: 'Onboard ES8311 system/safety audio lives under System → P4 SYSTEM AUDIO. Attraction audio is the Audio Node below.'
    }));
    host.append(audio);

    const an = (s && s.audioNode) || {};
    const node = el('div', { className: 'card' });
    node.append(el('h2', { text: 'Audio Node' }));
    if (lastSnap.p4Online && (an.seen || an.online)) {
      node.append(el('div', { className: 'value', text: an.online ? (an.state || 'ONLINE') : 'OFFLINE' }));
      node.append(statRow('Lifecycle', an.pending ? 'PENDING' : (an.lastLife || '—')));
      node.append(statRow('Asset', an.asset || '—'));
      node.append(statRow('Volume', an.volume != null ? String(an.volume) : '—'));
      node.append(statRow('Output', an.output || '—'));
      node.append(statRow('Last error', an.lastError || '—'));
      const assets = Array.isArray(an.inventory) ? an.inventory : [];
      if (assets.length) {
        const sel = el('select', { className: 'text-input' });
        for (const name of assets) {
          const opt = el('option', { text: name, value: name });
          if (name === assetPath) opt.selected = true;
          sel.append(opt);
        }
        sel.addEventListener('change', () => { assetPath = sel.value; });
        node.append(el('p', { className: 'sub', text: 'Library reported by the Audio Node (page ' + (an.inventoryPage ?? 0) + ' of ' + (an.inventoryTotal ?? assets.length) + '). Browser → P4 → Node.' }));
        node.append(sel);
        if ((an.inventoryTotal || 0) > assets.length) {
          node.append(el('button', {
            className: 'btn-cancel',
            text: 'Next library page',
            disabled: !!pending || !an.online,
            onClick: () => send('AUDIO:NODE:INVENTORY:' + ((an.inventoryPage || 0) + 1))
          }));
        }
      }
      const path = el('input', {
        className: 'text-input',
        type: 'text',
        value: assetPath,
        maxlength: '63'
      });
      path.addEventListener('input', () => { assetPath = path.value.trim(); });
      node.append(el('p', { className: 'sub', text: 'Relative WAV under /showduino/audio/' }));
      node.append(path);
      const row = el('div', { className: 'filter-row' });
      const busy = !!pending || emergency === 'EMERGENCY' || !an.online;
      const playing = an.state === 'PLAYING' || an.state === 'LOOPING' || an.state === 'LOADING';
      row.append(el('button', {
        className: 'btn-primary',
        text: pending && String(pending).startsWith('AUDIO:NODE:PLAY') ? 'PLAY…' : 'PLAY',
        disabled: busy,
        onClick: () => send('AUDIO:NODE:PLAY:' + assetPath)
      }));
      row.append(el('button', {
        className: 'btn-cancel',
        text: pending && String(pending).startsWith('AUDIO:NODE:LOOP') ? 'LOOP…' : 'LOOP',
        disabled: busy,
        onClick: () => send('AUDIO:NODE:LOOP:' + assetPath)
      }));
      row.append(el('button', {
        className: 'btn-cancel',
        text: pending === 'AUDIO:NODE:STOP' ? 'STOP…' : 'STOP',
        disabled: !lastSnap.p4Online || !!pending,
        onClick: () => send('AUDIO:NODE:STOP')
      }));
      row.append(el('button', {
        className: 'btn-cancel',
        text: pending === 'AUDIO:NODE:PAUSE' ? 'PAUSE…' : 'PAUSE',
        disabled: busy || !playing,
        onClick: () => send('AUDIO:NODE:PAUSE')
      }));
      row.append(el('button', {
        className: 'btn-cancel',
        text: pending === 'AUDIO:NODE:RESUME' ? 'RESUME…' : 'RESUME',
        disabled: busy || an.state !== 'PAUSED',
        onClick: () => send('AUDIO:NODE:RESUME')
      }));
      node.append(row);
      const fadeRow = el('div', { className: 'filter-row' });
      fadeRow.append(el('button', {
        className: 'btn-cancel',
        text: 'FADE IN',
        disabled: busy,
        onClick: () => send('AUDIO:NODE:PLAY:' + assetPath + ':FADE=2000')
      }));
      fadeRow.append(el('button', {
        className: 'btn-cancel',
        text: 'FADE OUT',
        disabled: !lastSnap.p4Online || !!pending,
        onClick: () => send('AUDIO:NODE:STOP:FADE=1500')
      }));
      fadeRow.append(el('button', {
        className: 'btn-cancel',
        text: 'DUCK',
        disabled: busy,
        onClick: () => send('AUDIO:NODE:DUCK')
      }));
      fadeRow.append(el('button', {
        className: 'btn-cancel',
        text: 'UNDUCK',
        disabled: busy,
        onClick: () => send('AUDIO:NODE:UNDUCK')
      }));
      node.append(fadeRow);
      const vol = el('input', {
        type: 'range',
        min: '0',
        max: '100',
        value: String(an.volume ?? 80)
      });
      vol.addEventListener('change', () => send('AUDIO:NODE:VOLUME:' + vol.value));
      node.append(vol);
      if (lastResult && String(pending || lastResult).includes('AUDIO:NODE')) {
        node.append(el('p', { className: 'sub', text: lastResult }));
      }
      if (emergency === 'EMERGENCY') {
        node.append(el('p', { className: 'sub', text: 'Programme audio is rejected while emergency is latched. P4 local emergency WAV is independent.' }));
      }

      const si = an.soundInput || {};
      const sound = el('div', { className: 'card' });
      sound.append(el('h2', { text: 'Sound input' }));
      sound.append(el('p', { className: 'sub', text: 'Microphone sensing only. P4 remains authoritative. Events do not start a show.' }));
      sound.append(statRow('Input', si.state || (si.ready ? 'READY' : 'OFF')));
      sound.append(statRow('Level', si.level != null ? String(si.level) : '—'));
      sound.append(statRow('Peak', si.peak != null ? String(si.peak) : '—'));
      sound.append(statRow('Noise floor', si.noiseFloor != null ? String(si.noiseFloor) : '—'));
      sound.append(statRow('Threshold', si.threshold != null ? String(si.threshold) : '—'));
      sound.append(statRow('Armed', si.armed ? 'YES' : 'NO'));
      sound.append(statRow('Cooldown', si.cooldownMs != null ? String(si.cooldownMs) : '—'));
      sound.append(statRow('Last event', si.lastEvent || 'NONE'));
      sound.append(statRow('Calibration', si.calibrated ? 'YES' : 'NO'));
      const meter = el('div', { className: 'text-input', style: 'height:10px;padding:0;background:#222;' });
      const fill = el('div', { style: 'height:10px;width:' + Math.max(0, Math.min(100, si.level || 0)) + '%;background:#7c5cff;' });
      meter.append(fill);
      sound.append(meter);
      const srow = el('div', { className: 'filter-row' });
      const sbusy = !!pending || emergency === 'EMERGENCY' || !an.online;
      srow.append(el('button', {
        className: 'btn-cancel',
        text: si.enabled === false ? 'ENABLE' : 'DISABLE',
        disabled: sbusy,
        onClick: () => send(si.enabled === false ? 'AUDIO:NODE:SOUND:ENABLE' : 'AUDIO:NODE:SOUND:DISABLE')
      }));
      srow.append(el('button', {
        className: 'btn-cancel',
        text: 'CALIBRATE',
        disabled: sbusy,
        onClick: () => send('AUDIO:NODE:SOUND:CALIBRATE')
      }));
      srow.append(el('button', {
        className: 'btn-cancel',
        text: 'TH -',
        disabled: sbusy,
        onClick: () => send('AUDIO:NODE:SOUND:THRESHOLD:' + Math.max(0, (si.threshold || 70) - 5))
      }));
      srow.append(el('button', {
        className: 'btn-cancel',
        text: 'TH +',
        disabled: sbusy,
        onClick: () => send('AUDIO:NODE:SOUND:THRESHOLD:' + Math.min(100, (si.threshold || 70) + 5))
      }));
      srow.append(el('button', {
        className: 'btn-cancel',
        text: 'TEST TRIGGER',
        disabled: sbusy,
        onClick: () => send('AUDIO:NODE:SOUND:TRIGGER:TEST')
      }));
      sound.append(srow);
      const crow = el('div', { className: 'filter-row' });
      crow.append(el('button', {
        className: 'btn-cancel',
        text: 'COOLDOWN 3s',
        disabled: sbusy,
        onClick: () => send('AUDIO:NODE:SOUND:COOLDOWN:3000')
      }));
      crow.append(el('button', {
        className: 'btn-cancel',
        text: 'INHIBIT 1s',
        disabled: sbusy,
        onClick: () => send('AUDIO:NODE:SOUND:INHIBIT:1000')
      }));
      crow.append(el('button', {
        className: 'btn-cancel',
        text: 'MODE LEVEL+TRANSIENT',
        disabled: sbusy,
        onClick: () => send('AUDIO:NODE:SOUND:MODE:LEVEL_TRANSIENT')
      }));
      sound.append(crow);
      host.append(sound);
    } else {
      node.append(el('p', { className: 'sub', text: lastSnap.p4Online
        ? 'Audio Node not discovered yet. Controls appear after ESP-NOW announce.'
        : 'Unavailable while P4 is offline.' }));
    }
    host.append(node);

    const future = el('div', { className: 'card' });
    future.append(el('h2', { text: 'Specialist-node roadmap' }));
    future.append(plannedNote('Audio Node, C3 Lamp Node and C3 Pixel Node are implemented. Next: MOSFET Node. The old Relay Node concept is retired. DMX remains parked/out of scope until explicitly revisited.'));
    host.append(future);
  }

  async function pollLighting() {
    if (!lastSnap.p4Online) {
      lighting = null;
      paint();
      return;
    }
    try {
      const data = await fetchLighting();
      lighting = isP4Offline(data) ? null : data;
    } catch (_) {
      lighting = null;
    }
    paint();
  }

  const unsub = subscribeStore((snap) => {
    lastSnap = snap;
    paint();
  });
  await pollLighting();
  const timer = setInterval(pollLighting, 4000);
  return () => {
    unsub();
    clearInterval(timer);
  };
}
OutputsPage.title = 'Outputs';
