import {
  ApiError,
  fetchAssets,
  fetchCapabilities,
  fetchCommands,
  fetchDeviceCapabilities,
  fetchDevices,
  fetchLogs,
  fetchNetwork,
  fetchRoutes,
  fetchSystem,
  fetchTime,
  fetchTimeStatus,
  postCommand,
  postRouteTest,
  request,
  cancelCommand
} from './api.js';

const subscribers = new Set();
const inFlight = new Set();

const LOOP_INTERVAL_MS = 1000;
const STALE_AFTER_MS = 15000;

const POLL_INTERVALS = {
  system: 5000,
  devices: 8000,
  network: 5000,
  logs: 4000,
  commands: 3000,
  time: 2000,
  timeStatus: 12000,
  capabilities: 20000,
  deviceCapabilities: 20000,
  routes: 20000,
  assets: 30000
};

const POLL_SECTIONS = Object.keys(POLL_INTERVALS);
const nextPollAt = {};

let started = false;
let socket = null;
let loopHandle = null;
let reconnectAttempt = 0;

const state = {
  connection: {
    status: 'booting',
    connected: false,
    stale: false,
    firmwareUnavailable: false,
    reconnectAtMs: 0,
    reconnectInSec: 0,
    lastMessageAtMs: 0,
    lastSyncAtMs: 0,
    lastError: '',
    wsUrl: '',
    attempts: 0
  },
  system: null,
  devices: [],
  network: null,
  logs: [],
  commands: {
    queue: [],
    running: [],
    history: [],
    queueDepth: 0,
    emergencyDepth: 0
  },
  time: null,
  timeStatus: null,
  capabilities: { capabilities: [] },
  deviceCapabilities: { byCapability: {} },
  routes: { rules: [], last: null },
  assets: {
    supported: false,
    deleteSupported: false,
    uploadSupported: false,
    source: 'unsupported',
    items: { images: [], audio: [], lighting: [], shows: [] },
    lastRefreshMs: 0,
    error: 'Asset endpoint unavailable'
  },
  modules: {
    timeline: {
      playbackState: 'unknown',
      currentShow: null,
      currentCue: null,
      nextCue: null,
      elapsedMs: null,
      remainingMs: null,
      lastAction: null
    },
    audio: {
      track: null,
      durationMs: null,
      elapsedMs: null,
      volume: null,
      mute: null,
      playbackStatus: 'unknown',
      lastAction: null
    },
    lighting: {
      activeScene: null,
      brightness: null,
      blackout: null,
      runningEffects: [],
      lastAction: null
    },
    settings: {
      network: {},
      time: {},
      hostname: '',
      preferences: {}
    }
  }
};

function emit() {
  for (const fn of subscribers) fn(getRuntimeSnapshot());
}

function appendRuntimeLog(severity, source, message, synthetic = true) {
  state.logs = [
    {
      timestampMs: Date.now(),
      severity: severity || 'info',
      source: source || 'runtime-store',
      message: message || '',
      synthetic
    },
    ...state.logs
  ].slice(0, 500);
}

function parsePayload(payload) {
  if (payload == null || payload === '') return null;
  if (typeof payload === 'object') return payload;
  if (typeof payload !== 'string') return null;
  try { return JSON.parse(payload); } catch (_) { return null; }
}

function normalizeCommandBody(command) {
  return {
    source: command.source || 'web-studio',
    destination: command.destination || 'any',
    category: command.category || 'system',
    action: command.action || '',
    priority: command.priority || 'normal',
    payload: typeof command.payload === 'string'
      ? command.payload
      : JSON.stringify(command.payload ?? {})
  };
}

function upsertDevice(device) {
  if (!device || !device.id) return;
  const now = Date.now();
  const list = state.devices.slice();
  const idx = list.findIndex((entry) => entry.id === device.id);
  const prev = idx >= 0 ? list[idx] : null;
  const merged = {
    ...(prev || {}),
    ...device,
    _seenAtLocalMs: now
  };
  if (idx >= 0) list[idx] = merged;
  else list.push(merged);
  state.devices = list;
}

function normalizeDevices(list) {
  const now = Date.now();
  return (Array.isArray(list) ? list : []).map((device) => ({
    ...device,
    _seenAtLocalMs: now
  }));
}

