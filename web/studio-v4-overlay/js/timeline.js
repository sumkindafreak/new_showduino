// TimelineEditor — Full DAW-style timeline for Showduino Studio
// Renders into any element with class "timeline-editor"

class TimelineEditor {
  constructor(api, state, log) {
    this._api      = api;
    this._state    = state;
    this._log      = log || console.log;
    this._el       = null;       // root .timeline-editor element
    this._pxPerMs  = 0.1;        // default zoom
    this._snapMs   = 1000;
    this._snapEnabled = true;
    this._gridEnabled = true;
    this._selectedClipId = null;
    this._selectedTrackId = null;
    this._playing  = false;
    this._rafId    = null;
    this._playStart = 0;          // Date.now() when play started
    this._playOffset = 0;         // ms offset when play started
    this._undoStack = [];
    this._redoStack = [];
    this._dragState = null;
    this._resizeState = null;
    this._markerDragState = null;
    this._timelineWidth = 30000;  // default canvas width in ms
    this._trackHeight = 60;
    this._markerTrackHeight = 28;
    this._rulerHeight = 30;
    this._headerWidth = 160;
    this._inspectorWidth = 280;
    this._scrollLeft = 0;
  }

  /* ── Init ────────────────────────────────────────────────────── */

  init() {
    this._el = document.querySelector('.timeline-editor');
    if (!this._el) {
      this._log('TimelineEditor: .timeline-editor container not found', 'WARN');
      return;
    }

    // Ensure project exists
    if (!this._state.project || !SHDOModel.validate(this._state.project)) {
      this._state.project = SHDOModel.createProject('Untitled Show');
    }

    this._render();
    this._bindGlobalKeys();
    this._startAutosave();
    this._log('TimelineEditor ready', 'INFO');
    window.timelineEditor = this;
  }

  _project() { return this._state.project; }
  _tracks()  { return this._project().tracks || []; }
  _clips()   { return this._project().clips  || []; }
  _markers() { return this._project().markers || []; }

  /* ── Rendering ───────────────────────────────────────────────── */

  _render() {
    this._el.innerHTML = '';
    this._el.style.display = 'flex';
    this._el.style.flexDirection = 'column';
    this._el.style.height = '100%';
    this._el.style.minHeight = '520px';
    this._el.style.background = '#1a1a1a';
    this._el.style.color = '#eee';
    this._el.style.fontFamily = 'monospace, sans-serif';
    this._el.style.overflow = 'hidden';

    this._el.appendChild(this._buildToolbar());

    const main = document.createElement('div');
    main.className = 'timeline-main';
    main.style.cssText = 'display:flex;flex:1;overflow:hidden;';

    main.appendChild(this._buildLeftPanel());

    this._rightPanel = this._buildRightPanel();
    main.appendChild(this._rightPanel);

    this._inspectorPanel = this._buildInspector();
    main.appendChild(this._inspectorPanel);

    this._el.appendChild(main);

    this._renderTracks();
    this._updateRuler();
  }

  _buildToolbar() {
    const bar = document.createElement('div');
    bar.className = 'tl-toolbar timeline-toolbar';
    bar.style.cssText = 'display:flex;gap:4px;padding:6px 8px;background:#2a2a2a;border-bottom:1px solid #444;flex-wrap:wrap;align-items:center;flex-shrink:0;';

    const addGroup = this._toolbarGroup('Add Track:');
    const trackTypes = ['mixed','audio','fx','relay','lighting','pixel','dmx','prop','trigger'];
    const trackIcons = { mixed:'＋', audio:'🎵', fx:'✨', relay:'⚡', lighting:'💡', pixel:'🌈', dmx:'🎛', prop:'⚙️', trigger:'🎯' };
    trackTypes.forEach(t => {
      const label = t === 'mixed' ? 'Lane' : t.charAt(0).toUpperCase() + t.slice(1);
      const btn = this._toolbarBtn(trackIcons[t] + ' ' + label, () => this.addTrack(t));
      btn.title = 'Add ' + t + ' track';
      addGroup.appendChild(btn);
    });
    bar.appendChild(addGroup);

    const sep = () => { const s = document.createElement('div'); s.style.cssText='width:1px;background:#555;height:24px;margin:0 4px;'; return s; };
    bar.appendChild(sep());

    // Transport
    const transGroup = this._toolbarGroup('');
    const playBtn = this._toolbarBtn('▶ Play', () => this.play());
    playBtn.id = 'tl-play-btn';
    const pauseBtn = this._toolbarBtn('⏸ Pause', () => this.pause());
    const stopBtn  = this._toolbarBtn('⏹ Stop',  () => this.stop());
    const rewindBtn = this._toolbarBtn('⏮ Rewind', () => this.rewind());
    transGroup.appendChild(playBtn);
    transGroup.appendChild(pauseBtn);
    transGroup.appendChild(stopBtn);
    transGroup.appendChild(rewindBtn);
    this._loopBtn = this._toolbarBtn('🔁 Loop', () => this._toggleLoop());
    this._loopBtn.title = 'Toggle loop';
    transGroup.appendChild(this._loopBtn);
    bar.appendChild(transGroup);
    bar.appendChild(sep());

    // Zoom
    const zoomGroup = this._toolbarGroup('Zoom:');
    zoomGroup.appendChild(this._toolbarBtn('🔍+', () => this._zoom(1.5)));
    zoomGroup.appendChild(this._toolbarBtn('🔍-', () => this._zoom(1 / 1.5)));
    zoomGroup.appendChild(this._toolbarBtn('Fit', () => this._zoomFit()));
    bar.appendChild(zoomGroup);
    bar.appendChild(sep());

    // Snap / Grid
    this._snapBtn = this._toolbarBtn('⊞ Snap: ON', () => this._toggleSnap());
    this._snapBtn.style.color = '#00ffcc';
    bar.appendChild(this._snapBtn);

    this._gridBtn = this._toolbarBtn('⋮ Grid: ON', () => this._toggleGrid());
    this._gridBtn.style.color = '#00ffcc';
    bar.appendChild(this._gridBtn);
    bar.appendChild(sep());

    // Marker
    bar.appendChild(this._toolbarBtn('📌 Marker', () => this._addMarkerAtPlayhead()));

    // Selected clip actions live under one deliberate three-dot menu.
    const moreBtn = this._toolbarBtn('⋮', event => this._showTimelineMoreMenu(event.currentTarget));
    moreBtn.classList.add('timeline-more-btn');
    moreBtn.setAttribute('aria-label', 'More timeline actions');
    moreBtn.setAttribute('aria-haspopup', 'menu');
    moreBtn.title = 'More timeline actions';
    moreBtn.style.fontSize = '18px';
    moreBtn.style.lineHeight = '1';
    bar.appendChild(moreBtn);
    bar.appendChild(sep());

    // Project
    bar.appendChild(this._toolbarBtn('💾 Save', () => this._saveToLocalStorage()));
    bar.appendChild(this._toolbarBtn('📂 Open', () => this._openProjectBrowser()));
    bar.appendChild(this._toolbarBtn('⬇ Export', () => this._exportJSON()));
    bar.appendChild(this._toolbarBtn('⬆ Import', () => this._importJSON()));
    bar.appendChild(this._toolbarBtn('＋ New', () => this._newProject()));

    // Timecode display
    const tc = document.createElement('div');
    tc.id = 'tl-timecode';
    tc.style.cssText = 'margin-left:auto;font-size:14px;font-weight:bold;color:#00ffcc;letter-spacing:2px;padding:4px 8px;background:#111;border-radius:4px;min-width:100px;text-align:center;';
    tc.textContent = '00:00.000';
    bar.appendChild(tc);

    return bar;
  }

  _toolbarGroup(label) {
    const g = document.createElement('div');
    g.style.cssText = 'display:flex;gap:3px;align-items:center;flex-wrap:wrap;';
    if (label) {
      const lbl = document.createElement('span');
      lbl.textContent = label;
      lbl.style.cssText = 'font-size:11px;color:#888;margin-right:2px;white-space:nowrap;';
      g.appendChild(lbl);
    }
    return g;
  }

