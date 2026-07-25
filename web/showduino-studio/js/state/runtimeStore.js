import {
  fetchCommands,
  fetchDeviceCapabilities,
  fetchDevices,
  fetchLogs,
  fetchNetwork,
  fetchRoutes,
  fetchSystem,
  fetchTime,
  fetchTimeStatus
} from '../api.js';
import { formatRelativeMs, severityRank } from '../utils.js';

const SOCKET_STALE_MS = 8000;
const OFFLINE_AFTER_MS = 30000;
const RECONNECT_BACKOFF_MS = [1200, 2500, 5000, 10000, 15000];
const MAX_EVENTS = 40;
const MAX_NOTIFICATIONS = 24;
const MAX_LOGS = 250;

const listeners = new Set();
let started = false;
let socket = null;
let reconnectTimer = null;
let watchdogTimer = null;
const pollTimers = [];

const state = {
  connectionStatus: {
    lifecycle: 'connecting',
    retries: 0,
    lastConnectedAt: 0,
    lastMessageAt: 0,
    latencyMs: null,
    reason: 'Bootstrapping runtime link'
  },
  runtimeStatus: {
    runtime: 'unknown',
    mode: 'monitor',
    transportHealth: 'unknown',
    currentShow: 'Not reported',
    cue: 'Not reported',
    elapsedMs: null,
    remainingMs: null,
    timelineLengthMs: null,
    progress: 0,
    warnings: [],
    executingActions: [],
    audio: { summary: 'Not reported', players: null },
    lighting: { summary: 'Not reported', lines: null }
  },
  systemState: {
    boardName: 'Show Engine',
    firmwareVersion: 'unknown',
    protocolVersion: 'unknown',
    uptime: null
  },
  networkState: {
    deviceCount: 0,
    onlineCount: 0,
    warningCount: 0,
    offlineCount: 0,
    averageRssi: null,
    heartbeatRate: null,
    health: 'unknown',
    computedMs: 0,
    wifi: null
  },
  emergencyState: {
    active: false,
    status: 'clear'
  },
  nodeCollection: {
    selectedNodeId: null,
    nodes: [],
    counts: { total: 0, online: 0, warning: 0, offline: 0 },
    connectedControllers: 0
  },
  showCollection: {
    currentShow: 'Not reported',
    cue: 'Not reported',
    elapsedMs: null,
    remainingMs: null,
    timelineLengthMs: null,
    progress: 0
  },
  assetCollection: {
    webuiPath: '/showduino/www',
    showsPath: '/showduino/shows',
    showPackagesPath: '/showduino/shows/packages',
    status: 'unknown'
  },
  configurationState: {
    wifi: {},
    mdnsHost: 'showduino-studio',
    apSsid: 'Showduino-Studio',
    webuiHost: 'c3-front-door'
  },
  notifications: [],
  latestEvents: [],
  logs: [],
  session: {
    role: 'operator',
    source: 'web-studio'
  },
  navigation: {
    currentRoute: '/dashboard'
  },
  diagnostics: {
    sources: {}
  },
  clock: {
    source: 'local',
    iso: new Date().toISOString(),
    label: new Date().toLocaleTimeString()
  }
};

function emit() {
  for (const fn of listeners) fn(state);
}

function pushNotification(level, message) {
  const next = {
    id: `${Date.now()}-${Math.random().toString(16).slice(2, 8)}`,
    level,
    message,
    at: Date.now()
  };
  state.notifications.unshift(next);
  if (state.notifications.length > MAX_NOTIFICATIONS) state.notifications.length = MAX_NOTIFICATIONS;
}

function pushEvent(level, source, message) {
  const next = {
    at: Date.now(),
    level: level || 'info',
    source: source || 'runtime',
    message: message || ''
  };
  state.latestEvents.unshift(next);
  if (state.latestEvents.length > MAX_EVENTS) state.latestEvents.length = MAX_EVENTS;
}

function updateConnectionLifecycle(lifecycle, reason) {
  if (state.connectionStatus.lifecycle === lifecycle && (!reason || reason === state.connectionStatus.reason)) return;
  state.connectionStatus.lifecycle = lifecycle;
  if (reason) state.connectionStatus.reason = reason;
  pushEvent(lifecycle === 'connected' ? 'info' : 'warn', 'connection', `${lifecycle}: ${state.connectionStatus.reason}`);
}