function mergeLogs(restLogs) {
  const incoming = Array.isArray(restLogs) ? restLogs : [];
  const synthetic = state.logs.filter((entry) => entry.synthetic);
  const seen = new Set();
  const merged = [];

  const pushIfNew = (entry) => {
    const key = `${entry.timestampMs || 0}|${entry.severity || ''}|${entry.source || ''}|${entry.message || ''}`;
    if (seen.has(key)) return;
    seen.add(key);
    merged.push(entry);
  };

  for (const entry of incoming) pushIfNew(entry);
  for (const entry of synthetic) pushIfNew(entry);
  merged.sort((a, b) => (b.timestampMs || 0) - (a.timestampMs || 0));
  state.logs = merged.slice(0, 500);
}

function friendlyActionName(action) {
  return action.replace(/\./g, ' ').replace(/\b\w/g, (m) => m.toUpperCase());
}

function refreshDerivedState() {
  const history = state.commands.history || [];
  const timelineHistory = history.find((cmd) => cmd.category === 'show' || cmd.category === 'scene');
  const audioHistory = history.find((cmd) => cmd.category === 'audio');
  const lightingHistory = history.find((cmd) => cmd.category === 'lighting');

  const timeline = {
    ...state.modules.timeline,
    playbackState: (state.system?.showState || state.modules.timeline.playbackState || 'unknown').toLowerCase(),
    elapsedMs: state.time?.uptimeSeconds != null ? state.time.uptimeSeconds * 1000 : state.modules.timeline.elapsedMs
  };
  if (timelineHistory) {
    const payload = parsePayload(timelineHistory.payload);
    const action = (timelineHistory.action || '').toLowerCase();
    timeline.lastAction = timelineHistory.action || timeline.lastAction;
    if (action.includes('pause')) timeline.playbackState = 'paused';
    else if (action.includes('stop')) timeline.playbackState = 'stopped';
    else if (action.includes('play') || action.includes('resume')) timeline.playbackState = 'playing';
    else if (action.includes('jump') || action.includes('cue')) timeline.playbackState = timeline.playbackState || 'playing';
    if (payload?.show || payload?.showId) timeline.currentShow = payload.show || payload.showId;
    if (payload?.cue != null) timeline.currentCue = payload.cue;
    if (payload?.cueIndex != null) timeline.currentCue = payload.cueIndex;
    if (payload?.nextCue != null) timeline.nextCue = payload.nextCue;
    if (payload?.elapsedMs != null) timeline.elapsedMs = payload.elapsedMs;
    if (payload?.remainingMs != null) timeline.remainingMs = payload.remainingMs;
  }
  if (state.system?.emergencyActive) timeline.playbackState = 'emergency';

  const audio = {
    ...state.modules.audio
  };
  if (audioHistory) {
    const payload = parsePayload(audioHistory.payload);
    const action = (audioHistory.action || '').toLowerCase();
    audio.lastAction = audioHistory.action || audio.lastAction;
    if (action.includes('pause')) audio.playbackStatus = 'paused';
    else if (action.includes('stop')) audio.playbackStatus = 'stopped';
    else if (action.includes('play') || action.includes('resume') || action.includes('next') || action.includes('previous')) {
      audio.playbackStatus = 'playing';
    }
    if (payload?.track) audio.track = payload.track;
    if (payload?.durationMs != null) audio.durationMs = payload.durationMs;
    if (payload?.elapsedMs != null) audio.elapsedMs = payload.elapsedMs;
    if (payload?.volume != null) audio.volume = payload.volume;
    if (payload?.mute != null) audio.mute = Boolean(payload.mute);
    if (action.includes('mute')) audio.mute = true;
    if (action.includes('unmute')) audio.mute = false;
  }

  const lighting = {
    ...state.modules.lighting
  };
  if (lightingHistory) {
    const payload = parsePayload(lightingHistory.payload);
    const action = (lightingHistory.action || '').toLowerCase();
    lighting.lastAction = lightingHistory.action || lighting.lastAction;
    if (payload?.scene) lighting.activeScene = payload.scene;
    if (payload?.brightness != null) lighting.brightness = payload.brightness;
    if (Array.isArray(payload?.effects)) lighting.runningEffects = payload.effects;
    if (action.includes('blackout')) lighting.blackout = !action.includes('release');
  }

  state.modules.timeline = timeline;
  state.modules.audio = audio;
  state.modules.lighting = lighting;
  state.modules.settings = {
    network: {
      mode: state.system?.wifi?.mode || '',
      ssid: state.system?.wifi?.ssid || '',
      ip: state.system?.wifi?.ip || '',
      hostname: state.system?.wifi?.hostname || state.system?.mdnsHost || ''
    },
    time: {
      timezone: state.time?.timezone || '',
      dstEnabled: Boolean(state.time?.dstEnabled),
      source: state.time?.source || '',
      lastSync: state.timeStatus?.lastSynchronisation || ''
    },
    hostname: state.system?.wifi?.hostname || state.system?.mdnsHost || '',
    preferences: {
      emergencyActive: Boolean(state.system?.emergencyActive),
      networkHealth: state.network?.networkHealth || state.network?.health || ''
    }
  };
}