  _toolbarBtn(text, onClick) {
    const btn = document.createElement('button');
    btn.className = 'btn-toolbar';
    btn.textContent = text;
    btn.style.cssText = 'padding:3px 8px;font-size:11px;white-space:nowrap;';
    btn.addEventListener('click', onClick);
    return btn;
  }

  _buildLeftPanel() {
    const left = document.createElement('div');
    left.className = 'tl-left';
    left.style.cssText = `width:${this._headerWidth}px;min-width:${this._headerWidth}px;background:#222;border-right:1px solid #444;display:flex;flex-direction:column;flex-shrink:0;`;

    const hdr = document.createElement('div');
    hdr.className = 'tl-track-list-header';
    hdr.style.cssText = `height:${this._rulerHeight + this._markerTrackHeight}px;background:#1a1a1a;border-bottom:1px solid #444;display:flex;align-items:center;padding:0 8px;font-size:12px;color:#888;box-sizing:border-box;`;
    hdr.textContent = 'Tracks';
    left.appendChild(hdr);

    this._trackListEl = document.createElement('div');
    this._trackListEl.className = 'tl-track-list';
    this._trackListEl.style.cssText = 'flex:1;overflow-y:auto;overflow-x:hidden;';
    left.appendChild(this._trackListEl);

    return left;
  }

  _buildRightPanel() {
    const right = document.createElement('div');
    right.className = 'tl-right';
    right.style.cssText = 'flex:1;display:flex;flex-direction:column;overflow:hidden;position:relative;';

    // Ruler
    this._rulerEl = document.createElement('div');
    this._rulerEl.className = 'tl-ruler timeline-ruler';
    this._rulerEl.style.cssText = `height:${this._rulerHeight}px;background:#333;border-bottom:1px solid #444;position:relative;overflow:hidden;flex-shrink:0;cursor:pointer;`;
    this._rulerEl.title = 'Click to set playhead position';
    this._rulerEl.addEventListener('click', e => this._rulerClick(e));
    right.appendChild(this._rulerEl);

    // Marker track
    this._markerTrackEl = document.createElement('div');
    this._markerTrackEl.className = 'tl-marker-track';
    this._markerTrackEl.style.cssText = `height:${this._markerTrackHeight}px;background:#1e1e1e;border-bottom:1px solid #444;position:relative;overflow:hidden;flex-shrink:0;`;
    right.appendChild(this._markerTrackEl);

    // Canvas scroll area
    this._canvasScroll = document.createElement('div');
    this._canvasScroll.className = 'tl-canvas-scroll';
    this._canvasScroll.style.cssText = 'flex:1;overflow-y:auto;overflow-x:scroll;position:relative;';

    this._canvas = document.createElement('div');
    this._canvas.className = 'tl-canvas tracks-canvas';
    this._canvas.style.cssText = `position:relative;min-width:${this._msToX(this._timelineWidth)}px;`;

    // Grid overlay
    this._gridEl = document.createElement('canvas');
    this._gridEl.className = 'tl-grid';
    this._gridEl.style.cssText = 'position:absolute;top:0;left:0;pointer-events:none;z-index:0;';
    this._canvas.appendChild(this._gridEl);

    // Playhead
    this._playheadEl = document.createElement('div');
    this._playheadEl.className = 'tl-playhead playhead';
    this._playheadEl.style.cssText = 'position:absolute;top:0;width:2px;background:#ff2244;z-index:200;pointer-events:none;';
    this._canvas.appendChild(this._playheadEl);

    this._canvasScroll.appendChild(this._canvas);
    right.appendChild(this._canvasScroll);

    // Ruler playhead
    this._rulerPlayhead = document.createElement('div');
    this._rulerPlayhead.style.cssText = 'position:absolute;top:0;width:2px;background:#ff2244;height:100%;z-index:10;pointer-events:none;';
    this._rulerEl.appendChild(this._rulerPlayhead);

    // Sync ruler and canvas horizontal scroll
    this._canvasScroll.addEventListener('scroll', () => {
      this._scrollLeft = this._canvasScroll.scrollLeft;
      this._rulerEl.scrollLeft = this._scrollLeft;
      this._markerTrackEl.style.transform = `translateX(-${this._scrollLeft}px)`;
      this._rulerEl.style.backgroundPositionX = `-${this._scrollLeft}px`;

      if (!this._syncingVerticalScroll) {
        this._syncingVerticalScroll = true;
        this._trackListEl.scrollTop = this._canvasScroll.scrollTop;
        this._syncingVerticalScroll = false;
      }

      this._updateRuler();
      this._updateGridCanvas();
    });

    this._trackListEl.addEventListener('scroll', () => {
      if (this._syncingVerticalScroll) return;
      this._syncingVerticalScroll = true;
      this._canvasScroll.scrollTop = this._trackListEl.scrollTop;
      this._syncingVerticalScroll = false;
    });

    return right;
  }

  _buildInspector() {
    const insp = document.createElement('div');
    insp.className = 'tl-inspector inspector';
    insp.style.cssText = `width:${this._inspectorWidth}px;min-width:${this._inspectorWidth}px;background:#2a2a2a;border-left:1px solid #444;display:flex;flex-direction:column;flex-shrink:0;overflow-y:auto;padding:12px;box-sizing:border-box;`;
    insp.innerHTML = '<h3 style="margin:0 0 8px;color:#00ffcc;font-size:14px;">Inspector</h3><p style="color:#888;font-size:12px;">Select a clip to inspect.</p>';
    return insp;
  }

  /* ── Track rendering ─────────────────────────────────────────── */

  _renderTracks() {
    this._trackListEl.innerHTML = '';
    // Remove old track rows from canvas (keep grid + playhead)
    Array.from(this._canvas.querySelectorAll('.tl-track-row')).forEach(el => el.remove());

    const tracks = this._tracks().slice().sort((a, b) => a.order - b.order);
    let totalHeight = 0;

    tracks.forEach(track => {
      const headerEl = this._buildTrackHeader(track);
      this._trackListEl.appendChild(headerEl);

      const rowEl = this._buildTrackRow(track);
      rowEl.style.top = totalHeight + 'px';
      this._canvas.appendChild(rowEl);

      totalHeight += this._trackHeight;
    });

    // Update canvas height
    this._canvas.style.height = Math.max(totalHeight, 300) + 'px';
    this._playheadEl.style.height = Math.max(totalHeight, 300) + 'px';

    this._updateGridCanvas();
    this._updateAllClips();
    this._updateMarkers();
    this._syncPlayheadPosition();
  }

  _buildTrackHeader(track) {
    const hdr = document.createElement('div');
    hdr.className = 'tl-track-header track-header';
    hdr.dataset.trackId = track.id;
    hdr.style.cssText = `height:${this._trackHeight}px;box-sizing:border-box;border-bottom:1px solid #444;padding:4px 6px;display:flex;flex-direction:column;justify-content:space-between;background:#222;`;
    if (!track.visible) hdr.style.opacity = '0.4';

    const nameRow = document.createElement('div');
    nameRow.style.cssText = 'display:flex;align-items:center;gap:4px;';
    const colorDot = document.createElement('div');
    colorDot.style.cssText = `width:10px;height:10px;border-radius:50%;background:${track.color};flex-shrink:0;`;
    const nameEl = document.createElement('span');
    nameEl.textContent = track.name;
    nameEl.style.cssText = 'font-size:11px;color:#eee;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;flex:1;cursor:pointer;';
    nameEl.title = 'Double-click to rename';
    nameEl.addEventListener('dblclick', () => this._renameTrack(track.id, nameEl));
    nameRow.appendChild(colorDot);
    nameRow.appendChild(nameEl);
    hdr.appendChild(nameRow);

    const btnRow = document.createElement('div');
    btnRow.style.cssText = 'display:flex;gap:2px;';
    btnRow.appendChild(this._trackBtn('M', track.muted ? '#ff8800' : '#555', () => this._toggleMute(track.id), 'Mute'));
    btnRow.appendChild(this._trackBtn('👁', track.visible ? '#00ffcc' : '#555', () => this._toggleVisible(track.id), 'Visible'));
    btnRow.appendChild(this._trackBtn('🔒', track.locked ? '#ffff00' : '#555', () => this._toggleLock(track.id), 'Lock'));
    btnRow.appendChild(this._trackBtn('⊕', '#555', () => this._duplicateTrack(track.id), 'Duplicate'));
    btnRow.appendChild(this._trackBtn('✕', '#555', () => this._deleteTrack(track.id), 'Delete'));
    hdr.appendChild(btnRow);

    return hdr;
  }

