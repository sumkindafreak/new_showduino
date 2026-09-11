/* Showduino Studio V4 — compile and deploy supported SHDO v2 timeline data.
 *
 * Local deployment is deliberately narrow:
 *   Studio -> Communications S3 /api/studio-timeline -> P4 RAM timeline
 *
 * It never starts the show automatically. The public HTTPS website exports the
 * same canonical .shdo because browsers should not be relied on for unauthenticated
 * cleartext control of a private-network Stage. Direct upload is for the isolated
 * Showduino commissioning SoftAP/local origin only.
 */
(function () {
  'use strict';

  const STAGE_ENDPOINT = '/api/studio-timeline';
  const STAGE_CAPABILITY = 'studio-ram-timeline-upload';
  const REQUEST_TIMEOUT_MS = 7000;
  const TIMELINE_COMMAND_MAX = 63;
  const TIMELINE_MAX_CUES = 2048;
  const PIXEL_SEGMENT_SLOTS = 16;

  function notify(message, level) {
    if (typeof window.studioLog === 'function') window.studioLog(message, level || 'INFO');
    else console.log(`[Studio Deploy] ${message}`);
  }

  function clamp(value, min, max, fallback = min) {
    const number = Number(value);
    return Math.min(max, Math.max(min, Number.isFinite(number) ? number : fallback));
  }

  function cleanToken(value) {
    return String(value ?? '').trim().replace(/[\r\n]/g, ' ');
  }

  function effectName(value) {
    return cleanToken(value || 'SOLID').toUpperCase().replace(/[\s-]+/g, '_');
  }

  function hexRgb(value, fallback = [16, 24, 32]) {
    const raw = String(value || '').trim().replace(/^#/, '');
    if (/^[0-9a-f]{3}$/i.test(raw)) return raw.split('').map((ch) => parseInt(ch + ch, 16));
    if (/^[0-9a-f]{6}$/i.test(raw)) return [parseInt(raw.slice(0, 2), 16), parseInt(raw.slice(2, 4), 16), parseInt(raw.slice(4, 6), 16)];
    return fallback;
  }

  function browserAllowsDirectLocalSend() {
    return window.location.protocol === 'http:';
  }

  function localHosts() {
    const values = [];
    if (browserAllowsDirectLocalSend() && window.location.origin) values.push(window.location.origin);
    values.push('http://showduino.local', 'http://showduino-studio.local', 'http://192.168.4.1');
    return Array.from(new Set(values));
  }

  async function fetchWithTimeout(url, options, timeoutMs = REQUEST_TIMEOUT_MS) {
    const controller = new AbortController();
    const timer = window.setTimeout(() => controller.abort(), timeoutMs);
    try {
      return await fetch(url, { ...(options || {}), signal: controller.signal, cache: 'no-store' });
    } catch (error) {
      if (error?.name === 'AbortError') throw new Error(`No response from Showduino after ${timeoutMs} ms.`);
      throw error;
    } finally {
      window.clearTimeout(timer);
    }
  }

  async function probe(baseUrl) {
    try {
      const response = await fetchWithTimeout(`${baseUrl}${STAGE_ENDPOINT}`, { headers: { Accept: 'application/json' } }, 2200);
      if (!response.ok) return null;
      const payload = await response.json();
      return {
        baseUrl,
        payload,
        importCapable: payload?.capability === STAGE_CAPABILITY && payload?.state === 'ready'
      };
    } catch (_) {
      return null;
    }
  }

  async function findLocalShowduino() {
    if (!browserAllowsDirectLocalSend()) return null;
    for (const host of localHosts()) {
      const found = await probe(host);
      if (found) return found;
    }
    return null;
  }

  function addCommand(commands, errors, timeMs, command, source) {
    const clean = cleanToken(command);
    if (!clean) {
      errors.push(`${source}: compiler produced an empty command.`);
      return;
    }
    if (clean.length > TIMELINE_COMMAND_MAX) {
      errors.push(`${source}: command is ${clean.length} characters; P4 timeline limit is ${TIMELINE_COMMAND_MAX}.`);
      return;
    }
    commands.push({
      timeMs: Math.max(0, Math.round(Number(timeMs) || 0)),
      command: clean,
      source,
      sequence: commands.length
    });
  }

  function compileAudio(clip, action, device, commands, errors) {
    if (device?.binding?.route !== 'audio-node') {
      errors.push(`${clip.name}: target is not bound to the Specialist Audio Node.`);
      return;
    }
    const file = cleanToken(action.params?.file || action.params?.asset || action.value || '');
    if (!file) {
      errors.push(`${clip.name}: no audio file is selected.`);
      return;
    }
    if (file.includes('..')) {
      errors.push(`${clip.name}: audio path may not contain '..'.`);
      return;
    }

    const start = clip.startMs;
    const volume = Math.round(clamp(action.params?.volume, 0, 100, 100));
    addCommand(commands, errors, start, `AUDIO:NODE:VOLUME:${volume}`, clip.name);
    addCommand(commands, errors, start, `AUDIO:NODE:${action.params?.loop ? 'LOOP' : 'PLAY'}:${file}`, clip.name);
    if (clip.durationMs > 0) addCommand(commands, errors, start + clip.durationMs, 'AUDIO:NODE:STOP', `${clip.name} stop`);
  }

  function compilePixel(clip, action, device, segmentSlot, commands, errors, warnings) {
    const pixels = window.ShowduinoPixelAuthoring;
    const prefixBase = pixels ? pixels.commandPrefix(device) : (device?.binding?.route === 'p4-show-pixels' ? 'PIXEL:' : null);
    if (!prefixBase) {
      errors.push(`${clip.name}: pixel target is not bound to a Showduino pixel output.`);
      return;
    }
    const slot = pixels ? pixels.authoredSegment(action.params || {}, segmentSlot) : segmentSlot;
    if (slot >= PIXEL_SEGMENT_SLOTS) {
      errors.push(`${clip.name}: scene needs more than ${PIXEL_SEGMENT_SLOTS} pixel segment slots.`);
      return;
    }

    const params = action.params || {};
    const localStart = Math.max(0, Math.round(Number(params.startPixel) || 0));
    const count = Math.max(1, Math.round(Number(params.length ?? params.count) || 1));
    const bindingStart = Math.max(0, Math.round(Number(device.binding?.pixelStart) || 0));
    const declaredCount = Math.max(0, Math.round(Number(device.binding?.pixelCount) || 0));
    if (declaredCount > 0 && localStart + count > declaredCount) {
      errors.push(`${clip.name}: segment ${localStart}..${localStart + count - 1} exceeds the device's ${declaredCount}-pixel region.`);
      return;
    }

    const effect = pixels ? (pixels.canonicalizeEffect(params.effect) || '') : effectName(params.effect);
    if (!effect) {
      errors.push(`${clip.name}: unsupported pixel effect.`);
      return;
    }

    const start = bindingStart + localStart;
    const r = Math.round(clamp(params.r, 0, 255, 0));
    const g = Math.round(clamp(params.g, 0, 255, 255));
    const b = Math.round(clamp(params.b, 0, 255, 200));
    const [r2, g2, b2] = hexRgb(params.secondary);
    const brightness = Math.round(clamp(params.brightness, 0, 255, 255));
    const speed = Math.round(clamp(params.speed, 1, 100, 50));
    const intensity = Math.round(clamp(params.intensity, 0, 100, 100));
    const randomness = Math.round(clamp(params.randomness, 0, 100, 0));
    const reverse = params.reverse ? 1 : 0;
    const prefix = `${prefixBase}SEGMENT:${slot}`;

    addCommand(commands, errors, clip.startMs, `${prefix}:RANGE:${start}:${count}`, clip.name);
    addCommand(commands, errors, clip.startMs, `${prefix}:FX:${effect}`, clip.name);
    addCommand(commands, errors, clip.startMs, `${prefix}:COLOR:${r}:${g}:${b}`, clip.name);
    addCommand(commands, errors, clip.startMs, `${prefix}:COLOR2:${r2}:${g2}:${b2}`, clip.name);
    addCommand(commands, errors, clip.startMs, `${prefix}:BRIGHTNESS:${brightness}`, clip.name);
    addCommand(commands, errors, clip.startMs, `${prefix}:SPEED:${speed}`, clip.name);
    addCommand(commands, errors, clip.startMs, `${prefix}:INTENSITY:${intensity}`, clip.name);
    addCommand(commands, errors, clip.startMs, `${prefix}:RANDOMNESS:${randomness}`, clip.name);
    addCommand(commands, errors, clip.startMs, `${prefix}:REVERSE:${reverse}`, clip.name);
    if (clip.durationMs > 0) addCommand(commands, errors, clip.startMs, `${prefix}:DURATION:${clip.durationMs}`, clip.name);
    addCommand(commands, errors, clip.startMs, `${prefix}:START`, clip.name);
    if (clip.durationMs > 0 && params.blackoutAtEnd === true) {
      addCommand(commands, errors, clip.startMs + clip.durationMs, `${prefix}:STOP`, `${clip.name} stop`);
    }

    const live = pixels?.getLiveSnapshot?.();
    const check = pixels?.validatePixelTarget?.(device?.binding?.nodeId || '', live);
    (check?.warnings || []).forEach((warning) => {
      warnings.push(`${clip.name}: ${warning.message}`);
    });
  }

  function compileShdoForStage(shdo) {
    const errors = [];
    const warnings = [];
    const commands = [];
    const devices = new Map((shdo?.devices || []).map((device) => [String(device.id), device]));
    const pixelSlots = Object.create(null);

    if (shdo?.schema !== window.ShowduinoPackage?.SCHEMA_NAME) {
      return { ok: false, errors: ['Deployment requires canonical SHDO v2.'], warnings, commands, uploadCommands: [] };
    }

    const clips = (shdo.clips || []).filter((clip) => clip.enabled !== false)
      .slice().sort((a, b) => Number(a.startMs) - Number(b.startMs));

    clips.forEach((clip) => {
      const action = clip.action || {};
      const type = String(action.type || clip.type || '').toLowerCase();
      const targetDeviceId = action.targetDeviceId || clip.targetDeviceId || '';
      const device = targetDeviceId ? devices.get(String(targetDeviceId)) : null;

      if (type === 'audio') {
        if (!device) errors.push(`${clip.name}: no logical Audio Node target.`);
        else compileAudio(clip, action, device, commands, errors);
      } else if (type === 'pixel') {
        if (!device) errors.push(`${clip.name}: no logical pixel target.`);
        else {
          const key = String(device.id || device.binding?.nodeId || 'pixel');
          const authored = window.ShowduinoPixelAuthoring?.authoredSegment?.(action.params || {}, null);
          const slot = Number.isFinite(authored) && authored !== null ? authored : (pixelSlots[key] || 0);
          if (!(Number.isFinite(authored) && authored !== null)) pixelSlots[key] = slot + 1;
          compilePixel(clip, action, device, slot, commands, errors, warnings);
        }
      } else if (type === 'mosfet') {
        errors.push(`${clip.name}: MOSFET Node runtime is not implemented yet.`);
      } else if (type === 'trigger') {
        errors.push(`${clip.name}: trigger/sensor runtime is not implemented yet.`);
      } else if (type === 'relay') {
        errors.push(`${clip.name}: legacy Relay actions must be migrated to MOSFET.`);
      } else if (type === 'dmx') {
        errors.push(`${clip.name}: DMX remains parked/out of scope.`);
      } else {
        errors.push(`${clip.name}: ${type || 'unknown'} has no P4 Stage compiler yet.`);
      }
    });

    commands.sort((a, b) => a.timeMs - b.timeMs || a.sequence - b.sequence);
    if (commands.length > TIMELINE_MAX_CUES) errors.push(`Compiled show has ${commands.length} commands; P4 RAM timeline limit is ${TIMELINE_MAX_CUES}.`);
    if (!commands.length && !errors.length) errors.push('This production has no deployable P4 timeline commands.');

    const uploadCommands = ['SHOW:TL:BEGIN'];
    commands.forEach((item) => uploadCommands.push(`SHOW:TL:C:${item.timeMs}:${item.command}`));
    uploadCommands.push('SHOW:TL:END');

    return {
      ok: errors.length === 0 && commands.length > 0,
      errors,
      warnings,
      commands,
      uploadCommands,
      target: 'esp32-p4-show-engine',
      transport: 'show-timeline-ram-v1'
    };
  }

  async function postTimelineCommand(baseUrl, cmd) {
    const response = await fetchWithTimeout(`${baseUrl}${STAGE_ENDPOINT}`, {
      method: 'POST',
      headers: { Accept: 'application/json', 'Content-Type': 'application/json' },
      body: JSON.stringify({ cmd })
    });
    let payload = {};
    try { payload = await response.json(); } catch (_) {}
    if (!response.ok || payload?.ok === false) {
      throw new Error(`${cmd} → ${payload?.error || payload?.replies || `HTTP ${response.status}`}`);
    }
    return payload;
  }

  function exportForShowEngine(reason) {
    window.ShowduinoProjects.exportCurrentProject();
    notify(reason || 'Canonical SHDO v2 package prepared for the P4 Show Engine.', 'INFO');
    return { deployed: false, exported: true, reason: reason || 'export' };
  }

  async function deployCurrentProject() {
    if (!window.ShowduinoProjects?.saveCurrentProject) throw new Error('Showduino Studio is still starting.');
    const project = await window.ShowduinoProjects.saveCurrentProject({ cloud: false });
    const shdo = window.ShowduinoPackage?.toShdo ? window.ShowduinoPackage.toShdo(project) : null;
    if (!shdo) throw new Error('Canonical SHDO v2 serializer is not available.');

    if (!browserAllowsDirectLocalSend()) {
      return exportForShowEngine('Public HTTPS Studio prepared the canonical .shdo file. Connect to the isolated Showduino local Studio to deploy directly.');
    }

    const plan = compileShdoForStage(shdo);
    if (!plan.ok) throw new Error(plan.errors.join('\n'));

    notify('Looking for the Showduino Communications S3…', 'NET');
    const target = await findLocalShowduino();
    if (!target) throw new Error('No local Showduino timeline endpoint answered. Check the Communications S3/P4 firmware and local Wi-Fi connection.');
    if (!target.importCapable) throw new Error(`Showduino answered but did not advertise ${STAGE_CAPABILITY}=ready.`);

    const replies = [];
    for (const command of plan.uploadCommands) {
      replies.push(await postTimelineCommand(target.baseUrl, command));
    }

    notify(`Loaded on P4 RAM timeline: ${project.project?.name || 'Untitled Show'} (${plan.commands.length} commands)`, 'NET');
    window.dispatchEvent(new CustomEvent('showduino:project-deployed', { detail: { target, replies, project, shdo, plan } }));
    return { deployed: true, target, replies, plan };
  }

  function initialise() {
    const actions = document.querySelector('.studio-project-actions');
    if (!actions || document.getElementById('studio-deploy-button')) return;

    const button = document.createElement('button');
    button.id = 'studio-deploy-button';
    button.className = 'studio-action-btn studio-deploy-btn';
    button.type = 'button';
    button.textContent = browserAllowsDirectLocalSend() ? 'Deploy to P4' : 'Prepare for P4';
    button.title = browserAllowsDirectLocalSend()
      ? 'Compile supported V4 clips and load them into the P4 RAM timeline (does not start the show)'
      : 'Export the canonical SHDO v2 package for the local Showduino Studio';

    button.addEventListener('click', async () => {
      button.disabled = true;
      const old = button.textContent;
      button.textContent = browserAllowsDirectLocalSend() ? 'Loading P4…' : 'Preparing…';
      try {
        const result = await deployCurrentProject();
        button.textContent = result.deployed ? 'Loaded ✓' : 'Show file ready ✓';
        setTimeout(() => { button.textContent = old; }, 2200);
      } catch (error) {
        notify(`Deploy failed: ${error.message}`, 'ERR');
        window.alert(`Could not load this production onto the P4.\n\n${error.message}\n\nThe P4 was not started and existing safety authority was not changed.`);
        button.textContent = old;
      } finally {
        button.disabled = false;
      }
    });

    actions.appendChild(button);
  }

  window.ShowduinoDeploy = Object.freeze({
    findLocalShowduino,
    deployCurrentProject,
    browserAllowsDirectLocalSend,
    compileShdoForStage
  });

  document.addEventListener('DOMContentLoaded', initialise);
})();