function refreshNodeFreshness() {
  const computedMs = state.networkState.computedMs || 0;
  let online = 0;
  let warning = 0;
  let offline = 0;
  let controllers = 0;

  for (const node of state.nodeCollection.nodes) {
    const remoteAgeMs = computedMs && node.lastSeenMs != null ? Math.max(0, computedMs - node.lastSeenMs) : null;
    const presence = (node.presence || (node.online ? 'online' : 'offline')).toLowerCase();
    if (presence === 'online') online += 1;
    else if (presence === 'warning') warning += 1;
    else offline += 1;
    if ((node.boardType || '').toLowerCase() === 'director' && presence === 'online') controllers += 1;

    let freshness = 'stale';
    if (presence === 'online') {
      if (remoteAgeMs == null || remoteAgeMs <= 5000) freshness = 'fresh';
      else if (remoteAgeMs <= 15000) freshness = 'aging';
    } else if (presence === 'warning') {
      freshness = 'aging';
    }

    node.freshness = freshness;
    node.freshnessAgeMs = remoteAgeMs;
    node.lastSeenLabel = remoteAgeMs == null ? 'Not reported' : formatRelativeMs(remoteAgeMs);
  }

  state.nodeCollection.counts = {
    total: state.nodeCollection.nodes.length,
    online,
    warning,
    offline
  };
  state.nodeCollection.connectedControllers = controllers;
}

function applySystem(system) {
  if (!system) return;
  state.systemState = {
    boardName: system.boardName || state.systemState.boardName,
    firmwareVersion: system.firmwareVersion || state.systemState.firmwareVersion,
    protocolVersion: system.protocolVersion || state.systemState.protocolVersion,
    uptime: system.uptime ?? state.systemState.uptime,
    role: system.role || 'show-engine'
  };

  state.runtimeStatus.runtime = (system.showState || state.runtimeStatus.runtime || 'unknown').toLowerCase();
  state.runtimeStatus.currentShow = system.currentShow || state.runtimeStatus.currentShow || 'Not reported';
  state.runtimeStatus.mode = (system.mode || state.runtimeStatus.mode || 'monitor').toLowerCase();
  state.emergencyState.active = Boolean(system.emergencyActive);
  state.emergencyState.status = state.emergencyState.active ? 'active' : 'clear';
  state.runtimeStatus.transportHealth = state.networkState.health || state.runtimeStatus.transportHealth;

  state.assetCollection.webuiPath = system.webuiPath || state.assetCollection.webuiPath;
  state.assetCollection.showsPath = system.showsPath || state.assetCollection.showsPath;
  state.assetCollection.showPackagesPath = system.showPackagesPath || state.assetCollection.showPackagesPath;
  state.assetCollection.status = system.storageReady ? 'ready' : 'recovery';

  state.configurationState.wifi = { ...(state.configurationState.wifi || {}), ...(system.wifi || {}) };
  state.configurationState.mdnsHost = system.mdnsHost || state.configurationState.mdnsHost;
  state.configurationState.apSsid = system.apSsid || state.configurationState.apSsid;
  state.configurationState.webuiHost = system.webuiHost || state.configurationState.webuiHost;
  state.showCollection.currentShow = state.runtimeStatus.currentShow;
}

function applyNetwork(network) {
  if (!network) return;
  state.networkState = {
    ...state.networkState,
    deviceCount: network.deviceCount ?? state.networkState.deviceCount,
    onlineCount: network.onlineCount ?? state.networkState.onlineCount,
    warningCount: network.warningCount ?? state.networkState.warningCount,
    offlineCount: network.offlineCount ?? state.networkState.offlineCount,
    averageRssi: network.averageRssi ?? state.networkState.averageRssi,
    heartbeatRate: network.heartbeatRate ?? state.networkState.heartbeatRate,
    health: (network.networkHealth || network.health || state.networkState.health || 'unknown').toLowerCase(),
    computedMs: network.computedMs ?? state.networkState.computedMs,
    recentEvents: Array.isArray(network.recentEvents) ? network.recentEvents : state.networkState.recentEvents,
    wifi: network.wifi || state.networkState.wifi
  };
  state.runtimeStatus.transportHealth = state.networkState.health;
}

