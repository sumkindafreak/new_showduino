/* Showduino Studio — desktop authoring UX pass
 *
 * Makes the existing DAW timeline behave the way users naturally expect:
 * drag an action from the top shelf onto a timeline lane, or click it to add
 * the cue at the current playhead. The timeline engine already supports
 * blockType drops; this file exposes that capability without changing SHDO v2.
 */
(function () {
  'use strict';

  const DESKTOP_QUERY = '(min-width: 761px)';
  const STYLE_ID = 'showduino-desktop-ux-style';
  const SHELF_CLASS = 'showduino-cue-shelf';

  const CUES = Object.freeze([
    { type: 'audio',   icon: '♪', name: 'Sound',         hint: 'Audio, ambience, music', trackType: 'audio',    duration: 5000 },
    { type: 'mosfet',  icon: '☀', name: 'Lighting',      hint: 'On, dim or pulse a light', trackType: 'lighting', duration: 1000 },
    { type: 'relay',   icon: '⚙', name: 'Prop / Switch', hint: 'Prop, solenoid or contactor', trackType: 'relay', duration: 500 },
    { type: 'pixel',   icon: '✦', name: 'Pixels / LEDs', hint: 'Colour and animated LED FX', trackType: 'pixel', duration: 3000 },
    { type: 'trigger', icon: '◎', name: 'Trigger',       hint: 'Start or fire an event', trackType: 'trigger', duration: 250 },
    { type: 'fx',      icon: '◈', name: 'Other Effect',  hint: 'Fog, air, motor, servo…', trackType: 'fx', duration: 1500 }
  ]);

  const ACCEPTS = Object.freeze({
    mixed:   ['audio', 'mosfet', 'relay', 'pixel', 'trigger', 'fx'],
    audio:   ['audio'],
    lighting:['mosfet', 'pixel'],
    relay:   ['relay'],
    prop:    ['relay', 'fx'],
    pixel:   ['pixel'],
    trigger: ['trigger'],
    fx:      ['fx']
  });

  let activeDropRow = null;
  let toastTimer = 0;

  function isDesktop() {
    return window.matchMedia(DESKTOP_QUERY).matches;
  }

  function injectStyles() {
    if (document.getElementById(STYLE_ID)) return;

    const style = document.createElement('style');
    style.id = STYLE_ID;
    style.textContent = `
      @media (min-width:761px) {
        .timeline-editor.${SHELF_CLASS}-ready {
          background:#10161a !important;
        }

        .showduino-cue-shelf {
          flex:0 0 auto;
          padding:.72rem .78rem .68rem;
          border-bottom:1px solid #24333b;
          background:linear-gradient(180deg,#111a1f,#0d1418);
        }

        .showduino-cue-shelf-head {
          display:flex;
          align-items:flex-end;
          justify-content:space-between;
          gap:1rem;
          margin-bottom:.56rem;
        }

        .showduino-cue-shelf-title strong {
          display:block;
          color:#edf5f7;
          font:800 .76rem/1.2 Inter,system-ui,sans-serif;
          letter-spacing:.01em;
        }

        .showduino-cue-shelf-title span {
          display:block;
          margin-top:.18rem;
          color:#718690;
          font:650 .58rem/1.35 Inter,system-ui,sans-serif;
        }

        .showduino-cue-shelf-help {
          flex:0 0 auto;
          color:#00d9ad;
          font:800 .54rem/1 ui-monospace,SFMono-Regular,Menlo,monospace;
          letter-spacing:.06em;
          white-space:nowrap;
        }

        .showduino-cue-shelf-grid {
          display:grid;
          grid-template-columns:repeat(6,minmax(108px,1fr));
          gap:.44rem;
        }

        .showduino-cue-tool {
          min-width:0;
          min-height:58px;
          display:grid;
          grid-template-columns:32px minmax(0,1fr);
          gap:.48rem;
          align-items:center;
          padding:.5rem .56rem;
          border:1px solid #2a3d46;
          border-radius:10px;
          background:linear-gradient(145deg,#121c21,#0c1317);
          color:#dce8eb;
          text-align:left;
          cursor:grab;
          user-select:none;
          transition:border-color .12s ease,transform .12s ease,background .12s ease;
        }

        .showduino-cue-tool:hover {
          border-color:rgba(0,255,200,.42);
          background:linear-gradient(145deg,#14252a,#0d181c);
          transform:translateY(-1px);
        }

        .showduino-cue-tool:active { cursor:grabbing; transform:translateY(0); }

        .showduino-cue-tool-icon {
          width:30px;
          height:30px;
          display:grid;
          place-items:center;
          border:1px solid #30474f;
          border-radius:8px;
          background:#081115;
          color:#00ffc8;
          font:700 .98rem/1 Inter,system-ui,sans-serif;
        }

        .showduino-cue-tool-copy { min-width:0; }
        .showduino-cue-tool-copy strong {
          display:block;
          overflow:hidden;
          text-overflow:ellipsis;
          white-space:nowrap;
          color:#e9f2f4;
          font:800 .65rem/1.15 Inter,system-ui,sans-serif;
        }
        .showduino-cue-tool-copy span {
          display:block;
          margin-top:.16rem;
          overflow:hidden;
          text-overflow:ellipsis;
          white-space:nowrap;
          color:#6f848e;
          font:600 .51rem/1.2 Inter,system-ui,sans-serif;
        }

        .showduino-cue-shelf-foot {
          display:flex;
          align-items:center;
          justify-content:space-between;
          gap:.8rem;
          margin-top:.48rem;
          color:#647983;
          font:650 .53rem/1.3 Inter,system-ui,sans-serif;
        }

        .showduino-add-lane {
          min-height:28px;
          padding:.28rem .52rem;
          border:1px solid #2a3f48;
          border-radius:7px;
          background:#0b1418;
          color:#9fb0b6;
          font:750 .54rem/1 Inter,system-ui,sans-serif;
          cursor:pointer;
        }

        .timeline-toolbar.showduino-toolbar-simplified {
          min-height:34px;
          padding:5px 8px !important;
          background:#182127 !important;
          border-bottom-color:#2a3941 !important;
        }

        .showduino-hidden-track-tools { display:none !important; }

        .tl-track-row.showduino-drop-target {
          outline:2px solid rgba(0,255,200,.75);
          outline-offset:-2px;
          background:rgba(0,255,200,.08) !important;
          z-index:2;
        }

        .tl-track-row.showduino-drop-target::after {
          content:'DROP CUE HERE';
          position:absolute;
          right:10px;
          top:50%;
          transform:translateY(-50%);
          padding:4px 7px;
          border:1px solid rgba(0,255,200,.35);
          border-radius:5px;
          background:rgba(2,15,13,.86);
          color:#78ffe0;
          font:850 9px/1 ui-monospace,SFMono-Regular,Menlo,monospace;
          letter-spacing:.08em;
          pointer-events:none;
        }

        body.showduino-cue-dragging .tl-track-row { cursor:copy; }

        .showduino-desktop-toast {
          position:fixed;
          left:50%;
          bottom:28px;
          z-index:30000;
          transform:translateX(-50%) translateY(8px);
          padding:.58rem .78rem;
          border:1px solid rgba(0,255,200,.38);
          border-radius:9px;
          background:rgba(5,15,18,.96);
          box-shadow:0 12px 38px rgba(0,0,0,.35);
          color:#dffff7;
          opacity:0;
          pointer-events:none;
          transition:opacity .16s ease,transform .16s ease;
          font:750 .62rem/1.3 Inter,system-ui,sans-serif;
        }

        .showduino-desktop-toast.visible {
          opacity:1;
          transform:translateX(-50%) translateY(0);
        }
      }

      @media (min-width:761px) and (max-width:1180px) {
        .showduino-cue-shelf-grid { grid-template-columns:repeat(3,minmax(120px,1fr)); }
      }
    `;
    document.head.appendChild(style);
  }

  function getTimeline() {
    return window.timelineEditor || null;
  }

  function timeLabel(ms) {
    const total = Math.max(0, Math.round(Number(ms) || 0));
    const minutes = Math.floor(total / 60000);
    const seconds = Math.floor((total % 60000) / 1000);
    const millis = total % 1000;
    return `${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}.${String(millis).padStart(3, '0')}`;
  }

  function showToast(message) {
    let toast = document.querySelector('.showduino-desktop-toast');
    if (!toast) {
      toast = document.createElement('div');
      toast.className = 'showduino-desktop-toast';
      document.body.appendChild(toast);
    }

    toast.textContent = message;
    toast.classList.add('visible');
    window.clearTimeout(toastTimer);
    toastTimer = window.setTimeout(() => toast.classList.remove('visible'), 1800);
  }

  function accepts(track, cueType) {
    if (!track) return false;
    const allowed = ACCEPTS[String(track.type || 'mixed').toLowerCase()] || [];
    return allowed.includes(cueType);
  }

  function findBestTrack(timeline, cue) {
    const tracks = typeof timeline._tracks === 'function' ? timeline._tracks() : [];
    const selected = tracks.find((track) => track.id === timeline._selectedTrackId);
    if (accepts(selected, cue.type) && !selected.locked) return selected;

    const preferred = tracks.find((track) => {
      return String(track.type || '').toLowerCase() === cue.trackType && !track.locked;
    });
    if (preferred) return preferred;

    const compatible = tracks.find((track) => accepts(track, cue.type) && !track.locked);
    return compatible || null;
  }

  function ensureTrack(timeline, cue) {
    let track = findBestTrack(timeline, cue);
    if (track) return track;

    if (typeof timeline.addTrack !== 'function') return null;
    const before = typeof timeline._tracks === 'function' ? timeline._tracks().length : 0;
    const laneName = `${cue.name} Lane`;
    timeline.addTrack(cue.trackType, laneName);

    const tracks = typeof timeline._tracks === 'function' ? timeline._tracks() : [];
    track = tracks[before] || tracks[tracks.length - 1] || null;
    return track;
  }

  function focusAddedClip(timeline, clipId, startMs) {
    window.requestAnimationFrame(() => {
      const scroll = timeline._canvasScroll;
      if (scroll && typeof timeline._msToX === 'function') {
        const x = timeline._msToX(startMs);
        const target = Math.max(0, x - (scroll.clientWidth * 0.35));
        scroll.scrollTo({ left: target, behavior: 'smooth' });
      }

      const clipEl = clipId
        ? document.querySelector(`.tl-clip[data-clip-id="${CSS.escape(clipId)}"]`)
        : null;
      if (clipEl) clipEl.scrollIntoView({ block: 'nearest', inline: 'nearest', behavior: 'smooth' });
    });
  }

  function addCueAtPlayhead(cue) {
    const timeline = getTimeline();
    if (!timeline || typeof timeline._addClip !== 'function') {
      showToast('Open Build Show first.');
      return;
    }

    const track = ensureTrack(timeline, cue);
    if (!track) {
      showToast('Could not create a lane for that cue.');
      return;
    }

    const startMs = Math.max(0, Number(timeline._state?.playhead || 0));
    const beforeIds = new Set((typeof timeline._clips === 'function' ? timeline._clips() : []).map((clip) => clip.id));
    timeline._addClip(track.id, cue.type, startMs, cue.duration);

    const created = (typeof timeline._clips === 'function' ? timeline._clips() : [])
      .find((clip) => !beforeIds.has(clip.id));

    focusAddedClip(timeline, created?.id || null, startMs);
    showToast(`${cue.name} added at ${timeLabel(startMs)}`);
  }

  function makeCueButton(cue) {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'showduino-cue-tool';
    button.draggable = true;
    button.dataset.cueType = cue.type;
    button.title = `Drag ${cue.name} onto a timeline lane, or click to add at the playhead`;
    button.innerHTML = `
      <span class="showduino-cue-tool-icon">${cue.icon}</span>
      <span class="showduino-cue-tool-copy"><strong>${cue.name}</strong><span>${cue.hint}</span></span>
    `;

    button.addEventListener('click', () => addCueAtPlayhead(cue));
    button.addEventListener('dragstart', (event) => {
      if (!event.dataTransfer) return;
      event.dataTransfer.effectAllowed = 'copy';
      event.dataTransfer.setData('blockType', cue.type);
      event.dataTransfer.setData('text/plain', cue.type);
      document.body.classList.add('showduino-cue-dragging');
      showToast(`Drag ${cue.name} onto the timeline`);
    });
    button.addEventListener('dragend', () => {
      document.body.classList.remove('showduino-cue-dragging');
      clearDropTarget();
    });

    return button;
  }

  function buildShelf() {
    const shelf = document.createElement('section');
    shelf.className = SHELF_CLASS;
    shelf.setAttribute('aria-label', 'Add actions to show');

    const head = document.createElement('div');
    head.className = 'showduino-cue-shelf-head';
    head.innerHTML = `
      <div class="showduino-cue-shelf-title">
        <strong>What should happen?</strong>
        <span>Drag an action onto the timeline — or click it to add at the playhead.</span>
      </div>
      <span class="showduino-cue-shelf-help">DRAG ↓ OR CLICK</span>
    `;

    const grid = document.createElement('div');
    grid.className = 'showduino-cue-shelf-grid';
    CUES.forEach((cue) => grid.appendChild(makeCueButton(cue)));

    const foot = document.createElement('div');
    foot.className = 'showduino-cue-shelf-foot';
    foot.innerHTML = '<span>Showduino will create the right lane automatically when you click an action.</span>';

    const addLane = document.createElement('button');
    addLane.type = 'button';
    addLane.className = 'showduino-add-lane';
    addLane.textContent = '＋ Add empty lane';
    addLane.title = 'Advanced: add a blank mixed timeline lane';
    addLane.addEventListener('click', () => {
      const timeline = getTimeline();
      if (timeline?.addTrack) {
        timeline.addTrack('mixed', 'Lane');
        showToast('Empty lane added');
      }
    });
    foot.appendChild(addLane);

    shelf.append(head, grid, foot);
    return shelf;
  }

  function simplifyLegacyToolbar(editor) {
    const toolbar = editor.querySelector('.timeline-toolbar');
    if (!toolbar || toolbar.classList.contains('showduino-toolbar-simplified')) return;

    toolbar.classList.add('showduino-toolbar-simplified');

    const groups = Array.from(toolbar.children);
    const trackTools = groups.find((child) => child.textContent?.trim().startsWith('Add Track:'));
    if (trackTools) trackTools.classList.add('showduino-hidden-track-tools');

    // Hide the separator immediately following the old Add Track group.
    if (trackTools?.nextElementSibling && !trackTools.nextElementSibling.textContent?.trim()) {
      trackTools.nextElementSibling.classList.add('showduino-hidden-track-tools');
    }

    // The Studio top bar already owns Save / Import / Export. Hide duplicates
    // here so the timeline toolbar reads as transport/editing controls only.
    Array.from(toolbar.querySelectorAll('button')).forEach((button) => {
      const text = button.textContent.trim();
      if (text === '💾 Save' || text === '📂 Open' || text === '⬇ Export' || text === '⬆ Import') {
        button.classList.add('showduino-hidden-track-tools');
      }
    });
  }

  function enhanceTimeline() {
    if (!isDesktop()) return;

    const editor = document.querySelector('.timeline-editor');
    if (!editor) return;

    if (!editor.querySelector(`.${SHELF_CLASS}`)) {
      const toolbar = editor.querySelector('.timeline-toolbar');
      const shelf = buildShelf();
      if (toolbar) editor.insertBefore(shelf, toolbar);
      else editor.prepend(shelf);
      editor.classList.add(`${SHELF_CLASS}-ready`);
    }

    simplifyLegacyToolbar(editor);
  }

  function setDropTarget(row) {
    if (activeDropRow === row) return;
    clearDropTarget();
    activeDropRow = row;
    activeDropRow.classList.add('showduino-drop-target');
  }

  function clearDropTarget() {
    if (!activeDropRow) return;
    activeDropRow.classList.remove('showduino-drop-target');
    activeDropRow = null;
  }

  function bindGlobalDragFeedback() {
    document.addEventListener('dragover', (event) => {
      if (!document.body.classList.contains('showduino-cue-dragging')) return;
      const row = event.target instanceof Element ? event.target.closest('.tl-track-row') : null;
      if (row) setDropTarget(row);
      else clearDropTarget();
    }, true);

    document.addEventListener('drop', (event) => {
      if (!document.body.classList.contains('showduino-cue-dragging')) return;
      const row = event.target instanceof Element ? event.target.closest('.tl-track-row') : null;
      document.body.classList.remove('showduino-cue-dragging');
      clearDropTarget();
      if (row) window.setTimeout(() => showToast('Cue added to timeline'), 0);
    }, true);

    document.addEventListener('dragend', () => {
      document.body.classList.remove('showduino-cue-dragging');
      clearDropTarget();
    }, true);
  }

  function boot() {
    injectStyles();
    enhanceTimeline();
    bindGlobalDragFeedback();

    const observer = new MutationObserver(() => {
      window.requestAnimationFrame(enhanceTimeline);
    });
    observer.observe(document.body, { childList: true, subtree: true });

    window.addEventListener('resize', enhanceTimeline);
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', boot, { once: true });
  } else {
    boot();
  }
})();
