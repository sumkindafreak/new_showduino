// SHDO Model - Showduino Studio v4 project data model
// Static factory and migration helpers for current Director -> Comms -> P4 -> Nodes productions.

class SHDOModel {
  static _uuid() {
    if (window.crypto && typeof window.crypto.randomUUID === 'function') return window.crypto.randomUUID();
    return 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, c => {
      const r = Math.random() * 16 | 0;
      return (c === 'x' ? r : (r & 0x3 | 0x8)).toString(16);
    });
  }

  static createProject(name = 'Untitled Show') {
    const now = new Date().toISOString();
    return {
      project: {
        id: SHDOModel._uuid(),
        name,
        version: '2.0.0',
        createdAt: now,
        updatedAt: now,
        bpm: 120,
        duration: 300000
      },
      architecture: {
        version: 1,
        creator: 'showduino-studio',
        runtimeAuthority: 'esp32-p4-show-engine',
        operator: 'esp32-s3-director',
        transport: 'esp32-s3-comms-controller',
        nodeAddressing: 'logical-device-id'
      },
      tracks: [],
      clips: [],
      markers: [],
      config: {
        snapEnabled: true,
        snapMs: 1000,
        gridEnabled: true,
        zoom: 0.1
      }
    };
  }

  static createTrack(type = 'audio', name = null, order = 0) {
    const colors = {
      mixed: '#7f8c98',
      audio: '#68d8ff',
      fx: '#bb86ff',
      relay: '#ffad5c',
      mosfet: '#ffc857',
      pixel: '#00ffc8',
      trigger: '#ff6f82',
      // Legacy colours retained so old imported projects remain readable.
      lighting: '#d7cc58',
      dmx: '#ff4b86',
      prop: '#5fcf7b'
    };
    return {
      id: SHDOModel._uuid(),
      type,
      name: name || (type === 'mixed' ? 'Mixed Lane' : `${type.charAt(0).toUpperCase() + type.slice(1)} Track`),
      muted: false,
      visible: true,
      locked: false,
      color: colors[type] || '#888888',
      order
    };
  }

  static createClip(trackId, type = 'audio', startMs = 0, durationMs = 5000, label = null) {
    const colors = {
      audio: '#68d8ff',
      fx: '#bb86ff',
      relay: '#ffad5c',
      mosfet: '#ffc857',
      pixel: '#00ffc8',
      trigger: '#ff6f82',
      // Legacy compatibility.
      lighting: '#d7cc58',
      dmx: '#ff4b86',
      prop: '#5fcf7b'
    };
    return {
      id: SHDOModel._uuid(),
      trackId,
      type,
      startMs,
      durationMs,
      label: label || (type.charAt(0).toUpperCase() + type.slice(1)),
      color: colors[type] || '#00ffc8',
      routing: {
        nodeId: '',
        output: ''
      },
      params: SHDOModel._defaultParams(type)
    };
  }

  static createMarker(timeMs = 0, label = 'Marker') {
    return {
      id: SHDOModel._uuid(),
      timeMs,
      label,
      color: '#ffc857'
    };
  }

  static _defaultParams(type) {
    switch (type) {
      case 'audio':
        return { file: '', volume: 100, loop: false, fadeIn: 0, fadeOut: 0, pan: 0, rate: 1 };
      case 'fx':
        return { effect: 'custom', intensity: 100, rampIn: 0, rampOut: 0, safeStop: true };
      case 'relay':
        return { out: 'out1', mode: 'hold', state: true, pulseMs: 0, safeOff: true };
      case 'mosfet':
        return { out: 'out1', mode: 'hold', state: true, duty: 100, pulseMs: 0, safeOff: true };
      case 'pixel':
        if (window.ShowduinoPixelAuthoring?.defaultPixelParams) {
          return window.ShowduinoPixelAuthoring.defaultPixelParams();
        }
        return {
          segment: 0,
          line: 1,
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
      case 'trigger':
        return { event: '', payload: '', scope: 'project', once: false };
      // Legacy types are retained for import compatibility only. Studio v4 does not offer new authoring controls for them.
      case 'lighting':
        return { r: 255, g: 255, b: 255, brightness: 255, effect: 'solid', legacy: true };
      case 'dmx':
        return { channels: {}, legacy: true };
      case 'prop':
        return { action: 'trigger', value: 1, legacy: true };
      default:
        return {};
    }
  }

  static validate(project) {
    if (!project || !project.project || !project.project.id) return false;
    if (!Array.isArray(project.tracks)) return false;
    if (!Array.isArray(project.clips)) return false;
    if (!Array.isArray(project.markers)) return false;
    return true;
  }

  static migrate(project) {
    if (!project || typeof project !== 'object') return project;
    const now = new Date().toISOString();
    project.project = project.project || {};
    project.project.id = project.project.id || SHDOModel._uuid();
    project.project.name = project.project.name || 'Untitled Show';
    project.project.version = '2.0.0';
    project.project.createdAt = project.project.createdAt || now;
    // Migration should never make a project look freshly edited just because a
    // dashboard/stat view inspected it. Normal save paths own updatedAt changes.
    project.project.updatedAt = project.project.updatedAt || now;

    project.architecture = {
      version: 1,
      creator: 'showduino-studio',
      runtimeAuthority: 'esp32-p4-show-engine',
      operator: 'esp32-s3-director',
      transport: 'esp32-s3-comms-controller',
      nodeAddressing: 'logical-device-id',
      ...(project.architecture || {})
    };

    if (!Array.isArray(project.tracks)) project.tracks = [];
    if (!Array.isArray(project.clips)) project.clips = [];
    if (!Array.isArray(project.markers)) project.markers = [];
    if (!project.config) project.config = {};
    if (typeof project.config.snapEnabled === 'undefined') project.config.snapEnabled = true;
    if (typeof project.config.snapMs === 'undefined') project.config.snapMs = 1000;
    if (typeof project.config.gridEnabled === 'undefined') project.config.gridEnabled = true;
    if (typeof project.config.zoom === 'undefined') project.config.zoom = 0.1;

    project.tracks.forEach((track, index) => {
      if (!track.id) track.id = SHDOModel._uuid();
      if (!track.name) track.name = `${track.type || 'Mixed'} Track`;
      if (!Number.isFinite(Number(track.order))) track.order = index;
      if (['dmx', 'lighting', 'prop'].includes(track.type)) track.legacy = true;
    });

    project.clips.forEach((clip) => {
      if (!clip.id) clip.id = SHDOModel._uuid();
      clip.routing = clip.routing && typeof clip.routing === 'object' ? clip.routing : { nodeId: '', output: '' };
      clip.routing.nodeId = clip.routing.nodeId || '';
      clip.routing.output = clip.routing.output || '';
      clip.params = clip.params && typeof clip.params === 'object' ? clip.params : SHDOModel._defaultParams(clip.type);

        if (clip.type === 'pixel') {
          const defaults = SHDOModel._defaultParams('pixel');
          const hadSegment = clip.params && (clip.params.segment != null || clip.params.segmentId != null);
          if (typeof clip.params.length === 'undefined' && typeof clip.params.count !== 'undefined') clip.params.length = clip.params.count || 10;
          if (window.ShowduinoPixelAuthoring?.migratePixelParams) {
            clip.params = window.ShowduinoPixelAuthoring.migratePixelParams(clip.params);
          } else {
            clip.params = { ...defaults, ...clip.params };
          }
          if (!hadSegment && clip.params) delete clip.params.segment;
          clip.params.startPixel = Math.max(0, Number(clip.params.startPixel) || 0);
          clip.params.length = Math.max(1, Number(clip.params.length) || 10);
          clip.routing = clip.routing && typeof clip.routing === 'object' ? clip.routing : { nodeId: '', output: '' };
          if (clip.routing.nodeId && window.ShowduinoPixelAuthoring?.canonicalNodeId) {
            clip.routing.nodeId = window.ShowduinoPixelAuthoring.canonicalNodeId(clip.routing.nodeId) || clip.routing.nodeId;
          }
        }

      if (clip.type === 'relay') clip.params = { ...SHDOModel._defaultParams('relay'), ...clip.params };
      if (clip.type === 'mosfet') clip.params = { ...SHDOModel._defaultParams('mosfet'), ...clip.params };
      if (clip.type === 'audio') clip.params = { ...SHDOModel._defaultParams('audio'), ...clip.params };
      if (['dmx', 'lighting', 'prop'].includes(clip.type)) {
        clip.legacy = true;
        clip.params.legacy = true;
      }
    });

    return project;
  }
}

window.SHDOModel = SHDOModel;