function mergeNode(node) {
  if (!node || !node.id) return;
  const idx = state.nodeCollection.nodes.findIndex((entry) => entry.id === node.id);
  if (idx >= 0) state.nodeCollection.nodes[idx] = { ...state.nodeCollection.nodes[idx], ...node };
  else state.nodeCollection.nodes.push({ ...node });
}

function applyNodes(nodes) {
  if (!Array.isArray(nodes)) return;
  const map = new Map(state.nodeCollection.nodes.map((node) => [node.id, node]));
  for (const node of nodes) {
    if (!node?.id) continue;
    map.set(node.id, { ...(map.get(node.id) || {}), ...node });
  }
  state.nodeCollection.nodes = [...map.values()];
  if (!state.nodeCollection.selectedNodeId && state.nodeCollection.nodes.length > 0) {
    state.nodeCollection.selectedNodeId = state.nodeCollection.nodes[0].id;
  }
}

function applyCommands(commands) {
  if (!commands) return;
  if (Array.isArray(commands.running)) {
    state.runtimeStatus.executingActions = commands.running.slice(0, 8).map((cmd) => ({
      id: cmd.id,
      action: `${cmd.category || 'command'}:${cmd.action || 'unknown'}`,
      destination: cmd.destination || 'any',
      status: cmd.status || 'started'
    }));
  }
  if (Array.isArray(commands.history)) {
    state.runtimeStatus.warnings = commands.history
      .filter((cmd) => ['failed', 'rejected', 'cancelled'].includes((cmd.status || '').toLowerCase()))
      .slice(0, 6)
      .map((cmd) => `${cmd.action || cmd.category || 'command'} ${cmd.status}`);
  }
}

function applyLogs(logPayload) {
  const list = Array.isArray(logPayload?.logs) ? logPayload.logs : Array.isArray(logPayload) ? logPayload : [];
  state.logs = list.slice(-MAX_LOGS);
  const important = state.logs
    .slice()
    .reverse()
    .filter((entry) => severityRank(entry.severity) >= severityRank('warn'))
    .slice(0, 5)
    .map((entry) => `${entry.severity || 'warn'}: ${entry.message || 'Unknown warning'}`);
  if (important.length > 0) state.runtimeStatus.warnings = important;
}

function applyClock(time, status) {
  if (!time && !status) return;
  if (time?.iso) {
    state.clock.source = time.source || 'rtc';
    state.clock.iso = time.iso;
    state.clock.label = time.time || new Date(time.iso).toLocaleTimeString();
  }
  if (status?.health && status.health !== 'ok') {
    pushEvent('warn', 'rtc', `RTC status: ${status.health}`);
  }
}

function applyTimeline(timeline) {
  if (!timeline) return;
  const elapsedMs = timeline.pos ?? state.showCollection.elapsedMs;
  const totalMs = timeline.len ?? state.showCollection.timelineLengthMs;
  state.showCollection.elapsedMs = elapsedMs;
  state.showCollection.timelineLengthMs = totalMs;
  state.showCollection.remainingMs = totalMs != null && elapsedMs != null ? Math.max(0, totalMs - elapsedMs) : null;
  state.showCollection.cue = timeline.cue != null ? String(timeline.cue) : state.showCollection.cue;
  state.showCollection.progress = totalMs ? Math.min(1, Math.max(0, elapsedMs / totalMs)) : 0;

  state.runtimeStatus.elapsedMs = state.showCollection.elapsedMs;
  state.runtimeStatus.remainingMs = state.showCollection.remainingMs;
  state.runtimeStatus.timelineLengthMs = state.showCollection.timelineLengthMs;
  state.runtimeStatus.cue = state.showCollection.cue;
  state.runtimeStatus.progress = state.showCollection.progress;
}

function applyDirectorPayload(msg) {
  if (!msg || typeof msg !== 'object') return;
  if (msg.mode) state.runtimeStatus.mode = String(msg.mode).toLowerCase();
  if (msg.status === 'ok') {
    state.connectionStatus.reason = 'Realtime sync active';
  }
  if (msg.timeline) applyTimeline(msg.timeline);
  if (msg.mp3) {
    const a = msg.mp3.A || {};
    const b = msg.mp3.B || {};
    const parts = [];
    if (Object.keys(a).length > 0) parts.push(`A ${a.playing ? 'playing' : 'idle'} @ ${a.vol ?? '—'}`);
    if (Object.keys(b).length > 0) parts.push(`B ${b.playing ? 'playing' : 'idle'} @ ${b.vol ?? '—'}`);
    state.runtimeStatus.audio = { summary: parts.length ? parts.join(' · ') : 'Not reported', players: msg.mp3 };
  }
  if (msg.led) {
    const lines = msg.led.lines ?? null;
    const b = Array.isArray(msg.led.brightness) ? msg.led.brightness.join(', ') : null;
    state.runtimeStatus.lighting = {
      summary: lines ? `${lines} lines${b ? ` · levels ${b}` : ''}` : 'Not reported',
      lines: msg.led
    };
  }
}

