/* Showduino Studio — simple phone-first cue builder.
 *
 * The phone UI edits the same SHDO v2 project used by desktop Studio. It does
 * not create a second show format and it never becomes runtime authority.
 * The ESP32-P4 remains responsible for live timeline execution and safety.
 */
(function () {
  'use strict';

  const PHONE_QUERY = '(max-width: 760px)';
  const CSS_PATH = 'css/studio-mobile-simple.css';
  const CURRENT_TYPES = ['audio', 'relay', 'mosfet', 'pixel', 'trigger', 'fx'];

  const TYPE_META = Object.freeze({
    audio:   { icon: '♪', name: 'Sound', subtitle: 'Play an audio file', duration: 5000 },
    relay:   { icon: '↯', name: 'Relay', subtitle: 'Switch or pulse an output', duration: 500 },
    mosfet:  { icon: '▰', name: 'Powered output', subtitle: 'MOSFET / PWM output', duration: 1000 },
    pixel:   { icon: '✦', name: 'Pixels', subtitle: 'Run a segmented pixel effect', duration: 3000 },
    trigger: { icon: '◎', name: 'Trigger', subtitle: 'Fire a logical event', duration: 250 },
    fx:      { icon: '◈', name: 'Other FX', subtitle: 'Fog, air, motor, servo and more', duration: 1500 }
  });

  const PIXEL_EFFECTS = (window.ShowduinoPixelAuthoring?.EFFECTS || []).map((fx) => ({
    id: fx.id,
    name: fx.name,
    family: fx.family,
    params: fx.params
  }));

  function pixels() {
    return window.ShowduinoPixelAuthoring || null;
  }

  let root = null;
  let activeTab = 'build';
  let pickerOpen = false;
  let editorOpen = false;
  let editorDraft = null;
  let editorExistingId = null;
  let preview = { playing:false, timeMs:0, startedAt:0, offsetMs:0, raf:0 };

  function esc(value) {
    return String(value ?? '')
      .replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;')
      .replaceAll('"','&quot;').replaceAll("'",'&#039;');
  }

  function clamp(value, min, max, fallback) {
    const number = Number(value);
    return Number.isFinite(number) ? Math.max(min, Math.min(max, number)) : fallback;
  }

  function copy(value) { return JSON.parse(JSON.stringify(value)); }
  function isPhone() { return window.matchMedia(PHONE_QUERY).matches; }
  function isV4() { return document.body.classList.contains('studio-v4'); }

  function makeId() {
    if (window.crypto && typeof window.crypto.randomUUID === 'function') return window.crypto.randomUUID();
    return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, (c) => {
      const r = Math.random() * 16 | 0;
      return (c === 'x' ? r : (r & 0x3 | 0x8)).toString(16);
    });
  }

  function injectCss() {
    if (document.querySelector(`link[href="${CSS_PATH}"]`)) return;
    const link = document.createElement('link');
    link.rel = 'stylesheet';
    link.href = CSS_PATH;
    document.head.appendChild(link);
  }

  function project() {
    const state = window.state;
    if (!state) return null;
    if (!state.project && window.SHDOModel) state.project = window.SHDOModel.createProject('Untitled Show');
    if (state.project && window.SHDOModel?.migrate) state.project = window.SHDOModel.migrate(state.project);
    return state.project;
  }

  function clips() { return project()?.clips || []; }
  function tracks() { return project()?.tracks || []; }

  function sortedClips(excludeId) {
    return clips()
      .filter((clip) => !excludeId || clip.id !== excludeId)
      .slice()
      .sort((a,b) => Number(a.startMs || 0) - Number(b.startMs || 0) || Number(a.durationMs || 0) - Number(b.durationMs || 0));
  }

  function projectDuration(excludeId) {
    return sortedClips(excludeId).reduce((max, clip) => {
      return Math.max(max, Number(clip.startMs || 0) + Math.max(0, Number(clip.durationMs || 0)));
    }, 0);
  }

  function nextCueStart(excludeId) {
    const end = projectDuration(excludeId);
    return Math.ceil(end / 100) * 100;
  }

  function formatMs(ms) {
    const total = Math.max(0, Math.round(Number(ms) || 0));
    const minutes = Math.floor(total / 60000);
    const seconds = Math.floor((total % 60000) / 1000);
    const millis = total % 1000;
    return `${String(minutes).padStart(2,'0')}:${String(seconds).padStart(2,'0')}.${String(millis).padStart(3,'0')}`;
  }

  function formatTimeInput(ms) {
    const total = Math.max(0, Math.round(Number(ms) || 0));
    const minutes = Math.floor(total / 60000);
    const seconds = Math.floor((total % 60000) / 1000);
    const millis = total % 1000;
    return `${minutes}:${String(seconds).padStart(2,'0')}.${String(millis).padStart(3,'0')}`;
  }

  function parseTimeInput(value) {
    const text = String(value ?? '').trim();
    if (!text) return 0;
    if (!text.includes(':')) {
      const seconds = Number(text);
      return Number.isFinite(seconds) ? Math.max(0, Math.round(seconds * 1000)) : 0;
    }
    const parts = text.split(':').map((part) => Number(part));
    if (parts.some((part) => !Number.isFinite(part))) return 0;
    let seconds = 0;
    parts.forEach((part) => { seconds = seconds * 60 + part; });
    return Math.max(0, Math.round(seconds * 1000));
  }

  function basename(path) {
    const text = String(path || '').replaceAll('\\','/');
    return text.split('/').filter(Boolean).pop() || '';
  }

  function knownNodes(type) {
    const result = [];
    clips().forEach((clip) => {
      if (type && clip.type !== type) return;
      const id = String(clip.routing?.nodeId || '').trim();
      if (id && !result.includes(id)) result.push(id);
    });
    return result.sort((a,b) => a.localeCompare(b));
  }

  function routeText(clip) {
    const node = String(clip.routing?.nodeId || '').trim();
    return node || 'Device not selected yet';
  }

  function cueDetail(clip) {
    const p = clip.params || {};
    if (clip.type === 'audio') return basename(p.file) || 'Choose audio file';
    if (clip.type === 'relay') return `${String(p.out || 'out1').toUpperCase()} · ${String(p.mode || 'hold').toUpperCase()}${p.mode === 'pulse' && p.pulseMs ? ` · ${p.pulseMs}ms` : ''}`;
    if (clip.type === 'mosfet') return `${String(p.out || 'out1').toUpperCase()} · ${clamp(p.duty,0,100,100)}% · ${String(p.mode || 'hold').toUpperCase()}`;
    if (clip.type === 'pixel') {
      const fx = (PIXEL_EFFECTS.find((item) => item.id === (pixels()?.canonicalizeEffect?.(p.effect) || String(p.effect || '').toUpperCase())) || PIXEL_EFFECTS[1]);
      const node = String(clip.routing?.nodeId || '').trim();
      const device = pixels()?.findOutput?.(node, { project: project(), selectedId: node });
      const label = device ? pixels().optionLabel(device).split(' · ')[0] : (node || 'No device');
      return `${label} · Seg ${clamp(p.segment,0,15,0)} · ${fx?.name || p.effect || 'Solid'}`;
    }
    if (clip.type === 'trigger') return p.event || 'Choose event name';
    if (clip.type === 'fx') return `${p.effect || 'custom'} · ${clamp(p.intensity,0,100,100)}%`;
    return clip.type || 'Cue';
  }

  function projectStats() {
    return {
      cues: clips().length,
      duration: projectDuration(),
      routed: clips().filter((clip) => clip.type === 'trigger' || String(clip.routing?.nodeId || '').trim()).length
    };
  }

  function headerHtml() {
    const name = project()?.project?.name || 'Untitled Show';
    return `<header class="sm-header">
      <div class="sm-brand"><span class="sm-brand-dot"></span><div class="sm-brand-copy"><strong>SHOWDUINO STUDIO</strong><span>${esc(name)}</span></div></div>
      <button class="sm-header-action" type="button" data-sm-header-preview>${activeTab === 'preview' ? 'Back to build' : 'Preview'}</button>
    </header>`;
  }

  function navHtml() {
    const tabs = [
      ['build','＋','BUILD'],
      ['preview','▶','PREVIEW'],
      ['project','▣','PROJECT']
    ];
    return `<nav class="sm-bottom-nav" aria-label="Mobile Studio">
      ${tabs.map(([id,icon,label]) => `<button class="sm-tab ${activeTab === id ? 'active' : ''}" type="button" data-sm-tab="${id}"><b>${icon}</b><span>${label}</span></button>`).join('')}
    </nav>`;
  }

  function buildHtml() {
    const stats = projectStats();
    const rows = sortedClips().map((clip) => {
      const meta = TYPE_META[clip.type] || { icon:'•', name:clip.type || 'Cue' };
      return `<button class="sm-cue-card sm-cue-row" type="button" data-sm-edit="${esc(clip.id)}">
        <span class="sm-cue-icon">${meta.icon}</span>
        <span class="sm-cue-body sm-cue-info">
          <span class="sm-cue-top"><span class="sm-cue-time">${formatMs(clip.startMs)}</span><span class="sm-cue-name">${esc(clip.label || meta.name)}</span></span>
          <span class="sm-cue-detail">${esc(cueDetail(clip))}</span>
          <span class="sm-cue-route">${esc(routeText(clip))}</span>
        </span>
        <span class="sm-cue-chevron">›</span>
      </button>`;
    }).join('');

    return `<section class="sm-screen">
      <span class="sm-kicker">BUILD</span>
      <h1 class="sm-title">What happens next?</h1>
      <p class="sm-copy">Add one cue at a time. Showduino handles the complicated timeline underneath.</p>

      <div class="sm-toolbar">
        <button class="sm-add-cue" type="button" data-sm-picker-open><strong>＋ Add cue</strong><span>Sound, output, pixels or trigger</span></button>
        <button class="sm-template-shortcut" type="button" data-sm-exit-template title="Add 10-pixel exit sign pattern">✦</button>
      </div>

      <div class="sm-summary"><span><strong>${stats.cues}</strong> CUE${stats.cues === 1 ? '' : 'S'}</span><span><strong>${formatMs(stats.duration).slice(0,5)}</strong> SHOW LENGTH</span></div>

      <div class="sm-section">
        <div class="sm-section-head"><h2>Show sequence</h2><span>TAP A CUE TO EDIT</span></div>
        <div class="sm-sequence">${rows || '<div class="sm-empty">Your show is empty.<br>Tap <strong>Add cue</strong> and choose the first thing that should happen.</div>'}</div>
      </div>
    </section>`;
  }

  function currentPreviewClip(timeMs) {
    return sortedClips().find((clip) => {
      const start = Number(clip.startMs || 0);
      const end = start + Math.max(1, Number(clip.durationMs || 0));
      return timeMs >= start && timeMs < end;
    }) || null;
  }

  function nextPreviewClip(timeMs) {
    return sortedClips().find((clip) => Number(clip.startMs || 0) > timeMs) || null;
  }

  function previewHtml() {
    const duration = Math.max(1, projectDuration());
    const current = currentPreviewClip(preview.timeMs);
    const next = nextPreviewClip(preview.timeMs);
    return `<section class="sm-screen">
      <span class="sm-kicker">PREVIEW</span>
      <h1 class="sm-title">Watch the cues fire.</h1>
      <p class="sm-copy">Browser preview only. The P4 still owns the real show.</p>

      <div class="sm-card sm-card-pad" style="margin-top:.75rem;">
        <div class="sm-preview-time" id="sm-preview-time">${formatMs(preview.timeMs)}</div>
        <input id="sm-preview-progress" class="sm-progress" type="range" min="0" max="${duration}" value="${Math.min(duration,preview.timeMs)}" step="10" aria-label="Preview position">
        <div class="sm-btn-row" style="margin-top:.6rem;">
          <button class="sm-btn primary" type="button" data-sm-preview-play>${preview.playing ? 'Pause' : 'Play'}</button>
          <button class="sm-btn" type="button" data-sm-preview-stop>Stop</button>
        </div>
        <div class="sm-now" id="sm-preview-now"><small>NOW</small><strong>${esc(current?.label || 'Waiting for cue')}</strong><span>${esc(current ? `${cueDetail(current)} · ${routeText(current)}` : 'No cue active')}</span></div>
      </div>

      <div class="sm-section">
        <div class="sm-section-head"><h2>Next cue</h2><span>${next ? formatMs(next.startMs) : 'END'}</span></div>
        <div class="sm-card sm-card-pad"><strong style="font-size:.75rem;">${esc(next?.label || 'End of show')}</strong><div style="margin-top:.18rem;color:#7f929c;font-size:.59rem;">${esc(next ? cueDetail(next) : 'Preview stops here.')}</div></div>
      </div>
    </section>`;
  }

  function readinessChecks() {
    const list = clips();
    const checks = [];
    if (!list.length) checks.push(['warn','No cues yet']);
    const unrouted = list.filter((clip) => clip.type !== 'trigger' && !String(clip.routing?.nodeId || '').trim());
    if (unrouted.length) checks.push(['warn',`${unrouted.length} cue${unrouted.length === 1 ? '' : 's'} still need a device`]);
    const missingAudio = list.filter((clip) => clip.type === 'audio' && !String(clip.params?.file || '').trim());
    if (missingAudio.length) checks.push(['warn',`${missingAudio.length} sound cue${missingAudio.length === 1 ? '' : 's'} need a file`]);
    const pixelApi = pixels();
    if (pixelApi) {
      list.filter((clip) => clip.type === 'pixel').forEach((clip) => {
        const result = pixelApi.validatePixelClip(clip, pixelApi.getLiveSnapshot());
        result.errors.forEach((item) => checks.push(['warn', item.message]));
        result.warnings.forEach((item) => checks.push(['warn', item.message]));
      });
    }
    if (!checks.length && list.length) checks.push(['ok','Show looks ready to deploy']);
    return checks;
  }

  function projectHtml() {
    const stats = projectStats();
    const name = project()?.project?.name || 'Untitled Show';
    const checks = readinessChecks();
    return `<section class="sm-screen">
      <span class="sm-kicker">PROJECT</span>
      <h1 class="sm-title">${esc(name)}</h1>
      <p class="sm-copy">Save, move or deploy the same .shdo v2 production used by desktop Studio.</p>

      <div class="sm-card sm-card-pad" style="margin-top:.75rem;">
        <div class="sm-summary" style="margin-top:0;"><span><strong>${stats.cues}</strong> CUES</span><span><strong>${formatMs(stats.duration).slice(0,5)}</strong> LENGTH</span><span><strong>${stats.routed}/${stats.cues}</strong> ROUTED</span></div>
        <div class="sm-btn-row" style="margin-top:.7rem;">
          <button class="sm-btn primary" type="button" data-sm-save>Save</button>
          <button class="sm-btn" type="button" data-sm-rename>Rename</button>
          <button class="sm-btn" type="button" data-sm-export>Export .shdo</button>
          <button class="sm-btn" type="button" data-sm-import>Import</button>
        </div>
      </div>

      <div class="sm-section">
        <div class="sm-section-head"><h2>Quick check</h2><span>BEFORE DEPLOY</span></div>
        <div class="sm-list">${checks.map(([state,text]) => `<div class="sm-list-row"><strong>${state === 'ok' ? '✓' : '!'} ${esc(text)}</strong></div>`).join('')}</div>
      </div>

      <div class="sm-section">
        <button class="sm-btn primary" style="width:100%;" type="button" data-sm-deploy>Send / prepare for Showduino</button>
        <div id="sm-deploy-result" style="margin-top:.4rem;color:#7f929c;font-size:.59rem;"></div>
      </div>

      <div class="sm-section">
        <div class="sm-section-head"><h2>Advanced timeline</h2><span>OPTIONAL</span></div>
        <div class="sm-card sm-card-pad"><p class="sm-copy">Need tracks, dragging, snapping or exact block work? The original DAW editor is still here — it is no longer the normal way to build a show on your phone.</p><div class="sm-btn-row" style="margin-top:.6rem;"><button class="sm-btn" type="button" data-sm-advanced>Open advanced timeline</button></div></div>
      </div>

      <div class="sm-rule"><strong>Safety stays outside the editor.</strong> Emergency is owned by the P4 and still forces every pixel bright white.</div>
    </section>`;
  }

  function screenHtml() {
    if (activeTab === 'preview') return previewHtml();
    if (activeTab === 'project') return projectHtml();
    return buildHtml();
  }

  function pickerHtml() {
    return `<section class="sm-picker" ${pickerOpen ? '' : 'hidden'} aria-modal="true" role="dialog">
      <div class="sm-picker-sheet">
        <div class="sm-picker-head"><strong>What should happen?</strong><button type="button" data-sm-picker-close aria-label="Close">×</button></div>
        <div class="sm-type-grid">
          ${CURRENT_TYPES.map((type) => {
            const meta = TYPE_META[type];
            return `<button class="sm-type" type="button" data-sm-add="${type}"><b>${meta.icon}</b><strong>${meta.name}</strong><span>${meta.subtitle}</span></button>`;
          }).join('')}
        </div>
        <button class="sm-btn" style="width:100%;margin-top:.55rem;" type="button" data-sm-exit-template>10-pixel exit sign pattern</button>
      </div>
    </section>`;
  }

  function nodeFieldHtml(draft) {
    if (draft.type === 'trigger') return '';
    if (draft.type === 'pixel' && pixels()) {
      return `<div class="sm-field sm-pixel-device"><label>PIXEL OUTPUT</label>${pixels().selectHtml({
        id: 'sm-edit-node',
        selectClass: 'sm-select sm-pixel-select',
        selectedId: draft.routing?.nodeId || pixels().P4_LOGICAL_ID,
        project: project(),
        live: pixels().getLiveSnapshot()
      })}</div>`;
    }
    const nodes = knownNodes(draft.type);
    const listId = `sm-node-list-${draft.type}`;
    return `<div class="sm-field"><label>DEVICE</label><input class="sm-input" id="sm-edit-node" list="${listId}" placeholder="e.g. Audio Node 1" value="${esc(draft.routing?.nodeId || '')}"><datalist id="${listId}">${nodes.map((node) => `<option value="${esc(node)}"></option>`).join('')}</datalist></div>`;
  }

  function commonFieldsHtml(draft) {
    return `<section class="sm-form-section"><h3>When?</h3>
      <div class="sm-field"><label>START TIME · TYPE 12.5 OR 0:12.500</label><input class="sm-input" id="sm-edit-start" inputmode="decimal" value="${formatTimeInput(draft.startMs)}"></div>
      <div class="sm-btn-row"><button class="sm-btn ghost" type="button" data-sm-time="after">After previous</button><button class="sm-btn ghost" type="button" data-sm-time="1">+1 sec</button><button class="sm-btn ghost" type="button" data-sm-time="5">+5 sec</button></div>
    </section>
    <section class="sm-form-section"><h3>What?</h3>
      <div class="sm-field"><label>CUE NAME</label><input class="sm-input" id="sm-edit-label" value="${esc(draft.label || TYPE_META[draft.type]?.name || 'Cue')}"></div>
      ${nodeFieldHtml(draft)}
    </section>`;
  }

  function pixelEffectOptions(selected) {
    return PIXEL_EFFECTS.map((fx) => `<option value="${fx.id}" ${selected === fx.id ? 'selected' : ''}>${esc(fx.name)}</option>`).join('');
  }

  function typeFieldsHtml(draft) {
    const p = draft.params || {};

    if (draft.type === 'audio') {
      return `<section class="sm-form-section"><h3>Sound</h3>
        <div class="sm-field"><label>AUDIO FILE</label><input class="sm-input" id="sm-audio-file" placeholder="audio/scream.wav" value="${esc(p.file || '')}"></div>
        <div class="sm-field"><label>VOLUME · <span id="sm-audio-volume-value">${clamp(p.volume,0,100,100)}</span>%</label><input class="sm-input" id="sm-audio-volume" type="range" min="0" max="100" value="${clamp(p.volume,0,100,100)}"></div>
        <label class="sm-checkline"><input id="sm-audio-loop" type="checkbox" ${p.loop ? 'checked' : ''}> Loop until stopped</label>
      </section>`;
    }

    if (draft.type === 'relay') {
      return `<section class="sm-form-section"><h3>Relay</h3>
        <div class="sm-grid"><div class="sm-field"><label>OUTPUT</label><select class="sm-select" id="sm-relay-out">${Array.from({length:8},(_,i) => `<option value="out${i+1}" ${(p.out || 'out1') === `out${i+1}` ? 'selected' : ''}>OUT${i+1}</option>`).join('')}</select></div>
        <div class="sm-field"><label>ACTION</label><select class="sm-select" id="sm-relay-mode"><option value="hold" ${(p.mode || 'hold') === 'hold' ? 'selected' : ''}>Switch on</option><option value="pulse" ${p.mode === 'pulse' ? 'selected' : ''}>Pulse</option><option value="toggle" ${p.mode === 'toggle' ? 'selected' : ''}>Toggle</option></select></div></div>
        <div class="sm-field" id="sm-relay-pulse-wrap"><label>PULSE LENGTH (MS)</label><input class="sm-input" id="sm-relay-pulse" type="number" min="10" step="10" value="${clamp(p.pulseMs || 500,10,600000,500)}"></div>
      </section>`;
    }

    if (draft.type === 'mosfet') {
      return `<section class="sm-form-section"><h3>Powered output</h3>
        <div class="sm-grid"><div class="sm-field"><label>OUTPUT</label><select class="sm-select" id="sm-mosfet-out">${Array.from({length:8},(_,i) => `<option value="out${i+1}" ${(p.out || 'out1') === `out${i+1}` ? 'selected' : ''}>OUT${i+1}</option>`).join('')}</select></div>
        <div class="sm-field"><label>ACTION</label><select class="sm-select" id="sm-mosfet-mode"><option value="hold" ${(p.mode || 'hold') === 'hold' ? 'selected' : ''}>Hold</option><option value="pulse" ${p.mode === 'pulse' ? 'selected' : ''}>Pulse</option></select></div></div>
        <div class="sm-field"><label>POWER · <span id="sm-mosfet-duty-value">${clamp(p.duty,0,100,100)}</span>%</label><input class="sm-input" id="sm-mosfet-duty" type="range" min="0" max="100" value="${clamp(p.duty,0,100,100)}"></div>
        <div class="sm-field" id="sm-mosfet-pulse-wrap"><label>PULSE LENGTH (MS)</label><input class="sm-input" id="sm-mosfet-pulse" type="number" min="10" step="10" value="${clamp(p.pulseMs || 500,10,600000,500)}"></div>
      </section>`;
    }

    if (draft.type === 'pixel') {
      const mode = p.segmentMode || 'range';
      const effect = pixels()?.canonicalizeEffect?.(p.effect) || (p.effect || 'SOLID');
      const segmentOptions = Array.from({ length: 16 }, (_, i) => `<option value="${i}" ${clamp(p.segment,0,15,0) === i ? 'selected' : ''}>${i}</option>`).join('');
      return `<section class="sm-form-section"><h3>Pixels</h3>
        <div class="sm-grid"><div class="sm-field"><label>SEGMENT</label><select class="sm-select" id="sm-pixel-segment">${segmentOptions}</select></div>
        <div class="sm-field"><label>EFFECT</label><select class="sm-select" id="sm-pixel-effect">${pixelEffectOptions(effect)}</select></div></div>
        <div class="sm-field" data-px-param="colour"><label>COLOUR</label><input class="sm-input" id="sm-pixel-colour" type="color" value="${rgbHex(p.r,p.g,p.b)}"></div>
        <div class="sm-field" data-px-param="secondary"><label>SECONDARY COLOUR</label><input class="sm-input" id="sm-pixel-secondary" type="color" value="${esc(p.secondary || '#101820')}"></div>
        <div class="sm-field" data-px-param="brightness"><label>BRIGHTNESS · <span id="sm-pixel-brightness-value">${clamp(p.brightness,0,255,255)}</span></label><input class="sm-input" id="sm-pixel-brightness" type="range" min="0" max="255" value="${clamp(p.brightness,0,255,255)}"></div>
        <div class="sm-field" data-px-param="speed"><label>SPEED · <span id="sm-pixel-speed-value">${clamp(p.speed,1,100,50)}</span></label><input class="sm-input" id="sm-pixel-speed" type="range" min="1" max="100" value="${clamp(p.speed,1,100,50)}"></div>
        <div class="sm-field" data-px-param="intensity"><label>INTENSITY · <span id="sm-pixel-intensity-value">${clamp(p.intensity,0,100,80)}</span></label><input class="sm-input" id="sm-pixel-intensity" type="range" min="0" max="100" value="${clamp(p.intensity,0,100,80)}"></div>
        <div class="sm-field" data-px-param="randomness"><label>RANDOMNESS · <span id="sm-pixel-randomness-value">${clamp(p.randomness,0,100,70)}</span></label><input class="sm-input" id="sm-pixel-randomness" type="range" min="0" max="100" value="${clamp(p.randomness,0,100,70)}"></div>
        <label class="sm-checkline" data-px-param="reverse"><input id="sm-pixel-reverse" type="checkbox" ${p.reverse ? 'checked' : ''}> Reverse direction</label>
        <div class="sm-field"><label>DURATION (SECONDS)</label><input class="sm-input" id="sm-edit-duration" type="number" min="0.01" step="0.01" value="${(Math.max(10,Number(draft.durationMs || 1000))/1000).toFixed(2)}"></div>
        <div class="sm-pixel-shell"><div class="sm-pixel-strip" id="sm-editor-pixel-preview"></div></div>
        <details class="sm-advanced"><summary>Range on this line</summary><div class="sm-advanced-body">
          <div class="sm-grid"><div class="sm-field"><label>START PIXEL</label><input class="sm-input" id="sm-pixel-start" type="number" min="0" value="${clamp(p.startPixel,0,100000,0)}"></div><div class="sm-field"><label>LENGTH</label><input class="sm-input" id="sm-pixel-length" type="number" min="1" value="${clamp(p.length,1,100000,10)}"></div></div>
          <div class="sm-field"><label>SEGMENT MODE</label><select class="sm-select" id="sm-pixel-mode"><option value="range" ${mode === 'range' ? 'selected' : ''}>Normal range</option><option value="repeat-marker" ${mode === 'repeat-marker' ? 'selected' : ''}>Repeating marker groups</option></select></div>
          <div class="sm-grid" id="sm-pixel-group-fields" ${mode === 'repeat-marker' ? '' : 'hidden'}><div class="sm-field"><label>GROUP SIZE</label><input class="sm-input" id="sm-pixel-group" type="number" min="1" value="${clamp(p.groupSize,1,1000,10)}"></div><div class="sm-field"><label>ACTIVE PIXEL</label><input class="sm-input" id="sm-pixel-marker" type="number" min="0" value="${clamp(p.markerOffset,0,999,0)}"></div></div>
          <label class="sm-checkline"><input id="sm-pixel-blackout" type="checkbox" ${p.blackoutAtEnd ? 'checked' : ''}> Blackout this segment when cue ends</label>
        </div></details>
      </section>`;
    }

    if (draft.type === 'trigger') {
      return `<section class="sm-form-section"><h3>Trigger</h3>
        <div class="sm-field"><label>EVENT NAME</label><input class="sm-input" id="sm-trigger-event" placeholder="scene_complete" value="${esc(p.event || '')}"></div>
        <details class="sm-advanced"><summary>Trigger options</summary><div class="sm-advanced-body"><div class="sm-field"><label>PAYLOAD</label><textarea class="sm-textarea" id="sm-trigger-payload">${esc(p.payload || '')}</textarea></div><div class="sm-field"><label>SCOPE</label><select class="sm-select" id="sm-trigger-scope">${['project','scene','track','node','global'].map((v) => `<option value="${v}" ${p.scope === v ? 'selected' : ''}>${v}</option>`).join('')}</select></div><label class="sm-checkline"><input id="sm-trigger-once" type="checkbox" ${p.once ? 'checked' : ''}> Fire once only</label></div></details>
      </section>`;
    }

    if (draft.type === 'fx') {
      return `<section class="sm-form-section"><h3>Other FX</h3>
        <div class="sm-field"><label>EFFECT</label><select class="sm-select" id="sm-fx-effect">${['custom','fog','air','vibration','scent','motor','servo','solenoid'].map((v) => `<option value="${v}" ${p.effect === v ? 'selected' : ''}>${v}</option>`).join('')}</select></div>
        <div class="sm-field"><label>INTENSITY · <span id="sm-fx-intensity-value">${clamp(p.intensity,0,100,100)}</span>%</label><input class="sm-input" id="sm-fx-intensity" type="range" min="0" max="100" value="${clamp(p.intensity,0,100,100)}"></div>
      </section>`;
    }

    return '';
  }

  function advancedCommonHtml(draft) {
    return `<details class="sm-advanced"><summary>Advanced cue settings</summary><div class="sm-advanced-body">
      ${draft.type === 'pixel' ? '' : `<div class="sm-field"><label>DURATION (SECONDS)</label><input class="sm-input" id="sm-edit-duration" type="number" min="0.01" step="0.01" value="${(Math.max(10,Number(draft.durationMs || 1000))/1000).toFixed(2)}"></div>`}
      ${draft.type === 'trigger' ? '' : `<div class="sm-field"><label>ROUTING OUTPUT OVERRIDE</label><input class="sm-input" id="sm-edit-output" placeholder="Usually leave blank" value="${esc(draft.routing?.output || '')}"></div>`}
      ${draft.type === 'relay' ? `<label class="sm-checkline"><input id="sm-relay-safe" type="checkbox" ${draft.params?.safeOff !== false ? 'checked' : ''}> Force relay OFF when stopped</label>` : ''}
      ${draft.type === 'mosfet' ? `<label class="sm-checkline"><input id="sm-mosfet-safe" type="checkbox" ${draft.params?.safeOff !== false ? 'checked' : ''}> Force output OFF when stopped</label>` : ''}
      ${draft.type === 'fx' ? `<label class="sm-checkline"><input id="sm-fx-safe" type="checkbox" ${draft.params?.safeStop !== false ? 'checked' : ''}> Stop this effect during emergency</label>` : ''}
    </div></details>`;
  }

  function editorHtml() {
    if (!editorDraft) return '';
    const meta = TYPE_META[editorDraft.type] || { name:'Cue' };
    return `<section class="sm-editor" ${editorOpen ? '' : 'hidden'} aria-modal="true" role="dialog">
      <header class="sm-editor-head"><button type="button" data-sm-editor-close aria-label="Close">←</button><div class="sm-editor-title"><strong>${editorExistingId ? 'Edit' : 'Add'} ${esc(meta.name)}</strong><span>ONLY THE IMPORTANT BITS FIRST</span></div></header>
      <div class="sm-editor-body">${commonFieldsHtml(editorDraft)}${typeFieldsHtml(editorDraft)}${advancedCommonHtml(editorDraft)}</div>
      <footer class="sm-editor-foot"><div class="sm-btn-row">${editorExistingId ? '<button class="sm-btn danger" type="button" data-sm-editor-delete>Delete</button><button class="sm-btn" type="button" data-sm-editor-duplicate>Duplicate after</button>' : '<button class="sm-btn" type="button" data-sm-editor-close>Cancel</button>'}</div><button class="sm-btn primary" type="button" data-sm-editor-save>Save cue</button></footer>
    </section>`;
  }

  function rgbHex(r,g,b) {
    return `#${[r,g,b].map((value) => clamp(value,0,255,0).toString(16).padStart(2,'0')).join('')}`;
  }

  function hexRgb(hex) {
    const safe = String(hex || '#000000').replace('#','').padEnd(6,'0').slice(0,6);
    return [0,2,4].map((index) => parseInt(safe.slice(index,index+2),16));
  }

  function editorShellsHtml() {
    return `${pickerHtml()}${editorHtml()}`;
  }

  function render() {
    if (!root) return;
    root.innerHTML = `${headerHtml()}<main class="sm-main">${screenHtml()}</main>${navHtml()}${editorShellsHtml()}`;
    bindLiveEditorControls();
    if (editorOpen && editorDraft?.type === 'pixel') renderEditorPixelPreview();
    if (activeTab === 'preview') updatePreviewDom();
  }

  function defaultNodeFor(type) {
    if (type === 'pixel') return pixels()?.P4_LOGICAL_ID || 'p4';
    const nodes = knownNodes(type);
    return nodes.length === 1 ? nodes[0] : '';
  }

  function openEditor(type, clipId, exitTemplate) {
    if (!CURRENT_TYPES.includes(type)) return;
    const existing = clipId ? clips().find((clip) => clip.id === clipId) : null;
    editorExistingId = existing?.id || null;
    editorDraft = existing
      ? copy(existing)
      : window.SHDOModel.createClip('pending', type, nextCueStart(), TYPE_META[type].duration, TYPE_META[type].name);
    editorDraft.routing = editorDraft.routing || { nodeId:'', output:'' };
    editorDraft.params = editorDraft.params || window.SHDOModel._defaultParams?.(type) || {};
    if (type === 'pixel' && pixels()?.migratePixelParams) editorDraft.params = pixels().migratePixelParams(editorDraft.params);
    if (!existing && type !== 'trigger') editorDraft.routing.nodeId = defaultNodeFor(type);
    if (type === 'pixel' && pixels()?.canonicalNodeId && editorDraft.routing.nodeId) {
      editorDraft.routing.nodeId = pixels().canonicalNodeId(editorDraft.routing.nodeId) || editorDraft.routing.nodeId;
    }
    if (exitTemplate && type === 'pixel') applyExitTemplateToDraft(editorDraft);
    pickerOpen = false;
    editorOpen = true;
    render();
    if (type === 'pixel') pixels()?.refreshLiveOutputs?.().then(() => { if (editorOpen) render(); });
  }

  function closeEditor() {
    editorOpen = false;
    editorDraft = null;
    editorExistingId = null;
    render();
  }

  function applyExitTemplateToDraft(draft) {
    if (!draft || draft.type !== 'pixel') return;
    draft.label = 'Exit Sign Markers';
    draft.durationMs = Math.max(3000, Number(draft.durationMs || 0));
    draft.params = {
      ...(draft.params || {}),
      segment: Number.isFinite(Number(draft.params?.segment)) ? draft.params.segment : 0,
      segmentMode:'repeat-marker',
      segmentName:'Exit Sign Markers',
      startPixel:0,
      length:100,
      groupSize:10,
      markerOffset:0,
      r:0,
      g:220,
      b:80,
      brightness:255,
      effect:'SOLID',
      speed:50,
      blackoutAtEnd:false
    };
  }

  function applyExitTemplateInEditor() {
    if (!editorDraft || editorDraft.type !== 'pixel') return;
    const values = {
      'sm-edit-label':'Exit Sign Markers',
      'sm-pixel-name':'Exit Sign Markers',
      'sm-pixel-start':'0',
      'sm-pixel-length':'100',
      'sm-pixel-group':'10',
      'sm-pixel-marker':'0',
      'sm-pixel-colour':'#00dc50',
      'sm-pixel-brightness':'255',
      'sm-pixel-effect':'SOLID',
      'sm-pixel-mode':'repeat-marker',
      'sm-pixel-speed':'50'
    };
    Object.entries(values).forEach(([id,value]) => {
      const element = document.getElementById(id);
      if (element) element.value = value;
    });
    document.getElementById('sm-pixel-group-fields')?.removeAttribute('hidden');
    renderEditorPixelPreview();
  }

  function bindLiveEditorControls() {
    if (!editorOpen) return;
    const relayMode = document.getElementById('sm-relay-mode');
    const relayPulse = document.getElementById('sm-relay-pulse-wrap');
    const syncRelay = () => { if (relayPulse && relayMode) relayPulse.hidden = relayMode.value !== 'pulse'; };
    relayMode?.addEventListener('change',syncRelay); syncRelay();

    const mosfetMode = document.getElementById('sm-mosfet-mode');
    const mosfetPulse = document.getElementById('sm-mosfet-pulse-wrap');
    const syncMosfet = () => { if (mosfetPulse && mosfetMode) mosfetPulse.hidden = mosfetMode.value !== 'pulse'; };
    mosfetMode?.addEventListener('change',syncMosfet); syncMosfet();

    document.getElementById('sm-audio-volume')?.addEventListener('input',(event) => {
      const out = document.getElementById('sm-audio-volume-value'); if (out) out.textContent = event.target.value;
    });
    document.getElementById('sm-mosfet-duty')?.addEventListener('input',(event) => {
      const out = document.getElementById('sm-mosfet-duty-value'); if (out) out.textContent = event.target.value;
    });
    document.getElementById('sm-fx-intensity')?.addEventListener('input',(event) => {
      const out = document.getElementById('sm-fx-intensity-value'); if (out) out.textContent = event.target.value;
    });
    document.getElementById('sm-pixel-brightness')?.addEventListener('input',(event) => {
      const out = document.getElementById('sm-pixel-brightness-value'); if (out) out.textContent = event.target.value;
    });
    document.getElementById('sm-pixel-speed')?.addEventListener('input',(event) => {
      const out = document.getElementById('sm-pixel-speed-value'); if (out) out.textContent = event.target.value;
    });
    document.getElementById('sm-pixel-intensity')?.addEventListener('input',(event) => {
      const out = document.getElementById('sm-pixel-intensity-value'); if (out) out.textContent = event.target.value;
    });
    document.getElementById('sm-pixel-randomness')?.addEventListener('input',(event) => {
      const out = document.getElementById('sm-pixel-randomness-value'); if (out) out.textContent = event.target.value;
    });

    const syncPixelParams = () => {
      const effect = document.getElementById('sm-pixel-effect')?.value || 'SOLID';
      document.querySelectorAll('[data-px-param]').forEach((el) => {
        const param = el.getAttribute('data-px-param');
        const show = !pixels() || pixels().effectSupports(effect, param);
        el.hidden = !show;
      });
      renderEditorPixelPreview();
    };
    document.getElementById('sm-pixel-effect')?.addEventListener('change', syncPixelParams);
    syncPixelParams();

    ['sm-pixel-start','sm-pixel-length','sm-pixel-group','sm-pixel-marker','sm-pixel-colour','sm-pixel-mode','sm-pixel-brightness'].forEach((id) => {
      document.getElementById(id)?.addEventListener('input',() => {
        if (id === 'sm-pixel-mode') {
          const fields = document.getElementById('sm-pixel-group-fields');
          if (fields) fields.hidden = document.getElementById('sm-pixel-mode').value !== 'repeat-marker';
        }
        renderEditorPixelPreview();
      });
    });
  }

  function renderEditorPixelPreview() {
    const target = document.getElementById('sm-editor-pixel-preview');
    if (!target) return;
    const mode = document.getElementById('sm-pixel-mode')?.value || 'range';
    const start = clamp(document.getElementById('sm-pixel-start')?.value,0,100000,0);
    const length = clamp(document.getElementById('sm-pixel-length')?.value,1,100000,10);
    const group = clamp(document.getElementById('sm-pixel-group')?.value,1,1000,10);
    const marker = clamp(document.getElementById('sm-pixel-marker')?.value,0,group-1,0);
    const colour = document.getElementById('sm-pixel-colour')?.value || '#00ffc8';
    const effect = document.getElementById('sm-pixel-effect')?.value || 'solid';
    target.style.setProperty('--sm-pixel',colour);
    target.innerHTML = Array.from({length:30},(_,index) => {
      const inRange = index >= start && index < start + length;
      let on = mode === 'repeat-marker' ? inRange && ((index - start) % group) === marker : inRange;
      if (effect === 'blackout') on = false;
      return `<i class="sm-pixel${on ? (mode === 'repeat-marker' ? ' on marker' : ' on') : ''}"></i>`;
    }).join('');
  }

  function derivedOutput(type, params, nodeId) {
    if (type === 'relay' || type === 'mosfet') return String(params.out || 'out1').toUpperCase();
    if (type === 'pixel') {
      const id = pixels()?.canonicalNodeId?.(nodeId) || '';
      return pixels()?.isP4Id?.(id) ? 'P4 Show Pixel Line' : id;
    }
    return '';
  }

  function readEditor() {
    if (!editorDraft) return null;
    const draft = copy(editorDraft);
    const params = draft.params || (draft.params = {});

    draft.label = document.getElementById('sm-edit-label')?.value.trim() || TYPE_META[draft.type]?.name || 'Cue';
    draft.startMs = parseTimeInput(document.getElementById('sm-edit-start')?.value);
    draft.durationMs = Math.max(10,Math.round(Math.max(.01,Number(document.getElementById('sm-edit-duration')?.value || (draft.durationMs / 1000) || 1)) * 1000));

    let nodeId = draft.type === 'trigger' ? '' : (document.getElementById('sm-edit-node')?.value.trim() || '');
    if (draft.type === 'pixel' && pixels()?.canonicalNodeId) nodeId = pixels().canonicalNodeId(nodeId) || nodeId;

    if (draft.type === 'audio') {
      params.file = document.getElementById('sm-audio-file')?.value.trim() || '';
      params.volume = clamp(document.getElementById('sm-audio-volume')?.value,0,100,100);
      params.loop = Boolean(document.getElementById('sm-audio-loop')?.checked);
    }

    if (draft.type === 'relay') {
      params.out = document.getElementById('sm-relay-out')?.value || 'out1';
      params.mode = document.getElementById('sm-relay-mode')?.value || 'hold';
      params.pulseMs = params.mode === 'pulse' ? clamp(document.getElementById('sm-relay-pulse')?.value,10,600000,500) : 0;
      params.state = true;
      params.safeOff = document.getElementById('sm-relay-safe') ? Boolean(document.getElementById('sm-relay-safe').checked) : params.safeOff !== false;
    }

    if (draft.type === 'mosfet') {
      params.out = document.getElementById('sm-mosfet-out')?.value || 'out1';
      params.mode = document.getElementById('sm-mosfet-mode')?.value || 'hold';
      params.duty = clamp(document.getElementById('sm-mosfet-duty')?.value,0,100,100);
      params.pulseMs = params.mode === 'pulse' ? clamp(document.getElementById('sm-mosfet-pulse')?.value,10,600000,500) : 0;
      params.state = true;
      params.safeOff = document.getElementById('sm-mosfet-safe') ? Boolean(document.getElementById('sm-mosfet-safe').checked) : params.safeOff !== false;
    }

    if (draft.type === 'pixel') {
      const [r,g,b] = hexRgb(document.getElementById('sm-pixel-colour')?.value);
      params.segment = clamp(document.getElementById('sm-pixel-segment')?.value,0,15,0);
      params.segmentName = `Segment ${params.segment}`;
      params.segmentMode = document.getElementById('sm-pixel-mode')?.value || 'range';
      params.startPixel = clamp(document.getElementById('sm-pixel-start')?.value,0,100000,0);
      params.length = clamp(document.getElementById('sm-pixel-length')?.value,1,100000,10);
      params.groupSize = clamp(document.getElementById('sm-pixel-group')?.value,1,1000,10);
      params.markerOffset = clamp(document.getElementById('sm-pixel-marker')?.value,0,params.groupSize-1,0);
      params.r = r; params.g = g; params.b = b;
      params.secondary = document.getElementById('sm-pixel-secondary')?.value || params.secondary || '#101820';
      params.brightness = clamp(document.getElementById('sm-pixel-brightness')?.value,0,255,255);
      params.effect = pixels()?.canonicalizeEffect?.(document.getElementById('sm-pixel-effect')?.value) || 'SOLID';
      params.speed = clamp(document.getElementById('sm-pixel-speed')?.value,1,100,50);
      params.intensity = clamp(document.getElementById('sm-pixel-intensity')?.value,0,100,80);
      params.randomness = clamp(document.getElementById('sm-pixel-randomness')?.value,0,100,70);
      params.reverse = Boolean(document.getElementById('sm-pixel-reverse')?.checked);
      params.blackoutAtEnd = Boolean(document.getElementById('sm-pixel-blackout')?.checked);
    }

    if (draft.type === 'trigger') {
      params.event = document.getElementById('sm-trigger-event')?.value.trim() || '';
      params.payload = document.getElementById('sm-trigger-payload')?.value || '';
      params.scope = document.getElementById('sm-trigger-scope')?.value || 'project';
      params.once = Boolean(document.getElementById('sm-trigger-once')?.checked);
    }

    if (draft.type === 'fx') {
      params.effect = document.getElementById('sm-fx-effect')?.value || 'custom';
      params.intensity = clamp(document.getElementById('sm-fx-intensity')?.value,0,100,100);
      params.safeStop = document.getElementById('sm-fx-safe') ? Boolean(document.getElementById('sm-fx-safe').checked) : params.safeStop !== false;
    }

    const override = document.getElementById('sm-edit-output')?.value.trim() || '';
    draft.routing = { nodeId, output: override || derivedOutput(draft.type,params,nodeId) };
    return draft;
  }

  function findOrCreateTrack(type) {
    let track = tracks().find((item) => item.type === type && !item.locked);
    if (track) return track;
    track = window.SHDOModel.createTrack(type,TYPE_META[type]?.name || `${type} Track`,tracks().length);
    project().tracks.push(track);
    return track;
  }

  async function saveLocal() {
    try { await window.ShowduinoProjects?.saveCurrentProject?.({cloud:false}); }
    catch (error) { console.warn('[Mobile Studio] local save failed',error); }
    window.ShowduinoStudioV4?.refreshProjectStats?.();
  }

  async function saveEditorCue() {
    const draft = readEditor();
    if (!draft) return;
    const current = project();
    if (editorExistingId) {
      const index = current.clips.findIndex((clip) => clip.id === editorExistingId);
      if (index >= 0) {
        draft.id = editorExistingId;
        draft.trackId = current.clips[index].trackId;
        current.clips[index] = draft;
      }
    } else {
      const track = findOrCreateTrack(draft.type);
      draft.trackId = track.id;
      current.clips.push(draft);
    }
    current.project.updatedAt = new Date().toISOString();
    await saveLocal();
    editorOpen = false;
    editorDraft = null;
    editorExistingId = null;
    activeTab = 'build';
    render();
  }

  async function deleteEditorCue() {
    if (!editorExistingId) return;
    const clip = clips().find((item) => item.id === editorExistingId);
    if (!clip || !window.confirm(`Delete "${clip.label || clip.type}"?`)) return;
    project().clips = clips().filter((item) => item.id !== editorExistingId);
    project().project.updatedAt = new Date().toISOString();
    await saveLocal();
    editorOpen = false;
    editorDraft = null;
    editorExistingId = null;
    render();
  }

  async function duplicateEditorCue() {
    if (!editorExistingId) return;
    const source = readEditor();
    if (!source) return;
    const duplicate = copy(source);
    duplicate.id = makeId();
    duplicate.label = `${source.label || TYPE_META[source.type]?.name || 'Cue'} copy`;
    duplicate.startMs = Math.ceil((Number(source.startMs || 0) + Math.max(10,Number(source.durationMs || 0))) / 100) * 100;
    duplicate.trackId = source.trackId || findOrCreateTrack(source.type).id;
    project().clips.push(duplicate);
    project().project.updatedAt = new Date().toISOString();
    await saveLocal();
    editorOpen = false;
    editorDraft = null;
    editorExistingId = null;
    activeTab = 'build';
    render();
  }

  function adjustEditorTime(mode) {
    const input = document.getElementById('sm-edit-start');
    if (!input) return;
    if (mode === 'after') {
      input.value = formatTimeInput(nextCueStart(editorExistingId));
      return;
    }
    const addMs = Math.max(0,Number(mode) || 0) * 1000;
    input.value = formatTimeInput(parseTimeInput(input.value) + addMs);
  }

  function startPreview() {
    if (preview.playing) {
      preview.playing = false;
      if (preview.raf) cancelAnimationFrame(preview.raf);
      render();
      return;
    }
    const duration = projectDuration();
    if (duration <= 0) return;
    if (preview.timeMs >= duration) preview.timeMs = 0;
    preview.playing = true;
    preview.offsetMs = preview.timeMs;
    preview.startedAt = performance.now();
    render();
    preview.raf = requestAnimationFrame(tickPreview);
  }

  function stopPreview() {
    if (preview.raf) cancelAnimationFrame(preview.raf);
    preview = { playing:false, timeMs:0, startedAt:0, offsetMs:0, raf:0 };
    render();
  }

  function tickPreview() {
    if (!preview.playing) return;
    preview.timeMs = preview.offsetMs + (performance.now() - preview.startedAt);
    const duration = projectDuration();
    if (preview.timeMs >= duration) {
      preview.timeMs = duration;
      preview.playing = false;
      updatePreviewDom();
      return;
    }
    updatePreviewDom();
    preview.raf = requestAnimationFrame(tickPreview);
  }

  function seekPreview(ms) {
    const duration = projectDuration();
    preview.timeMs = clamp(ms,0,duration,0);
    if (preview.playing) {
      preview.offsetMs = preview.timeMs;
      preview.startedAt = performance.now();
    }
    updatePreviewDom();
  }

  function updatePreviewDom() {
    if (activeTab !== 'preview') return;
    const time = document.getElementById('sm-preview-time');
    if (time) time.textContent = formatMs(preview.timeMs);
    const progress = document.getElementById('sm-preview-progress');
    if (progress) progress.value = String(Math.floor(preview.timeMs));
    const current = currentPreviewClip(preview.timeMs);
    const now = document.getElementById('sm-preview-now');
    if (now) now.innerHTML = `<small>NOW</small><strong>${esc(current?.label || 'Waiting for cue')}</strong><span>${esc(current ? `${cueDetail(current)} · ${routeText(current)}` : 'No cue active')}</span>`;
  }

  async function saveProjectNow() {
    try { await window.ShowduinoProjects?.saveCurrentProject?.({cloud:true}); render(); }
    catch (error) { window.alert(error?.message || 'Could not save this production.'); }
  }

  function openAdvancedTimeline() {
    document.body.classList.remove('studio-mobile-native');
    document.body.classList.add('studio-mobile-advanced');
    window.ShowduinoStudioV4?.openPanel?.('timeline-editor');
  }

  async function deploy() {
    const out = document.getElementById('sm-deploy-result');
    if (out) out.textContent = 'Preparing production…';
    try {
      const result = await window.ShowduinoDeploy?.deployCurrentProject?.();
      if (out) out.textContent = result?.deployed ? 'Production sent to the local Showduino.' : 'A .shdo production file has been prepared for transfer.';
    } catch (error) {
      if (out) out.textContent = `Could not prepare production: ${error?.message || error}`;
    }
  }

  function handleClick(event) {
    const tab = event.target.closest('[data-sm-tab]');
    if (tab) {
      activeTab = tab.dataset.smTab;
      if (activeTab !== 'preview' && preview.playing) {
        preview.playing = false;
        if (preview.raf) cancelAnimationFrame(preview.raf);
      }
      render();
      return;
    }

    if (event.target.closest('[data-sm-header-preview]')) {
      activeTab = activeTab === 'preview' ? 'build' : 'preview';
      render();
      return;
    }

    if (event.target.closest('[data-sm-picker-open]')) { pickerOpen = true; render(); return; }
    if (event.target.closest('[data-sm-picker-close]')) { pickerOpen = false; render(); return; }

    const add = event.target.closest('[data-sm-add]');
    if (add) { openEditor(add.dataset.smAdd,null,false); return; }

    if (event.target.closest('[data-sm-exit-template]')) { openEditor('pixel',null,true); return; }

    const edit = event.target.closest('[data-sm-edit]');
    if (edit) {
      const clip = clips().find((item) => item.id === edit.dataset.smEdit);
      if (!clip) return;
      if (!CURRENT_TYPES.includes(clip.type)) {
        window.alert('This imported legacy cue needs the Advanced Timeline to edit it.');
        return;
      }
      openEditor(clip.type,clip.id,false);
      return;
    }

    if (event.target.closest('[data-sm-editor-close]')) { closeEditor(); return; }
    if (event.target.closest('[data-sm-editor-save]')) { saveEditorCue(); return; }
    if (event.target.closest('[data-sm-editor-delete]')) { deleteEditorCue(); return; }
    if (event.target.closest('[data-sm-editor-duplicate]')) { duplicateEditorCue(); return; }
    if (event.target.closest('[data-sm-editor-exit]')) { applyExitTemplateInEditor(); return; }

    const timeButton = event.target.closest('[data-sm-time]');
    if (timeButton) { adjustEditorTime(timeButton.dataset.smTime); return; }

    if (event.target.closest('[data-sm-preview-play]')) { startPreview(); return; }
    if (event.target.closest('[data-sm-preview-stop]')) { stopPreview(); return; }
    if (event.target.closest('[data-sm-save]')) { saveProjectNow(); return; }
    if (event.target.closest('[data-sm-rename]')) { window.ShowduinoProjects?.renameCurrentProject?.(); window.setTimeout(render,50); return; }
    if (event.target.closest('[data-sm-export]')) { window.ShowduinoProjects?.exportCurrentProject?.(); return; }
    if (event.target.closest('[data-sm-import]')) { document.getElementById('studio-import-file')?.click(); return; }
    if (event.target.closest('[data-sm-advanced]')) { openAdvancedTimeline(); return; }
    if (event.target.closest('[data-sm-deploy]')) { deploy(); }
  }

  function handleInput(event) {
    if (event.target.id === 'sm-preview-progress') seekPreview(Number(event.target.value));
  }

  function mount() {
    if (!isV4() || !isPhone()) return;
    injectCss();
    document.body.classList.add('studio-mobile-native');
    document.body.classList.remove('studio-mobile-advanced');
    if (!root) {
      root = document.createElement('div');
      root.id = 'studio-mobile-app';
      document.body.appendChild(root);
      root.addEventListener('click',handleClick);
      root.addEventListener('input',handleInput);
    }
    root.style.display = '';
    render();
  }

  function syncViewport() {
    if (!isV4()) return;
    if (isPhone()) {
      if (document.body.classList.contains('studio-mobile-advanced')) return;
      mount();
    } else {
      document.body.classList.remove('studio-mobile-native','studio-mobile-advanced');
      if (root) root.style.display = 'none';
    }
  }

  function initialise() {
    if (!isV4()) return;
    injectCss();
    syncViewport();
    const media = window.matchMedia(PHONE_QUERY);
    if (media.addEventListener) media.addEventListener('change',syncViewport);
    else media.addListener(syncViewport);
    window.addEventListener('orientationchange',() => window.setTimeout(syncViewport,120));
    window.addEventListener('resize',() => window.setTimeout(syncViewport,80));
    window.addEventListener('showduino:project-saved',() => {
      if (document.body.classList.contains('studio-mobile-native')) render();
    });
    window.addEventListener('showduino:v4-saved',() => {
      if (document.body.classList.contains('studio-mobile-native')) render();
    });
  }

  window.ShowduinoMobileStudio = Object.freeze({
    isPhone,
    openCue:(type) => openEditor(type,null,false),
    openAdvancedTimeline,
    render
  });

  document.addEventListener('DOMContentLoaded',initialise);
})();
