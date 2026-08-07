import { fetchSystem, postCommand } from '../api.js';
import { el, archBlock } from '../utils.js';

const TRACK_TYPES = [
  ['audio', '🎵 Audio'],
  ['fx', '✨ FX'],
  ['relay', '⚡ Relay'],
  ['lighting', '💡 Lighting'],
  ['pixel', '🌈 Pixel'],
  ['dmx', '🎨 DMX'],
  ['trigger', '📌 Trigger']
];

const FX_LIBRARY = [
  ['Audio', 'Audio Cue', '00:05.000'],
  ['Effects', 'Impact Hit', '00:01.200'],
  ['Effects', 'Ambient Loop', '00:15.000'],
  ['Relays', 'Relay Pulse', '00:00.500'],
  ['Relays', 'Relay Hold', '—']
];

function button(text, className = 'show-editor-btn', onClick = null) {
  return el('button', { className, text, ...(onClick ? { onClick } : {}) });
}

function toggle(label, checked = true) {
  const input = el('input', { type: 'checkbox' });
  input.checked = checked;
  return el('label', { className: 'show-editor-toggle' }, [input, el('span', { text: label })]);
}

async function submitRuntimeCommand(category, action, priority = 'normal', payload = '') {
  return postCommand({
    source: 'web-studio',
    destination: category === 'emergency' ? 'broadcast' : 'ian',
    category,
    action,
    priority,
    payload
  });
}

function buildLibrary() {
  const panel = el('aside', { className: 'show-editor-panel show-editor-library' });
  panel.append(el('div', { className: 'show-editor-panel-title', text: 'FX Library' }));

  const search = el('input', {
    className: 'show-editor-search',
    type: 'search',
    placeholder: 'Search FX…'
  });
  panel.append(search);

  const list = el('div', { className: 'show-editor-library-list' });
  const groups = new Map();

  for (const [category, name, duration] of FX_LIBRARY) {
    if (!groups.has(category)) {
      const group = el('section', { className: 'show-editor-fx-category' });
      group.append(el('h4', { text: category }));
      groups.set(category, group);
      list.append(group);
    }

    groups.get(category).append(el('div', {
      className: 'show-editor-fx-item',
      draggable: 'true',
      'data-search': `${category} ${name}`.toLowerCase()
    }, [
      el('span', { text: name }),
      el('span', { className: 'show-editor-duration', text: duration })
    ]));
  }

  search.addEventListener('input', () => {
    const query = search.value.trim().toLowerCase();
    for (const item of list.querySelectorAll('.show-editor-fx-item')) {
      item.hidden = query && !item.dataset.search.includes(query);
    }
  });

  panel.append(list);
  return panel;
}

function buildInspector() {
  const panel = el('aside', { className: 'show-editor-panel show-editor-inspector' });
  panel.append(el('div', { className: 'show-editor-panel-title', text: 'Inspector' }));
  panel.append(el('div', { className: 'show-editor-empty', text: 'Select a track or clip to inspect' }));
  return panel;
}

function buildTimeline(state) {
  const area = el('section', { className: 'show-editor-timeline' });
  const titleRow = el('div', { className: 'show-editor-panel-title show-editor-timeline-title' }, [
    el('span', { text: 'Timeline' }),
    el('span', { className: 'show-editor-zoom-readout', text: '100%' })
  ]);
  const ruler = el('div', { className: 'show-editor-ruler' });
  for (let i = 0; i <= 30; i += 5) ruler.append(el('span', { text: `${i}s` }));

  const tracks = el('div', { className: 'show-editor-tracks' });
  const empty = el('div', { className: 'show-editor-empty', text: 'Add tracks to start building your show' });
  tracks.append(empty);

  state.timelineArea = area;
  state.tracksHost = tracks;
  state.emptyState = empty;
  state.zoomReadout = titleRow.querySelector('.show-editor-zoom-readout');

  area.append(titleRow, ruler, tracks);
  return area;
}

function addTrack(state, type, label) {
  state.emptyState.hidden = true;
  const row = el('div', { className: 'show-editor-track', 'data-type': type });
  const trackName = el('button', { className: 'show-editor-track-name', text: label });
  const lane = el('div', { className: 'show-editor-track-lane' }, [
    el('span', { className: 'show-editor-track-hint', text: 'Drop cues here' })
  ]);

  trackName.addEventListener('click', () => {
    for (const existing of state.tracksHost.querySelectorAll('.show-editor-track')) existing.classList.remove('selected');
    row.classList.add('selected');
    state.inspector.innerHTML = '';
    state.inspector.append(
      el('div', { className: 'show-editor-panel-title', text: 'Inspector' }),
      el('div', { className: 'show-editor-inspector-body' }, [
        el('span', { className: 'show-editor-kicker', text: 'Selected track' }),
        el('strong', { text: label }),
        el('span', { className: 'show-editor-muted', text: `Type: ${type}` })
      ])
    );
  });

  row.append(trackName, lane);
  state.tracksHost.append(row);
}