function applySocketMessage(message) {
  if (!message) return;
  state.connectionStatus.lastMessageAt = Date.now();
  if (['degraded', 'reconnecting', 'offline', 'connecting'].includes(state.connectionStatus.lifecycle)) {
    updateConnectionLifecycle('connected', 'Realtime link stable');
  }

  if (message.event === 'snapshot') {
    applyNodes(message.devices || []);
    applyNetwork(message.network || null);
  } else if (message.event === 'queue.updated') {
    applyCommands({ queueDepth: message.queueDepth, emergencyDepth: message.emergencyDepth });
  } else if (message.event === 'time.updated') {
    applyClock(message.data || null, null);
  } else if (message.event === 'time.sync' || message.event === 'time.unsynced' || message.event === 'rtc.status') {
    applyClock(message.data || null, message.data || null);
  } else if (message.event === 'network.stats' && message.stats) {
    applyNetwork(message.stats);
  } else if (message.command) {
    applyCommands({ running: [message.command], history: [message.command] });
  } else if (message.device) {
    mergeNode(message.device);
  } else if (message.network) {
    applyNetwork(message.network);
  } else if (message.stats) {
    applyNetwork(message.stats);
  }

  applyDirectorPayload(message);

  if (message.event && !['time.updated', 'network.stats'].includes(message.event)) {
    pushEvent('info', 'socket', message.event);
  }

  refreshNodeFreshness();
  emit();
}

function connectSocket() {
  if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) return;
  const proto = location.protocol === 'https:' ? 'wss' : 'ws';
  const host = location.hostname || '192.168.4.1';
  const url = `${proto}://${host}:81/`;
  if (!state.connectionStatus.lastConnectedAt) updateConnectionLifecycle('connecting', 'Opening realtime link');
  else updateConnectionLifecycle('reconnecting', 'Restoring realtime link');

  try {
    socket = new WebSocket(url);
  } catch (_) {
    scheduleReconnect('WebSocket construction failed');
    return;
  }

  socket.onopen = () => {
    if (reconnectTimer) {
      clearTimeout(reconnectTimer);
      reconnectTimer = null;
    }
    state.connectionStatus.retries = 0;
    state.connectionStatus.lastConnectedAt = Date.now();
    state.connectionStatus.lastMessageAt = Date.now();
    updateConnectionLifecycle('connected', 'Realtime link stable');
    pushNotification('info', 'Connected to Showduino realtime feed');
    emit();
  };

  socket.onmessage = (event) => {
    try {
      applySocketMessage(JSON.parse(event.data));
    } catch (_) {
      pushEvent('warn', 'socket', 'Malformed realtime frame ignored');
    }
  };

  socket.onerror = () => {
    try { socket?.close(); } catch (_) {}
  };

  socket.onclose = () => {
    scheduleReconnect('Realtime link closed');
  };
}

function scheduleReconnect(reason) {
  if (reconnectTimer) return;
  state.connectionStatus.retries += 1;
  const now = Date.now();
  const lastGood = state.connectionStatus.lastConnectedAt || 0;
  const awayForMs = lastGood ? now - lastGood : Number.MAX_SAFE_INTEGER;
  if (awayForMs > OFFLINE_AFTER_MS) updateConnectionLifecycle('offline', reason || 'Realtime link offline');
  else updateConnectionLifecycle('reconnecting', reason || 'Reconnecting realtime link');
  const delay = RECONNECT_BACKOFF_MS[Math.min(state.connectionStatus.retries - 1, RECONNECT_BACKOFF_MS.length - 1)];
  reconnectTimer = setTimeout(() => {
    reconnectTimer = null;
    connectSocket();
  }, delay);
  emit();
}