  _trackBtn(text, color, onClick, title = '') {
    const btn = document.createElement('button');
    btn.textContent = text;
    btn.title = title;
    btn.style.cssText = `padding:1px 4px;font-size:10px;background:${color};border:1px solid #555;color:#fff;cursor:pointer;border-radius:2px;`;
    btn.addEventListener('click', onClick);
    return btn;
  }

  _buildTrackRow(track) {
    const row = document.createElement('div');
    row.className = 'tl-track-row';
    row.dataset.trackId = track.id;
    row.style.cssText = `position:absolute;left:0;right:0;height:${this._trackHeight}px;border-bottom:1px solid #333;box-sizing:border-box;background:${track.muted ? '#1a1414' : '#1a1a1a'};`;
    if (!track.visible) row.style.opacity = '0.3';

    // Drop target for new clips
    row.addEventListener('dblclick', e => {
      if (track.locked) return;
      const x = e.clientX - this._canvas.getBoundingClientRect().left;
      const startMs = this._snapMs ? this._snapValue(this._xToMs(x)) : this._xToMs(x);
      this._addClip(track.id, track.type, startMs);
    });

    row.addEventListener('dragover', e => { e.preventDefault(); row.style.background='rgba(0,255,204,0.08)'; });
    row.addEventListener('dragleave', () => { row.style.background = track.muted ? '#1a1414' : '#1a1a1a'; });
    row.addEventListener('drop', e => {
      e.preventDefault();
      row.style.background = track.muted ? '#1a1414' : '#1a1a1a';
      const type = e.dataTransfer.getData('blockType') || track.type;
      const x = e.clientX - this._canvas.getBoundingClientRect().left;
      const startMs = this._snapValue(this._xToMs(x));
      this._addClip(track.id, type, startMs);
    });

    return row;
  }

  /* ── Clips ───────────────────────────────────────────────────── */

  _updateAllClips() {
    // Remove old clip elements
    Array.from(this._canvas.querySelectorAll('.tl-clip')).forEach(el => el.remove());
    this._clips().forEach(clip => this._renderClip(clip));
  }

  _renderClip(clip) {
    const track = this._tracks().find(t => t.id === clip.trackId);
    if (!track) return;

    const trackIndex = this._tracks().slice().sort((a, b) => a.order - b.order).findIndex(t => t.id === clip.trackId);
    if (trackIndex < 0) return;

    const el = document.createElement('div');
    el.className = 'tl-clip clip';
    el.dataset.clipId = clip.id;
    el.draggable = false;

    const top = trackIndex * this._trackHeight + 8;
    const left = this._msToX(clip.startMs);
    const width = Math.max(this._msToX(clip.durationMs), 8);

    el.style.cssText = `
      position:absolute;
      top:${top}px;
      left:${left}px;
      width:${width}px;
      height:${this._trackHeight - 16}px;
      background:${clip.color || track.color || '#00aaff'};
      border:1px solid rgba(255,255,255,0.3);
      border-radius:4px;
      box-sizing:border-box;
      cursor:move;
      user-select:none;
      overflow:hidden;
      z-index:10;
      display:flex;
      align-items:center;
      padding:0 4px;
    `;

    if (clip.id === this._selectedClipId) {
      el.style.outline = '2px solid #fff';
      el.style.boxShadow = '0 0 8px rgba(255,255,255,0.5)';
    }

    // Label
    const lbl = document.createElement('span');
    lbl.textContent = clip.label || clip.type;
    lbl.style.cssText = 'font-size:10px;color:#000;font-weight:bold;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;pointer-events:none;flex:1;';
    el.appendChild(lbl);

    // Left resize handle
    const leftHandle = document.createElement('div');
    leftHandle.style.cssText = 'position:absolute;left:0;top:0;width:6px;height:100%;cursor:w-resize;background:rgba(255,255,255,0.2);z-index:11;';
    leftHandle.addEventListener('mousedown', e => { e.stopPropagation(); this._startResize(e, clip.id, 'left'); });
    el.appendChild(leftHandle);

    // Right resize handle
    const rightHandle = document.createElement('div');
    rightHandle.style.cssText = 'position:absolute;right:0;top:0;width:6px;height:100%;cursor:e-resize;background:rgba(255,255,255,0.2);z-index:11;';
    rightHandle.addEventListener('mousedown', e => { e.stopPropagation(); this._startResize(e, clip.id, 'right'); });
    el.appendChild(rightHandle);

    el.addEventListener('mousedown', e => {
      if (e.target === leftHandle || e.target === rightHandle) return;
      this._startDrag(e, clip.id);
    });

    el.addEventListener('click', e => {
      if (this._dragState) return;
      this._selectClip(clip.id);
    });

    el.addEventListener('contextmenu', e => {
      e.preventDefault();
      this._showClipContextMenu(e, clip.id);
    });

    this._canvas.appendChild(el);
  }

  _refreshClipEl(clipId) {
    const existing = this._canvas.querySelector(`[data-clip-id="${clipId}"]`);
    if (existing) existing.remove();
    const clip = this._clips().find(c => c.id === clipId);
    if (clip) this._renderClip(clip);
  }

  /* ── Drag ────────────────────────────────────────────────────── */

  _startDrag(e, clipId) {
    const sourceTrack = this._getClipTrack(clipId);
    if (sourceTrack && sourceTrack.locked) return;

    e.preventDefault();
    const clip = this._clips().find(c => c.id === clipId);
    if (!clip) return;

    this._selectClip(clipId);

    const startX = e.clientX;
    const startY = e.clientY;
    const origStartMs = clip.startMs;
    const sortedTracks = this._tracks().slice().sort((a, b) => a.order - b.order);
    let moved = false;

    this._dragState = { clipId };

    const onMove = mv => {
      const dx = mv.clientX - startX;
      const dy = mv.clientY - startY;
      if (!moved && Math.hypot(dx, dy) < 3) return;
      moved = true;

      const dMs = dx / this._pxPerMs;
      let newStart = Math.max(0, origStartMs + dMs);
      if (this._snapEnabled) newStart = this._snapValue(newStart);

      const canvasRect = this._canvas.getBoundingClientRect();
      const pointerY = mv.clientY - canvasRect.top;
      const targetIndex = Math.max(0, Math.min(sortedTracks.length - 1, Math.floor(pointerY / this._trackHeight)));
      const candidateTrack = sortedTracks[targetIndex];
      const targetTrack = candidateTrack && !candidateTrack.locked
        ? candidateTrack
        : this._tracks().find(track => track.id === clip.trackId);

      if (!targetTrack || this._overlaps(clipId, targetTrack.id, newStart, clip.durationMs)) return;

      clip.startMs = newStart;
      clip.trackId = targetTrack.id;

      const renderedTrackIndex = sortedTracks.findIndex(track => track.id === targetTrack.id);
      const el = this._canvas.querySelector(`[data-clip-id="${clipId}"]`);
      if (el) {
        el.style.left = this._msToX(newStart) + 'px';
        el.style.top = (renderedTrackIndex * this._trackHeight + 8) + 'px';
      }

      this._syncPlayheadPosition();
      this._updateTimecodeDisplay();
    };

    const onUp = () => {
      document.removeEventListener('mousemove', onMove);
      document.removeEventListener('mouseup', onUp);
      this._dragState = null;

      if (moved) {
        this._pushUndo();
        this._autosave();
        this._showInspector(clipId);
      }
    };

    document.addEventListener('mousemove', onMove);
    document.addEventListener('mouseup', onUp);
  }