function buildControlStrip(state) {
  const strip = el('section', { className: 'show-editor-control-strip' });

  const project = el('div', { className: 'show-editor-project' }, [
    el('span', { className: 'show-editor-kicker', text: 'Current Show' }),
    el('span', { className: 'show-editor-project-name', contenteditable: 'true', text: 'Untitled Show' }),
    button('💾 Save', 'show-editor-btn show-editor-btn-compact')
  ]);

  const transportStatus = el('span', { className: 'show-editor-runtime-state', text: 'UNKNOWN' });
  state.transportStatus = transportStatus;

  const rewind = button('↩ Rewind', 'show-editor-btn');
  rewind.disabled = true;
  rewind.title = 'The current P4 Stage Runtime has no seek/rewind command yet.';

  const play = button('▶ Play', 'show-editor-btn show-editor-btn-primary', async () => {
    try {
      const action = state.runtimeState === 'PAUSED' ? 'resume' : 'start';
      await submitRuntimeCommand('show', action, 'high');
      transportStatus.textContent = action === 'resume' ? 'RESUME QUEUED' : 'START QUEUED';
    } catch (err) {
      transportStatus.textContent = `ERROR: ${err.message}`;
    }
  });

  const pause = button('⏸ Pause', 'show-editor-btn', async () => {
    try {
      await submitRuntimeCommand('show', 'pause', 'high');
      transportStatus.textContent = 'PAUSE QUEUED';
    } catch (err) {
      transportStatus.textContent = `ERROR: ${err.message}`;
    }
  });

  const stop = button('⏹ Stop', 'show-editor-btn', async () => {
    try {
      await submitRuntimeCommand('show', 'stop', 'high');
      transportStatus.textContent = 'STOP QUEUED';
      state.timeMs = 0;
      state.paintTime();
    } catch (err) {
      transportStatus.textContent = `ERROR: ${err.message}`;
    }
  });

  const transport = el('div', { className: 'show-editor-transport' }, [
    rewind,
    play,
    pause,
    stop,
    toggle('Loop', true)
  ]);

  const time = el('div', {
    className: 'show-editor-time',
    text: '00:00.000',
    title: 'Editor cursor. Authoritative runtime state is shown alongside the transport controls.'
  });
  state.timeDisplay = time;
  state.paintTime = () => {
    const total = Math.max(0, state.timeMs || 0);
    const minutes = Math.floor(total / 60000).toString().padStart(2, '0');
    const seconds = Math.floor((total % 60000) / 1000).toString().padStart(2, '0');
    const ms = Math.floor(total % 1000).toString().padStart(3, '0');
    time.textContent = `${minutes}:${seconds}.${ms}`;
  };

  const connectionBadge = el('span', { className: 'show-editor-status-badge offline', text: 'OFFLINE' });
  state.connectionBadge = connectionBadge;

  const clearEstop = button('CLEAR E-STOP', 'show-editor-btn', async () => {
    if (!window.confirm('Clear the Showduino emergency stop? Outputs remain under Stage Runtime safety rules.')) return;
    try {
      await submitRuntimeCommand('emergency', 'clear', 'emergency');
      transportStatus.textContent = 'E-STOP CLEAR QUEUED';
    } catch (err) {
      transportStatus.textContent = `ERROR: ${err.message}`;
    }
  });
  clearEstop.hidden = true;
  state.clearEstop = clearEstop;

  const panic = button('PANIC', 'show-editor-btn show-editor-btn-danger', async () => {
    if (!window.confirm('ACTIVATE SHOWDUINO EMERGENCY STOP?')) return;
    try {
      await submitRuntimeCommand('emergency', 'panic', 'emergency');
      transportStatus.textContent = 'EMERGENCY STOP QUEUED';
    } catch (err) {
      transportStatus.textContent = `ERROR: ${err.message}`;
    }
  });

  const status = el('div', { className: 'show-editor-status' }, [
    connectionBadge,
    transportStatus,
    clearEstop,
    panic
  ]);

  strip.append(project, transport, time, status);
  return strip;
}

