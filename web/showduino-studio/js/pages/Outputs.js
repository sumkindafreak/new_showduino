import { fetchLighting, postCommand, isP4Offline } from '../api.js';
import { subscribeStore } from '../store.js';
import { el, p4OfflineBanner, plannedNote, statRow } from '../utils.js';
import { emergencyWord } from '../status.js';

export async function OutputsPage(container) {
  container.append(el('p', {
    className: 'info-panel',
    text: 'P4 local audio is system/safety only. Programme audio is the Audio Node. Relay and MOSFET hardware remain future Nodes.'
  }));

  const host = el('div', { className: 'page-stack' });
  container.append(host);

  let lighting = null;
  let pending = null;
  let lastResult = '';
  let lastSnap = { p4Online: false, system: null };
  let assetPath = 'system-test.wav';

  async function send(cmd) {
    pending = cmd;
    paint();
    try {
      const data = await postCommand(cmd);
      lastResult = isP4Offline(data) ? 'P4 OFFLINE' : (data.replies || JSON.stringify(data));
    } catch (err) {
      lastResult = err.message;
    }
    pending = null;
    paint();
  }

  function paint() {
    host.innerHTML = '';
    if (!lastSnap.p4Online) host.append(p4OfflineBanner());
    const s = lastSnap.system;
    const emergency = emergencyWord(s);

    const pixels = el('div', { className: emergency === 'EMERGENCY' ? 'card danger-card' : 'card' });
    pixels.append(el('h2', { text: 'Emergency NeoPixel line' }));
    if (lastSnap.p4Online && lighting) {
      pixels.append(statRow('Line', lighting.emergencyPixelsReady ? 'READY' : 'FAULT'));
      pixels.append(statRow('White', lighting.emergencyPixelsWhite ? 'ACTIVE' : 'OFF'));
      pixels.append(statRow('Emergency latch', emergency === 'EMERGENCY' ? 'EMERGENCY' : 'CLEAR'));
      pixels.append(el('p', { className: 'sub', text: 'Dedicated emergency line. Not a general show-pixel engine.' }));
    } else {
      pixels.append(el('p', { className: 'sub', text: lastSnap.p4Online ? 'Waiting for P4 lighting status…' : 'Unavailable while P4 is offline.' }));
    }
    host.append(pixels);

    const showPx = el('div', { className: 'card' });
    showPx.append(el('h2', { text: 'Show Pixel Line' }));
    showPx.append(plannedNote('One local show-pixel line is planned. No engine or controls are implemented yet.'));
    host.append(showPx);

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
    future.append(el('h2', { text: 'Node outputs' }));
    future.append(plannedNote('Relay, MOSFET, LED, and DMX outputs belong to future Showduino Nodes.'));
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