  _startResize(e, clipId, edge) {
    const track = this._getClipTrack(clipId);
    if (track && track.locked) return;
    e.preventDefault();
    const clip = this._clips().find(c => c.id === clipId);
    if (!clip) return;

    const startX = e.clientX;
    const origStart = clip.startMs;
    const origDuration = clip.durationMs;
    const origEnd = origStart + origDuration;

    const onMove = mv => {
      const dx = mv.clientX - startX;
      const dMs = dx / this._pxPerMs;
      if (edge === 'right') {
        const newDur = Math.max(100, origDuration + dMs);
        const snapped = this._snapEnabled ? this._snapValue(origStart + newDur) - origStart : newDur;
        clip.durationMs = Math.max(100, snapped);
      } else {
        let newStart = Math.max(0, origStart + dMs);
        if (this._snapEnabled) newStart = this._snapValue(newStart);
        const newDur = origEnd - newStart;
        if (newDur >= 100) {
          clip.startMs = newStart;
          clip.durationMs = newDur;
        }
      }
      const el = this._canvas.querySelector(`[data-clip-id="${clipId}"]`);
      if (el) {
        el.style.left = this._msToX(clip.startMs) + 'px';
        el.style.width = Math.max(8, this._msToX(clip.durationMs)) + 'px';
      }
    };

    const onUp = () => {
      document.removeEventListener('mousemove', onMove);
      document.removeEventListener('mouseup', onUp);
      this._pushUndo();
      this._autosave();
    };

    document.addEventListener('mousemove', onMove);
    document.addEventListener('mouseup', onUp);
  }

  /* ── Inspector ───────────────────────────────────────────────── */

  _selectClip(clipId) {
    this._selectedClipId = clipId;
    // Refresh selection highlight
    this._canvas.querySelectorAll('.tl-clip').forEach(el => {
      el.style.outline = '';
      el.style.boxShadow = '';
    });
    const el = this._canvas.querySelector(`[data-clip-id="${clipId}"]`);
    if (el) { el.style.outline = '2px solid #fff'; el.style.boxShadow = '0 0 8px rgba(255,255,255,0.5)'; }
    this._showInspector(clipId);
  }

  _showInspector(clipId) {
    const clip = this._clips().find(c => c.id === clipId);
    if (!clip) return;
    const track = this._tracks().find(t => t.id === clip.trackId);

    this._inspectorPanel.innerHTML = `
      <h3 style="margin:0 0 8px;color:#00ffcc;font-size:14px;">Inspector</h3>
      <div class="inspector-field" style="margin-bottom:8px;">
        <label style="font-size:11px;color:#888;display:block;margin-bottom:2px;">Label</label>
        <input id="insp-label" type="text" value="${clip.label || ''}" style="width:100%;background:#111;border:1px solid #555;color:#eee;padding:4px;box-sizing:border-box;border-radius:3px;font-size:12px;"/>
      </div>
      <div class="inspector-field" style="margin-bottom:8px;">
        <label style="font-size:11px;color:#888;display:block;margin-bottom:2px;">Start (ms)</label>
        <input id="insp-start" type="number" value="${Math.round(clip.startMs)}" style="width:100%;background:#111;border:1px solid #555;color:#eee;padding:4px;box-sizing:border-box;border-radius:3px;font-size:12px;"/>
      </div>
      <div class="inspector-field" style="margin-bottom:8px;">
        <label style="font-size:11px;color:#888;display:block;margin-bottom:2px;">Duration (ms)</label>
        <input id="insp-duration" type="number" value="${Math.round(clip.durationMs)}" style="width:100%;background:#111;border:1px solid #555;color:#eee;padding:4px;box-sizing:border-box;border-radius:3px;font-size:12px;"/>
      </div>
      <div class="inspector-field" style="margin-bottom:8px;">
        <label style="font-size:11px;color:#888;display:block;margin-bottom:2px;">Color</label>
        <input id="insp-color" type="color" value="${clip.color || '#00aaff'}" style="width:100%;height:30px;background:#111;border:1px solid #555;cursor:pointer;"/>
      </div>
      <div class="inspector-field" style="margin-bottom:8px;">
        <label style="font-size:11px;color:#888;display:block;margin-bottom:2px;">Track</label>
        <span style="font-size:12px;color:#ccc;">${track ? track.name : 'Unknown'} (${clip.type})</span>
      </div>
      <hr style="border-color:#444;margin:8px 0;"/>
      <div id="insp-params">${this._buildParamsHTML(clip)}</div>
      <div style="display:flex;gap:4px;margin-top:10px;flex-wrap:wrap;">
        <button onclick="window.timelineEditor._duplicateClip('${clipId}')" class="btn-secondary" style="font-size:11px;padding:4px 8px;">⊕ Duplicate</button>
        <button onclick="window.timelineEditor._deleteClip('${clipId}')" class="btn-secondary" style="font-size:11px;padding:4px 8px;color:#ff4444;">✕ Delete</button>
        ${clip.type === 'audio' ? `<button onclick="window.audioBrowser && window.audioBrowser.open()" class="btn-secondary" style="font-size:11px;padding:4px 8px;">🎵 Browse</button>` : ''}
        ${clip.type === 'dmx'   ? `<button onclick="window.dmxEditor && window.dmxEditor.open('${clipId}')" class="btn-secondary" style="font-size:11px;padding:4px 8px;">🎛 DMX</button>` : ''}
        ${clip.type === 'pixel' || clip.type === 'lighting' ? `<button onclick="window.ledStudio && window.ledStudio.open('${clipId}')" class="btn-secondary" style="font-size:11px;padding:4px 8px;">💡 LED</button>` : ''}
      </div>
    `;

    // Wire up inputs
    const bind = (id, prop, parser) => {
      const el = document.getElementById(id);
      if (!el) return;
      el.addEventListener('change', () => {
        clip[prop] = parser ? parser(el.value) : el.value;
        this._refreshClipEl(clipId);
        this._autosave();
        if (window.updateClipLabel && prop === 'label') window.updateClipLabel(clipId, el.value);
        if (window.updateClipStart && prop === 'startMs') window.updateClipStart(clipId, clip.startMs);
        if (window.updateClipDuration && prop === 'durationMs') window.updateClipDuration(clipId, clip.durationMs);
      });
    };

    bind('insp-label',    'label',      null);
    bind('insp-start',    'startMs',    Number);
    bind('insp-duration', 'durationMs', Number);
    bind('insp-color',    'color',      null);

    this._bindParamInputs(clip);
  }

  _buildParamsHTML(clip) {
    const p = clip.params || {};
    let html = '<div style="font-size:11px;color:#888;margin-bottom:4px;">Parameters</div>';
    switch (clip.type) {
      case 'audio':
        html += `
          <div style="margin-bottom:6px;">
            <label style="font-size:11px;color:#888;display:block;">File</label>
            <input id="param-file" type="text" value="${p.file || ''}" placeholder="audio file" style="width:100%;background:#111;border:1px solid #555;color:#eee;padding:3px;box-sizing:border-box;font-size:11px;border-radius:3px;"/>
          </div>
          <div style="margin-bottom:6px;">
            <label style="font-size:11px;color:#888;display:block;">Volume (%)</label>
            <input id="param-volume" type="range" min="0" max="100" value="${p.volume !== undefined ? p.volume : 100}" style="width:100%;"/>
          </div>
          <label style="font-size:11px;color:#888;"><input id="param-loop" type="checkbox" ${p.loop ? 'checked' : ''}/> Loop</label>`;
        break;
      case 'relay':
        html += `
          <label style="font-size:11px;color:#888;display:block;margin-bottom:4px;">Output</label>
          <select id="param-out" style="width:100%;background:#111;border:1px solid #555;color:#eee;padding:3px;font-size:11px;border-radius:3px;">
            <option value="out1" ${p.out==='out1'?'selected':''}>OUT1</option>
            <option value="out2" ${p.out==='out2'?'selected':''}>OUT2</option>
          </select>
          <label style="font-size:11px;color:#888;display:flex;align-items:center;gap:4px;margin-top:6px;">
            <input id="param-state" type="checkbox" ${p.state ? 'checked' : ''}/> Active (ON)
          </label>`;
        break;
      case 'lighting':
      case 'pixel':
        html += `
          <label style="font-size:11px;color:#888;display:block;margin-bottom:2px;">Color</label>
          <input id="param-rgb-color" type="color" value="${this._rgbToHex(p.r||255,p.g||255,p.b||255)}" style="width:100%;height:28px;background:#111;border:1px solid #555;cursor:pointer;margin-bottom:6px;"/>
          <label style="font-size:11px;color:#888;display:block;margin-bottom:2px;">Brightness</label>
          <input id="param-brightness" type="range" min="0" max="255" value="${p.brightness !== undefined ? p.brightness : 255}" style="width:100%;margin-bottom:6px;"/>
          <label style="font-size:11px;color:#888;display:block;margin-bottom:2px;">Effect</label>
          <select id="param-effect" style="width:100%;background:#111;border:1px solid #555;color:#eee;padding:3px;font-size:11px;border-radius:3px;">
            ${['solid','fade','pulse','flash','rainbow','chase'].map(e => `<option ${p.effect===e?'selected':''}>${e}</option>`).join('')}
          </select>`;
        break;
      default:
        html += `<pre style="font-size:10px;color:#666;overflow:auto;max-height:100px;">${JSON.stringify(p, null, 2)}</pre>`;
    }
    return html;
  }

