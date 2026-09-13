/* Showduino Studio — desktop clarity pass
 * Presentation/discoverability only. SHDO v2 and runtime behaviour are untouched.
 */
(function () {
  'use strict';

  const DESKTOP_QUERY = '(min-width: 761px)';
  const STYLE_ID = 'showduino-desktop-clarity-style';

  const TYPE_META = Object.freeze({
    audio:    { icon:'♪', name:'Sound', hint:'Drag a sound cue here' },
    relay:    { icon:'⚙', name:'Prop / Switch', hint:'Drag a prop or switched action here' },
    mosfet:   { icon:'☀', name:'Lighting', hint:'Drag a lighting action here' },
    lighting: { icon:'☀', name:'Lighting', hint:'Drag a lighting action here' },
    pixel:    { icon:'✦', name:'Pixels / LEDs', hint:'Drag a pixel or LED effect here' },
    trigger:  { icon:'◎', name:'Trigger', hint:'Drag a trigger here' },
    fx:       { icon:'◈', name:'Other Effect', hint:'Drag an effect here' },
    prop:     { icon:'⚙', name:'Prop / Effect', hint:'Drag a prop or effect here' },
    mixed:    { icon:'+', name:'General Lane', hint:'Drag any compatible cue here' }
  });

  const isDesktop = () => window.matchMedia(DESKTOP_QUERY).matches;
  const timeline = () => window.timelineEditor || null;
  const metaFor = (type) => TYPE_META[String(type || 'mixed').toLowerCase()] || TYPE_META.mixed;
  const tracks = () => timeline() && typeof timeline()._tracks === 'function' ? timeline()._tracks() : [];
  const clips = () => timeline() && typeof timeline()._clips === 'function' ? timeline()._clips() : [];

  function injectStyles() {
    if (document.getElementById(STYLE_ID)) return;
    const style = document.createElement('style');
    style.id = STYLE_ID;
    style.textContent = `
      @media (min-width:761px) {
        .timeline-editor.showduino-clarity-ready {
          --sd-line:#263740;
          --sd-muted:#758a94;
          --sd-accent:#00ffc8;
          background:#10161a !important;
        }
        .timeline-editor.showduino-clarity-ready .timeline-main { background:#0d1317 !important; }
        .timeline-editor.showduino-clarity-ready .tl-left {
          width:190px !important; min-width:190px !important;
          background:#11191e !important; border-right:1px solid var(--sd-line) !important;
        }
        .timeline-editor.showduino-clarity-ready .tl-track-list-header {
          padding:0 12px !important; background:#0f161a !important;
          border-bottom:1px solid var(--sd-line) !important; color:#8da0a8 !important;
          font:850 10px/1 Inter,system-ui,sans-serif !important;
          letter-spacing:.09em; text-transform:uppercase;
        }
        .timeline-editor.showduino-clarity-ready .tl-track-header {
          position:relative; padding:7px 8px !important;
          background:#121b20 !important; border-bottom:1px solid #25343b !important;
          font-family:Inter,system-ui,sans-serif !important;
        }
        .timeline-editor.showduino-clarity-ready .tl-track-header:hover { background:#152129 !important; }
        .sd-lane-name-row { min-height:24px; gap:7px !important; }
        .sd-lane-name-row > span:not(.sd-lane-type) {
          font:800 11px/1.2 Inter,system-ui,sans-serif !important; color:#e6eff1 !important;
        }
        .sd-lane-type {
          margin-left:auto; padding:3px 5px; border:1px solid #2d4149; border-radius:5px;
          background:#0c1418; color:#78909a;
          font:800 8px/1 ui-monospace,SFMono-Regular,Menlo,monospace;
          letter-spacing:.05em; text-transform:uppercase; white-space:nowrap;
        }
        .sd-lane-menu {
          width:24px; height:24px; flex:0 0 auto; display:grid; place-items:center; padding:0;
          border:1px solid transparent; border-radius:6px; background:transparent; color:#78909a;
          cursor:pointer; font:900 14px/1 Inter,system-ui,sans-serif;
        }
        .sd-lane-menu:hover { border-color:#344a53; background:#0d171b; color:#dce8eb; }
        .sd-lane-tools { display:none !important; gap:3px !important; margin-top:3px; }
        .tl-track-header.sd-tools-open .sd-lane-tools { display:flex !important; }

        .timeline-editor.showduino-clarity-ready .tl-track-row {
          border-bottom:1px solid #223039 !important; background:#0f161a !important;
        }
        .tl-track-row.sd-empty-lane::before {
          content:attr(data-empty-hint); position:absolute; left:18px; top:50%; transform:translateY(-50%);
          color:#4f656f; font:650 10px/1 Inter,system-ui,sans-serif;
          pointer-events:none; z-index:1;
        }
        body.showduino-cue-dragging .tl-track-row.sd-empty-lane::before { content:'Drop here'; color:#6abca9; }
        .timeline-editor.showduino-clarity-ready .tl-ruler {
          background:#182228 !important; border-bottom-color:#2b3a42 !important;
        }
        .timeline-editor.showduino-clarity-ready .tl-marker-track {
          background:#10171b !important; border-bottom-color:#26343b !important; opacity:.62;
        }
        .timeline-editor.showduino-clarity-ready .tl-canvas-scroll { background:#0c1215; }

        .timeline-editor.showduino-clarity-ready .tl-clip {
          min-width:32px; border:1px solid rgba(255,255,255,.28) !important;
          border-radius:7px !important; box-shadow:0 3px 10px rgba(0,0,0,.22);
          transition:filter .12s ease,box-shadow .12s ease;
        }
        .timeline-editor.showduino-clarity-ready .tl-clip:hover {
          filter:brightness(1.08); box-shadow:0 4px 14px rgba(0,0,0,.3);
        }
        .timeline-editor.showduino-clarity-ready .tl-clip.sd-selected-cue {
          outline:2px solid #f2fbfb !important; outline-offset:1px;
          box-shadow:0 0 0 3px rgba(0,255,200,.13),0 5px 18px rgba(0,0,0,.38) !important;
        }
        .sd-clip-label {
          color:#071012 !important; font:900 10px/1 Inter,system-ui,sans-serif !important;
          text-shadow:0 1px 0 rgba(255,255,255,.16);
        }

        .sd-empty-state {
          position:absolute; z-index:160; left:50%; top:74px; width:min(430px,calc(100% - 48px));
          transform:translateX(-50%); padding:18px; border:1px dashed #344a53; border-radius:14px;
          background:rgba(13,21,25,.95); box-shadow:0 16px 40px rgba(0,0,0,.22);
          color:#dfeaec; text-align:center; font-family:Inter,system-ui,sans-serif;
        }
        .sd-empty-state b { display:block; font-size:15px; }
        .sd-empty-state p {
          margin:7px auto 12px; max-width:340px; color:#7f939c; font-size:11px; line-height:1.5;
        }
        .sd-empty-actions { display:flex; justify-content:center; flex-wrap:wrap; gap:6px; }
        .sd-empty-actions button {
          min-height:34px; padding:7px 10px; border:1px solid #31464f; border-radius:8px;
          background:#101a1f; color:#d7e4e7; cursor:pointer; font:800 10px/1 Inter,system-ui,sans-serif;
        }
        .sd-empty-actions button:first-child {
          border-color:rgba(0,255,200,.42); color:#afffe9; background:rgba(0,255,200,.055);
        }

        .timeline-editor.showduino-clarity-ready .tl-inspector {
          padding:0 !important; border-left:1px solid #293941 !important; background:#10181c !important;
          transition:width .16s ease,min-width .16s ease; font-family:Inter,system-ui,sans-serif !important;
        }
        .timeline-editor.showduino-clarity-ready .tl-inspector.sd-inspector-empty {
          width:52px !important; min-width:52px !important; overflow:hidden !important;
        }
        .timeline-editor.showduino-clarity-ready .tl-inspector.sd-inspector-active {
          width:300px !important; min-width:300px !important; padding:14px !important;
        }
        .sd-inspector-closed {
          width:52px; min-height:100%; display:flex; align-items:center; padding-top:14px; box-sizing:border-box;
          writing-mode:vertical-rl; color:#647982;
          font:850 9px/1 ui-monospace,SFMono-Regular,Menlo,monospace;
          letter-spacing:.12em; text-transform:uppercase;
        }
        .sd-inspector-heading {
          margin:0 0 4px !important; color:#e7f1f3 !important;
          font:850 14px/1.2 Inter,system-ui,sans-serif !important;
        }
        .sd-inspector-subtitle {
          display:block; margin-bottom:12px; color:#738993;
          font:750 9px/1.2 ui-monospace,SFMono-Regular,Menlo,monospace;
          letter-spacing:.06em; text-transform:uppercase;
        }
        .timeline-editor.showduino-clarity-ready .tl-inspector .inspector-field label,
        .timeline-editor.showduino-clarity-ready .tl-inspector #insp-params label {
          color:#7f949d !important; font:750 10px/1.2 Inter,system-ui,sans-serif !important;
        }
        .timeline-editor.showduino-clarity-ready .tl-inspector input[type='text'],
        .timeline-editor.showduino-clarity-ready .tl-inspector input[type='number'],
        .timeline-editor.showduino-clarity-ready .tl-inspector select {
          min-height:34px; border:1px solid #32464f !important; border-radius:7px !important;
          background:#0b1216 !important; color:#e4edef !important; padding:6px 8px !important;
          font:700 11px/1 Inter,system-ui,sans-serif !important;
        }
        .sd-inspector-advanced { display:none !important; }
        .tl-inspector.sd-show-advanced .sd-inspector-advanced { display:block !important; }
        .sd-inspector-more {
          width:100%; min-height:32px; margin:4px 0 10px; border:1px solid #2c3d45; border-radius:7px;
          background:#0d1519; color:#82959d; cursor:pointer; font:800 9px/1 Inter,system-ui,sans-serif;
        }
        .timeline-toolbar.showduino-toolbar-simplified button.sd-quiet-tool { display:none !important; }
      }
    `;
    document.head.appendChild(style);
  }

  function enhanceToolbar(editor) {
    const toolbar = editor.querySelector('.timeline-toolbar');
    if (!toolbar) return;
    toolbar.querySelectorAll('button').forEach((button) => {
      const text = button.textContent.trim();
      if (['⋮ Grid: ON','⋮ Grid: OFF','📌 Marker','＋ New'].includes(text)) button.classList.add('sd-quiet-tool');
    });
  }

  function enhanceLaneHeaders(editor) {
    const allTracks = tracks();
    editor.querySelectorAll('.tl-track-header').forEach((header) => {
      const track = allTracks.find((item) => item.id === header.dataset.trackId);
      if (!track) return;
      const nameRow = header.firstElementChild;
      const tools = header.lastElementChild;
      if (!nameRow || !tools) return;

      nameRow.classList.add('sd-lane-name-row');
      tools.classList.add('sd-lane-tools');

      if (!nameRow.querySelector('.sd-lane-type')) {
        const type = document.createElement('span');
        type.className = 'sd-lane-type';
        type.textContent = metaFor(track.type).name;
        nameRow.appendChild(type);
      }

      if (!nameRow.querySelector('.sd-lane-menu')) {
        const menu = document.createElement('button');
        menu.type = 'button';
        menu.className = 'sd-lane-menu';
        menu.textContent = '⋯';
        menu.title = 'Lane options';
        menu.setAttribute('aria-label', `Options for ${track.name}`);
        menu.addEventListener('click', (event) => {
          event.stopPropagation();
          header.classList.toggle('sd-tools-open');
        });
        nameRow.appendChild(menu);
      }
    });

    const listHeader = editor.querySelector('.tl-track-list-header');
    if (listHeader && listHeader.textContent !== 'Show lanes') listHeader.textContent = 'Show lanes';
  }

  function enhanceLaneRows(editor) {
    const allTracks = tracks();
    const allClips = clips();
    editor.querySelectorAll('.tl-track-row').forEach((row) => {
      const track = allTracks.find((item) => item.id === row.dataset.trackId);
      if (!track) return;
      const hasClip = allClips.some((clip) => clip.trackId === track.id);
      row.classList.toggle('sd-empty-lane', !hasClip);
      if (!hasClip) row.dataset.emptyHint = metaFor(track.type).hint;
      else delete row.dataset.emptyHint;
    });
  }

  function clipDetail(clip) {
    const p = clip.params || {};
    if (clip.type === 'audio') return p.file ? String(p.file).split('/').pop() : 'Choose sound file';
    if (clip.type === 'relay') return `${String(p.out || 'out1').toUpperCase()} · ${p.state === false ? 'OFF' : 'ON'}`;
    if (clip.type === 'mosfet') return `${String(p.out || 'out1').toUpperCase()} · ${Number.isFinite(Number(p.duty)) ? Number(p.duty) : 100}%`;
    if (clip.type === 'pixel' || clip.type === 'lighting') return String(p.effect || 'Solid');
    if (clip.type === 'trigger') return p.event || 'Trigger event';
    if (clip.type === 'fx') return p.effect || 'Effect';
    return metaFor(clip.type).name;
  }

  function enhanceClips(editor) {
    const allClips = clips();
    const selected = timeline()?._selectedClipId || null;
    editor.querySelectorAll('.tl-clip').forEach((element) => {
      const clip = allClips.find((item) => item.id === element.dataset.clipId);
      if (!clip) return;
      const meta = metaFor(clip.type);
      const label = element.querySelector('span');
      const friendly = `${meta.icon} ${clip.label || meta.name}`;
      if (label) {
        label.classList.add('sd-clip-label');
        if (label.textContent !== friendly) label.textContent = friendly;
      }
      element.classList.toggle('sd-selected-cue', clip.id === selected);
      element.title = `${clip.label || meta.name}\n${clipDetail(clip)}\nStart ${(Number(clip.startMs || 0) / 1000).toFixed(2)}s · Length ${(Number(clip.durationMs || 0) / 1000).toFixed(2)}s`;
    });
  }

  function clickCueShelf(type) {
    document.querySelector(`.showduino-cue-tool[data-cue-type="${type}"]`)?.click();
  }

  function updateEmptyState(editor) {
    const scroll = editor.querySelector('.tl-canvas-scroll');
    if (!scroll) return;
    const allClips = clips();
    let empty = scroll.querySelector('.sd-empty-state');

    if (allClips.length) {
      empty?.remove();
      return;
    }
    if (empty) return;

    empty = document.createElement('section');
    empty.className = 'sd-empty-state';
    empty.innerHTML = `
      <b>Your show is empty</b>
      <p>Start with the first thing the audience should see or hear. Drag an action from above, or use a shortcut.</p>
      <div class="sd-empty-actions">
        <button type="button" data-empty-add="audio">♪ Add sound</button>
        <button type="button" data-empty-add="mosfet">☀ Add lighting</button>
        <button type="button" data-empty-add="relay">⚙ Add prop</button>
        <button type="button" data-empty-add="pixel">✦ Add pixels</button>
      </div>`;
    empty.querySelectorAll('[data-empty-add]').forEach((button) => {
      button.addEventListener('click', () => clickCueShelf(button.dataset.emptyAdd));
    });
    scroll.appendChild(empty);
  }

  function friendlyInspectorLabel(label) {
    const replacements = {
      'Label':'Cue name', 'Start (ms)':'Start time (ms)', 'Duration (ms)':'Duration (ms)',
      'Color':'Timeline colour', 'Track':'Lane', 'File':'Sound file', 'Volume (%)':'Volume',
      'Output':'Which output?', 'Brightness':'Brightness', 'Effect':'Effect'
    };
    const text = label.textContent.trim();
    if (replacements[text] && replacements[text] !== text) label.textContent = replacements[text];
  }

  function enhanceInspector(editor) {
    const inspector = editor.querySelector('.tl-inspector');
    if (!inspector) return;
    const selectedId = timeline()?._selectedClipId || null;
    const clip = clips().find((item) => item.id === selectedId);

    if (!clip) {
      inspector.classList.add('sd-inspector-empty');
      inspector.classList.remove('sd-inspector-active', 'sd-show-advanced');
      if (!inspector.querySelector('.sd-inspector-closed')) {
        inspector.innerHTML = '<div class="sd-inspector-closed" title="Select a cue to edit it">Cue settings</div>';
      }
      return;
    }

    inspector.classList.remove('sd-inspector-empty');
    inspector.classList.add('sd-inspector-active');

    const heading = inspector.querySelector('h3');
    if (heading) {
      heading.classList.add('sd-inspector-heading');
      if (heading.textContent !== 'Cue settings') heading.textContent = 'Cue settings';
      if (!heading.nextElementSibling?.classList.contains('sd-inspector-subtitle')) {
        const subtitle = document.createElement('span');
        subtitle.className = 'sd-inspector-subtitle';
        subtitle.textContent = metaFor(clip.type).name;
        heading.after(subtitle);
      }
    }

    inspector.querySelectorAll('label').forEach(friendlyInspectorLabel);
    inspector.querySelectorAll('.inspector-field').forEach((field) => {
      const text = field.querySelector('label')?.textContent.trim();
      if (text === 'Timeline colour' || text === 'Lane') field.classList.add('sd-inspector-advanced');
    });

    if (!inspector.querySelector('.sd-inspector-more')) {
      const more = document.createElement('button');
      more.type = 'button';
      more.className = 'sd-inspector-more';
      more.textContent = 'More timeline settings';
      more.addEventListener('click', () => {
        inspector.classList.toggle('sd-show-advanced');
        more.textContent = inspector.classList.contains('sd-show-advanced')
          ? 'Hide timeline settings' : 'More timeline settings';
      });
      const params = inspector.querySelector('#insp-params');
      if (params) inspector.insertBefore(more, params);
      else inspector.appendChild(more);
    }
  }

  function refresh() {
    if (!isDesktop()) return;
    const editor = document.querySelector('.timeline-editor');
    if (!editor) return;
    editor.classList.add('showduino-clarity-ready');
    enhanceToolbar(editor);
    enhanceLaneHeaders(editor);
    enhanceLaneRows(editor);
    enhanceClips(editor);
    updateEmptyState(editor);
    enhanceInspector(editor);
  }

  function boot() {
    injectStyles();
    refresh();
    let queued = false;
    const observer = new MutationObserver(() => {
      if (queued) return;
      queued = true;
      window.requestAnimationFrame(() => {
        queued = false;
        refresh();
      });
    });
    observer.observe(document.body, {
      childList:true, subtree:true, attributes:true, attributeFilter:['class']
    });
    window.addEventListener('resize', refresh);
    window.addEventListener('showduino:v4-saved', refresh);
    window.addEventListener('showduino:project-saved', refresh);
  }

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', boot, { once:true });
  else boot();
})();
