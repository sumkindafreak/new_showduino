/**
 * Showduino Studio – Live WebSocket Manager
 *
 * Manages the persistent WebSocket connection to the Showduino firmware.
 * Applies all incoming messages to the shared runtime state.
 *
 * Connection lifecycle:
 *   connecting → connected → (degraded ↔ connected) → reconnecting → offline
 *
 * Reconnect backoff: 1s → 2s → 4s → 8s → 16s → 30s (max)
 */

import {
  ConnectionState,
  getState,
  setConnection,
  setConnected,
  setReconnecting,
  setOffline,
  setDegraded,
  upsertNode,
  applyNetworkData,
  applyQueueUpdate,
  upsertCommand,
  applyTimeData,
  bumpCapabilityTick,
  applyRouteResult,
  setNodes,
  applyRuntimeData,
  applyEmergencyData,
} from './state/runtime.js';

// ─── State ────────────────────────────────────────────────────────────────────

let _socket = null;
let _reconnectTimer = null;
let _degradedTimer = null;
let _attempt = 0;
let _started = false;

const BACKOFF_STEPS = [1000, 2000, 4000, 8000, 16000, 30000];
const DEGRADED_TIMEOUT_MS = 15000; // mark degraded if no message for 15s

// ─── Reconnect backoff ────────────────────────────────────────────────────────

function backoffDelay(attempt) {
  return BACKOFF_STEPS[Math.min(attempt, BACKOFF_STEPS.length - 1)];
}

function scheduleReconnect() {
  if (_reconnectTimer) return;
  _attempt += 1;
  const delay = backoffDelay(_attempt);
  setReconnecting(_attempt);
  _reconnectTimer = setTimeout(() => {
    _reconnectTimer = null;
    connect();
  }, delay);
}

// ─── Degraded heartbeat ───────────────────────────────────────────────────────

function resetDegradedTimer() {
  if (_degradedTimer) clearTimeout(_degradedTimer);
  _degradedTimer = setTimeout(() => {
    const s = getState();
    if (s.connection === ConnectionState.CONNECTED) {
      setDegraded();
    }
  }, DEGRADED_TIMEOUT_MS);
}

function clearDegradedTimer() {
  if (_degradedTimer) {
    clearTimeout(_degradedTimer);
    _degradedTimer = null;
  }
}

// ─── Message processing ───────────────────────────────────────────────────────

function applyMessage(msg) {
  if (!msg) return;

  // Reset degraded timer on any message
  resetDegradedTimer();

  // Restore to CONNECTED if we were degraded, and always track last message time
  const s = getState();
  const nextConn = s.connection === ConnectionState.DEGRADED ? ConnectionState.CONNECTED : s.connection;
  setConnection(nextConn, { lastMessageAt: Date.now() });

  const event = msg.event;

  // ── Snapshot ──
  if (event === 'snapshot') {
    if (Array.isArray(msg.devices)) setNodes(msg.devices);
    if (msg.network) applyNetworkData(msg.network);
    return;
  }

  // ── Queue depth ──
  if (event === 'queue.updated') {
    applyQueueUpdate(msg);
    return;
  }

  // ── Time ──
  if (event === 'time.updated') {
    applyTimeData(msg.data || null);
    return;
  }
  if (event === 'time.sync' || event === 'time.unsynced' || event === 'rtc.status') {
    applyTimeData(
      msg.event === 'rtc.status' ? null : (msg.data || null),
      msg.data || null
    );
    return;
  }

  // ── Capabilities / routing ──
  if (event === 'capability.updated' || event === 'routing.table.updated') {
    bumpCapabilityTick(msg.data || null);
    return;
  }
  if (event === 'route.resolved' || event === 'route.failed') {
    applyRouteResult(msg.data || null);
    return;
  }

  // ── Runtime (show engine) ──
  if (event === 'runtime.updated' || event === 'show.runtime') {
    applyRuntimeData(msg.data || msg);
    return;
  }

  // ── Emergency ──
  if (event === 'emergency.updated' || event === 'emergency.active' || event === 'emergency.cleared') {
    applyEmergencyData({
      active: event !== 'emergency.cleared',
      reason: msg.reason || null,
      level: msg.level || (event === 'emergency.cleared' ? null : 'critical'),
      ...(msg.data || {}),
    });
    return;
  }

  // ── Device / node ──
  if (msg.device) upsertNode(msg.device);

  // ── Command ──
  if (msg.command) upsertCommand(msg.command);

  // ── Network stats ──
  if (msg.stats) applyNetworkData({ ...getState().network, ...msg.stats });
  if (msg.network) applyNetworkData(msg.network);
}

// ─── Connection ───────────────────────────────────────────────────────────────

function connect() {
  if (_socket && (_socket.readyState === WebSocket.OPEN || _socket.readyState === WebSocket.CONNECTING)) {
    return;
  }

  setConnection(ConnectionState.CONNECTING);

  const proto = location.protocol === 'https:' ? 'wss' : 'ws';
  const host = location.hostname || '192.168.4.1';
  const url = `${proto}://${host}:81/`;

  try {
    _socket = new WebSocket(url);
  } catch (_) {
    scheduleReconnect();
    return;
  }

  _socket.onopen = () => {
    _attempt = 0;
    setConnected(url);
    resetDegradedTimer();
  };

  _socket.onmessage = (ev) => {
    try {
      applyMessage(JSON.parse(ev.data));
    } catch (_) {}
  };

  _socket.onclose = () => {
    clearDegradedTimer();
    const s = getState();
    if (s.connection !== ConnectionState.OFFLINE) {
      scheduleReconnect();
    }
  };

  _socket.onerror = () => {
    try { _socket.close(); } catch (_) {}
  };
}

export function connectLive() {
  if (!_started) {
    _started = true;
    connect();
  }
}

export function disconnectLive() {
  clearDegradedTimer();
  if (_reconnectTimer) { clearTimeout(_reconnectTimer); _reconnectTimer = null; }
  if (_socket) { try { _socket.close(); } catch (_) {} _socket = null; }
  setOffline();
}

// ─── Legacy compatibility ─────────────────────────────────────────────────────
// Pages that previously imported subscribeLive/getLiveSnapshot from live.js
// can now import from state/runtime.js directly; these re-exports ease migration.

export { getState as getLiveSnapshot, subscribe as subscribeLive } from './state/runtime.js';

// Auto-connect on module load
connectLive();