  _bindParamInputs(clip) {
    const p = clip.params || {};
    const save = () => { clip.params = p; this._autosave(); };

    const fileEl = document.getElementById('param-file');
    if (fileEl) fileEl.addEventListener('change', () => { p.file = fileEl.value; save(); });

    const volEl = document.getElementById('param-volume');
    if (volEl) volEl.addEventListener('input', () => { p.volume = Number(volEl.value); save(); });

    const loopEl = document.getElementById('param-loop');
    if (loopEl) loopEl.addEventListener('change', () => { p.loop = loopEl.checked; save(); });

    const outEl = document.getElementById('param-out');
    if (outEl) outEl.addEventListener('change', () => { p.out = outEl.value; save(); });

    const stateEl = document.getElementById('param-state');
    if (stateEl) stateEl.addEventListener('change', () => { p.state = stateEl.checked; save(); });

    const rgbEl = document.getElementById('param-rgb-color');
    if (rgbEl) rgbEl.addEventListener('change', () => {
      const [r, g, b] = this._hexToRgb(rgbEl.value);
      p.r = r; p.g = g; p.b = b;
      clip.color = rgbEl.value;
      this._refreshClipEl(clip.id);
      save();
    });

    const brightEl = document.getElementById('param-brightness');
    if (brightEl) brightEl.addEventListener('input', () => { p.brightness = Number(brightEl.value); save(); });

    const effectEl = document.getElementById('param-effect');
    if (effectEl) effectEl.addEventListener('change', () => { p.effect = effectEl.value; save(); });
  }

  /* ── Track operations ────────────────────────────────────────── */

  addTrack(type = 'audio', name = null) {
    this._pushUndo();

    const order = this._tracks().length;
    const typeCount = this._tracks().filter(track => track.type === type).length;
    const title = type === 'mixed' ? 'Lane' : type.charAt(0).toUpperCase() + type.slice(1);
    const trackName = name || (type === 'mixed'
      ? `Lane ${typeCount + 1}`
      : `${title} Track ${typeCount + 1}`);
    const track = SHDOModel.createTrack(type, trackName, order);

    this._project().tracks.push(track);
    this._renderTracks();

    requestAnimationFrame(() => {
      this._canvasScroll.scrollTop = this._canvasScroll.scrollHeight;
      this._trackListEl.scrollTop = this._trackListEl.scrollHeight;
      const header = this._trackListEl.querySelector(`[data-track-id="${track.id}"]`);
      if (header) header.scrollIntoView({ block: 'nearest' });
    });

    this._autosave();
    this._log(`Track added: ${track.name}`, 'INFO');
    return track;
  }

  _deleteTrack(id) {
    if (!confirm('Delete this track and all its clips?')) return;
    this._pushUndo();
    this._project().tracks = this._tracks().filter(t => t.id !== id);
    this._project().clips  = this._clips().filter(c => c.trackId !== id);
    this._renderTracks();
    this._autosave();
    this._log('Track deleted', 'INFO');
  }

  _duplicateTrack(id) {
    this._pushUndo();
    const src = this._tracks().find(t => t.id === id);
    if (!src) return;
    const newTrack = { ...src, id: SHDOModel._uuid(), name: src.name + ' (copy)', order: this._tracks().length };
    this._project().tracks.push(newTrack);
    const newClips = this._clips().filter(c => c.trackId === id).map(c => ({
      ...c, id: SHDOModel._uuid(), trackId: newTrack.id
    }));
    this._project().clips.push(...newClips);
    this._renderTracks();
    this._autosave();
  }

  _toggleMute(id) {
    const t = this._tracks().find(t => t.id === id);
    if (t) { t.muted = !t.muted; this._renderTracks(); this._autosave(); }
  }

  _toggleVisible(id) {
    const t = this._tracks().find(t => t.id === id);
    if (t) { t.visible = !t.visible; this._renderTracks(); this._autosave(); }
  }

  _toggleLock(id) {
    const t = this._tracks().find(t => t.id === id);
    if (t) { t.locked = !t.locked; this._renderTracks(); this._autosave(); }
  }

  _renameTrack(id, nameEl) {
    const t = this._tracks().find(t => t.id === id);
    if (!t) return;
    const input = document.createElement('input');
    input.type = 'text';
    input.value = t.name;
    input.style.cssText = 'font-size:11px;width:90px;background:#111;border:1px solid #00ffcc;color:#eee;padding:2px;';
    nameEl.replaceWith(input);
    input.focus();
    input.select();
    const done = () => {
      t.name = input.value || t.name;
      input.replaceWith(nameEl);
      nameEl.textContent = t.name;
      this._autosave();
    };
    input.addEventListener('blur', done);
    input.addEventListener('keydown', e => { if (e.key === 'Enter') done(); });
  }

  /* ── Clip operations ─────────────────────────────────────────── */

  _addClip(trackId, type, startMs = 0, durationMs = 5000) {
    this._pushUndo();
    const clip = SHDOModel.createClip(trackId, type, startMs, durationMs);
    this._project().clips.push(clip);
    this._renderClip(clip);
    this._selectClip(clip.id);
    this._autosave();
    this._log(`Clip added: ${clip.label}`, 'INFO');
    return clip;
  }

  _deleteClip(id) {
    this._pushUndo();
    this._project().clips = this._clips().filter(c => c.id !== id);
    const el = this._canvas.querySelector(`[data-clip-id="${id}"]`);
    if (el) el.remove();
    if (this._selectedClipId === id) {
      this._selectedClipId = null;
      this._inspectorPanel.innerHTML = '<h3 style="margin:0 0 8px;color:#00ffcc;font-size:14px;">Inspector</h3><p style="color:#888;font-size:12px;">Select a clip to inspect.</p>';
    }
    this._autosave();
    this._log('Clip deleted', 'INFO');
  }

  _duplicateClip(id) {
    this._pushUndo();
    const src = this._clips().find(c => c.id === id);
    if (!src) return;
    const copy = {
      ...src,
      id: SHDOModel._uuid(),
      startMs: src.startMs + src.durationMs,
      params: { ...(src.params || {}) },
      routing: src.routing && typeof src.routing === 'object' ? { ...src.routing } : { nodeId: '', output: '' }
    };
    this._project().clips.push(copy);
    this._renderClip(copy);
    this._selectClip(copy.id);
    this._autosave();
  }

