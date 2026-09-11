/* Showduino Studio v4 — current-system creator shell and authoring extensions.
 *
 * This file deliberately patches the existing proven Studio engine instead of
 * replacing timeline drag/drop, snapping, undo, autosave or project handling.
 * Studio remains a creator/preview tool; the ESP32-P4 remains live authority.
 */
(function () {
  'use strict';

  const reducedMotion = window.matchMedia?.('(prefers-reduced-motion: reduce)')?.matches || false;
  const animeApi = window.anime || null;

  function pixels() {
    return window.ShowduinoPixelAuthoring || null;
  }

  const PIXEL_EFFECTS = Object.freeze((pixels()?.EFFECTS || []).map((effect) => ({
    id: effect.id,
    name: effect.name,
    family: effect.family,
    params: effect.params
  })));

  const LEGACY_TYPES = new Set(['dmx', 'lighting', 'prop']);
  const CURRENT_TRACK_TYPES = new Set(['mixed', 'audio', 'relay', 'mosfet', 'pixel', 'fx', 'trigger']);

  function esc(value) {
    return String(value ?? '')
      .replaceAll('&', '&amp;')
      .replaceAll('<', '&lt;')
      .replaceAll('>', '&gt;')
      .replaceAll('"', '&quot;')
      .replaceAll("'", '&#039;');
  }

  function clamp(value, min, max, fallback) {
    const number = Number(value);
    if (!Number.isFinite(number)) return fallback;
    return Math.max(min, Math.min(max, number));
  }

  function hexFromRgb(r, g, b) {
    return `#${[r, g, b].map((value) => clamp(value, 0, 255, 0).toString(16).padStart(2, '0')).join('')}`;
  }

  function rgbFromHex(hex) {
    const safe = String(hex || '#000000').replace('#', '').padEnd(6, '0').slice(0, 6);
    return [0, 2, 4].map((index) => parseInt(safe.slice(index, index + 2), 16));
  }

  function effectById(id) {
    if (pixels()?.effectById) return pixels().effectById(id);
    return PIXEL_EFFECTS.find((effect) => effect.id === id) || PIXEL_EFFECTS[1] || PIXEL_EFFECTS[0];
  }

  function ensurePixelParams(params) {
    if (pixels()?.migratePixelParams) return pixels().migratePixelParams(params);
    const p = params || {};
    p.segment = clamp(p.segment, 0, 15, 0);
    p.segmentMode = p.segmentMode || 'range';
    p.segmentName = p.segmentName || `Segment ${p.segment}`;
    p.startPixel = clamp(p.startPixel ?? 0, 0, 100000, 0);
    p.length = clamp(p.length ?? p.count ?? 10, 1, 100000, 10);
    p.groupSize = clamp(p.groupSize ?? 10, 1, 1000, 10);
    p.markerOffset = clamp(p.markerOffset ?? 0, 0, Math.max(0, p.groupSize - 1), 0);
    p.effect = effectById(p.effect || 'SOLID').id;
    p.r = clamp(p.r ?? 0, 0, 255, 0);
    p.g = clamp(p.g ?? 255, 0, 255, 255);
    p.b = clamp(p.b ?? 200, 0, 255, 200);
    p.secondary = p.secondary || '#101820';
    p.brightness = clamp(p.brightness ?? 255, 0, 255, 255);
    p.speed = clamp(p.speed ?? 50, 1, 100, 50);
    p.intensity = clamp(p.intensity ?? 80, 0, 100, 80);
    p.randomness = clamp(p.randomness ?? 70, 0, 100, 70);
    p.reverse = Boolean(p.reverse);
    p.fadeMs = clamp(p.fadeMs ?? 0, 0, 600000, 0);
    p.blackoutAtEnd = Boolean(p.blackoutAtEnd);
    return p;
  }

  function ensureMosfetParams(params) {
    const p = params || {};
    p.out = p.out || 'out1';
    p.mode = p.mode || 'hold';
    p.state = p.state !== false;
    p.duty = clamp(p.duty ?? 100, 0, 100, 100);
    p.pulseMs = clamp(p.pulseMs ?? 0, 0, 600000, 0);
    p.safeOff = p.safeOff !== false;
    return p;
  }

  function currentProject() {
    const state = window.state;
    if (!state) return null;
    if (!state.project && window.SHDOModel) state.project = window.SHDOModel.createProject('Untitled Show');
    if (state.project && window.SHDOModel?.migrate) window.SHDOModel.migrate(state.project);
    return state.project;
  }

  function projectStats() {
    const project = currentProject();
    const tracks = project?.tracks || [];
    const clips = project?.clips || [];
    const pixelClips = clips.filter((clip) => clip.type === 'pixel');
    const audioClips = clips.filter((clip) => clip.type === 'audio');
    const routed = clips.filter((clip) => String(clip.routing?.nodeId || '').trim()).length;
    return {
      tracks: tracks.length,
      clips: clips.length,
      pixels: pixelClips.length,
      audio: audioClips.length,
      routed,
      legacy: clips.filter((clip) => LEGACY_TYPES.has(clip.type)).length
    };
  }

  function openPanel(panel) {
    const item = document.querySelector(`.sidebar-nav li[data-panel="${panel}"]`);
    if (item) item.click();
    else window.loadPanel?.(panel);
  }

  function dashboardHtml() {
    const stats = projectStats();
    const project = currentProject();
    const name = project?.project?.name || 'Untitled Show';
    return `
      <div class="v4-panel v4-dashboard">
        <div class="v4-panel-head">
          <div><span class="v4-eyebrow">SHOWDUINO STUDIO</span><h1>${esc(name)}</h1><p>Author the production here. Preview it here. Save and deploy it from here. The browser never becomes the live show engine.</p></div>
          <div class="v4-button-row"><button class="v4-btn primary" type="button" onclick="ShowduinoStudioV4.openPanel('timeline-editor')">Open Timeline</button><button class="v4-btn" type="button" onclick="ShowduinoStudioV4.openPanel('playback')">Pixel FX Lab</button></div>
        </div>

        <div class="v4-grid four" id="v4-project-stats">
          <article class="v4-card"><span class="v4-eyebrow">TRACKS</span><strong class="v4-stat" data-v4-stat="tracks">${stats.tracks}</strong><div class="v4-meta">AUTHORING LANES</div></article>
          <article class="v4-card"><span class="v4-eyebrow">CUES</span><strong class="v4-stat" data-v4-stat="clips">${stats.clips}</strong><div class="v4-meta">TIMELINE CLIPS</div></article>
          <article class="v4-card"><span class="v4-eyebrow">PIXEL FX</span><strong class="v4-stat" data-v4-stat="pixels">${stats.pixels}</strong><div class="v4-meta">SEGMENTED CLIPS</div></article>
          <article class="v4-card"><span class="v4-eyebrow">ROUTED</span><strong class="v4-stat" data-v4-stat="routed">${stats.routed}</strong><div class="v4-meta">DEVICE-ID TARGETS</div></article>
        </div>

        <div class="v4-architecture">
          <article class="v4-role"><small>01 / OPERATOR</small><h3>Director</h3><p>ESP32-S3 touchscreen requests actions and displays state. It does not keep a running show alive.</p></article>
          <article class="v4-role"><small>02 / TRANSPORT</small><h3>S3 Comms</h3><p>ESP-NOW ↔ UART bridge. Messages move through it; show decisions do not live there.</p></article>
          <article class="v4-role"><small>03 / AUTHORITY</small><h3>ESP32-P4</h3><p>Owns runtime state, safety, timeline execution, cue dispatch and emergency behaviour.</p></article>
          <article class="v4-role"><small>04 / ACTION</small><h3>Specialist Nodes</h3><p>Relay, MOSFET, pixel and audio nodes perform commanded work and report results.</p></article>
        </div>

        <div class="v4-grid three" style="margin-top:.8rem;">
          <article class="v4-card"><h3>Timeline</h3><p>DAW-style authoring with snapping, markers, drag/drop presets, clip inspection and project autosave.</p><div class="v4-button-row"><button class="v4-btn" onclick="ShowduinoStudioV4.openPanel('timeline-editor')">Edit production</button></div></article>
          <article class="v4-card"><h3>Pixel FX Lab</h3><p>Preview a library of ${PIXEL_EFFECTS.length} effects, define arbitrary segments and create repeating 10-pixel exit-sign patterns.</p><div class="v4-button-row"><button class="v4-btn" onclick="ShowduinoStudioV4.openPanel('playback')">Design pixel FX</button></div></article>
          <article class="v4-card"><h3>Project Check</h3><p>Find unrouted cues, missing audio files, invalid pixel segments and legacy clip types before deployment.</p><div class="v4-button-row"><button class="v4-btn" onclick="ShowduinoStudioV4.openPanel('diagnostics')">Check show</button></div></article>
        </div>

        <div class="v4-rule" style="margin-top:.8rem;"><strong>System rule:</strong> emergency is not a timeline effect. The P4 emergency state interrupts normal playback and commands all pixels bright white. Clearing the initiating condition does not auto-resume the show.</div>
      </div>`;
  }

  function pixelLabHtml() {
    const options = PIXEL_EFFECTS.map((effect) => `<button class="pixel-fx-option ${effect.id === 'LIGHTNING' ? 'active' : ''}" type="button" data-pixel-effect="${effect.id}" onclick="ShowduinoStudioV4.selectPixelEffect('${effect.id}')"><strong>${esc(effect.name)}</strong><span>${esc(effect.family)}</span></button>`).join('');
    return `
      <div class="v4-panel pixel-lab-panel">
        <div class="v4-panel-head"><div><span class="v4-eyebrow">PIXEL FX LAB</span><h1>Design by segment.</h1><p>Build an effect for exactly the pixels you want, preview it in the browser, then drop the resulting clip onto the timeline.</p></div><div class="v4-button-row"><button class="v4-btn" onclick="ShowduinoStudioV4.applyExitSignTemplate()">10-pixel Exit Sign</button><button class="v4-btn primary" onclick="ShowduinoStudioV4.insertPixelLabClip()">Add to Timeline</button></div></div>

        <div class="pixel-lab-layout">
          <section class="v4-card pixel-lab-controls">
            <div class="v4-inspector-grid">
              ${fieldHtml('pixel-lab-line','Pixel line',1,'number','min="1" max="32"')}
              ${fieldHtml('pixel-lab-segment-name','Segment name','Segment A','text')}
            </div>
            ${selectHtml('pixel-lab-mode','Segment mode','range',[{value:'range',label:'Range: start + length'},{value:'repeat-marker',label:'Repeat marker inside groups'}])}
            <div class="v4-inspector-grid" data-range-fields>
              ${fieldHtml('pixel-lab-start','Start pixel',0,'number','min="0"')}
              ${fieldHtml('pixel-lab-length','Segment length',10,'number','min="1"')}
            </div>
            <div class="v4-inspector-grid" data-marker-fields style="display:none;">
              ${fieldHtml('pixel-lab-group-size','Group size',10,'number','min="1"')}
              ${fieldHtml('pixel-lab-marker-offset','Active pixel in group',0,'number','min="0"')}
            </div>
            <div class="v4-inspector-grid">
              ${fieldHtml('pixel-lab-colour','Primary colour','#00ffc8','color')}
              ${fieldHtml('pixel-lab-secondary','Secondary colour','#101820','color')}
            </div>
            <div class="v4-inspector-grid">
              ${fieldHtml('pixel-lab-brightness','Brightness',255,'number','min="0" max="255"')}
              ${fieldHtml('pixel-lab-speed','Speed',120,'number','min="1" max="1000"')}
            </div>
            <label class="v4-check"><input id="pixel-lab-blackout" type="checkbox"> Blackout segment when clip ends</label>
            <input id="pixel-lab-effect" type="hidden" value="LIGHTNING">
            <div class="v4-rule" style="margin-top:.65rem;">Segments are logical authoring ranges. Different clips on the same physical line can use different effects and ranges.</div>
          </section>

          <section class="pixel-lab-stage">
            <div class="pixel-lab-big-strip">
              <div class="pixel-preview-label"><span id="pixel-lab-preview-label">LINE 1 · SEGMENT A · LIGHTNING</span><span id="pixel-lab-preview-range">PX 0–9</span></div>
              <div id="pixel-lab-preview" class="v4-pixel-strip" aria-label="Pixel effect preview"></div>
            </div>
            <div class="v4-card"><div class="v4-panel-head" style="margin:0 0 .6rem;align-items:center;"><div><span class="v4-eyebrow">FX LIBRARY</span><p>${PIXEL_EFFECTS.length} reusable pixel effects. The effect lives in the clip; routing decides which segment receives it.</p></div><input id="pixel-lab-search" type="search" placeholder="Search FX…" oninput="ShowduinoStudioV4.filterPixelEffects(this.value)" style="max-width:180px;padding:.48rem .55rem;border:1px solid #344752;border-radius:6px;background:#080d11;color:#e7eef1;"></div><div class="pixel-fx-library" id="pixel-fx-library">${options}</div></div>
          </section>
        </div>

        <section class="v4-card emergency-preview" style="margin-top:.8rem;">
          <div class="v4-panel-head" style="margin:0 0 .6rem;"><div><span class="v4-eyebrow">LOCKED SYSTEM OVERRIDE</span><h2 style="font-size:1.2rem;">Emergency = every pixel bright white.</h2><p>This is deliberately not selectable as a show effect. It belongs to the P4 safety runtime and overrides every authored segment.</p></div></div>
          <div id="pixel-emergency-preview" class="v4-pixel-strip" aria-label="All pixels bright white in emergency"></div>
        </section>
      </div>`;
  }

  function deployHtml() {
    return `
      <div class="v4-panel deploy-panel">
        <div class="v4-panel-head"><div><span class="v4-eyebrow">DEPLOYMENT</span><h1>Creator → Show Engine.</h1><p>Deployment moves a finished production toward the ESP32-P4. It does not turn this browser into a live controller.</p></div></div>
        <div class="v4-grid three">
          <article class="v4-card"><span class="v4-eyebrow">01 / SAVE</span><h3>Freeze the project snapshot</h3><p>Save the current `.shdo` data locally before transfer. Cloud backup is optional and never part of live runtime.</p></article>
          <article class="v4-card"><span class="v4-eyebrow">02 / CHECK</span><h3>Validate routing + assets</h3><p>Make sure node IDs, audio references, pixel segments and output safety settings are complete.</p></article>
          <article class="v4-card"><span class="v4-eyebrow">03 / INSTALL</span><h3>Install on the P4</h3><p>When a compatible local Show Engine import API is available, Studio can send directly. Until then Studio exports the same `.shdo` snapshot.</p></article>
        </div>
        <div class="v4-rule warning" style="margin-top:.8rem;"><strong>Current implementation reality:</strong> direct browser-to-P4 install is capability-gated. If the local import API is not available, Studio safely falls back to exporting the production file rather than pretending deployment succeeded.</div>
        <div class="v4-grid two" style="margin-top:.8rem;">
          <article class="v4-card"><h3>Production package</h3><div id="v4-deploy-summary" class="project-check-list"></div><div class="v4-button-row"><button class="v4-btn primary" onclick="ShowduinoStudioV4.prepareDeploy()">Prepare / Send Show</button><button class="v4-btn" onclick="ShowduinoProjects.exportCurrentProject()">Export .shdo</button></div></article>
          <article class="v4-card"><h3>Live path</h3><div class="node-target-list"><div class="node-target"><code>DIRECTOR</code><span>ESP-NOW operator requests</span><strong>→</strong></div><div class="node-target"><code>S3 COMMS</code><span>ESP-NOW ↔ UART transport</span><strong>→</strong></div><div class="node-target"><code>P4</code><span>Runtime + safety + timeline</span><strong>→</strong></div><div class="node-target"><code>NODES</code><span>Relay · MOSFET · Pixel · Audio</span><strong>✓</strong></div></div></article>
        </div>
      </div>`;
  }

  function nodesHtml() {
    return `
      <div class="v4-panel nodes-panel">
        <div class="v4-panel-head"><div><span class="v4-eyebrow">NODE MAP</span><h1>Route by role and device ID.</h1><p>Studio stores logical targets in production data. Physical discovery and completion reporting can fill this view when a Showduino system is connected.</p></div><div class="v4-button-row"><button class="v4-btn" onclick="ShowduinoStudioV4.refreshNodeMap()">Refresh authored targets</button></div></div>
        <div class="node-role-grid">
          <article class="node-role-card core"><div class="node-status"><span>DIRECTOR</span><span>OPERATOR</span></div><h3>ESP32-S3 Director</h3><p>Requests commands and displays state. Not a timeline or safety authority.</p></article>
          <article class="node-role-card core"><div class="node-status"><span>COMMS</span><span>BRIDGE</span></div><h3>S3 Comms Controller</h3><p>Transports ESP-NOW traffic to/from the P4 over UART.</p></article>
          <article class="node-role-card core"><div class="node-status"><span>P4</span><span>AUTHORITY</span></div><h3>Show Engine</h3><p>Owns timeline, cue state, storage, emergency and completion-driven runtime.</p></article>
          <article class="node-role-card"><div class="node-status"><span>NODES</span><span>ACTION</span></div><h3>Specialist Outputs</h3><p>Relay, MOSFET, pixel and audio nodes perform commanded work.</p></article>
        </div>
        <section class="v4-card" style="margin-top:.8rem;"><span class="v4-eyebrow">AUTHORED TARGETS</span><div id="v4-node-targets" class="node-target-list"></div></section>
        <div class="v4-rule" style="margin-top:.8rem;"><strong>Addressing rule:</strong> author against logical device IDs, not hard-coded ESP-NOW MAC addresses. The runtime owns the mapping between production targets and physical nodes.</div>
      </div>`;
  }

  function diagnosticsHtml() {
    return `
      <div class="v4-panel diagnostics-panel">
        <div class="v4-panel-head"><div><span class="v4-eyebrow">PROJECT CHECK</span><h1>Catch problems before the P4 sees them.</h1><p>This is an authoring validation pass. Hardware commissioning remains a separate P4/node diagnostic job.</p></div><div class="v4-button-row"><button class="v4-btn primary" onclick="ShowduinoStudioV4.refreshDiagnostics()">Run check</button></div></div>
        <div class="v4-grid four" id="v4-diagnostic-stats"></div>
        <section class="v4-card" style="margin-top:.8rem;"><div id="v4-project-checks" class="project-check-list"></div></section>
      </div>`;
  }

  function helpHtml() {
    return `
      <div class="v4-panel help-panel">
        <div class="v4-panel-head"><div><span class="v4-eyebrow">STUDIO QUICK GUIDE</span><h1>Create clearly. Route deliberately.</h1><p>Studio is the authoring environment for the current Director → Comms → P4 → Nodes architecture.</p></div></div>
        <div class="v4-grid three">
          <article class="v4-card"><h3>1. Add tracks</h3><p>Use Audio, Relay, MOSFET, Pixel, FX or Trigger tracks. Mixed lanes can hold any current clip type.</p></article>
          <article class="v4-card"><h3>2. Drop cues</h3><p>Drag presets from the library or double-click a lane. Select a cue to edit routing and type-specific parameters.</p></article>
          <article class="v4-card"><h3>3. Route targets</h3><p>Give output clips a logical node ID and output. Pixel clips also define the exact physical segment they own.</p></article>
          <article class="v4-card"><h3>4. Preview</h3><p>Browser playback is a design preview only. It does not represent the P4's authoritative live clock.</p></article>
          <article class="v4-card"><h3>5. Validate</h3><p>Run Project Check before deployment to find missing routes, files, invalid segments and legacy clips.</p></article>
          <article class="v4-card"><h3>6. Deploy</h3><p>Install on a compatible local Show Engine or export the exact same `.shdo` production package.</p></article>
        </div>
        <div class="v4-rule danger" style="margin-top:.8rem;"><strong>Emergency:</strong> Studio does not author or clear emergency state. On the live P4, emergency interrupts the show, drives all pixels bright white, latches, and requires deliberate recovery.</div>
        <div class="v4-rule warning" style="margin-top:.6rem;"><strong>DMX:</strong> deliberately outside the current implementation scope. Legacy DMX clips are preserved when importing old projects, but Studio v4 does not offer new DMX authoring.</div>
      </div>`;
  }

  function fieldHtml(id, label, value, type, attrs) {
    return `<div class="v4-field"><label for="${id}">${esc(label)}</label><input id="${id}" type="${type || 'text'}" value="${esc(value)}" ${attrs || ''} oninput="ShowduinoStudioV4.onPixelLabInput?.()"></div>`;
  }

  function selectHtml(id, label, value, options) {
    return `<div class="v4-field"><label for="${id}">${esc(label)}</label><select id="${id}" onchange="ShowduinoStudioV4.onPixelLabInput?.()">${options.map((option) => `<option value="${esc(option.value)}" ${String(option.value) === String(value) ? 'selected' : ''}>${esc(option.label)}</option>`).join('')}</select></div>`;
  }

  function inspectorField(id, label, value, type, attrs) {
    return `<div class="v4-field"><label for="${id}">${esc(label)}</label><input id="${id}" type="${type || 'text'}" value="${esc(value)}" ${attrs || ''}></div>`;
  }

  function inspectorSelect(id, label, value, options) {
    return `<div class="v4-field"><label for="${id}">${esc(label)}</label><select id="${id}">${options.map((option) => { const item = typeof option === 'string' ? { value: option, label: option } : option; return `<option value="${esc(item.value)}" ${String(item.value) === String(value) ? 'selected' : ''}>${esc(item.label)}</option>`; }).join('')}</select></div>`;
  }

  function buildPixelInspector(clip) {
    const p = ensurePixelParams(clip.params || (clip.params = {}));
    clip.routing = clip.routing || { nodeId: '', output: '' };
    if (!clip.routing.nodeId) clip.routing.nodeId = pixels()?.P4_LOGICAL_ID || 'p4';
    const primary = hexFromRgb(p.r, p.g, p.b);
    const deviceHtml = pixels() ? pixels().selectHtml({
      id: 'v4-pixel-device',
      selectClass: 'v4-pixel-select',
      selectedId: clip.routing.nodeId,
      project: currentProject(),
      live: pixels().getLiveSnapshot()
    }) : inspectorField('v4-pixel-device','Pixel output', clip.routing.nodeId || 'p4','text');
    const segmentOptions = Array.from({ length: 16 }, (_, i) => ({ value: String(i), label: String(i) }));
    return `
      <section class="v4-inspector-section"><h4>Pixel output</h4>
        <div class="v4-field sd-pixel-device">${deviceHtml}</div>
        <div class="v4-inspector-grid">${inspectorSelect('v4-pixel-segment','Segment', String(p.segment ?? 0), segmentOptions)}${inspectorSelect('v4-pixel-effect','Effect', p.effect, PIXEL_EFFECTS.map((effect) => ({ value: effect.id, label: `${effect.name} · ${effect.family}` })))}</div>
      </section>
      <section class="v4-inspector-section"><h4>Effect parameters</h4>
        <div class="v4-inspector-grid" data-px-param="colour">${inspectorField('v4-pixel-colour','Colour',primary,'color')}<div data-px-param="secondary">${inspectorField('v4-pixel-secondary','Secondary',p.secondary,'color')}</div></div>
        <div class="v4-inspector-grid">${inspectorField('v4-pixel-brightness','Brightness',p.brightness,'number','min="0" max="255"')}${inspectorField('v4-pixel-speed','Speed',p.speed,'number','min="1" max="100"')}</div>
        <div class="v4-inspector-grid" data-px-param="intensity">${inspectorField('v4-pixel-intensity','Intensity',p.intensity ?? 80,'number','min="0" max="100"')}<div data-px-param="randomness">${inspectorField('v4-pixel-randomness','Randomness',p.randomness ?? 70,'number','min="0" max="100"')}</div></div>
        <label class="v4-check" data-px-param="reverse"><input id="v4-pixel-reverse" type="checkbox" ${p.reverse ? 'checked' : ''}> Reverse direction</label>
        <label class="v4-check"><input id="v4-pixel-blackout" type="checkbox" ${p.blackoutAtEnd ? 'checked' : ''}> Blackout segment at clip end</label>
      </section>
      <section class="v4-inspector-section"><h4>Range on this line</h4>
        ${inspectorSelect('v4-pixel-mode','Segment mode',p.segmentMode,[{value:'range',label:'Range: start + length'},{value:'repeat-marker',label:'Repeat marker groups'}])}
        <div class="v4-inspector-grid" id="v4-pixel-range-fields">${inspectorField('v4-pixel-start','Start pixel',p.startPixel,'number','min="0"')}${inspectorField('v4-pixel-length','Length',p.length,'number','min="1"')}</div>
        <div class="v4-inspector-grid" id="v4-pixel-marker-fields" ${p.segmentMode === 'repeat-marker' ? '' : 'style="display:none;"'}>${inspectorField('v4-pixel-group','Group size',p.groupSize,'number','min="1"')}${inspectorField('v4-pixel-marker','Active offset',p.markerOffset,'number','min="0"')}</div>
        <div class="pixel-preview-shell"><div class="pixel-preview-label"><span>AUTHORING PREVIEW</span><span>SEG ${esc(p.segment ?? 0)}</span></div><div id="v4-inspector-pixel-preview" class="v4-pixel-strip"></div></div>
      </section>
      <div class="v4-rule danger"><strong>Emergency is locked:</strong> this clip can never override the P4 emergency rule. Emergency drives every pixel on every line bright white.</div>`;
  }

  function buildMosfetInspector(clip) {
    const p = ensureMosfetParams(clip.params || (clip.params = {}));
    return `
      <section class="v4-inspector-section"><h4>MOSFET output</h4>
        <div class="v4-inspector-grid">${inspectorField('v4-mosfet-output','Output',p.out,'text','placeholder="out1"')}${inspectorSelect('v4-mosfet-mode','Mode',p.mode,['hold','pulse','pwm'])}</div>
        <div class="v4-inspector-grid">${inspectorField('v4-mosfet-duty','Duty / level %',p.duty,'number','min="0" max="100"')}${inspectorField('v4-mosfet-pulse','Pulse (ms)',p.pulseMs,'number','min="0"')}</div>
        <label class="v4-check"><input id="v4-mosfet-state" type="checkbox" ${p.state ? 'checked' : ''}> Active state</label>
        <label class="v4-check"><input id="v4-mosfet-safe" type="checkbox" ${p.safeOff ? 'checked' : ''}> Force OFF when stopped / safety reset</label>
      </section>`;
  }

  function renderPixelStrip(root, params, emergency) {
    if (!root) return;
    const p = ensurePixelParams({ ...(params || {}) });
    root.innerHTML = '';
    const count = root.closest('.pixel-lab-big-strip') ? 50 : 40;
    const windowStart = p.segmentMode === 'range' ? Math.floor(p.startPixel / count) * count : 0;
    const primary = hexFromRgb(p.r, p.g, p.b);
    const activePixels = [];

    for (let index = 0; index < count; index += 1) {
      const globalIndex = windowStart + index;
      const pixel = document.createElement('span');
      pixel.className = 'v4-pixel';
      pixel.style.setProperty('--pixel-colour', primary);

      let active = false;
      if (emergency) {
        pixel.classList.add('emergency');
        active = true;
      } else if (p.segmentMode === 'repeat-marker') {
        active = globalIndex % p.groupSize === Math.min(p.markerOffset, p.groupSize - 1);
        if (active) pixel.classList.add('marker');
      } else {
        active = globalIndex >= p.startPixel && globalIndex < p.startPixel + p.length;
        if (active && p.effect !== 'blackout') pixel.classList.add('active');
      }
      if (active) activePixels.push(pixel);
      root.appendChild(pixel);
    }

    if (emergency || reducedMotion || !animeApi?.animate || !activePixels.length) return;
    animateEffect(activePixels, p.effect, p.speed);
  }

  function animateEffect(pixels, effect, speed) {
    const duration = Math.max(140, 1150 - clamp(speed, 1, 1000, 120));
    const stagger = animeApi.stagger || ((value) => value);
    const motion = new Set(['chase','comet','scanner','meteor','wipe','theatre','wave','ripple']);
    const organic = new Set(['flicker','fire','ember','sparkle','twinkle','uv-flicker','confetti']);
    const impact = new Set(['flash','strobe','lightning','police']);

    if (motion.has(effect)) {
      animeApi.animate(pixels, { opacity: [.2,1,.35], scale: [.82,1.15,.9], delay: stagger(Math.max(15, duration / Math.max(2,pixels.length))), duration, loop: true, ease: 'inOut(2)' });
    } else if (organic.has(effect)) {
      animeApi.animate(pixels, { opacity: [.35,1,.55], scale: [.92,1.08,.96], delay: stagger(Math.max(18, duration / Math.max(2,pixels.length)), { from: 'random' }), duration: duration * .75, loop: true, alternate: true, ease: 'inOut(2)' });
    } else if (impact.has(effect)) {
      animeApi.animate(pixels, { opacity: [.15,1,.15,1], scale: [.95,1.08,.95], duration: Math.max(120,duration * .55), loop: true, ease: 'out(2)' });
    } else if (effect === 'pulse' || effect === 'breathe' || effect === 'fade') {
      animeApi.animate(pixels, { opacity: [.35,1], scale: [.96,1.05], duration, loop: true, alternate: true, ease: 'inOut(2)' });
    } else if (effect === 'rainbow') {
      animeApi.animate(pixels, { opacity: [.55,1], scale: [.98,1.06], delay: stagger(35), duration, loop: true, alternate: true, ease: 'inOut(2)' });
    }
  }

  function getPixelLabParams() {
    const colour = document.getElementById('pixel-lab-colour')?.value || '#00ffc8';
    const [r,g,b] = rgbFromHex(colour);
    const groupSize = clamp(document.getElementById('pixel-lab-group-size')?.value, 1, 1000, 10);
    return ensurePixelParams({
      line: document.getElementById('pixel-lab-line')?.value,
      segmentName: document.getElementById('pixel-lab-segment-name')?.value || 'Segment A',
      segmentMode: document.getElementById('pixel-lab-mode')?.value || 'range',
      startPixel: document.getElementById('pixel-lab-start')?.value,
      length: document.getElementById('pixel-lab-length')?.value,
      groupSize,
      markerOffset: clamp(document.getElementById('pixel-lab-marker-offset')?.value, 0, groupSize - 1, 0),
      effect: document.getElementById('pixel-lab-effect')?.value || 'LIGHTNING',
      r,g,b,
      secondary: document.getElementById('pixel-lab-secondary')?.value || '#101820',
      brightness: document.getElementById('pixel-lab-brightness')?.value,
      speed: document.getElementById('pixel-lab-speed')?.value,
      blackoutAtEnd: Boolean(document.getElementById('pixel-lab-blackout')?.checked)
    });
  }

  function refreshPixelLab() {
    const root = document.getElementById('pixel-lab-preview');
    if (!root) return;
    const p = getPixelLabParams();
    const effect = effectById(p.effect);
    renderPixelStrip(root, p, false);
    renderPixelStrip(document.getElementById('pixel-emergency-preview'), p, true);

    const mode = document.getElementById('pixel-lab-mode');
    const range = document.querySelector('[data-range-fields]');
    const marker = document.querySelector('[data-marker-fields]');
    if (range) range.style.display = mode?.value === 'repeat-marker' ? 'none' : 'grid';
    if (marker) marker.style.display = mode?.value === 'repeat-marker' ? 'grid' : 'none';

    const label = document.getElementById('pixel-lab-preview-label');
    if (label) label.textContent = `LINE ${p.line} · ${String(p.segmentName).toUpperCase()} · ${effect.name.toUpperCase()}`;
    const rangeLabel = document.getElementById('pixel-lab-preview-range');
    if (rangeLabel) rangeLabel.textContent = p.segmentMode === 'repeat-marker' ? `1 ACTIVE / ${p.groupSize} · OFFSET ${p.markerOffset}` : `PX ${p.startPixel}–${p.startPixel + p.length - 1}`;
  }

  function selectPixelEffect(id) {
    const effect = effectById(id);
    const input = document.getElementById('pixel-lab-effect');
    if (input) input.value = effect.id;
    document.querySelectorAll('.pixel-fx-option').forEach((button) => button.classList.toggle('active', button.dataset.pixelEffect === effect.id));
    refreshPixelLab();
  }

  function filterPixelEffects(query) {
    const term = String(query || '').trim().toLowerCase();
    document.querySelectorAll('.pixel-fx-option').forEach((button) => {
      button.hidden = term && !button.textContent.toLowerCase().includes(term);
    });
  }

  function applyExitSignTemplate() {
    const set = (id, value) => { const element = document.getElementById(id); if (element) element.value = value; };
    set('pixel-lab-mode','repeat-marker');
    set('pixel-lab-segment-name','Emergency Exit Markers');
    set('pixel-lab-group-size',10);
    set('pixel-lab-marker-offset',0);
    set('pixel-lab-colour','#00d86d');
    set('pixel-lab-secondary','#000000');
    set('pixel-lab-brightness',255);
    set('pixel-lab-effect','solid');
    selectPixelEffect('solid');
  }

  async function insertPixelLabClip() {
    const project = currentProject();
    if (!project || !window.SHDOModel) return;
    const p = getPixelLabParams();
    let track = (project.tracks || []).find((candidate) => candidate.type === 'pixel' && !candidate.locked);
    if (!track) {
      track = window.SHDOModel.createTrack('pixel', `Pixel Line ${p.line}`, project.tracks.length);
      project.tracks.push(track);
    }
    const startMs = Math.max(0, Number(window.state?.playhead || window.timelineEditor?._state?.playhead || 0));
    const effect = effectById(p.effect);
    const clip = window.SHDOModel.createClip(track.id, 'pixel', startMs, 3000, `${p.segmentName} · ${effect.name}`);
    clip.params = p;
    clip.routing = { nodeId: pixels()?.P4_LOGICAL_ID || 'p4', output: pixels()?.P4_LABEL || 'P4 Show Pixel Line' };
    project.clips.push(clip);
    try { await window.ShowduinoProjects?.saveCurrentProject?.({ cloud: false }); } catch (_) {}
    refreshProjectStats();
    window.alert(`Added ${effect.name} to ${p.segmentName}. Open Timeline to route and position the clip.`);
  }

  function runProjectChecks() {
    const project = currentProject();
    const tracks = project?.tracks || [];
    const clips = project?.clips || [];
    const checks = [];

    const currentClips = clips.filter((clip) => !LEGACY_TYPES.has(clip.type));
    const outputTypes = new Set(['relay','mosfet','pixel','audio','fx']);
    const unrouted = currentClips.filter((clip) => outputTypes.has(clip.type) && !String(clip.routing?.nodeId || '').trim());
    checks.push({ level: unrouted.length ? 'warn' : 'ok', title: 'Logical device routing', detail: unrouted.length ? `${unrouted.length} output cue${unrouted.length === 1 ? '' : 's'} do not yet have a target node ID.` : 'Every current output cue has a logical node target.', code: `${unrouted.length} OPEN` });

    const missingAudio = clips.filter((clip) => clip.type === 'audio' && !String(clip.params?.file || '').trim());
    checks.push({ level: missingAudio.length ? 'warn' : 'ok', title: 'Audio references', detail: missingAudio.length ? `${missingAudio.length} audio cue${missingAudio.length === 1 ? '' : 's'} have no file assigned.` : 'Every audio cue has a file reference.', code: `${missingAudio.length} OPEN` });

    const pixelApi = pixels();
    if (pixelApi) {
      currentClips.filter((clip) => clip.type === 'pixel').forEach((clip) => {
        const result = pixelApi.validatePixelClip(clip, pixelApi.getLiveSnapshot());
        result.errors.forEach((item) => checks.push({ level: 'error', title: 'Pixel authoring', detail: item.message, code: item.code }));
        result.warnings.forEach((item) => checks.push({ level: 'warn', title: 'Pixel hardware', detail: item.message, code: item.code }));
      });
    }

    const unsafeOutputs = clips.filter((clip) => (clip.type === 'relay' || clip.type === 'mosfet') && clip.params?.safeOff === false);
    checks.push({ level: unsafeOutputs.length ? 'warn' : 'ok', title: 'Output stop behaviour', detail: unsafeOutputs.length ? `${unsafeOutputs.length} relay/MOSFET cue${unsafeOutputs.length === 1 ? '' : 's'} are configured not to force OFF on stop.` : 'Relay and MOSFET cues use safe OFF behaviour.', code: `${unsafeOutputs.length} REVIEW` });

    const legacy = clips.filter((clip) => LEGACY_TYPES.has(clip.type));
    checks.push({ level: legacy.length ? 'warn' : 'ok', title: 'Legacy clip types', detail: legacy.length ? `${legacy.length} old DMX / lighting / prop cue${legacy.length === 1 ? '' : 's'} are preserved for compatibility but are outside the current system scope.` : 'No out-of-scope legacy clip types are present.', code: `${legacy.length} LEGACY` });

    const duplicateTrackNames = tracks.map((track) => track.name).filter((name, index, all) => all.indexOf(name) !== index);
    checks.push({ level: duplicateTrackNames.length ? 'warn' : 'ok', title: 'Track naming', detail: duplicateTrackNames.length ? 'Some tracks share the same name. Unique names make routing and commissioning easier.' : 'Track names are unambiguous.', code: duplicateTrackNames.length ? 'REVIEW' : 'OK' });

    return checks;
  }

  function refreshDiagnostics() {
    const container = document.getElementById('v4-project-checks');
    if (!container) return;
    const stats = projectStats();
    const checks = runProjectChecks();
    const errors = checks.filter((check) => check.level === 'error').length;
    const warnings = checks.filter((check) => check.level === 'warn').length;
    const score = Math.max(0, 100 - errors * 30 - warnings * 10);

    const statsRoot = document.getElementById('v4-diagnostic-stats');
    if (statsRoot) statsRoot.innerHTML = `
      <article class="v4-card"><span class="v4-eyebrow">READINESS</span><strong class="v4-stat">${score}%</strong><div class="v4-meta">AUTHORING CHECK</div></article>
      <article class="v4-card"><span class="v4-eyebrow">ERRORS</span><strong class="v4-stat">${errors}</strong><div class="v4-meta">MUST REVIEW</div></article>
      <article class="v4-card"><span class="v4-eyebrow">WARNINGS</span><strong class="v4-stat">${warnings}</strong><div class="v4-meta">SHOULD REVIEW</div></article>
      <article class="v4-card"><span class="v4-eyebrow">LEGACY</span><strong class="v4-stat">${stats.legacy}</strong><div class="v4-meta">OUT-OF-SCOPE CLIPS</div></article>`;

    container.innerHTML = checks.map((check) => `<article class="project-check-item ${check.level}"><span class="check-icon">${check.level === 'ok' ? '✓' : check.level === 'error' ? '!' : '?'}</span><div><strong>${esc(check.title)}</strong><p>${esc(check.detail)}</p></div><small>${esc(check.code)}</small></article>`).join('');
  }

  function authoredTargets() {
    const clips = currentProject()?.clips || [];
    const map = new Map();
    clips.forEach((clip) => {
      const id = String(clip.routing?.nodeId || '').trim();
      if (!id) return;
      const existing = map.get(id) || { id, types: new Set(), count: 0, outputs: new Set() };
      existing.types.add(clip.type);
      if (clip.routing?.output) existing.outputs.add(clip.routing.output);
      existing.count += 1;
      map.set(id, existing);
    });
    return [...map.values()];
  }

  function refreshNodeMap() {
    const root = document.getElementById('v4-node-targets');
    if (!root) return;
    const targets = authoredTargets();
    if (!targets.length) {
      root.innerHTML = '<div class="v4-rule warning">No logical node IDs are used by this production yet. Select an output clip on the Timeline and set its Target node.</div>';
      return;
    }
    root.innerHTML = targets.map((target) => `<div class="node-target"><code>${esc(target.id)}</code><span>${esc([...target.types].join(' · '))} · ${target.count} cue${target.count === 1 ? '' : 's'}${target.outputs.size ? ` · ${esc([...target.outputs].join(', '))}` : ''}</span><strong>AUTHORED</strong></div>`).join('');
  }

  function refreshDeploySummary() {
    const root = document.getElementById('v4-deploy-summary');
    if (!root) return;
    const stats = projectStats();
    const checks = runProjectChecks();
    const blocking = checks.filter((check) => check.level === 'error').length;
    const warnings = checks.filter((check) => check.level === 'warn').length;
    root.innerHTML = `
      <div class="project-check-item ${blocking ? 'error' : 'ok'}"><span class="check-icon">${blocking ? '!' : '✓'}</span><div><strong>Project structure</strong><p>${stats.tracks} tracks · ${stats.clips} cues · ${stats.pixels} pixel clips · ${stats.audio} audio clips</p></div><small>SHDO V${window.ShowduinoPackage?.FORMAT_VERSION || 1}</small></div>
      <div class="project-check-item ${warnings ? 'warn' : 'ok'}"><span class="check-icon">${warnings ? '?' : '✓'}</span><div><strong>Readiness</strong><p>${blocking} errors · ${warnings} warnings. Run Project Check for detail.</p></div><small>${blocking ? 'BLOCK' : 'READY'}</small></div>`;
  }

  async function prepareDeploy() {
    refreshDeploySummary();
    const checks = runProjectChecks();
    const blockers = checks.filter((check) => check.level === 'error');
    if (blockers.length) {
      window.alert(`Deployment check found ${blockers.length} blocking issue${blockers.length === 1 ? '' : 's'}. Open Diagnostics first.`);
      openPanel('diagnostics');
      return;
    }
    try {
      if (window.ShowduinoDeploy?.deployCurrentProject) await window.ShowduinoDeploy.deployCurrentProject();
      else window.ShowduinoProjects?.exportCurrentProject?.();
    } catch (error) {
      window.alert(`Could not prepare the production.\n\n${error.message}`);
    }
  }

  function refreshProjectStats() {
    const stats = projectStats();
    Object.entries(stats).forEach(([key,value]) => document.querySelectorAll(`[data-v4-stat="${key}"]`).forEach((element) => { element.textContent = String(value); }));
    refreshNodeMap();
    refreshDiagnostics();
    refreshDeploySummary();
  }

  function hydratePanel(root) {
    if (!root) return;
    if (root.querySelector?.('#pixel-lab-preview') || root.id === 'pixel-lab-preview') window.setTimeout(refreshPixelLab, 0);
    if (root.querySelector?.('#v4-node-targets') || root.id === 'v4-node-targets') window.setTimeout(refreshNodeMap, 0);
    if (root.querySelector?.('#v4-project-checks') || root.id === 'v4-project-checks') window.setTimeout(refreshDiagnostics, 0);
    if (root.querySelector?.('#v4-deploy-summary') || root.id === 'v4-deploy-summary') window.setTimeout(refreshDeploySummary, 0);
    window.setTimeout(refreshProjectStats, 0);
    animatePanel(root);
  }

  function animatePanel(root) {
    if (reducedMotion || !animeApi?.animate) return;
    const targets = root.querySelectorAll?.('.v4-card, .v4-role, .node-role-card, .pixel-lab-big-strip') || [];
    if (!targets.length) return;
    animeApi.animate(targets, { opacity: [0,1], y: [10,0], delay: animeApi.stagger?.(45) || 0, duration: 420, ease: 'out(3)' });
  }

  function unlockCurrentPanels() {
    document.querySelectorAll('.sidebar-nav li.locked').forEach((item) => {
      item.classList.remove('locked');
      item.title = '';
    });
  }

  function patchPanels() {
    if (!window.PanelManager) return;
    window.PanelManager.prototype.introduction = function () { return dashboardHtml(); };
    window.PanelManager.prototype.playback = function () { return pixelLabHtml(); };
    window.PanelManager.prototype.connect = async function () { return deployHtml(); };
    window.PanelManager.prototype.devices = function () { return nodesHtml(); };
    window.PanelManager.prototype.diagnostics = async function () { return diagnosticsHtml(); };
    window.PanelManager.prototype.help = function () { return helpHtml(); };
  }

  function patchTimeline() {
    if (!window.TimelineEditor || window.TimelineEditor.prototype.__studioV4Patched) return;
    const proto = window.TimelineEditor.prototype;
    proto.__studioV4Patched = true;

    const originalInit = proto.init;
    proto.init = function () {
      if (this._state?.project && window.SHDOModel?.migrate) window.SHDOModel.migrate(this._state.project);
      return originalInit.call(this);
    };

    const originalToolbar = proto._buildToolbar;
    proto._buildToolbar = function () {
      const bar = originalToolbar.call(this);
      const buttons = [...bar.querySelectorAll('button')];
      buttons.forEach((button) => {
        const text = button.textContent.toLowerCase();
        if (text.includes('lighting') || text.includes('dmx') || text.includes('prop')) button.remove();
      });

      const trackGroup = [...bar.children].find((child) => [...child.querySelectorAll('button')].some((button) => button.textContent.toLowerCase().includes('relay')));
      if (trackGroup && ![...trackGroup.querySelectorAll('button')].some((button) => button.textContent.includes('MOSFET'))) {
        const relayButton = [...trackGroup.querySelectorAll('button')].find((button) => button.textContent.toLowerCase().includes('relay'));
        const mosfet = this._toolbarBtn('▰ MOSFET', () => this.addTrack('mosfet'));
        mosfet.title = 'Add MOSFET output track';
        if (relayButton?.nextSibling) trackGroup.insertBefore(mosfet, relayButton.nextSibling);
        else trackGroup.appendChild(mosfet);
      }
      return bar;
    };

    if (typeof proto._buildProfessionalTypeInspector === 'function') {
      const originalTypeInspector = proto._buildProfessionalTypeInspector;
      proto._buildProfessionalTypeInspector = function (clip) {
        if (clip.type === 'pixel') return buildPixelInspector(clip);
        if (clip.type === 'mosfet') return buildMosfetInspector(clip);
        if (LEGACY_TYPES.has(clip.type)) return `<div class="v4-rule warning"><strong>Legacy ${esc(clip.type)} clip:</strong> preserved so old projects remain readable, but this clip type is outside the current Showduino implementation scope. You can move, duplicate or delete it; new clips of this type cannot be authored in Studio v4.</div>`;
        return originalTypeInspector.call(this, clip);
      };
    }

    if (typeof proto._bindProfessionalTypeInspector === 'function') {
      const originalBindTypeInspector = proto._bindProfessionalTypeInspector;
      proto._bindProfessionalTypeInspector = function (clip) {
        if (clip.type === 'pixel') {
          clip.params = ensurePixelParams(clip.params || {});
          const p = clip.params;
          clip.routing = clip.routing || { nodeId: '', output: '' };
          const save = () => { this._refreshClipEl?.(clip.id); this._autosave?.(); renderPixelStrip(document.getElementById('v4-inspector-pixel-preview'), p, false); };
          const bind = (id, fn, eventName = 'change') => { const element = document.getElementById(id); element?.addEventListener(eventName, () => { fn(element); save(); }); };
          const syncParams = () => {
            document.querySelectorAll('#inspector-root [data-px-param], .v4-inspector-section [data-px-param]').forEach((el) => {
              const param = el.getAttribute('data-px-param');
              if (!param) return;
              el.hidden = pixels() ? !pixels().effectSupports(p.effect, param) : false;
            });
          };
          bind('v4-pixel-device', (el) => {
            const id = pixels()?.canonicalNodeId?.(el.value) || el.value.trim();
            clip.routing.nodeId = id;
            clip.routing.output = pixels()?.isP4Id?.(id) ? pixels().P4_LABEL : id;
            const routeNode = document.getElementById('route-node');
            if (routeNode) routeNode.value = id;
          });
          bind('v4-pixel-segment', (el) => { p.segment = clamp(el.value,0,15,0); p.segmentName = `Segment ${p.segment}`; clip.label = `${pixels()?.optionLabel?.(pixels().findOutput(clip.routing.nodeId, { project: currentProject(), selectedId: clip.routing.nodeId })) || clip.routing.nodeId} · Seg ${p.segment} · ${effectById(p.effect).name}`; });
          bind('v4-pixel-mode', (el) => { p.segmentMode = el.value; const a=document.getElementById('v4-pixel-range-fields'); const b=document.getElementById('v4-pixel-marker-fields'); if(a)a.style.display=p.segmentMode==='repeat-marker'?'none':'grid'; if(b)b.style.display=p.segmentMode==='repeat-marker'?'grid':'none'; });
          bind('v4-pixel-start', (el) => { p.startPixel = clamp(el.value,0,100000,0); });
          bind('v4-pixel-length', (el) => { p.length = clamp(el.value,1,100000,10); });
          bind('v4-pixel-group', (el) => { p.groupSize = clamp(el.value,1,1000,10); p.markerOffset = Math.min(p.markerOffset,p.groupSize-1); });
          bind('v4-pixel-marker', (el) => { p.markerOffset = clamp(el.value,0,Math.max(0,p.groupSize-1),0); });
          bind('v4-pixel-effect', (el) => { p.effect = effectById(el.value).id; syncParams(); });
          bind('v4-pixel-colour', (el) => { const [r,g,b]=rgbFromHex(el.value); p.r=r;p.g=g;p.b=b; clip.color=el.value; });
          bind('v4-pixel-secondary', (el) => { p.secondary = el.value; });
          bind('v4-pixel-brightness', (el) => { p.brightness = clamp(el.value,0,255,255); });
          bind('v4-pixel-speed', (el) => { p.speed = clamp(el.value,1,100,50); });
          bind('v4-pixel-intensity', (el) => { p.intensity = clamp(el.value,0,100,80); });
          bind('v4-pixel-randomness', (el) => { p.randomness = clamp(el.value,0,100,70); });
          bind('v4-pixel-reverse', (el) => { p.reverse = el.checked; });
          bind('v4-pixel-blackout', (el) => { p.blackoutAtEnd = el.checked; });
          syncParams();
          pixels()?.refreshLiveOutputs?.().then(() => {});
          window.setTimeout(() => renderPixelStrip(document.getElementById('v4-inspector-pixel-preview'), p, false), 0);
          return;
        }

        if (clip.type === 'mosfet') {
          const p = ensureMosfetParams(clip.params || (clip.params = {}));
          const save = () => { this._refreshClipEl?.(clip.id); this._autosave?.(); };
          const bind = (id, fn) => { const element=document.getElementById(id); element?.addEventListener('change',()=>{fn(element);save();}); };
          bind('v4-mosfet-output',(el)=>{p.out=el.value.trim()||'out1';});
          bind('v4-mosfet-mode',(el)=>{p.mode=el.value;});
          bind('v4-mosfet-duty',(el)=>{p.duty=clamp(el.value,0,100,100);});
          bind('v4-mosfet-pulse',(el)=>{p.pulseMs=clamp(el.value,0,600000,0);});
          bind('v4-mosfet-state',(el)=>{p.state=el.checked;});
          bind('v4-mosfet-safe',(el)=>{p.safeOff=el.checked;});
          return;
        }

        if (LEGACY_TYPES.has(clip.type)) return;
        return originalBindTypeInspector.call(this, clip);
      };
    }
  }

  function patchMobileMenu() {
    const button = document.getElementById('studio-mobile-menu');
    button?.addEventListener('click', () => document.body.classList.toggle('studio-v4-menu-open'));
    document.querySelectorAll('.sidebar-nav li').forEach((item) => item.addEventListener('click', () => document.body.classList.remove('studio-v4-menu-open')));
  }

  function boot() {
    patchPanels();
    patchTimeline();

    document.addEventListener('DOMContentLoaded', () => {
      window.setTimeout(() => {
        unlockCurrentPanels();
        patchMobileMenu();
        const workspace = document.querySelector('.workspace');
        if (workspace) {
          hydratePanel(workspace);
          const observer = new MutationObserver((mutations) => mutations.forEach((mutation) => mutation.addedNodes.forEach((node) => { if (node instanceof Element) hydratePanel(node); })));
          observer.observe(workspace, { childList: true, subtree: false });
        }
        window.setInterval(unlockCurrentPanels, 1800);
      }, 0);
    }, { once: true });

    window.addEventListener('showduino:project-saved', refreshProjectStats);
    window.addEventListener('showduino:project-deployed', refreshProjectStats);
    window.addEventListener('showduino:v4-saved', refreshProjectStats);
    window.ShowduinoPixelAuthoring?.refreshLiveOutputs?.();
  }

  window.ShowduinoStudioV4 = Object.freeze({
    PIXEL_EFFECTS,
    openPanel,
    refreshProjectStats,
    refreshPixelLab,
    onPixelLabInput: refreshPixelLab,
    selectPixelEffect,
    filterPixelEffects,
    applyExitSignTemplate,
    insertPixelLabClip,
    refreshDiagnostics,
    refreshNodeMap,
    prepareDeploy,
    renderPixelStrip,
    ensurePixelParams,
    ensureMosfetParams
  });

  boot();
})();