function applyCommandUpdate(cmd) {
  if (!cmd || !cmd.id) return;
  const history = state.commands.history.slice();
  const hIdx = history.findIndex((entry) => entry.id === cmd.id);
  if (hIdx >= 0) history[hIdx] = { ...history[hIdx], ...cmd };
  else history.unshift(cmd);

  const queue = state.commands.queue.filter((entry) => entry.id !== cmd.id);
  const running = state.commands.running.filter((entry) => entry.id !== cmd.id);
  if (cmd.status === 'queued') queue.unshift(cmd);
  if (cmd.status === 'started') running.unshift(cmd);

  state.commands = {
    ...state.commands,
    queue,
    running,
    history: history.slice(0, 300)
  };
}

function normalizeAssetItems(raw) {
  const items = { images: [], audio: [], lighting: [], shows: [] };
  if (!raw) return items;

  if (Array.isArray(raw.assets)) {
    for (const item of raw.assets) {
      const name = typeof item === 'string' ? item : (item.path || item.name || '');
      if (!name) continue;
      const lower = name.toLowerCase();
      if (/\.(png|jpg|jpeg|gif|webp|bmp|svg)$/.test(lower)) items.images.push(name);
      else if (/\.(mp3|wav|ogg|flac|aac)$/.test(lower)) items.audio.push(name);
      else if (/\.(json|show|timeline)$/.test(lower)) items.shows.push(name);
      else if (/\.(scene|fx|light|lx)$/.test(lower)) items.lighting.push(name);
    }
    return items;
  }

  if (raw.images) items.images = Array.isArray(raw.images) ? raw.images : [];
  if (raw.audio) items.audio = Array.isArray(raw.audio) ? raw.audio : [];
  if (raw.lighting) items.lighting = Array.isArray(raw.lighting) ? raw.lighting : [];
  if (raw.shows) items.shows = Array.isArray(raw.shows) ? raw.shows : [];
  return items;
}

function applyLiveMessage(msg) {
  if (!msg || !msg.event) return;
  state.connection.lastMessageAtMs = Date.now();
  state.connection.lastError = '';
  state.connection.firmwareUnavailable = false;

  switch (msg.event) {
    case 'snapshot':
      state.devices = normalizeDevices(msg.devices);
      if (msg.network) state.network = msg.network;
      break;
    case 'queue.updated':
      state.commands = {
        ...state.commands,
        queueDepth: msg.queueDepth ?? state.commands.queueDepth,
        emergencyDepth: msg.emergencyDepth ?? state.commands.emergencyDepth
      };
      break;
    case 'time.updated':
      if (msg.data) state.time = msg.data;
      break;
    case 'time.sync':
    case 'time.unsynced':
    case 'rtc.status':
    case 'time.alarm':
    case 'time.alarm.armed':
    case 'time.alarm.cleared':
      if (msg.data) state.timeStatus = msg.data;
      break;
    case 'capability.updated':
      refreshRuntimeSection('capabilities', { force: true });
      refreshRuntimeSection('deviceCapabilities', { force: true });
      break;
    case 'routing.table.updated':
      refreshRuntimeSection('routes', { force: true });
      break;
    case 'route.resolved':
    case 'route.failed':
      state.routes = {
        ...state.routes,
        last: msg.data || state.routes.last
      };
      break;
    default:
      break;
  }

  if (msg.command) applyCommandUpdate(msg.command);
  if (msg.device) upsertDevice(msg.device);
  if (msg.stats) {
    state.network = {
      ...(state.network || {}),
      ...msg.stats
    };
  }
  if (msg.network) state.network = msg.network;

  appendRuntimeLog(
    msg.event && msg.event.includes('failed') ? 'warn' : 'debug',
    'websocket',
    msg.event
  );
  refreshDerivedState();
}

