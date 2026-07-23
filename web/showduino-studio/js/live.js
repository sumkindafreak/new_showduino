const listeners = new Set();
let socket = null;
let reconnectTimer = null;
let snapshot = {
  devices: [],
  network: null,
  commands: { queue: [], running: [], history: [], queueDepth: 0, emergencyDepth: 0 },
  capabilityTick: 0,
  lastRoute: null,
  time: null,
  timeStatus: null
};

export function getLiveSnapshot() { return snapshot; }

export function subscribeLive(fn) {
  listeners.add(fn);
  fn(snapshot);
  return () => listeners.delete(fn);
}

function emit() {
  for (const fn of listeners) fn(snapshot);
}

function upsertDevice(device) {
  if (!device || !device.id) return;
  const list = snapshot.devices.slice();
  const idx = list.findIndex((d) => d.id === device.id);
  if (idx >= 0) list[idx] = { ...list[idx], ...device };
  else list.push(device);
  snapshot = { ...snapshot, devices: list };
}

function upsertCommand(cmd) {
  if (!cmd || !cmd.id) return;
  const hist = snapshot.commands.history.slice();
  const hi = hist.findIndex((c) => c.id === cmd.id);
  if (hi >= 0) hist[hi] = { ...hist[hi], ...cmd };
  else hist.unshift(cmd);
  if (hist.length > 200) hist.length = 200;

  let queue = snapshot.commands.queue.filter((c) => c.id !== cmd.id);
  let running = snapshot.commands.running.filter((c) => c.id !== cmd.id);
  if (cmd.status === 'queued') queue = [cmd, ...queue];
  if (cmd.status === 'started') running = [cmd, ...running];

  snapshot = {
    ...snapshot,
    commands: {
      ...snapshot.commands,
      queue,
      running,
      history: hist
    }
  };
}

function applyMessage(msg) {
  if (!msg || !msg.event) return;
  if (msg.event === 'snapshot') {
    snapshot = {
      devices: Array.isArray(msg.devices) ? msg.devices : snapshot.devices,
      network: msg.network || snapshot.network,
      commands: snapshot.commands,
      capabilityTick: snapshot.capabilityTick,
      lastRoute: snapshot.lastRoute
    };
    emit();
    return;
  }
  if (msg.event === 'queue.updated') {
    snapshot = {
      ...snapshot,
      commands: {
        ...snapshot.commands,
        queueDepth: msg.queueDepth ?? snapshot.commands.queueDepth,
        emergencyDepth: msg.emergencyDepth ?? snapshot.commands.emergencyDepth
      }
    };
    emit();
    return;
  }
  if (msg.event === 'time.updated') {
    snapshot = { ...snapshot, time: msg.data || snapshot.time };
    emit();
    return;
  }
  if (msg.event === 'time.sync' || msg.event === 'time.unsynced' || msg.event === 'rtc.status') {
    snapshot = {
      ...snapshot,
      timeStatus: msg.data || snapshot.timeStatus,
      time: msg.event === 'rtc.status' ? snapshot.time : (msg.data?.iso ? msg.data : snapshot.time)
    };
    emit();
    return;
  }
  if (msg.event === 'capability.updated' || msg.event === 'routing.table.updated') {
    snapshot = { ...snapshot, capabilityTick: snapshot.capabilityTick + 1, lastRoute: msg.data || snapshot.lastRoute };
    emit();
    return;
  }
  if (msg.event === 'route.resolved' || msg.event === 'route.failed') {
    snapshot = { ...snapshot, lastRoute: msg.data || null, capabilityTick: snapshot.capabilityTick + 1 };
    emit();
    return;
  }
  if (msg.command) upsertCommand(msg.command);
  if (msg.device) upsertDevice(msg.device);
  if (msg.stats) snapshot = { ...snapshot, network: { ...(snapshot.network || {}), ...msg.stats } };
  if (msg.network) snapshot = { ...snapshot, network: msg.network };
  emit();
}

export function connectLive() {
  if (socket && (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING)) return;
  const proto = location.protocol === 'https:' ? 'wss' : 'ws';
  const host = location.hostname || '192.168.4.1';
  const url = `${proto}://${host}:81/`;
  try { socket = new WebSocket(url); } catch (_) { scheduleReconnect(); return; }
  socket.onopen = () => { if (reconnectTimer) { clearTimeout(reconnectTimer); reconnectTimer = null; } };
  socket.onmessage = (ev) => { try { applyMessage(JSON.parse(ev.data)); } catch (_) {} };
  socket.onclose = () => scheduleReconnect();
  socket.onerror = () => { try { socket.close(); } catch (_) {} };
}

function scheduleReconnect() {
  if (reconnectTimer) return;
  reconnectTimer = setTimeout(() => { reconnectTimer = null; connectLive(); }, 2000);
}

connectLive();