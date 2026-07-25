/**
 * Showduino Studio – Shared Runtime State
 *
 * Single authoritative model for all application state.
 * All pages observe this model; no page maintains its own runtime.
 *
 * Connection lifecycle:
 *   connecting → connected → (degraded ↔ connected) → reconnecting → offline
 */

export const ConnectionState = Object.freeze({
  CONNECTING: 'connecting',
  CONNECTED: 'connected',
  DEGRADED: 'degraded',
  RECONNECTING: 'reconnecting',
  OFFLINE: 'offline',
});

// ─── Default state shapes ────────────────────────────────────────────────────

const defaultRuntime = () => ({
  state: 'idle',       // idle | running | paused | stopped | error
  currentShow: null,   // { id, name, description }
  position: 0,         // ms into show
  duration: 0,         // total ms
  cue: null,           // { number, name, timeMs }
  nextCue: null,
  elapsed: 0,
  remaining: 0,
  transport: null,     // raw transport object from engine
});

const defaultEmergency = () => ({
  active: false,
  reason: null,
  level: null,         // null | 'warning' | 'critical'
});

const defaultSystem = () => ({
  boardName: null,
  firmwareVersion: null,
  protocolVersion: null,
  uptime: null,
  cpuMhz: null,
  heapFree: null,
  heapTotal: null,
  psramFree: null,
  psramTotal: null,
  storageReady: false,
  storageWritable: false,
  wifi: null,
  mdnsHost: null,
});

const defaultNetwork = () => ({
  deviceCount: 0,
  onlineCount: 0,
  offlineCount: 0,
  warningCount: 0,
  averageRssi: null,
  networkHealth: null,
  heartbeatRate: null,
  latencyMs: null,
});

// ─── Mutable state ────────────────────────────────────────────────────────────

let _state = {
  // Connection
  connection: ConnectionState.CONNECTING,
  wsUrl: null,
  connectedAt: null,
  reconnectAttempts: 0,
  lastMessageAt: null,

  // Sub-states
  system: defaultSystem(),
  runtime: defaultRuntime(),
  network: defaultNetwork(),
  emergency: defaultEmergency(),

  // Collections
  nodes: [],           // device/node list
  shows: [],           // show list
  assets: [],

  // Command bus (legacy WebSocket feed)
  commands: { queue: [], running: [], history: [], queueDepth: 0, emergencyDepth: 0 },
  capabilityTick: 0,
  lastRoute: null,

  // Time (authoritative clock from SUE)
  time: null,
  timeStatus: null,

  // Notifications
  notifications: [],

  // Freshness tracking
  systemFreshAt: 0,
  networkFreshAt: 0,
};

// ─── Listener registry ────────────────────────────────────────────────────────

const _listeners = new Set();

export function getState() {
  return _state;
}

export function subscribe(fn) {
  _listeners.add(fn);
  fn(_state);
  return () => _listeners.delete(fn);
}

function _emit() {
  for (const fn of _listeners) {
    try { fn(_state); } catch (_) {}
  }
}

function _merge(patch) {
  _state = { ..._state, ...patch };
  _emit();
}

// ─── Connection lifecycle mutations ──────────────────────────────────────────

export function setConnection(connState, extras = {}) {
  _merge({ connection: connState, ...extras });
}

export function setConnected(wsUrl) {
  _merge({
    connection: ConnectionState.CONNECTED,
    wsUrl,
    connectedAt: Date.now(),
    reconnectAttempts: 0,
  });
}

export function setDegraded() {
  if (_state.connection === ConnectionState.CONNECTED) {
    _merge({ connection: ConnectionState.DEGRADED });
  }
}

export function setReconnecting(attempt) {
  _merge({
    connection: ConnectionState.RECONNECTING,
    reconnectAttempts: attempt,
  });
}

export function setOffline() {
  _merge({ connection: ConnectionState.OFFLINE });
}

// ─── System state ─────────────────────────────────────────────────────────────

export function applySystemData(sys) {
  if (!sys) return;
  _merge({
    system: { ...defaultSystem(), ...sys },
    systemFreshAt: Date.now(),
  });
}

// ─── Runtime state ────────────────────────────────────────────────────────────