function scheduleReconnect() {
  const delay = Math.min(30000, 2000 * Math.pow(2, Math.min(reconnectAttempt, 4)));
  reconnectAttempt += 1;
  state.connection.reconnectAtMs = Date.now() + delay;
  state.connection.attempts = reconnectAttempt;
  state.connection.status = 'reconnecting';
  state.connection.connected = false;
}

function connectSocket() {
  if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) return;
  const proto = location.protocol === 'https:' ? 'wss' : 'ws';
  const host = location.hostname || '192.168.4.1';
  const url = `${proto}://${host}:81/`;
  state.connection.wsUrl = url;
  state.connection.status = 'connecting';
  state.connection.connected = false;
  try {
    socket = new WebSocket(url);
  } catch (_) {
    scheduleReconnect();
    return;
  }

  socket.onopen = () => {
    reconnectAttempt = 0;
    state.connection.connected = true;
    state.connection.status = 'connected';
    state.connection.reconnectAtMs = 0;
    state.connection.reconnectInSec = 0;
    state.connection.lastMessageAtMs = Date.now();
    appendRuntimeLog('info', 'connection', 'WebSocket connected');
    emit();
  };

  socket.onmessage = (event) => {
    try {
      const msg = JSON.parse(event.data);
      applyLiveMessage(msg);
      emit();
    } catch (_) {
      appendRuntimeLog('warn', 'connection', 'Malformed WebSocket event ignored');
      emit();
    }
  };

  socket.onerror = () => {
    try { socket.close(); } catch (_) {}
  };

  socket.onclose = () => {
    state.connection.connected = false;
    scheduleReconnect();
    appendRuntimeLog('warn', 'connection', 'WebSocket disconnected');
    emit();
  };
}

function normalizeError(error) {
  if (error instanceof ApiError) return error;
  return new ApiError(error?.message || 'Request failed');
}

async function refreshRuntimeSection(section, { force = false } = {}) {
  if (inFlight.has(section)) return null;
  if (!POLL_INTERVALS[section]) return null;

  inFlight.add(section);
  nextPollAt[section] = Date.now() + POLL_INTERVALS[section];

  try {
    let data = null;
    if (section === 'system') data = await fetchSystem();
    if (section === 'devices') data = await fetchDevices();
    if (section === 'network') data = await fetchNetwork();
    if (section === 'logs') data = await fetchLogs();
    if (section === 'commands') data = await fetchCommands();
    if (section === 'time') data = await fetchTime();
    if (section === 'timeStatus') data = await fetchTimeStatus();
    if (section === 'capabilities') data = await fetchCapabilities();
    if (section === 'deviceCapabilities') data = await fetchDeviceCapabilities();
    if (section === 'routes') data = await fetchRoutes();
    if (section === 'assets') data = await fetchAssets();

    if (section === 'system' && data) state.system = data;
    if (section === 'devices' && data) state.devices = normalizeDevices(data.devices || []);
    if (section === 'network' && data) state.network = data;
    if (section === 'logs' && data) mergeLogs(data.logs || data);
    if (section === 'commands' && data) {
      state.commands = {
        queue: data.queue || [],
        running: data.running || [],
        history: data.history || [],
        queueDepth: data.queueDepth ?? (data.queue || []).length,
        emergencyDepth: data.emergencyDepth ?? 0
      };
    }
    if (section === 'time' && data) state.time = data;
    if (section === 'timeStatus' && data) state.timeStatus = data;
    if (section === 'capabilities' && data) state.capabilities = data;
    if (section === 'deviceCapabilities' && data) state.deviceCapabilities = data;
    if (section === 'routes' && data) state.routes = data;
    if (section === 'assets') {
      if (data == null) {
        state.assets = {
          ...state.assets,
          supported: false,
          deleteSupported: false,
          uploadSupported: false,
          source: 'unsupported',
          lastRefreshMs: Date.now(),
          error: 'Asset endpoint unavailable'
        };
      } else {
        const items = normalizeAssetItems(data);
        state.assets = {
          ...state.assets,
          supported: true,
          deleteSupported: Boolean(data.deleteSupported),
          uploadSupported: Boolean(data.uploadSupported),
          source: 'api',
          items,
          lastRefreshMs: Date.now(),
          error: ''
        };
      }
    }

    state.connection.lastSyncAtMs = Date.now();
    state.connection.firmwareUnavailable = false;
    state.connection.lastError = '';
    refreshDerivedState();
    emit();
    return data;
  } catch (error) {
    const err = normalizeError(error);
    if (err.status >= 500 || err.timeout) {
      state.connection.firmwareUnavailable = true;
    }
    state.connection.lastError = err.message;
    appendRuntimeLog('error', section, err.message);
    emit();
    throw err;
  } finally {
    inFlight.delete(section);
  }
}

