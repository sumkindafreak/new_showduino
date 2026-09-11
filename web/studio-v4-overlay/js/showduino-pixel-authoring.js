/* Showduino Studio V4 — shared pixel-output authoring model.
 *
 * P4 GPIO23 Show Pixel Line and C3 Pixel Nodes are instances of the same
 * PIXEL LINE → SEGMENTS → EFFECTS → PARAMETERS contract.
 *
 * Logical Node ID is production identity. Friendly name is presentation.
 * Live discovery never silently retargets a saved cue.
 */
(function (root) {
  'use strict';

  var P4_LOGICAL_ID = 'p4';
  var P4_DEVICE_ID = 'p4-show-pixels';
  var P4_ROUTE = 'p4-show-pixels';
  var P4_TYPE = 'p4-pixel-line';
  var P4_LABEL = 'P4 Show Pixel Line';
  var PIXEL_ROUTE = 'pixel-node';
  var PIXEL_TYPE = 'pixel-node';
  var PIXEL_SEGMENT_SLOTS = 16;
  var P4_MAX_PIXELS = 1024;
  var PIXEL_NODE_MAX_PIXELS = 512;
  var PIXEL_NODE_ID_MAX = 12;
  var PIXEL_NODE_NAME_MAX = 20;

  var P4_ALIASES = {
    p4: true,
    'p4-local': true,
    'p4-show-pixels': true,
    'show-pixels': true,
    'show-pixel': true,
    gpio23: true,
    'p4-gpio23': true
  };

  var EFFECTS = Object.freeze([
    { id: 'OFF', name: 'Off', family: 'UTILITY', params: [] },
    { id: 'SOLID', name: 'Solid', family: 'STATIC', params: ['colour', 'brightness'] },
    { id: 'FADE_IN', name: 'Fade In', family: 'LEVEL', params: ['colour', 'brightness', 'speed'] },
    { id: 'FADE_OUT', name: 'Fade Out', family: 'LEVEL', params: ['colour', 'brightness', 'speed'] },
    { id: 'PULSE', name: 'Pulse', family: 'LEVEL', params: ['colour', 'brightness', 'speed'] },
    { id: 'BREATHE', name: 'Breathe', family: 'LEVEL', params: ['colour', 'brightness', 'speed'] },
    { id: 'FLICKER', name: 'Flicker', family: 'ORGANIC', params: ['colour', 'brightness', 'speed', 'intensity', 'randomness'] },
    { id: 'CANDLE', name: 'Candle', family: 'ORGANIC', params: ['colour', 'secondary', 'brightness', 'speed', 'randomness'] },
    { id: 'FIRE', name: 'Fire', family: 'ORGANIC', params: ['colour', 'secondary', 'brightness', 'speed', 'intensity', 'randomness'] },
    { id: 'LIGHTNING', name: 'Lightning', family: 'IMPACT', params: ['colour', 'brightness', 'speed', 'intensity', 'randomness'] },
    { id: 'STROBE', name: 'Strobe', family: 'IMPACT', params: ['colour', 'brightness', 'speed'] },
    { id: 'RANDOM_STROBE', name: 'Random Strobe', family: 'IMPACT', params: ['colour', 'brightness', 'speed', 'randomness'] },
    { id: 'CHASE', name: 'Chase', family: 'MOTION', params: ['colour', 'brightness', 'speed', 'intensity', 'reverse'] },
    { id: 'BOUNCE', name: 'Bounce', family: 'MOTION', params: ['colour', 'brightness', 'speed', 'intensity', 'reverse'] },
    { id: 'COMET', name: 'Comet', family: 'MOTION', params: ['colour', 'brightness', 'speed', 'intensity', 'reverse'] },
    { id: 'WIPE', name: 'Wipe', family: 'MOTION', params: ['colour', 'brightness', 'speed', 'reverse'] },
    { id: 'REVERSE_WIPE', name: 'Reverse Wipe', family: 'MOTION', params: ['colour', 'brightness', 'speed', 'reverse'] },
    { id: 'BUILD', name: 'Build', family: 'MOTION', params: ['colour', 'brightness', 'speed', 'reverse'] },
    { id: 'SPARKLE', name: 'Sparkle', family: 'ORGANIC', params: ['colour', 'brightness', 'speed', 'intensity', 'randomness'] },
    { id: 'TWINKLE', name: 'Twinkle', family: 'ORGANIC', params: ['colour', 'brightness', 'speed', 'intensity', 'randomness'] },
    { id: 'GLITCH', name: 'Glitch', family: 'IMPACT', params: ['colour', 'secondary', 'brightness', 'speed', 'intensity'] },
    { id: 'WARNING', name: 'Warning', family: 'COLOUR', params: ['colour', 'secondary', 'brightness', 'speed'] },
    { id: 'PORTAL', name: 'Portal', family: 'COLOUR', params: ['colour', 'secondary', 'brightness', 'speed'] },
    { id: 'RAINBOW', name: 'Rainbow', family: 'COLOUR', params: ['brightness', 'speed'] },
    { id: 'CUSTOM_SEQUENCE', name: 'Custom Sequence', family: 'COLOUR', params: ['colour', 'secondary', 'brightness', 'speed', 'intensity'] }
  ]);

  var EFFECT_BY_ID = {};
  EFFECTS.forEach(function (fx) { EFFECT_BY_ID[fx.id] = fx; });

  var EFFECT_ALIASES = {
    OFF: 'OFF',
    BLACKOUT: 'OFF',
    SOLID: 'SOLID',
    FADE: 'FADE_IN',
    FADE_IN: 'FADE_IN',
    FADEOUT: 'FADE_OUT',
    FADE_OUT: 'FADE_OUT',
    PULSE: 'PULSE',
    BREATHE: 'BREATHE',
    FLICKER: 'FLICKER',
    UV_FLICKER: 'FLICKER',
    CANDLE: 'CANDLE',
    EMBER: 'CANDLE',
    FIRE: 'FIRE',
    LIGHTNING: 'LIGHTNING',
    STROBE: 'STROBE',
    FLASH: 'STROBE',
    RANDOM_STROBE: 'RANDOM_STROBE',
    CHASE: 'CHASE',
    THEATRE: 'CHASE',
    THEATER: 'CHASE',
    BOUNCE: 'BOUNCE',
    SCANNER: 'BOUNCE',
    RIPPLE: 'BOUNCE',
    COMET: 'COMET',
    METEOR: 'COMET',
    WIPE: 'WIPE',
    COLOUR_WIPE: 'WIPE',
    COLOR_WIPE: 'WIPE',
    REVERSE_WIPE: 'REVERSE_WIPE',
    BUILD: 'BUILD',
    SPARKLE: 'SPARKLE',
    CONFETTI: 'SPARKLE',
    TWINKLE: 'TWINKLE',
    GLITCH: 'GLITCH',
    WARNING: 'WARNING',
    WARNING_RED: 'WARNING',
    POLICE: 'WARNING',
    PORTAL: 'PORTAL',
    PORTAL_GLOW: 'PORTAL',
    RAINBOW: 'RAINBOW',
    WAVE: 'PULSE',
    CUSTOM_SEQUENCE: 'CUSTOM_SEQUENCE'
  };

  var liveSnapshot = { fetched: false, p4Online: null, lighting: null, system: null, nodes: [] };

  function clamp(value, min, max, fallback) {
    var number = Number(value);
    if (!isFinite(number)) return fallback;
    return Math.max(min, Math.min(max, number));
  }

  function text(value) {
    return String(value == null ? '' : value).trim();
  }

  function sameId(a, b) {
    return text(a).toLowerCase() === text(b).toLowerCase();
  }

  function token(value) {
    return text(value).toUpperCase().replace(/[\s-]+/g, '_').replace(/[^A-Z0-9_]/g, '');
  }

  function isP4Id(value) {
    var id = text(value).toLowerCase();
    if (!id) return false;
    if (P4_ALIASES[id]) return true;
    return id.indexOf('show-pixel') >= 0 || id === 'p4 show pixel line';
  }

  function canonicalNodeId(value) {
    var id = text(value);
    if (!id) return '';
    if (isP4Id(id)) return P4_LOGICAL_ID;
    return id;
  }

  function pixelNodeIdOk(value) {
    var id = text(value);
    if (!id || isP4Id(id)) return false;
    if (id.length < 2 || id.length > PIXEL_NODE_ID_MAX) return false;
    if (!/^[A-Za-z][A-Za-z0-9_-]*$/.test(id)) return false;
    return true;
  }

  function canonicalizeEffect(value) {
    var key = token(value || 'SOLID');
    if (EFFECT_ALIASES[key]) return EFFECT_ALIASES[key];
    if (EFFECT_BY_ID[key]) return key;
    return '';
  }

  function effectById(value) {
    var id = canonicalizeEffect(value) || 'SOLID';
    return EFFECT_BY_ID[id] || EFFECT_BY_ID.SOLID;
  }

  function effectSupports(effectId, paramName) {
    var fx = effectById(effectId);
    return fx.params.indexOf(paramName) >= 0;
  }

  function routeForNodeId(nodeId) {
    var id = canonicalNodeId(nodeId);
    if (!id) return '';
    return isP4Id(id) ? P4_ROUTE : PIXEL_ROUTE;
  }

  function typeForNodeId(nodeId) {
    return isP4Id(nodeId) ? P4_TYPE : PIXEL_TYPE;
  }

  function packageDeviceId(nodeId) {
    var id = canonicalNodeId(nodeId);
    if (!id) return '';
    if (isP4Id(id)) return P4_DEVICE_ID;
    return id;
  }

  function defaultP4Output(live) {
    var lighting = live && live.lighting ? live.lighting : {};
    var p4Online = live && live.p4Online;
    var haveLive = lighting.showPixelsReady != null || p4Online != null || lighting.showPixelsMax != null;
    var initialised = haveLive ? lighting.showPixelsReady === true : true;
    var online = p4Online === false ? false : (haveLive ? p4Online !== false : true);
    var pixelCount = Number(lighting.showPixelsConfiguredCount || lighting.showPixelsCount || 0) || 0;
    return {
      logicalId: P4_LOGICAL_ID,
      deviceId: P4_DEVICE_ID,
      route: P4_ROUTE,
      type: P4_TYPE,
      name: P4_LABEL,
      friendlyName: P4_LABEL,
      online: online,
      missing: false,
      initialised: initialised,
      pixelCount: pixelCount,
      maxPixels: Number(lighting.showPixelsMax) > 0 ? Number(lighting.showPixelsMax) : P4_MAX_PIXELS,
      segments: PIXEL_SEGMENT_SLOTS,
      state: !online ? 'OFFLINE' : (initialised ? 'READY' : 'NOT INITIALISED'),
      source: 'p4'
    };
  }

  function nodeFromLive(node) {
    var id = text(node && (node.id || node.nodeId || node.logicalId));
    if (!pixelNodeIdOk(id)) return null;
    var friendly = text(node.name || node.friendlyName || '');
    var online = node.online === true;
    var initialised = node.initialised === true;
    var pixelCount = Number(node.pixelCount || 0) || 0;
    var maxPixels = Number(node.maxPixels) > 0 ? Number(node.maxPixels) : PIXEL_NODE_MAX_PIXELS;
    var state = text(node.state);
    if (!state) {
      if (!online) state = 'OFFLINE';
      else if (!initialised) state = 'NOT INITIALISED';
      else state = 'ONLINE';
    }
    return {
      logicalId: id,
      deviceId: id,
      route: PIXEL_ROUTE,
      type: PIXEL_TYPE,
      name: friendly || id,
      friendlyName: friendly || id,
      online: online,
      missing: false,
      initialised: initialised,
      pixelCount: pixelCount,
      maxPixels: maxPixels,
      segments: Number(node.segments) >= 0 ? Number(node.segments) : PIXEL_SEGMENT_SLOTS,
      state: state,
      firmware: text(node.firmware || node.firmwareVersion),
      source: 'live'
    };
  }

  function placeholderOutput(nodeId, name, flags) {
    var id = canonicalNodeId(nodeId);
    var p4 = isP4Id(id);
    flags = flags || {};
    return {
      logicalId: p4 ? P4_LOGICAL_ID : id,
      deviceId: packageDeviceId(id),
      route: routeForNodeId(id),
      type: typeForNodeId(id),
      name: text(name) || (p4 ? P4_LABEL : id),
      friendlyName: text(name) || (p4 ? P4_LABEL : id),
      online: false,
      missing: flags.missing !== false,
      initialised: false,
      pixelCount: 0,
      maxPixels: p4 ? P4_MAX_PIXELS : PIXEL_NODE_MAX_PIXELS,
      segments: PIXEL_SEGMENT_SLOTS,
      state: flags.missing === false ? 'OFFLINE' : 'MISSING',
      source: 'project'
    };
  }

  function mergeOutput(existing, incoming) {
    if (!existing) return incoming;
    if (incoming.source === 'live' || (incoming.online && !existing.online)) {
      existing.online = incoming.online;
      existing.initialised = incoming.initialised;
      existing.pixelCount = incoming.pixelCount || existing.pixelCount;
      existing.maxPixels = incoming.maxPixels || existing.maxPixels;
      existing.segments = incoming.segments || existing.segments;
      existing.state = incoming.state || existing.state;
      existing.firmware = incoming.firmware || existing.firmware;
      existing.missing = false;
      if (incoming.source === 'live') existing.source = 'live';
    }
    if (incoming.friendlyName && incoming.friendlyName !== incoming.logicalId) {
      existing.friendlyName = incoming.friendlyName;
      existing.name = incoming.friendlyName;
    }
    if (incoming.source === 'live') existing.missing = false;
    return existing;
  }

  function collectProjectPixelIds(project) {
    var found = [];
    function remember(id, name) {
      var logical = canonicalNodeId(id);
      if (!logical) return;
      if (isP4Id(logical)) logical = P4_LOGICAL_ID;
      else if (!pixelNodeIdOk(logical) && !isP4Id(logical)) return;
      var key = logical.toLowerCase();
      for (var i = 0; i < found.length; i++) {
        if (found[i].logicalId.toLowerCase() === key) {
          if (name && !found[i].name) found[i].name = name;
          return;
        }
      }
      found.push({ logicalId: logical, name: text(name) });
    }

    var devices = project && Array.isArray(project.devices) ? project.devices : [];
    devices.forEach(function (device) {
      var route = text(device && device.binding && device.binding.route);
      var nodeId = text(device && device.binding && device.binding.nodeId) || text(device && device.id);
      if (route === P4_ROUTE || route === PIXEL_ROUTE || text(device && device.type).indexOf('pixel') >= 0) {
        remember(nodeId || device.id, device && device.name);
      }
    });

    var clips = project && Array.isArray(project.clips) ? project.clips : [];
    clips.forEach(function (clip) {
      if (String(clip && (clip.type || clip.action && clip.action.type) || '').toLowerCase() !== 'pixel') return;
      var nodeId = text(clip.routing && clip.routing.nodeId) ||
        text(clip.action && clip.action.targetDeviceId) ||
        text(clip.targetDeviceId);
      remember(nodeId, clip.target || clip.action && clip.action.target);
    });
    return found;
  }

  function listPixelOutputs(options) {
    options = options || {};
    var selectedId = canonicalNodeId(options.selectedId);
    var live = options.live || liveSnapshot;
    var map = {};
    var order = [];

    function add(item) {
      if (!item || !item.logicalId) return;
      var key = item.logicalId.toLowerCase();
      if (map[key]) {
        map[key] = mergeOutput(map[key], item);
        return;
      }
      map[key] = item;
      order.push(key);
    }

    add(defaultP4Output(live));

    var nodes = (live && Array.isArray(live.nodes)) ? live.nodes : [];
    nodes.forEach(function (node) {
      var item = nodeFromLive(node);
      if (item) add(item);
    });

    collectProjectPixelIds(options.project).forEach(function (entry) {
      if (map[entry.logicalId.toLowerCase()]) {
        if (entry.name) {
          map[entry.logicalId.toLowerCase()].friendlyName = map[entry.logicalId.toLowerCase()].friendlyName || entry.name;
        }
        return;
      }
      add(placeholderOutput(entry.logicalId, entry.name, { missing: true }));
    });

    if (selectedId && !map[selectedId.toLowerCase()]) {
      if (isP4Id(selectedId)) add(defaultP4Output(live));
      else add(placeholderOutput(selectedId, '', { missing: true }));
    }

    return order.map(function (key) { return map[key]; });
  }

  function optionLabel(output) {
    if (!output) return 'Select pixel output';
    var id = output.logicalId === P4_LOGICAL_ID ? P4_LABEL : output.logicalId;
    var friendly = output.friendlyName && output.friendlyName !== output.logicalId && output.logicalId !== P4_LOGICAL_ID
      ? output.friendlyName
      : '';
    var title = friendly ? (id + ' — ' + friendly) : id;
    if (output.logicalId !== P4_LOGICAL_ID) {
      if (output.missing) title += ' · Missing / Offline';
      else if (output.online === false) title += ' · OFFLINE';
    } else if (output.online === false) {
      title += ' · OFFLINE';
    }
    return title;
  }

  function statusText(output) {
    if (!output) return 'No device selected';
    if (output.logicalId !== P4_LOGICAL_ID && !pixelNodeIdOk(output.logicalId) && !isP4Id(output.logicalId)) {
      return 'Malformed Pixel Node ID';
    }
    if (output.missing) return 'This production expects this output, but it is not currently available.';
    if (output.online === false) return 'OFFLINE';
    if (!output.initialised) return 'NOT INITIALISED';
    var bits = [];
    if (output.online) bits.push('ONLINE');
    if (output.pixelCount > 0) bits.push(output.pixelCount + ' PIXELS');
    if (output.initialised) bits.push('INITIALISED');
    return bits.join(' · ') || 'READY';
  }

  function statusKind(output) {
    if (!output || !output.logicalId) return 'error';
    if (output.logicalId !== P4_LOGICAL_ID && !pixelNodeIdOk(output.logicalId)) return 'error';
    if (output.missing || output.online === false || !output.initialised) return 'warn';
    return 'ok';
  }

  function escapeHtml(value) {
    return String(value == null ? '' : value)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;')
      .replace(/'/g, '&#039;');
  }

  function selectHtml(options) {
    options = options || {};
    var selectedId = canonicalNodeId(options.selectedId);
    var outputs = listPixelOutputs(options);
    var found = false;
    var html = '<select class="' + (options.selectClass || 'sm-select') + '" id="' + escapeHtml(options.id || 'pixel-output') + '" aria-label="Pixel output">';
    outputs.forEach(function (output) {
      var selected = sameId(output.logicalId, selectedId);
      if (selected) found = true;
      html += '<option value="' + escapeHtml(output.logicalId) + '"' + (selected ? ' selected' : '') + '>' +
        escapeHtml(optionLabel(output)) + '</option>';
    });
    if (selectedId && !found) {
      html += '<option value="' + escapeHtml(selectedId) + '" selected>' +
        escapeHtml(optionLabel(placeholderOutput(selectedId, '', { missing: true }))) + '</option>';
    }
    html += '</select>';
    var current = outputs.filter(function (item) { return sameId(item.logicalId, selectedId); })[0] ||
      (selectedId ? placeholderOutput(selectedId, '', { missing: true }) : null);
    var kind = statusKind(current);
    if (current && kind !== 'ok') {
      html += '<div class="sd-pixel-status sd-pixel-status-' + kind + '">' + escapeHtml(statusText(current)) + '</div>';
    } else if (current && !current.initialised) {
      html += '<div class="sd-pixel-status sd-pixel-status-warn">' + escapeHtml(statusText(current)) + '</div>';
    }
    return html;
  }

  function shdoDeviceForNode(nodeId, extras) {
    extras = extras || {};
    var logical = canonicalNodeId(nodeId);
    if (!logical) return null;
    var p4 = isP4Id(logical);
    var device = {
      id: packageDeviceId(logical),
      name: extras.name || (p4 ? P4_LABEL : logical),
      type: typeForNodeId(logical),
      enabled: true,
      binding: {
        route: routeForNodeId(logical),
        nodeId: p4 ? P4_LOGICAL_ID : logical
      },
      capabilities: {
        segmentedPixels: true,
        maxPixels: extras.maxPixels || (p4 ? P4_MAX_PIXELS : PIXEL_NODE_MAX_PIXELS),
        segments: PIXEL_SEGMENT_SLOTS
      },
      metadata: extras.metadata || {}
    };
    if (extras.pixelStart != null) device.binding.pixelStart = Number(extras.pixelStart) || 0;
    if (extras.pixelCount != null) device.binding.pixelCount = Number(extras.pixelCount) || 0;
    return device;
  }

  function commandPrefix(device) {
    var route = text(device && device.binding && device.binding.route);
    var nodeId = text(device && device.binding && device.binding.nodeId);
    if (route === P4_ROUTE || (!route && isP4Id(nodeId))) return 'PIXEL:';
    if (route === PIXEL_ROUTE) {
      if (!pixelNodeIdOk(nodeId)) return null;
      return 'PIXEL:NODE:' + nodeId + ':';
    }
    return null;
  }

  function authoredSegment(params, fallbackSlot) {
    if (!params || typeof params !== 'object') return fallbackSlot;
    var hasSegment = Object.prototype.hasOwnProperty.call(params, 'segment') ||
      Object.prototype.hasOwnProperty.call(params, 'segmentId');
    if (!hasSegment) return fallbackSlot;
    var raw = params.segment != null ? params.segment : params.segmentId;
    if (raw === '' || raw == null) return fallbackSlot;
    var slot = Number(raw);
    if (!isFinite(slot)) return fallbackSlot;
    return clamp(slot, 0, PIXEL_SEGMENT_SLOTS - 1, fallbackSlot);
  }

  function defaultPixelParams() {
    return {
      segment: 0,
      segmentMode: 'range',
      segmentName: 'Segment 0',
      startPixel: 0,
      length: 10,
      groupSize: 10,
      markerOffset: 0,
      r: 0,
      g: 255,
      b: 200,
      secondary: '#101820',
      brightness: 255,
      effect: 'SOLID',
      speed: 50,
      intensity: 80,
      randomness: 70,
      reverse: false,
      fadeMs: 0,
      blackoutAtEnd: false
    };
  }

  function migratePixelParams(params) {
    var defaults = defaultPixelParams();
    var p = params && typeof params === 'object' ? params : {};
    if (p.length == null && p.count != null) p.length = p.count;
    var merged = {};
    Object.keys(defaults).forEach(function (key) { merged[key] = defaults[key]; });
    Object.keys(p).forEach(function (key) { merged[key] = p[key]; });
    merged.segment = clamp(merged.segment != null ? merged.segment : merged.segmentId, 0, PIXEL_SEGMENT_SLOTS - 1, 0);
    merged.startPixel = clamp(merged.startPixel, 0, 100000, 0);
    merged.length = clamp(merged.length, 1, 100000, 10);
    merged.brightness = clamp(merged.brightness, 0, 255, 255);
    merged.speed = clamp(merged.speed, 1, 100, 50);
    merged.intensity = clamp(merged.intensity, 0, 100, 80);
    merged.randomness = clamp(merged.randomness, 0, 100, 70);
    merged.reverse = Boolean(merged.reverse);
    merged.blackoutAtEnd = Boolean(merged.blackoutAtEnd);
    var fx = canonicalizeEffect(merged.effect);
    merged.effect = fx || 'SOLID';
    if (!merged.segmentName || merged.segmentName === 'Segment A') {
      merged.segmentName = 'Segment ' + merged.segment;
    }
    return merged;
  }

  function validatePixelTarget(nodeId, live) {
    var errors = [];
    var warnings = [];
    var logical = canonicalNodeId(nodeId);
    if (!logical) {
      errors.push({ code: 'NO_DEVICE', kind: 'authoring', message: 'No device selected' });
      return { ok: false, errors: errors, warnings: warnings, output: null };
    }
    if (!isP4Id(logical) && !pixelNodeIdOk(logical)) {
      errors.push({ code: 'BAD_ID', kind: 'authoring', message: 'Malformed Pixel Node ID' });
      return { ok: false, errors: errors, warnings: warnings, output: null };
    }
    var outputs = listPixelOutputs({ live: live || liveSnapshot, selectedId: logical });
    var output = outputs.filter(function (item) { return sameId(item.logicalId, logical); })[0] ||
      placeholderOutput(logical, '', { missing: true });
    if (output.missing) {
      warnings.push({ code: 'MISSING', kind: 'hardware', message: logical + ' currently missing / offline' });
    } else if (output.online === false) {
      warnings.push({ code: 'OFFLINE', kind: 'hardware', message: (isP4Id(logical) ? P4_LABEL : logical) + ' currently offline' });
    }
    if (!output.missing && output.online !== false && !output.initialised) {
      warnings.push({ code: 'NOT_INITIALISED', kind: 'hardware', message: (isP4Id(logical) ? P4_LABEL : logical) + ' is NOT INITIALISED' });
    }
    return { ok: errors.length === 0, errors: errors, warnings: warnings, output: output };
  }

  function validatePixelClip(clip, live) {
    var name = text(clip && (clip.name || clip.label)) || 'Pixel cue';
    var params = migratePixelParams(clip && clip.params);
    var nodeId = text(clip && clip.routing && clip.routing.nodeId) ||
      text(clip && clip.targetDeviceId);
    var result = validatePixelTarget(nodeId, live);
    result.params = params;
    if (params.segment < 0 || params.segment >= PIXEL_SEGMENT_SLOTS) {
      result.errors.push({ code: 'BAD_SEGMENT', kind: 'authoring', message: name + ': segment must be 0–15' });
    }
    if (!canonicalizeEffect(params.effect)) {
      result.errors.push({ code: 'BAD_EFFECT', kind: 'authoring', message: name + ': unsupported effect' });
    }
    if (params.length < 1) {
      result.errors.push({ code: 'BAD_RANGE', kind: 'authoring', message: name + ': segment length must be at least 1' });
    }
    var maxPixels = result.output ? result.output.maxPixels : P4_MAX_PIXELS;
    if (result.output && !result.output.missing && params.startPixel + params.length > maxPixels) {
      result.warnings.push({
        code: 'RANGE_LIMIT',
        kind: 'hardware',
        message: name + ': range exceeds this output\'s ' + maxPixels + '-pixel maximum'
      });
    }
    result.ok = result.errors.length === 0;
    return result;
  }

  function ingestLivePayloads(payloads) {
    var lighting = null;
    var system = null;
    var p4Online = null;
    var nodes = [];
    var seen = {};
    function rememberNode(node) {
      var item = nodeFromLive(node);
      if (!item) return;
      var key = item.logicalId.toLowerCase();
      if (seen[key]) return;
      seen[key] = true;
      nodes.push(node);
    }

    (payloads || []).forEach(function (payload) {
      if (!payload || typeof payload !== 'object') return;
      if (payload.showPixelsReady != null || payload.showPixelsMax != null || payload.showPixelPin != null) {
        lighting = payload;
      }
      if (payload.role === 'stage' || payload.audioNode || payload.lampNode) system = payload;
      if (payload.p4Online === false) p4Online = false;
      if (payload.p4Online === true) p4Online = true;
      if (Array.isArray(payload.pixelNodes)) payload.pixelNodes.forEach(rememberNode);
      if (Array.isArray(payload.devices)) {
        payload.devices.forEach(function (device) {
          var role = text(device && (device.role || device.type)).toUpperCase();
          if (role !== 'PIXEL' && role !== 'PIXEL-NODE' && role !== 'PIXEL_NODE') return;
          rememberNode({
            id: device.id,
            name: device.friendlyName || device.name,
            online: device.online,
            initialised: device.initialised,
            pixelCount: device.pixelCount,
            maxPixels: device.maxPixels,
            state: device.state,
            firmware: device.firmwareVersion || device.firmware
          });
        });
      }
    });
    if (system && system.p4Online === false) p4Online = false;
    if (system && system.p4Online === true && p4Online == null) p4Online = true;
    liveSnapshot = {
      fetched: true,
      p4Online: p4Online,
      lighting: lighting,
      system: system,
      nodes: nodes
    };
    return liveSnapshot;
  }

  async function refreshLiveOutputs(fetchImpl) {
    var fetchFn = fetchImpl || (typeof fetch === 'function' ? fetch : null);
    if (!fetchFn) return liveSnapshot;
    var payloads = [];
    var paths = ['/api/comms', '/api/system', '/api/lighting', '/api/devices'];
    for (var i = 0; i < paths.length; i++) {
      try {
        var response = await fetchFn(paths[i], { headers: { Accept: 'application/json' }, cache: 'no-store' });
        if (!response) continue;
        var body = await response.json();
        if (body && typeof body === 'object') payloads.push(body);
      } catch (_) {}
    }
    if (payloads.length) ingestLivePayloads(payloads);
    return liveSnapshot;
  }

  function findOutput(nodeId, options) {
    var outputs = listPixelOutputs(options);
    var logical = canonicalNodeId(nodeId);
    for (var i = 0; i < outputs.length; i++) {
      if (sameId(outputs[i].logicalId, logical)) return outputs[i];
    }
    return null;
  }

  var api = {
    P4_LOGICAL_ID: P4_LOGICAL_ID,
    P4_DEVICE_ID: P4_DEVICE_ID,
    P4_ROUTE: P4_ROUTE,
    P4_TYPE: P4_TYPE,
    P4_LABEL: P4_LABEL,
    PIXEL_ROUTE: PIXEL_ROUTE,
    PIXEL_TYPE: PIXEL_TYPE,
    PIXEL_SEGMENT_SLOTS: PIXEL_SEGMENT_SLOTS,
    P4_MAX_PIXELS: P4_MAX_PIXELS,
    PIXEL_NODE_MAX_PIXELS: PIXEL_NODE_MAX_PIXELS,
    EFFECTS: EFFECTS,
    clamp: clamp,
    sameId: sameId,
    isP4Id: isP4Id,
    canonicalNodeId: canonicalNodeId,
    pixelNodeIdOk: pixelNodeIdOk,
    canonicalizeEffect: canonicalizeEffect,
    effectById: effectById,
    effectSupports: effectSupports,
    routeForNodeId: routeForNodeId,
    typeForNodeId: typeForNodeId,
    packageDeviceId: packageDeviceId,
    listPixelOutputs: listPixelOutputs,
    optionLabel: optionLabel,
    statusText: statusText,
    statusKind: statusKind,
    selectHtml: selectHtml,
    escapeHtml: escapeHtml,
    shdoDeviceForNode: shdoDeviceForNode,
    commandPrefix: commandPrefix,
    authoredSegment: authoredSegment,
    defaultPixelParams: defaultPixelParams,
    migratePixelParams: migratePixelParams,
    validatePixelTarget: validatePixelTarget,
    validatePixelClip: validatePixelClip,
    ingestLivePayloads: ingestLivePayloads,
    refreshLiveOutputs: refreshLiveOutputs,
    findOutput: findOutput,
    getLiveSnapshot: function () { return liveSnapshot; },
    resetLiveSnapshot: function () {
      liveSnapshot = { fetched: false, p4Online: null, lighting: null, system: null, nodes: [] };
    }
  };

  root.ShowduinoPixelAuthoring = Object.freeze(api);
  if (typeof module !== 'undefined' && module.exports) module.exports = root.ShowduinoPixelAuthoring;
})(typeof globalThis !== 'undefined' ? globalThis : this);
