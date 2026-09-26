/* Showduino Studio — MOSFET Node identity, outputs and timeline compile helpers. */
(function (root) {
  'use strict';

  var OUT_COUNT = 4;
  var TIMELINE_COMMAND_MAX = 63;
  var DURATION_MAX_MS = 600000;
  var ROUTE = 'mosfet-node';
  var TYPE = 'mosfet-node';

  function text(value) {
    return String(value == null ? '' : value).trim();
  }

  function clamp(value, min, max, fallback) {
    var n = Number(value);
    if (!Number.isFinite(n)) n = fallback;
    return Math.min(max, Math.max(min, n));
  }

  function parseOutIndex(value) {
    var raw = text(value).toLowerCase();
    var m = raw.match(/^out\s*([1-4])$/i) || raw.match(/^([1-4])$/);
    if (!m) return 0;
    return Number(m[1]);
  }

  function outToken(index) {
    return 'out' + index;
  }

  function canonicalNodeId(value) {
    var raw = text(value).toUpperCase();
    if (!raw) return '';
    var m = raw.match(/^MOSFET-0*([1-9][0-9]{0,3})$/);
    if (!m) return '';
    var n = Number(m[1]);
    if (!Number.isFinite(n) || n < 1 || n > 8) return '';
    return 'MOSFET-' + String(n).padStart(2, '0');
  }

  function isMosfetId(value) {
    return !!canonicalNodeId(value);
  }

  function normaliseMode(value) {
    var mode = text(value).toLowerCase();
    if (mode === 'pwm') return 'hold';
    if (mode === 'fade') return 'fade';
    if (mode === 'pulse') return 'pulse';
    return 'hold';
  }

  function defaultParams() {
    return {
      out: 'out1',
      mode: 'hold',
      state: true,
      duty: 100,
      pulseMs: 500,
      fadeInMs: 0,
      fadeOutMs: 0,
      safeOff: true
    };
  }

  function migrateParams(params) {
    var p = Object.assign({}, defaultParams(), params || {});
    p.out = outToken(parseOutIndex(p.out) || 1);
    p.mode = normaliseMode(p.mode);
    p.duty = clamp(p.duty, 0, 100, 100);
    p.pulseMs = clamp(p.pulseMs, 0, DURATION_MAX_MS, 500);
    p.fadeInMs = clamp(p.fadeInMs, 0, DURATION_MAX_MS, 0);
    p.fadeOutMs = clamp(p.fadeOutMs, 0, DURATION_MAX_MS, 0);
    p.safeOff = true;
    p.state = p.state !== false;
    return p;
  }

  function liveOutputs(device) {
    var outputs = Array.isArray(device && device.outputs) ? device.outputs : [];
    var list = [];
    for (var i = 0; i < outputs.length; i++) {
      var o = outputs[i] || {};
      var ch = Number(o.channel) || parseOutIndex(o.id || o.label) || (i + 1);
      if (ch < 1 || ch > OUT_COUNT) continue;
      list.push({
        id: outToken(ch),
        channel: ch,
        name: text(o.name || o.label) || ('OUT' + ch),
        level: clamp(o.level, 0, 100, 0),
        active: !!o.active,
        pin: o.pin
      });
    }
    if (!list.length) {
      for (var n = 1; n <= OUT_COUNT; n++) {
        list.push({ id: outToken(n), channel: n, name: 'OUT' + n, level: 0, active: false });
      }
    }
    return list.slice(0, OUT_COUNT);
  }

  function discoverFromLive(live) {
    var devices = [];
    var src = [];
    if (live && Array.isArray(live.devices)) src = live.devices;
    else if (live && Array.isArray(live.mosfetNodes)) src = live.mosfetNodes;
    for (var i = 0; i < src.length; i++) {
      var d = src[i] || {};
      var role = text(d.role || d.type || '').toUpperCase();
      var id = canonicalNodeId(d.id || d.nodeId || d.name);
      if (!id && (role === 'MOSFET' || text(d.type).toLowerCase() === 'mosfet-node')) {
        id = canonicalNodeId(d.id);
      }
      if (!id) continue;
      devices.push({
        id: id,
        name: text(d.name || d.friendlyName) || id,
        online: d.online !== false,
        route: ROUTE,
        type: TYPE,
        outputs: liveOutputs(d)
      });
    }
    return devices;
  }

  function commandLengthOk(command) {
    return !!command && command.length <= TIMELINE_COMMAND_MAX;
  }

  function nodeCommand(nodeId, rest) {
    var id = canonicalNodeId(nodeId);
    if (!id) return '';
    return 'MOSFET:NODE:' + id + ':' + rest;
  }

  function compileClip(clip, device, pushCommand, errors) {
    var p = migrateParams(clip && clip.params);
    var nodeId = canonicalNodeId(
      (device && (device.binding && device.binding.nodeId || device.nodeId || device.id)) ||
      (clip && clip.routing && clip.routing.nodeId)
    );
    if (!nodeId) {
      errors.push((clip && clip.name || 'MOSFET') + ': MOSFET Node ID missing or invalid.');
      return;
    }
    var ch = parseOutIndex(p.out || (clip && clip.routing && clip.routing.output));
    if (!ch) {
      errors.push((clip && clip.name || 'MOSFET') + ': output must be OUT1–OUT4.');
      return;
    }
    var startMs = Math.max(0, Number(clip.startMs) || 0);
    var durationMs = Math.max(0, Number(clip.durationMs) || 0);
    var endMs = startMs + durationMs;
    var duty = clamp(p.duty, 0, 100, 100);
    if (p.state === false) duty = 0;
    var mode = normaliseMode(p.mode);

    function add(timeMs, rest) {
      var cmd = nodeCommand(nodeId, rest);
      if (!commandLengthOk(cmd)) {
        errors.push((clip && clip.name || 'MOSFET') + ': command exceeds ' + TIMELINE_COMMAND_MAX + ' chars: ' + cmd);
        return;
      }
      pushCommand(timeMs, cmd);
    }

    if (mode === 'pulse') {
      var pulseMs = clamp(p.pulseMs || durationMs || 500, 1, DURATION_MAX_MS, 500);
      if (duty <= 0) add(startMs, 'OUT:' + ch + ':OFF');
      else add(startMs, 'OUT:' + ch + ':PULSE:' + duty + ':' + pulseMs);
      add(endMs, 'OUT:' + ch + ':OFF');
      return;
    }

    if (mode === 'fade') {
      var fadeIn = clamp(p.fadeInMs, 0, DURATION_MAX_MS, 0);
      var fadeOut = clamp(p.fadeOutMs, 0, DURATION_MAX_MS, 0);
      if (fadeOut > durationMs) fadeOut = durationMs;
      if (fadeIn > 0 && duty > 0) add(startMs, 'OUT:' + ch + ':FADE:' + duty + ':' + fadeIn);
      else if (duty > 0) add(startMs, 'OUT:' + ch + ':LEVEL:' + duty);
      else add(startMs, 'OUT:' + ch + ':OFF');
      if (fadeOut > 0) {
        var fadeStart = Math.max(startMs, endMs - fadeOut);
        add(fadeStart, 'OUT:' + ch + ':FADE:0:' + fadeOut);
      } else {
        add(endMs, 'OUT:' + ch + ':OFF');
      }
      return;
    }

    /* hold (includes legacy pwm) */
    if (duty <= 0) add(startMs, 'OUT:' + ch + ':OFF');
    else add(startMs, 'OUT:' + ch + ':LEVEL:' + duty);
    add(endMs, 'OUT:' + ch + ':OFF');
  }

  root.ShowduinoMosfetAuthoring = {
    OUT_COUNT: OUT_COUNT,
    ROUTE: ROUTE,
    TYPE: TYPE,
    TIMELINE_COMMAND_MAX: TIMELINE_COMMAND_MAX,
    canonicalNodeId: canonicalNodeId,
    isMosfetId: isMosfetId,
    parseOutIndex: parseOutIndex,
    outToken: outToken,
    normaliseMode: normaliseMode,
    defaultParams: defaultParams,
    migrateParams: migrateParams,
    liveOutputs: liveOutputs,
    discoverFromLive: discoverFromLive,
    compileClip: compileClip,
    nodeCommand: nodeCommand
  };
})(typeof window !== 'undefined' ? window : globalThis);