function buildToolbar(state) {
  const toolbar = el('section', { className: 'show-editor-toolbar' });

  const addTrackGroup = el('div', { className: 'show-editor-toolbar-group' }, [
    el('span', { className: 'show-editor-toolbar-label', text: 'Add Track:' })
  ]);
  for (const [type, label] of TRACK_TYPES) {
    addTrackGroup.append(button(label, 'show-editor-btn show-editor-btn-small', () => addTrack(state, type, label.replace(/^\S+\s/, ''))));
  }

  const viewGroup = el('div', { className: 'show-editor-toolbar-group' });
  viewGroup.append(el('span', { className: 'show-editor-toolbar-label', text: 'View:' }));
  viewGroup.append(
    button('Zoom +', 'show-editor-btn show-editor-btn-small', () => {
      state.zoom = Math.min(200, state.zoom + 10);
      state.zoomReadout.textContent = `${state.zoom}%`;
      state.timelineArea.style.setProperty('--timeline-zoom', state.zoom / 100);
    }),
    button('Zoom −', 'show-editor-btn show-editor-btn-small', () => {
      state.zoom = Math.max(50, state.zoom - 10);
      state.zoomReadout.textContent = `${state.zoom}%`;
      state.timelineArea.style.setProperty('--timeline-zoom', state.zoom / 100);
    }),
    button('Fit', 'show-editor-btn show-editor-btn-small', () => {
      state.zoom = 100;
      state.zoomReadout.textContent = '100%';
      state.timelineArea.style.setProperty('--timeline-zoom', 1);
    }),
    toggle('Snap', true),
    toggle('Grid', true)
  );

  const projectGroup = el('div', { className: 'show-editor-toolbar-group' }, [
    el('span', { className: 'show-editor-toolbar-label', text: 'Project:' }),
    button('📥 Import', 'show-editor-btn show-editor-btn-small'),
    button('📤 Export', 'show-editor-btn show-editor-btn-small'),
    button('📂 Open', 'show-editor-btn show-editor-btn-small'),
    button('💾 Save', 'show-editor-btn show-editor-btn-small')
  ]);

  const editGroup = el('div', { className: 'show-editor-toolbar-group' }, [
    el('span', { className: 'show-editor-toolbar-label', text: 'Edit:' }),
    button('📍 Marker', 'show-editor-btn show-editor-btn-small'),
    button('Select All', 'show-editor-btn show-editor-btn-small', () => {
      for (const track of state.tracksHost.querySelectorAll('.show-editor-track')) track.classList.add('selected');
    }),
    button('📋 Copy', 'show-editor-btn show-editor-btn-small'),
    button('📋 Paste', 'show-editor-btn show-editor-btn-small'),
    button('✂ Delete', 'show-editor-btn show-editor-btn-small', () => {
      for (const track of state.tracksHost.querySelectorAll('.show-editor-track.selected')) track.remove();
      if (!state.tracksHost.querySelector('.show-editor-track')) state.emptyState.hidden = false;
    })
  ]);

  toolbar.append(addTrackGroup, viewGroup, projectGroup, editGroup);
  return toolbar;
}

async function buildProjectInfo() {
  const details = el('details', { className: 'show-editor-project-info' });
  details.append(el('summary', { text: 'Project storage details' }));
  const body = el('div', { className: 'show-editor-project-info-body' });
  details.append(body);

  try {
    const sys = await fetchSystem();
    body.append(
      archBlock('Shows Root', sys.showsPath),
      archBlock('Show Index', sys.showIndexPath),
      archBlock('Show Packages', sys.showPackagesPath),
      archBlock('Show Trash', sys.showTrashPath),
      archBlock('Favourites', sys.showFavouritesPath),
      archBlock('Recent Shows', sys.showRecentPath),
      archBlock('SD Ready', sys.storageReady ? 'Mounted' : 'Recovery mode')
    );
  } catch (err) {
    body.append(el('p', { className: 'show-editor-muted', text: err.message }));
  }

  return details;
}

async function refreshRuntime(state) {
  try {
    const sys = await fetchSystem();
    state.runtimeState = sys.showState || 'UNKNOWN';
    state.transportStatus.textContent = state.runtimeState;
    state.connectionBadge.textContent = 'ONLINE';
    state.connectionBadge.classList.remove('offline');
    state.connectionBadge.classList.add('online');
    const emergency = Boolean(sys.emergencyActive) || state.runtimeState === 'EMERGENCY_STOP';
    state.clearEstop.hidden = !emergency;
  } catch (_) {
    state.connectionBadge.textContent = 'OFFLINE';
    state.connectionBadge.classList.remove('online');
    state.connectionBadge.classList.add('offline');
  }
}

export async function ShowsPage(container) {
  const state = { zoom: 100, timeMs: 0, runtimeState: 'UNKNOWN' };
  const shell = el('div', { className: 'show-editor' });

  shell.append(buildControlStrip(state));
  shell.append(buildToolbar(state));

  const workspace = el('div', { className: 'show-editor-workspace' });
  const library = buildLibrary();
  const timeline = buildTimeline(state);
  const inspector = buildInspector();
  state.inspector = inspector;
  workspace.append(library, timeline, inspector);
  shell.append(workspace);
  shell.append(await buildProjectInfo());

  container.append(shell);
  await refreshRuntime(state);
  const runtimeTimer = window.setInterval(() => refreshRuntime(state), 1000);

  return () => window.clearInterval(runtimeTimer);
}

ShowsPage.title = 'Show Editor';