function refreshDueSections() {
  const now = Date.now();
  for (const section of POLL_SECTIONS) {
    if (now >= (nextPollAt[section] || 0)) {
      refreshRuntimeSection(section).catch(() => {});
    }
  }
}

function runtimeTick() {
  const now = Date.now();

  if (!state.connection.connected && state.connection.reconnectAtMs && now >= state.connection.reconnectAtMs) {
    connectSocket();
  }

  state.connection.stale = Boolean(
    state.connection.connected &&
    state.connection.lastMessageAtMs &&
    now - state.connection.lastMessageAtMs > STALE_AFTER_MS
  );

  if (state.connection.connected) {
    state.connection.reconnectInSec = 0;
  } else if (state.connection.reconnectAtMs > now) {
    state.connection.reconnectInSec = Math.ceil((state.connection.reconnectAtMs - now) / 1000);
  } else {
    state.connection.reconnectInSec = 0;
  }

  refreshDueSections();
  emit();
}

function primePolling() {
  const now = Date.now();
  for (const section of POLL_SECTIONS) nextPollAt[section] = now;
}

async function bootstrapRuntime() {
  primePolling();
  connectSocket();
  await Promise.allSettled(POLL_SECTIONS.map((section) => refreshRuntimeSection(section, { force: true })));
}

function commandForAction(action, args = {}) {
  switch (action) {
    case 'node.restart':
      return { category: 'network', action: 'restart-node', destination: args.id || 'any', payload: args };
    case 'node.identify':
      return { category: 'network', action: 'identify-node', destination: args.id || 'any', payload: args };
    case 'node.rename':
      return { category: 'network', action: 'rename-node', destination: args.id || 'any', payload: args };
    case 'node.refresh':
      return { category: 'network', action: 'refresh-node', destination: args.id || 'any', payload: args };

    case 'timeline.play':
      return { category: 'show', action: 'play', destination: 'any', payload: args };
    case 'timeline.pause':
      return { category: 'show', action: 'pause', destination: 'any', payload: args };
    case 'timeline.stop':
      return { category: 'show', action: 'stop', destination: 'any', payload: args };
    case 'timeline.next':
      return { category: 'show', action: 'next-cue', destination: 'any', payload: args };
    case 'timeline.previous':
      return { category: 'show', action: 'previous-cue', destination: 'any', payload: args };
    case 'timeline.jump':
      return { category: 'show', action: 'jump-cue', destination: 'any', payload: args };

    case 'audio.play':
      return { category: 'audio', action: 'play', destination: 'any', payload: args };
    case 'audio.pause':
      return { category: 'audio', action: 'pause', destination: 'any', payload: args };
    case 'audio.stop':
      return { category: 'audio', action: 'stop', destination: 'any', payload: args };
    case 'audio.next':
      return { category: 'audio', action: 'next', destination: 'any', payload: args };
    case 'audio.previous':
      return { category: 'audio', action: 'previous', destination: 'any', payload: args };
    case 'audio.volume':
      return { category: 'audio', action: 'set-volume', destination: 'any', payload: args };
    case 'audio.mute':
      return { category: 'audio', action: args.mute ? 'mute' : 'unmute', destination: 'any', payload: args };

    case 'lighting.scene':
      return { category: 'lighting', action: 'trigger-scene', destination: 'any', payload: args };
    case 'lighting.blackout':
      return { category: 'lighting', action: 'blackout', destination: 'any', payload: args };
    case 'lighting.releaseBlackout':
      return { category: 'lighting', action: 'release-blackout', destination: 'any', payload: args };
    case 'lighting.brightness':
      return { category: 'lighting', action: 'master-brightness', destination: 'any', payload: args };

    case 'settings.network':
      return { category: 'network', action: 'configure-network', destination: 'sue', payload: args };
    case 'settings.time':
      return { category: 'system', action: 'configure-time', destination: 'sue', payload: args };
    case 'settings.hostname':
      return { category: 'network', action: 'set-hostname', destination: 'sue', payload: args };
    case 'settings.preferences':
      return { category: 'system', action: 'set-preferences', destination: 'sue', payload: args };
    case 'settings.reboot':
      return { category: 'system', action: 'reboot', destination: 'sue', payload: args };
    case 'settings.factoryReset':
      return { category: 'system', action: 'factory-reset', destination: 'sue', payload: args };

    case 'log.clear':
      return { category: 'system', action: 'clear-log', destination: 'sue', payload: args };

    case 'assets.refresh':
    case 'assets.delete':
    case 'assets.upload':
      return null;
    default:
      return null;
  }
}