export function applyRuntimeData(data) {
  if (!data) return;
  const r = _state.runtime;
  _merge({
    runtime: {
      ...r,
      state: data.state || data.runtimeState || r.state,
      currentShow: data.currentShow || data.show || r.currentShow,
      position: data.position ?? data.positionMs ?? r.position,
      duration: data.duration ?? data.durationMs ?? r.duration,
      cue: data.cue || data.currentCue || r.cue,
      nextCue: data.nextCue || r.nextCue,
      elapsed: data.elapsed ?? data.elapsedMs ?? r.elapsed,
      remaining: data.remaining ?? data.remainingMs ?? r.remaining,
      transport: data.transport || r.transport,
    },
  });
}

// ─── Network/node state ───────────────────────────────────────────────────────

export function applyNetworkData(net) {
  if (!net) return;
  _merge({
    network: { ...defaultNetwork(), ...net },
    networkFreshAt: Date.now(),
  });
}

export function upsertNode(device) {
  if (!device || !device.id) return;
  const list = _state.nodes.slice();
  const idx = list.findIndex((d) => d.id === device.id);
  if (idx >= 0) list[idx] = { ...list[idx], ...device };
  else list.push(device);
  _merge({ nodes: list });
}

export function setNodes(nodes) {
  if (!Array.isArray(nodes)) return;
  _merge({ nodes });
}

// ─── Emergency state ──────────────────────────────────────────────────────────

export function applyEmergencyData(data) {
  if (!data) return;
  _merge({ emergency: { ...defaultEmergency(), ...data } });
}

// ─── Shows ────────────────────────────────────────────────────────────────────

export function setShows(shows) {
  if (!Array.isArray(shows)) return;
  _merge({ shows });
}

// ─── Command bus (legacy WebSocket) ──────────────────────────────────────────

export function upsertCommand(cmd) {
  if (!cmd || !cmd.id) return;
  const hist = _state.commands.history.slice();
  const hi = hist.findIndex((c) => c.id === cmd.id);
  if (hi >= 0) hist[hi] = { ...hist[hi], ...cmd };
  else hist.unshift(cmd);
  if (hist.length > 200) hist.length = 200;

  let queue = _state.commands.queue.filter((c) => c.id !== cmd.id);
  let running = _state.commands.running.filter((c) => c.id !== cmd.id);
  if (cmd.status === 'queued') queue = [cmd, ...queue];
  if (cmd.status === 'started') running = [cmd, ...running];

  _merge({
    commands: { ..._state.commands, queue, running, history: hist },
  });
}

export function applyQueueUpdate(msg) {
  _merge({
    commands: {
      ..._state.commands,
      queueDepth: msg.queueDepth ?? _state.commands.queueDepth,
      emergencyDepth: msg.emergencyDepth ?? _state.commands.emergencyDepth,
    },
  });
}

// ─── Time ─────────────────────────────────────────────────────────────────────

export function applyTimeData(time, status = null) {
  const patch = {};
  if (time) patch.time = time;
  if (status) patch.timeStatus = status;
  if (Object.keys(patch).length) _merge(patch);
}

// ─── Routing ──────────────────────────────────────────────────────────────────

export function bumpCapabilityTick(route = null) {
  _merge({
    capabilityTick: _state.capabilityTick + 1,
    lastRoute: route || _state.lastRoute,
  });
}

export function applyRouteResult(data) {
  _merge({ lastRoute: data || null, capabilityTick: _state.capabilityTick + 1 });
}

// ─── Notifications ────────────────────────────────────────────────────────────

let _notifId = 0;
export function addNotification(message, level = 'info', ttlMs = 5000) {
  const id = ++_notifId;
  const notifs = [{ id, message, level, ts: Date.now() }, ..._state.notifications].slice(0, 20);
  _merge({ notifications: notifs });
  if (ttlMs > 0) {
    setTimeout(() => {
      _merge({ notifications: _state.notifications.filter((n) => n.id !== id) });
    }, ttlMs);
  }
  return id;
}

export function dismissNotification(id) {
  _merge({ notifications: _state.notifications.filter((n) => n.id !== id) });
}