function pollSource(name, fn) {
  const startedAt = Date.now();
  return fn()
    .then((payload) => {
      const latencyMs = Date.now() - startedAt;
      state.diagnostics.sources[name] = {
        ok: true,
        latencyMs,
        lastSuccessAt: Date.now(),
        error: null
      };
      if (name === 'network') state.connectionStatus.latencyMs = latencyMs;
      return payload;
    })
    .catch((err) => {
      state.diagnostics.sources[name] = {
        ok: false,
        latencyMs: Date.now() - startedAt,
        lastFailureAt: Date.now(),
        error: err.message
      };
      pushEvent('warn', name, err.message);
      throw err;
    });
}

function startPolling() {
  const jobs = [
    {
      name: 'system',
      intervalMs: 6000,
      run: async () => applySystem(await pollSource('system', () => fetchSystem({ timeoutMs: 2800 })))
    },
    {
      name: 'devices',
      intervalMs: 5000,
      run: async () => applyNodes((await pollSource('devices', () => fetchDevices({ timeoutMs: 2800 }))).devices || [])
    },
    {
      name: 'network',
      intervalMs: 4000,
      run: async () => applyNetwork(await pollSource('network', () => fetchNetwork({ timeoutMs: 2800 })))
    },
    {
      name: 'commands',
      intervalMs: 4000,
      run: async () => applyCommands(await pollSource('commands', () => fetchCommands({ timeoutMs: 2800 })))
    },
    {
      name: 'time',
      intervalMs: 7000,
      run: async () => {
        const [time, status] = await Promise.all([
          pollSource('time', () => fetchTime({ timeoutMs: 2400 })),
          pollSource('timeStatus', () => fetchTimeStatus({ timeoutMs: 2400 }))
        ]);
        applyClock(time, status);
      }
    },
    {
      name: 'logs',
      intervalMs: 5000,
      run: async () => applyLogs(await pollSource('logs', () => fetchLogs({ timeoutMs: 2800 })))
    },
    {
      name: 'capabilities',
      intervalMs: 12000,
      run: async () => {
        const grouped = await pollSource('deviceCapabilities', () => fetchDeviceCapabilities({ timeoutMs: 2800 }));
        state.diagnostics.deviceCapabilities = grouped;
      }
    },
    {
      name: 'routes',
      intervalMs: 15000,
      run: async () => {
        const routes = await pollSource('routes', () => fetchRoutes({ timeoutMs: 2800 }));
        state.diagnostics.routes = routes;
      }
    }
  ];

  for (const job of jobs) {
    let inFlight = false;
    const run = async () => {
      if (inFlight) return;
      inFlight = true;
      try {
        await job.run();
      } catch (_) {
        if (state.connectionStatus.lifecycle === 'connected') updateConnectionLifecycle('degraded', `${job.name} refresh failed`);
      } finally {
        inFlight = false;
        refreshNodeFreshness();
        emit();
      }
    };
    run();
    const timer = setInterval(run, job.intervalMs);
    pollTimers.push(timer);
  }
}

function startWatchdog() {
  watchdogTimer = setInterval(() => {
    state.clock.iso = new Date().toISOString();
    state.clock.label = new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit', second: '2-digit' });
    if (state.connectionStatus.lastMessageAt > 0) {
      const age = Date.now() - state.connectionStatus.lastMessageAt;
      if (state.connectionStatus.lifecycle === 'connected' && age > SOCKET_STALE_MS) {
        updateConnectionLifecycle('degraded', `No realtime frames for ${Math.floor(age / 1000)}s`);
      }
      if (['reconnecting', 'connecting'].includes(state.connectionStatus.lifecycle) && age > OFFLINE_AFTER_MS) {
        updateConnectionLifecycle('offline', 'Realtime link timeout');
      }
    }
    refreshNodeFreshness();
    emit();
  }, 1000);
}

export function startRuntimeStore() {
  if (started) return;
  started = true;
  startPolling();
  connectSocket();
  startWatchdog();
}

export function subscribeRuntime(fn) {
  listeners.add(fn);
  fn(state);
  return () => listeners.delete(fn);
}

export function getRuntimeState() {
  return state;
}

export function setNavigationRoute(route) {
  state.navigation.currentRoute = route || '/dashboard';
  emit();
}

export function selectNode(nodeId) {
  if (!nodeId) return;
  state.nodeCollection.selectedNodeId = nodeId;
  emit();
}