  _showTimelineMoreMenu(anchor) {
    const existing = document.getElementById('tl-more-menu');
    if (existing) {
      existing.remove();
      return;
    }

    const selectedClip = this.getSelectedClip();
    const menu = document.createElement('div');
    menu.id = 'tl-more-menu';
    menu.setAttribute('role', 'menu');

    const rect = anchor.getBoundingClientRect();
    menu.style.cssText = `
      position:fixed;
      top:${rect.bottom + 6}px;
      left:${Math.max(8, Math.min(rect.left, window.innerWidth - 210))}px;
      width:200px;
      padding:5px;
      background:#202a32;
      border:1px solid #465764;
      border-radius:6px;
      box-shadow:0 10px 28px rgba(0,0,0,.55);
      z-index:10000;
    `;

    const addItem = (label, action, enabled = true) => {
      const button = document.createElement('button');
      button.type = 'button';
      button.setAttribute('role', 'menuitem');
      button.textContent = label;
      button.disabled = !enabled;
      button.style.cssText = `
        display:block;
        width:100%;
        padding:8px 10px;
        border:0;
        border-radius:4px;
        background:transparent;
        color:${enabled ? '#e8f0f2' : '#6d7b82'};
        text-align:left;
        font:12px monospace;
        cursor:${enabled ? 'pointer' : 'not-allowed'};
      `;

      if (enabled) {
        button.addEventListener('mouseenter', () => { button.style.background = '#30404b'; });
        button.addEventListener('mouseleave', () => { button.style.background = 'transparent'; });
        button.addEventListener('click', () => {
          menu.remove();
          action();
        });
      }

      menu.appendChild(button);
    };

    addItem('Inspector', () => {
      if (window.ShowduinoInspectorDrawer) {
        window.ShowduinoInspectorDrawer.open(selectedClip.id);
      }
    }, Boolean(selectedClip));

    addItem('Duplicate clip', () => this._duplicateClip(selectedClip.id), Boolean(selectedClip));
    addItem('Delete clip', () => this._deleteClip(selectedClip.id), Boolean(selectedClip));

    document.body.appendChild(menu);

    const closeMenu = event => {
      if (!menu.contains(event.target) && event.target !== anchor) menu.remove();
    };
    setTimeout(() => document.addEventListener('pointerdown', closeMenu, { once: true }), 0);
  }

  _showClipContextMenu(e, clipId) {
    const existing = document.getElementById('tl-context-menu');
    if (existing) existing.remove();

    const menu = document.createElement('div');
    menu.id = 'tl-context-menu';
    menu.style.cssText = `position:fixed;top:${e.clientY}px;left:${e.clientX}px;background:#333;border:1px solid #555;border-radius:4px;z-index:9999;min-width:130px;box-shadow:0 4px 12px rgba(0,0,0,0.5);`;

    const item = (label, fn) => {
      const li = document.createElement('div');
      li.textContent = label;
      li.style.cssText = 'padding:6px 12px;font-size:12px;cursor:pointer;color:#eee;';
      li.addEventListener('mouseenter', () => li.style.background='#444');
      li.addEventListener('mouseleave', () => li.style.background='');
      li.addEventListener('click', () => { menu.remove(); fn(); });
      menu.appendChild(li);
    };

    item('✏️ Inspect',    () => {
      this._selectClip(clipId);
      if (window.ShowduinoInspectorDrawer) window.ShowduinoInspectorDrawer.open(clipId);
    });
    item('⊕ Duplicate',  () => this._duplicateClip(clipId));
    item('✂️ Split at Playhead', () => this._splitClip(clipId));
    item('✕ Delete',     () => this._deleteClip(clipId));

    document.body.appendChild(menu);
    setTimeout(() => document.addEventListener('click', () => menu.remove(), { once: true }), 0);
  }

  _splitClip(clipId) {
    const clip = this._clips().find(c => c.id === clipId);
    if (!clip) return;
    const ph = this._state.playhead;
    if (ph <= clip.startMs || ph >= clip.startMs + clip.durationMs) return;
    this._pushUndo();

    const leftDur = ph - clip.startMs;
    const rightStart = ph;
    const rightDur = clip.durationMs - leftDur;

    clip.durationMs = leftDur;
    const rightClip = { ...clip, id: SHDOModel._uuid(), startMs: rightStart, durationMs: rightDur, params: { ...clip.params } };
    this._project().clips.push(rightClip);
    this._renderClip(rightClip);
    this._refreshClipEl(clipId);
    this._autosave();
  }

  /* ── Markers ─────────────────────────────────────────────────── */

  _addMarkerAtPlayhead() {
    const label = prompt('Marker label:', 'Marker ' + (this._markers().length + 1));
    if (label === null) return;
    const marker = SHDOModel.createMarker(this._state.playhead, label);
    this._project().markers.push(marker);
    this._updateMarkers();
    this._autosave();
  }

  _updateMarkers() {
    this._markerTrackEl.innerHTML = '';
    this._markers().forEach(m => {
      const x = this._msToX(m.timeMs);
      const el = document.createElement('div');
      el.style.cssText = `position:absolute;left:${x}px;top:0;height:100%;display:flex;align-items:center;`;

      const line = document.createElement('div');
      line.style.cssText = `position:absolute;left:0;top:0;width:2px;height:100%;background:${m.color || '#ffff00'};`;

      const lbl = document.createElement('div');
      lbl.textContent = m.label;
      lbl.style.cssText = `position:absolute;left:4px;top:2px;font-size:10px;color:${m.color || '#ffff00'};white-space:nowrap;background:#1e1e1e;padding:0 2px;`;

      el.appendChild(line);
      el.appendChild(lbl);

      // Click to jump
      el.style.cursor = 'pointer';
      el.addEventListener('click', () => { this._state.playhead = m.timeMs; this._syncPlayheadPosition(); });
      // Right-click to delete
      el.addEventListener('contextmenu', e => {
        e.preventDefault();
        if (confirm(`Delete marker "${m.label}"?`)) {
          this._project().markers = this._markers().filter(x => x.id !== m.id);
          this._updateMarkers();
          this._autosave();
        }
      });
      this._markerTrackEl.appendChild(el);
    });
  }

  /* ── Ruler ───────────────────────────────────────────────────── */

  _rulerClick(e) {
    const rect = this._rulerEl.getBoundingClientRect();
    const x = e.clientX - rect.left + this._scrollLeft;
    this._state.playhead = Math.max(0, this._xToMs(x));
    this._syncPlayheadPosition();
    this._updateTimecodeDisplay();
  }

  _updateRuler() {
    this._rulerEl.innerHTML = '';
    this._rulerEl.appendChild(this._rulerPlayhead);

    const canvasWidth = this._canvas.offsetWidth || 3000;
    const startMs = this._xToMs(this._scrollLeft);
    const endMs = startMs + this._xToMs(canvasWidth + this._scrollLeft) - startMs;

    let step = 1000;
    if (this._pxPerMs < 0.01) step = 30000;
    else if (this._pxPerMs < 0.05) step = 10000;
    else if (this._pxPerMs < 0.2)  step = 5000;
    else if (this._pxPerMs < 0.5)  step = 2000;
    else step = 1000;

    const first = Math.floor(startMs / step) * step;
    for (let t = first; t <= endMs + step; t += step) {
      const x = this._msToX(t) - this._scrollLeft;
      if (x < -20 || x > canvasWidth + 20) continue;
      const tick = document.createElement('div');
      tick.style.cssText = `position:absolute;left:${x}px;top:0;width:1px;height:100%;background:#555;`;
      this._rulerEl.appendChild(tick);
      const lbl = document.createElement('div');
      lbl.textContent = this._formatMs(t);
      lbl.style.cssText = `position:absolute;left:${x + 3}px;top:4px;font-size:10px;color:#888;white-space:nowrap;`;
      this._rulerEl.appendChild(lbl);
    }
  }

  /* ── Grid ────────────────────────────────────────────────────── */

  _updateGridCanvas() {
    if (!this._gridEl) return;
    const w = this._canvas.offsetWidth || 3000;
    const h = this._canvas.offsetHeight || 300;
    this._gridEl.width = w;
    this._gridEl.height = h;
    const ctx = this._gridEl.getContext('2d');
    ctx.clearRect(0, 0, w, h);
    if (!this._gridEnabled) return;

    ctx.strokeStyle = 'rgba(255,255,255,0.06)';
    ctx.lineWidth = 1;

    let step = this._snapMs || 1000;
    if (this._pxPerMs * step < 5) step *= 4;
    if (this._pxPerMs * step < 5) step *= 10;

    const startMs = this._xToMs(this._scrollLeft);
    const first = Math.floor(startMs / step) * step;
    for (let t = first; t <= startMs + w / this._pxPerMs; t += step) {
      const x = this._msToX(t);
      ctx.beginPath();
      ctx.moveTo(x, 0);
      ctx.lineTo(x, h);
      ctx.stroke();
    }
  }