export function initializeRuntimeStore() {
  if (started) return;
  started = true;
  state.connection.status = 'initializing';
  bootstrapRuntime().finally(() => {
    if (!loopHandle) loopHandle = setInterval(runtimeTick, LOOP_INTERVAL_MS);
  });
}

export function subscribeRuntime(listener) {
  subscribers.add(listener);
  listener(getRuntimeSnapshot());
  return () => subscribers.delete(listener);
}

export function getRuntimeSnapshot() {
  return {
    ...state,
    connection: { ...state.connection },
    devices: state.devices.slice(),
    logs: state.logs.slice(),
    commands: {
      ...state.commands,
      queue: state.commands.queue.slice(),
      running: state.commands.running.slice(),
      history: state.commands.history.slice()
    },
    assets: {
      ...state.assets,
      items: {
        images: state.assets.items.images.slice(),
        audio: state.assets.items.audio.slice(),
        lighting: state.assets.items.lighting.slice(),
        shows: state.assets.items.shows.slice()
      }
    },
    modules: {
      timeline: { ...state.modules.timeline },
      audio: { ...state.modules.audio },
      lighting: { ...state.modules.lighting },
      settings: {
        ...state.modules.settings,
        network: { ...state.modules.settings.network },
        time: { ...state.modules.settings.time },
        preferences: { ...state.modules.settings.preferences }
      }
    }
  };
}

export async function refreshRuntime(section = 'system') {
  if (section === 'all') {
    await Promise.allSettled(POLL_SECTIONS.map((name) => refreshRuntimeSection(name, { force: true })));
    return getRuntimeSnapshot();
  }
  await refreshRuntimeSection(section, { force: true });
  return getRuntimeSnapshot();
}

export async function submitCommand(command) {
  const body = normalizeCommandBody(command);
  const result = await postCommand(body);
  appendRuntimeLog('info', 'command', `${body.category}:${body.action} queued`);
  await refreshRuntimeSection('commands', { force: true });
  refreshDerivedState();
  emit();
  return result;
}

export async function cancelCommandById(id) {
  const result = await cancelCommand(id);
  await refreshRuntimeSection('commands', { force: true });
  return result;
}

export async function runRouteTest(command) {
  const body = normalizeCommandBody(command);
  const result = await postRouteTest(body);
  state.routes = {
    ...state.routes,
    last: result
  };
  emit();
  return result;
}

export async function dispatchOperatorCommand(action, args = {}) {
  if (action === 'assets.refresh') {
    return refreshRuntimeSection('assets', { force: true });
  }
  if (action === 'assets.delete') {
    if (!state.assets.supported) throw new ApiError('Asset delete unsupported by firmware');
    if (!args.path) throw new ApiError('Asset path is required');
    const result = await request('/api/assets/' + encodeURIComponent(args.path), { method: 'DELETE' });
    await refreshRuntimeSection('assets', { force: true });
    return result;
  }
  if (action === 'assets.upload') {
    if (!state.assets.supported || !state.assets.uploadSupported) {
      throw new ApiError('Asset upload unsupported by firmware');
    }
    if (!args.name || args.content == null) throw new ApiError('Asset upload requires name and content');
    const result = await request('/api/assets/upload', { method: 'POST', body: { name: args.name, content: args.content }, timeoutMs: 12000 });
    await refreshRuntimeSection('assets', { force: true });
    return result;
  }
  if (action === 'log.clear') {
    state.logs = state.logs.filter((entry) => entry.synthetic);
    emit();
  }

  const command = commandForAction(action, args);
  if (!command) throw new ApiError(`${friendlyActionName(action)} is not supported`);
  return submitCommand(command);
}
