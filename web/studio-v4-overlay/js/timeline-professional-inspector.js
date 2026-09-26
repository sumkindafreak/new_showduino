/* global TimelineEditor */
(function () {
  'use strict';

  if (typeof TimelineEditor === 'undefined') {
    console.error('[Showduino Inspector] TimelineEditor is unavailable.');
    return;
  }

  const originalShowInspector = TimelineEditor.prototype._showInspector;

  function esc(value) {
    return String(value ?? '')
      .replaceAll('&', '&amp;')
      .replaceAll('<', '&lt;')
      .replaceAll('>', '&gt;')
      .replaceAll('"', '&quot;')
      .replaceAll("'", '&#039;');
  }

  function clampNumber(value, min, max, fallback) {
    const number = Number(value);
    if (!Number.isFinite(number)) return fallback;
    return Math.max(min, Math.min(max, number));
  }

  function hexFromRgb(r, g, b) {
    return `#${[r, g, b].map((value) => clampNumber(value, 0, 255, 0).toString(16).padStart(2, '0')).join('')}`;
  }

  function rgbFromHex(hex) {
    const normalised = String(hex || '#000000').replace('#', '').padEnd(6, '0').slice(0, 6);
    return [0, 2, 4].map((index) => parseInt(normalised.slice(index, index + 2), 16));
  }

  function field(id, label, value, type = 'text', attrs = '') {
    return `<div class="inspector-field"><label for="${id}">${label}</label><input id="${id}" class="inspector-control" type="${type}" value="${esc(value)}" ${attrs}></div>`;
  }

  function selectField(id, label, value, options) {
    return `<div class="inspector-field"><label for="${id}">${label}</label><select id="${id}" class="inspector-control">${options.map((option) => {
      const candidate = typeof option === 'string' ? { value: option, label: option } : option;
      return `<option value="${esc(candidate.value)}" ${String(candidate.value) === String(value) ? 'selected' : ''}>${esc(candidate.label)}</option>`;
    }).join('')}</select></div>`;
  }

  function rangeField(id, label, value, min, max, step = 1) {
    return `<div class="inspector-field"><label for="${id}"><span>${label}</span><span id="${id}-value" class="inspector-value">${esc(value)}</span></label><input id="${id}" class="inspector-range" type="range" min="${min}" max="${max}" step="${step}" value="${esc(value)}"></div>`;
  }

  function checkboxField(id, label, checked) {
    return `<label class="inspector-check"><input id="${id}" type="checkbox" ${checked ? 'checked' : ''}> ${label}</label>`;
  }

  function bindValue(id, handler, eventName = 'change') {
    const element = document.getElementById(id);
    if (!element) return;
    element.addEventListener(eventName, () => handler(element));
  }

  TimelineEditor.prototype._showInspector = function (clipId) {
    const clip = this._clips().find((candidate) => candidate.id === clipId);
    if (!clip) {
      originalShowInspector.call(this, clipId);
      return;
    }

    const track = this._tracks().find((candidate) => candidate.id === clip.trackId);
    clip.params = clip.params || {};
    clip.routing = clip.routing || { nodeId: '', output: '', universe: 1, channel: 1 };

    this._inspectorPanel.innerHTML = `
      <div class="inspector-shell">
        <div class="inspector-head">
          <h3 class="inspector-title">${esc(clip.label || clip.type)}</h3>
          <div class="inspector-subtitle">${esc(clip.type)} clip · ${esc(track ? track.name : 'Unassigned track')}</div>
        </div>

        <section class="inspector-section">
          <h4>Clip</h4>
          ${field('insp-label-pro', 'Label', clip.label || '')}
          <div class="inspector-grid">
            ${field('insp-start-pro', 'Start (ms)', clip.startMs, 'number', 'min="0" step="10"')}
            ${field('insp-duration-pro', 'Duration (ms)', clip.durationMs, 'number', 'min="100" step="10"')}
          </div>
          <div class="inspector-grid">
            ${field('insp-color-pro', 'Clip colour', clip.color || '#00ffcc', 'color')}
            ${selectField('insp-track-pro', 'Track', clip.trackId, this._tracks().map((candidate) => ({ value: candidate.id, label: candidate.name })))}
          </div>
        </section>

        <section class="inspector-section">
          <h4>Routing</h4>
          <div class="inspector-route">
            ${clip.type === 'pixel' && window.ShowduinoPixelAuthoring?.selectHtml
              ? `<div class="v4-field"><label>Pixel output</label>${window.ShowduinoPixelAuthoring.selectHtml({
                  selectedId: clip.routing.nodeId || '',
                  project: this._state?.project,
                  id: 'route-node'
                })}</div>`
              : field('route-node', 'Target node', clip.routing.nodeId || '', 'text', 'placeholder="audio-node / p4 / LED-01"')}
            <div class="inspector-grid">
              ${field('route-output', 'Output', clip.routing.output || '', 'text',
                clip.type === 'pixel' ? 'placeholder="gpio22 / Line 1"' : 'placeholder="OUT1 / Line 1"')}
              ${field('route-channel', 'Channel', clip.routing.channel || 1, 'number', 'min="1" max="512"')}
            </div>
            ${field('route-universe', 'DMX universe', clip.routing.universe || 1, 'number', 'min="1" max="63999"')}
          </div>
          <p class="inspector-note">${clip.type === 'pixel'
            ? 'Pixel cues target P4 GPIO23, standalone Pixel Nodes, or the Audio Node GPIO22 line. Audio playback stays on the same Audio Node identity.'
            : 'Routing is stored in the project and maps to connected Showduino hardware via logical device IDs.'}</p>
        </section>

        ${this._buildProfessionalTypeInspector(clip)}

        <section class="inspector-section">
          <h4>Actions</h4>
          <div class="inspector-actions">
            <button id="insp-duplicate" class="inspector-button" type="button">Duplicate</button>
            <button id="insp-split" class="inspector-button" type="button">Split at playhead</button>
            <button id="insp-delete" class="inspector-button danger" type="button">Delete</button>
          </div>
        </section>
      </div>`;

    const save = () => {
      this._refreshClipEl(clip.id);
      this._autosave();
    };

    bindValue('insp-label-pro', (element) => { clip.label = element.value.trim() || clip.type; save(); });
    bindValue('insp-start-pro', (element) => { clip.startMs = Math.max(0, Number(element.value) || 0); this._renderTracks(); this._showInspector(clip.id); });
    bindValue('insp-duration-pro', (element) => { clip.durationMs = Math.max(100, Number(element.value) || 100); this._renderTracks(); this._showInspector(clip.id); });
    bindValue('insp-color-pro', (element) => { clip.color = element.value; save(); });
    bindValue('insp-track-pro', (element) => {
      const target = this._tracks().find((candidate) => candidate.id === element.value);
      if (!target || target.locked) return;
      clip.trackId = target.id;
      this._renderTracks();
      this._selectClip(clip.id);
      this._autosave();
    });

    bindValue('route-node', (element) => { clip.routing.nodeId = element.value.trim(); this._autosave(); });
    bindValue('route-output', (element) => { clip.routing.output = element.value.trim(); this._autosave(); });
    bindValue('route-channel', (element) => { clip.routing.channel = clampNumber(element.value, 1, 512, 1); this._autosave(); });
    bindValue('route-universe', (element) => { clip.routing.universe = clampNumber(element.value, 1, 63999, 1); this._autosave(); });

    this._bindProfessionalTypeInspector(clip);

    document.getElementById('insp-duplicate')?.addEventListener('click', () => this._duplicateClip(clip.id));
    document.getElementById('insp-split')?.addEventListener('click', () => this._splitClip(clip.id));
    document.getElementById('insp-delete')?.addEventListener('click', () => this._deleteClip(clip.id));
  };

  TimelineEditor.prototype._buildProfessionalTypeInspector = function (clip) {
    const p = clip.params || {};

    if (clip.type === 'audio') {
      return `<section class="inspector-section"><h4>Audio</h4>
        ${field('audio-file', 'Audio file', p.file || '', 'text', 'placeholder="audio/scene.mp3"')}
        ${rangeField('audio-volume', 'Volume', p.volume ?? 100, 0, 100)}
        <div class="inspector-grid">${field('audio-fade-in', 'Fade in (ms)', p.fadeIn || 0, 'number', 'min="0" step="50"')}${field('audio-fade-out', 'Fade out (ms)', p.fadeOut || 0, 'number', 'min="0" step="50"')}</div>
        <div class="inspector-grid">${rangeField('audio-pan', 'Pan', p.pan ?? 0, -100, 100)}${field('audio-rate', 'Playback rate', p.rate ?? 1, 'number', 'min="0.25" max="4" step="0.05"')}</div>
        ${checkboxField('audio-loop', 'Loop audio', Boolean(p.loop))}
      </section>`;
    }

    if (clip.type === 'relay') {
      return `<section class="inspector-section"><h4>Relay</h4>
        ${selectField('relay-output', 'Relay output', p.out || 'out1', ['out1','out2','out3','out4','out5','out6','out7','out8'])}
        <div class="inspector-grid">${selectField('relay-mode', 'Mode', p.mode || (p.pulseMs > 0 ? 'pulse' : 'hold'), ['hold','pulse','toggle'])}${field('relay-pulse', 'Pulse (ms)', p.pulseMs || 0, 'number', 'min="0" step="10"')}</div>
        ${checkboxField('relay-state', 'Active state', p.state !== false)}
        ${checkboxField('relay-safe-off', 'Force OFF when stopped', p.safeOff !== false)}
      </section>`;
    }

    if (clip.type === 'lighting' || clip.type === 'pixel') {
      const colour = hexFromRgb(p.r ?? 255, p.g ?? 255, p.b ?? 255);
      return `<section class="inspector-section"><h4>${clip.type === 'pixel' ? 'Pixel effect' : 'Lighting effect'}</h4>
        ${field('light-colour', 'Colour', colour, 'color')}
        ${rangeField('light-brightness', 'Brightness', p.brightness ?? 255, 0, 255)}
        ${selectField('light-effect', 'Effect', p.effect || 'solid', ['solid','fade','pulse','flash','strobe','rainbow','chase','sparkle','flicker'])}
        <div class="inspector-grid">${field('light-speed', 'Speed', p.speed ?? 100, 'number', 'min="1" max="1000"')}${field('light-fade', 'Fade (ms)', p.fadeMs ?? 0, 'number', 'min="0" step="10"')}</div>
        ${clip.type === 'pixel' ? `<div class="inspector-grid">${field('pixel-line', 'Pixel line', p.line ?? 1, 'number', 'min="1" max="16"')}${field('pixel-count', 'Pixel count', p.count ?? 0, 'number', 'min="0"')}</div>` : ''}
        ${checkboxField('light-blackout', 'Blackout at clip end', Boolean(p.blackoutAtEnd))}
      </section>`;
    }

    if (clip.type === 'dmx') {
      const channels = p.channels || {};
      return `<section class="inspector-section"><h4>DMX</h4>
        <div class="inspector-grid">${field('dmx-universe', 'Universe', p.universe ?? 1, 'number', 'min="1" max="63999"')}${field('dmx-start', 'Start channel', p.startChannel ?? 1, 'number', 'min="1" max="512"')}</div>
        ${field('dmx-fade', 'Fade time (ms)', p.fadeMs ?? 0, 'number', 'min="0" step="10"')}
        <div class="dmx-channel-grid">${Array.from({ length: 8 }, (_, index) => {
          const channel = index + 1;
          return `<label class="dmx-channel"><span>CH ${channel}</span><input id="dmx-ch-${channel}" class="inspector-control" type="number" min="0" max="255" value="${esc(channels[channel] ?? 0)}"></label>`;
        }).join('')}</div>
        <div class="inspector-actions" style="margin-top:8px;"><button id="dmx-open-editor" class="inspector-button" type="button">Open full DMX editor</button></div>
      </section>`;
    }

    if (clip.type === 'prop') {
      return `<section class="inspector-section"><h4>Prop</h4>
        ${field('prop-target', 'Prop ID', p.target || '', 'text', 'placeholder="door_01 / creature_head"')}
        ${selectField('prop-action', 'Action', p.action || 'trigger', ['trigger','start','stop','reset','set','move','custom'])}
        <div class="inspector-grid">${field('prop-value', 'Value', p.value ?? 1, 'number', 'step="0.01"')}${field('prop-duration', 'Action duration (ms)', p.actionDurationMs ?? 0, 'number', 'min="0" step="10"')}</div>
        ${field('prop-payload', 'Payload', p.payload || '', 'text', 'placeholder="Optional command data"')}
      </section>`;
    }

    if (clip.type === 'trigger') {
      return `<section class="inspector-section"><h4>Trigger</h4>
        ${field('trigger-event', 'Event name', p.event || '', 'text', 'placeholder="scene_complete"')}
        ${field('trigger-payload', 'Payload', p.payload || '', 'text', 'placeholder="JSON or text payload"')}
        ${selectField('trigger-scope', 'Scope', p.scope || 'project', ['project','scene','track','node','global'])}
        ${checkboxField('trigger-once', 'Fire once only', Boolean(p.once))}
      </section>`;
    }

    if (clip.type === 'fx') {
      return `<section class="inspector-section"><h4>FX</h4>
        ${selectField('fx-effect', 'Effect', p.effect || 'custom', ['custom','fog','air','vibration','scent','motor','servo','solenoid'])}
        ${rangeField('fx-intensity', 'Intensity', p.intensity ?? 100, 0, 100)}
        <div class="inspector-grid">${field('fx-ramp-in', 'Ramp in (ms)', p.rampIn ?? 0, 'number', 'min="0" step="10"')}${field('fx-ramp-out', 'Ramp out (ms)', p.rampOut ?? 0, 'number', 'min="0" step="10"')}</div>
        ${checkboxField('fx-safe-stop', 'Stop during emergency', p.safeStop !== false)}
      </section>`;
    }

    return `<section class="inspector-section"><h4>Parameters</h4><p class="inspector-note">No specialised controls are available for this clip type yet.</p></section>`;
  };

  TimelineEditor.prototype._bindProfessionalTypeInspector = function (clip) {
    const p = clip.params || (clip.params = {});
    const save = () => { this._refreshClipEl(clip.id); this._autosave(); };
    const bindText = (id, key) => bindValue(id, (element) => { p[key] = element.value; save(); });
    const bindNumber = (id, key, min, max, fallback) => bindValue(id, (element) => { p[key] = clampNumber(element.value, min, max, fallback); save(); });
    const bindCheck = (id, key) => bindValue(id, (element) => { p[key] = element.checked; save(); });
    const bindRange = (id, key, min, max, fallback) => bindValue(id, (element) => {
      p[key] = clampNumber(element.value, min, max, fallback);
      const output = document.getElementById(`${id}-value`);
      if (output) output.textContent = String(p[key]);
      save();
    }, 'input');

    if (clip.type === 'audio') {
      bindText('audio-file', 'file'); bindRange('audio-volume', 'volume', 0, 100, 100); bindNumber('audio-fade-in', 'fadeIn', 0, 600000, 0); bindNumber('audio-fade-out', 'fadeOut', 0, 600000, 0); bindRange('audio-pan', 'pan', -100, 100, 0); bindNumber('audio-rate', 'rate', 0.25, 4, 1); bindCheck('audio-loop', 'loop');
    }
    if (clip.type === 'relay') {
      bindText('relay-output', 'out'); bindText('relay-mode', 'mode'); bindNumber('relay-pulse', 'pulseMs', 0, 600000, 0); bindCheck('relay-state', 'state'); bindCheck('relay-safe-off', 'safeOff');
    }
    if (clip.type === 'lighting' || clip.type === 'pixel') {
      bindValue('light-colour', (element) => { const [r, g, b] = rgbFromHex(element.value); p.r = r; p.g = g; p.b = b; clip.color = element.value; save(); });
      bindRange('light-brightness', 'brightness', 0, 255, 255); bindText('light-effect', 'effect'); bindNumber('light-speed', 'speed', 1, 1000, 100); bindNumber('light-fade', 'fadeMs', 0, 600000, 0); bindCheck('light-blackout', 'blackoutAtEnd');
      if (clip.type === 'pixel') { bindNumber('pixel-line', 'line', 1, 16, 1); bindNumber('pixel-count', 'count', 0, 100000, 0); }
    }
    if (clip.type === 'dmx') {
      p.channels = p.channels || {};
      bindNumber('dmx-universe', 'universe', 1, 63999, 1); bindNumber('dmx-start', 'startChannel', 1, 512, 1); bindNumber('dmx-fade', 'fadeMs', 0, 600000, 0);
      for (let channel = 1; channel <= 8; channel += 1) bindNumber(`dmx-ch-${channel}`, channel, 0, 255, 0);
      document.getElementById('dmx-open-editor')?.addEventListener('click', () => window.dmxEditor?.open(clip.id));
    }
    if (clip.type === 'prop') {
      bindText('prop-target', 'target'); bindText('prop-action', 'action'); bindNumber('prop-value', 'value', -1000000, 1000000, 1); bindNumber('prop-duration', 'actionDurationMs', 0, 600000, 0); bindText('prop-payload', 'payload');
    }
    if (clip.type === 'trigger') {
      bindText('trigger-event', 'event'); bindText('trigger-payload', 'payload'); bindText('trigger-scope', 'scope'); bindCheck('trigger-once', 'once');
    }
    if (clip.type === 'fx') {
      bindText('fx-effect', 'effect'); bindRange('fx-intensity', 'intensity', 0, 100, 100); bindNumber('fx-ramp-in', 'rampIn', 0, 600000, 0); bindNumber('fx-ramp-out', 'rampOut', 0, 600000, 0); bindCheck('fx-safe-stop', 'safeStop');
    }
  };
})();