  /* ── Zoom ────────────────────────────────────────────────────── */

  _zoom(factor) {
    const newPx = Math.min(2, Math.max(0.005, this._pxPerMs * factor));
    const scrollMs = this._xToMs(this._canvasScroll.scrollLeft);
    this._pxPerMs = newPx;
    if (this._project().config) this._project().config.zoom = newPx;
    this._canvas.style.minWidth = this._msToX(Math.max(this._timelineWidth, 60000)) + 'px';
    this._updateRuler();
    this._updateGridCanvas();
    this._updateAllClips();
    this._updateMarkers();
    this._syncPlayheadPosition();
    // Restore scroll position
    this._canvasScroll.scrollLeft = this._msToX(scrollMs);
  }

  _zoomFit() {
    const dur = this._project().project.duration || 60000;
    const w = this._canvasScroll.offsetWidth || 800;
    this._pxPerMs = w / dur;
    this._zoom(1);
  }

  /* ── Playback ────────────────────────────────────────────────── */

  play() {
    if (this._playing) return;
    this._playing = true;
    this._state.transport = 'playing';
    this._playStart = Date.now();
    this._playOffset = this._state.playhead;
    document.getElementById('tl-play-btn').style.color = '#00ffcc';
    if (window.appPlay) window.appPlay();
    this._tick();
    this._log('Playback started', 'INFO');
  }

  pause() {
    if (!this._playing) return;
    this._playing = false;
    this._state.transport = 'paused';
    if (this._rafId) cancelAnimationFrame(this._rafId);
    document.getElementById('tl-play-btn').style.color = '';
    if (window.appPause) window.appPause();
    this._log('Playback paused', 'INFO');
  }

  stop() {
    this._playing = false;
    this._state.transport = 'stopped';
    if (this._rafId) cancelAnimationFrame(this._rafId);
    document.getElementById('tl-play-btn').style.color = '';
    if (window.appStop) window.appStop();
    this._log('Playback stopped', 'INFO');
  }

  rewind() {
    this.stop();
    this._state.playhead = 0;
    this._syncPlayheadPosition();
    this._updateTimecodeDisplay();
  }

  _toggleLoop() {
    if (!this._project().config) this._project().config = {};
    this._project().config.loop = !this._project().config.loop;
    this._loopBtn.style.color = this._project().config.loop ? '#00ffcc' : '';
    this._autosave();
  }

  _tick() {
    if (!this._playing) return;
    const elapsed = Date.now() - this._playStart;
    const pos = this._playOffset + elapsed;
    const dur = this._project().project.duration || 300000;

    if (pos >= dur) {
      if (this._project().config && this._project().config.loop) {
        this._playStart = Date.now();
        this._playOffset = 0;
        this._state.playhead = 0;
      } else {
        this.stop();
        return;
      }
    } else {
      this._state.playhead = pos;
    }

    this._syncPlayheadPosition();
    this._updateTimecodeDisplay();
    this._highlightActiveClips();

    // Auto-scroll to keep playhead visible
    const phX = this._msToX(this._state.playhead);
    const scrollLeft = this._canvasScroll.scrollLeft;
    const viewWidth = this._canvasScroll.offsetWidth;
    if (phX > scrollLeft + viewWidth - 40) {
      this._canvasScroll.scrollLeft = phX - 40;
    }

    this._rafId = requestAnimationFrame(() => this._tick());
  }

  _highlightActiveClips() {
    const ph = this._state.playhead;
    this._canvas.querySelectorAll('.tl-clip').forEach(el => {
      const id = el.dataset.clipId;
      const clip = this._clips().find(c => c.id === id);
      if (clip) {
        const active = ph >= clip.startMs && ph <= clip.startMs + clip.durationMs;
        el.style.boxShadow = active ? `0 0 6px ${clip.color || '#fff'}` : (id === this._selectedClipId ? '0 0 8px rgba(255,255,255,0.5)' : '');
      }
    });
  }

  _syncPlayheadPosition() {
    const x = this._msToX(this._state.playhead);
    if (this._playheadEl)    this._playheadEl.style.left = x + 'px';
    if (this._rulerPlayhead) this._rulerPlayhead.style.left = (x - this._scrollLeft) + 'px';
  }

  _updateTimecodeDisplay() {
    const tc = document.getElementById('tl-timecode');
    if (tc) tc.textContent = this._formatMs(this._state.playhead);
  }

  /* ── Helpers ─────────────────────────────────────────────────── */

  _msToX(ms)  { return ms * this._pxPerMs; }
  _xToMs(x)   { return x / this._pxPerMs; }

  _snapValue(ms) {
    if (!this._snapEnabled || !this._snapMs) return ms;
    return Math.round(ms / this._snapMs) * this._snapMs;
  }

  _overlaps(excludeId, trackId, startMs, durationMs) {
    const end = startMs + durationMs;
    return this._clips().some(c =>
      c.id !== excludeId && c.trackId === trackId &&
      startMs < c.startMs + c.durationMs && end > c.startMs
    );
  }

  _getClipTrack(clipId) {
    const clip = this._clips().find(c => c.id === clipId);
    return clip ? this._tracks().find(t => t.id === clip.trackId) : null;
  }

  _formatMs(ms) {
    const total = Math.floor(ms);
    const min   = Math.floor(total / 60000);
    const sec   = Math.floor((total % 60000) / 1000);
    const milli = total % 1000;
    return `${String(min).padStart(2,'0')}:${String(sec).padStart(2,'0')}.${String(milli).padStart(3,'0')}`;
  }

  _hexToRgb(hex) {
    hex = hex.replace('#','');
    return [parseInt(hex.slice(0,2),16), parseInt(hex.slice(2,4),16), parseInt(hex.slice(4,6),16)];
  }

  _rgbToHex(r, g, b) {
    return '#' + [r,g,b].map(v => Math.max(0,Math.min(255,v)).toString(16).padStart(2,'0')).join('');
  }

  /* ── Snap / Grid toggles ─────────────────────────────────────── */

  _toggleSnap() {
    this._snapEnabled = !this._snapEnabled;
    this._snapBtn.textContent = `⊞ Snap: ${this._snapEnabled ? 'ON' : 'OFF'}`;
    this._snapBtn.style.color = this._snapEnabled ? '#00ffcc' : '#888';
  }

  _toggleGrid() {
    this._gridEnabled = !this._gridEnabled;
    this._gridBtn.textContent = `⋮ Grid: ${this._gridEnabled ? 'ON' : 'OFF'}`;
    this._gridBtn.style.color = this._gridEnabled ? '#00ffcc' : '#888';
    this._updateGridCanvas();
  }

  /* ── Undo / Redo ─────────────────────────────────────────────── */

  _pushUndo() {
    this._undoStack.push(JSON.stringify(this._project()));
    if (this._undoStack.length > 50) this._undoStack.shift();
    this._redoStack = [];
  }

  _undo() {
    if (!this._undoStack.length) return;
    this._redoStack.push(JSON.stringify(this._project()));
    this._state.project = JSON.parse(this._undoStack.pop());
    this._renderTracks();
    this._log('Undo', 'INFO');
  }

  _redo() {
    if (!this._redoStack.length) return;
    this._undoStack.push(JSON.stringify(this._project()));
    this._state.project = JSON.parse(this._redoStack.pop());
    this._renderTracks();
    this._log('Redo', 'INFO');
  }

  /* ── Keyboard shortcuts ──────────────────────────────────────── */

  _bindGlobalKeys() {
    document.addEventListener('keydown', e => {
      if (e.target && (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA' || e.target.tagName === 'SELECT')) return;
      if (e.code === 'Space')  { e.preventDefault(); this._playing ? this.pause() : this.play(); }
      if (e.key === 'Delete' && this._selectedClipId) this._deleteClip(this._selectedClipId);
      if (e.ctrlKey && e.key === 'z') { e.preventDefault(); this._undo(); }
      if (e.ctrlKey && e.key === 'y') { e.preventDefault(); this._redo(); }
      if (e.ctrlKey && e.key === 's') { e.preventDefault(); this._saveToLocalStorage(); if (window.saveProject) window.saveProject(); }
      if (e.ctrlKey && e.key === '+') { e.preventDefault(); this._zoom(1.5); }
      if (e.ctrlKey && e.key === '-') { e.preventDefault(); this._zoom(1/1.5); }
    });
  }

  /* ── Save / Load ─────────────────────────────────────────────── */

  _autosave() {
    if (!this._project()) return;
    this._project().project.updatedAt = new Date().toISOString();
    localStorage.setItem('showduino_current_project', JSON.stringify(this._project()));
  }

  _startAutosave() {
    const saved = localStorage.getItem('showduino_current_project');
    if (saved) {
      try {
        const p = JSON.parse(saved);
        if (SHDOModel.validate(p)) {
          this._state.project = SHDOModel.migrate(p);
          this._log('Restored project from autosave', 'INFO');
        }
      } catch (_) {}
    }
    setInterval(() => this._autosave(), 30000);
  }

  _saveToLocalStorage() {
    this._autosave();
    const id = this._project().project.id;
    localStorage.setItem(`showduino_project_${id}`, JSON.stringify(this._project()));
    this._log(`Project saved: ${this._project().project.name}`, 'INFO');
    this._showToast('Project saved ✓');
  }

  _exportJSON() {
    const json = JSON.stringify(this._project(), null, 2);
    const blob = new Blob([json], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = (this._project().project.name || 'project') + '.shdo';
    a.click();
    URL.revokeObjectURL(url);
    this._log('Project exported', 'INFO');
  }

  _importJSON() {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.shdo,.json';
    input.addEventListener('change', () => {
      const file = input.files[0];
      if (!file) return;
      const reader = new FileReader();
      reader.onload = e => {
        try {
          const p = JSON.parse(e.target.result);
          if (!SHDOModel.validate(p)) throw new Error('Invalid project format');
          this._pushUndo();
          this._state.project = SHDOModel.migrate(p);
          this._render();
          this._log(`Project imported: ${p.project.name}`, 'INFO');
          this._showToast('Project imported ✓');
        } catch (err) {
          this._log(`Import failed: ${err.message}`, 'ERR');
          alert('Invalid .shdo file: ' + err.message);
        }
      };
      reader.readAsText(file);
    });
    input.click();
  }

  _newProject() {
    if (!confirm('Create new project? Unsaved changes will be lost.')) return;
    this._pushUndo();
    this._state.project = SHDOModel.createProject('Untitled Show');
    this._render();
    this._log('New project created', 'INFO');
  }

  _openProjectBrowser() {
    const modal = this._buildProjectBrowserModal();
    document.body.appendChild(modal);
    modal.style.display = 'flex';
  }

  _buildProjectBrowserModal() {
    const overlay = document.createElement('div');
    overlay.style.cssText = 'position:fixed;inset:0;background:rgba(0,0,0,0.7);display:flex;align-items:center;justify-content:center;z-index:9000;';

    const box = document.createElement('div');
    box.style.cssText = 'background:#2a2a2a;border:1px solid #444;border-radius:8px;padding:20px;width:500px;max-width:95vw;max-height:80vh;display:flex;flex-direction:column;gap:12px;';

    const title = document.createElement('h3');
    title.textContent = 'Project Browser';
    title.style.cssText = 'margin:0;color:#00ffcc;font-size:16px;';
    box.appendChild(title);

    // List local projects
    const list = document.createElement('div');
    list.style.cssText = 'flex:1;overflow-y:auto;display:flex;flex-direction:column;gap:6px;max-height:300px;';

    const projects = [];
    for (let i = 0; i < localStorage.length; i++) {
      const key = localStorage.key(i);
      if (key && key.startsWith('showduino_project_')) {
        try {
          const p = JSON.parse(localStorage.getItem(key));
          if (p && p.project) projects.push(p);
        } catch (_) {}
      }
    }

    if (projects.length === 0) {
      list.innerHTML = '<p style="color:#666;font-size:12px;">No saved projects found.</p>';
    } else {
      projects.sort((a, b) => new Date(b.project.updatedAt) - new Date(a.project.updatedAt)).forEach(p => {
        const row = document.createElement('div');
        row.style.cssText = 'display:flex;align-items:center;gap:8px;background:#333;padding:8px;border-radius:4px;';
        const info = document.createElement('div');
        info.style.flex = '1';
        info.innerHTML = `<div style="font-size:13px;color:#eee;">${p.project.name}</div><div style="font-size:10px;color:#666;">${new Date(p.project.updatedAt).toLocaleString()}</div>`;
        const openBtn = document.createElement('button');
        openBtn.textContent = 'Open';
        openBtn.className = 'btn-primary';
        openBtn.style.cssText = 'font-size:11px;padding:3px 8px;';
        openBtn.addEventListener('click', () => {
          this._pushUndo();
          this._state.project = SHDOModel.migrate(p);
          this._render();
          overlay.remove();
          this._log(`Opened: ${p.project.name}`, 'INFO');
        });
        const delBtn = document.createElement('button');
        delBtn.textContent = '✕';
        delBtn.className = 'btn-secondary';
        delBtn.style.cssText = 'font-size:11px;padding:3px 6px;color:#ff4444;';
        delBtn.addEventListener('click', () => {
          if (confirm(`Delete "${p.project.name}"?`)) {
            localStorage.removeItem(`showduino_project_${p.project.id}`);
            row.remove();
          }
        });
        row.appendChild(info);
        row.appendChild(openBtn);
        row.appendChild(delBtn);
        list.appendChild(row);
      });
    }
    box.appendChild(list);

    const footer = document.createElement('div');
    footer.style.cssText = 'display:flex;gap:8px;justify-content:flex-end;';
    const newBtn = document.createElement('button');
    newBtn.textContent = '＋ New Project';
    newBtn.className = 'btn-primary';
    newBtn.addEventListener('click', () => { overlay.remove(); this._newProject(); });
    const closeBtn = document.createElement('button');
    closeBtn.textContent = 'Close';
    closeBtn.className = 'btn-secondary';
    closeBtn.addEventListener('click', () => overlay.remove());
    footer.appendChild(newBtn);
    footer.appendChild(closeBtn);
    box.appendChild(footer);

    overlay.appendChild(box);
    overlay.addEventListener('click', e => { if (e.target === overlay) overlay.remove(); });
    return overlay;
  }

  /* ── Toast ───────────────────────────────────────────────────── */

  _showToast(msg) {
    const t = document.createElement('div');
    t.textContent = msg;
    t.style.cssText = 'position:fixed;bottom:60px;right:20px;background:#00ffcc;color:#000;padding:8px 16px;border-radius:4px;font-size:13px;z-index:9999;font-weight:bold;';
    document.body.appendChild(t);
    setTimeout(() => t.remove(), 2000);
  }

  /* ── Public helpers ──────────────────────────────────────────── */

  getSelectedClip() {
    return this._clips().find(c => c.id === this._selectedClipId) || null;
  }

  setSelectedClipFile(file) {
    const clip = this.getSelectedClip();
    if (clip && clip.type === 'audio') {
      clip.params = clip.params || {};
      clip.params.file = file;
      const fileEl = document.getElementById('param-file');
      if (fileEl) fileEl.value = file;
      this._autosave();
    }
  }

  setClipParams(clipId, params) {
    const clip = this._clips().find(c => c.id === clipId);
    if (clip) { clip.params = { ...clip.params, ...params }; this._autosave(); }
  }

  getCurrentTimeMs() { return this._state.playhead; }

  seekTo(ms) {
    this._state.playhead = Math.max(0, ms);
    if (this._playing) {
      this._playStart = Date.now();
      this._playOffset = this._state.playhead;
    }
    this._syncPlayheadPosition();
    this._updateTimecodeDisplay();
  }
}

window.TimelineEditor = TimelineEditor;
