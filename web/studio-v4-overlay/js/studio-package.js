/* Showduino Studio V4 — canonical SHDO v2 interchange package. */
(function () {
  'use strict';

  const FORMAT_NAME = 'showduino-production';
  const FORMAT_VERSION = 2;
  const SCHEMA_NAME = 'showduino-production-v2';

  const ARCHITECTURE = Object.freeze({
    version: 1,
    creator: 'showduino-studio',
    runtimeAuthority: 'esp32-p4-show-engine',
    operator: 'esp32-s3-director',
    transport: 'esp32-s3-comms-controller',
    nodeAddressing: 'logical-device-id'
  });

  const SAFETY = Object.freeze({
    runtimeAuthority: 'esp32-p4-show-engine',
    policy: 'firmware-authoritative',
    emergency: Object.freeze({
      stopTimeline: true,
      pixelOverride: 'all-white',
      requiresManualClear: true,
      autoResume: false,
      productionCannotDisable: true
    })
  });

  function clone(value) {
    return value === undefined ? undefined : JSON.parse(JSON.stringify(value));
  }

  function slug(value, fallback = 'item') {
    const text = String(value || '').trim().toLowerCase()
      .replace(/[^a-z0-9]+/g, '-')
      .replace(/^-+|-+$/g, '');
    return text || fallback;
  }

  function int(value, fallback = 0) {
    const number = Number(value);
    return Number.isFinite(number) ? Math.max(0, Math.round(number)) : fallback;
  }

  function typeColor(type) {
    return ({
      audio: '#68d8ff', fx: '#bb86ff', relay: '#ffad5c', mosfet: '#ffc857',
      pixel: '#00ffc8', trigger: '#ff6f82', lighting: '#d7cc58',
      dmx: '#ff4b86', prop: '#5fcf7b'
    })[type] || '#888888';
  }

  function pixelAuthoring() {
    return window.ShowduinoPixelAuthoring || null;
  }

  function deviceTypeForClip(clip) {
    const node = String(clip?.routing?.nodeId || '').trim();
    const pixels = pixelAuthoring();
    switch (clip?.type) {
      case 'audio': return 'audio-node';
      case 'mosfet': return 'mosfet-node';
      case 'trigger': return 'input-node';
      case 'pixel': return pixels ? pixels.typeForNodeId(node) : (pixelsIsP4(node) ? 'p4-pixel-line' : 'pixel-node');
      case 'lighting': return 'lantern-node';
      case 'video': return 'projection';
      default: return 'custom';
    }
  }

  function pixelsIsP4(node) {
    const id = String(node || '').trim().toLowerCase();
    return id === 'p4' || id === 'p4-local' || id === 'p4-show-pixels' || id.includes('show-pixel');
  }

  function bindingRouteForClip(clip) {
    const node = String(clip?.routing?.nodeId || '').trim();
    const pixels = pixelAuthoring();
    switch (clip?.type) {
      case 'audio': return 'audio-node';
      case 'mosfet': return 'mosfet-node';
      case 'trigger': return 'input-node';
      case 'pixel': return pixels ? pixels.routeForNodeId(node) : (pixelsIsP4(node) ? 'p4-show-pixels' : 'pixel-node');
      case 'lighting': return 'lantern-node';
      case 'video': return 'projection';
      default: return 'unbound';
    }
  }

  function normaliseAssets(assets) {
    if (Array.isArray(assets)) return clone(assets);
    if (!assets || typeof assets !== 'object') return [];
    return Object.entries(assets).map(([id, value], index) => {
      if (typeof value === 'string') {
        return { id: id || `asset-${index + 1}`, name: value.split('/').pop() || value, type: 'data', path: value };
      }
      const item = value && typeof value === 'object' ? value : {};
      return {
        ...clone(item),
        id: String(item.id || id || `asset-${index + 1}`),
        name: String(item.name || item.label || item.path || id || `Asset ${index + 1}`),
        type: String(item.type || 'data'),
        path: String(item.path || item.file || item.url || '')
      };
    });
  }

  function ensurePackageMetadata(project) {
    if (!project || typeof project !== 'object') return project;
    if (window.SHDOModel?.migrate) window.SHDOModel.migrate(project);

    project.package = {
      format: FORMAT_NAME,
      version: FORMAT_VERSION,
      architecture: 'director-comms-p4-nodes',
      runtimeAuthority: 'esp32-p4-show-engine',
      ...(project.package && typeof project.package === 'object' ? project.package : {})
    };
    project.package.format = FORMAT_NAME;
    project.package.version = FORMAT_VERSION;
    project.package.architecture = 'director-comms-p4-nodes';
    project.package.runtimeAuthority = 'esp32-p4-show-engine';
    return project;
  }

  function deriveDevices(project, clips) {
    const devices = Array.isArray(project.devices) ? clone(project.devices) : [];
    const ids = new Set(devices.map((device) => String(device?.id || '')).filter(Boolean));
    const routeMap = new Map();

    devices.forEach((device) => {
      const binding = device?.binding || {};
      const node = String(binding.nodeId || '').trim().toLowerCase();
      const output = String(binding.outputLabel ?? binding.output ?? '').trim().toLowerCase();
      if (node) routeMap.set(`${device.type || ''}|${node}|${output}`, device.id);
    });

    const clipTargets = new Map();
    clips.forEach((clip) => {
      if (clip.targetDeviceId && ids.has(String(clip.targetDeviceId))) {
        clipTargets.set(clip.id, String(clip.targetDeviceId));
        return;
      }

      const nodeId = String(clip?.routing?.nodeId || '').trim();
      const outputRaw = String(clip?.routing?.output || '').trim();
      if (!nodeId) return;

        const pixels = pixelAuthoring();
        const logical = clip.type === 'pixel' && pixels ? pixels.canonicalNodeId(nodeId) : nodeId;
        const type = deviceTypeForClip(clip);
        const key = `${type}|${String(logical).toLowerCase()}|${outputRaw.toLowerCase()}`;
        let deviceId = routeMap.get(key);
        if (!deviceId && clip.type === 'pixel' && pixels) {
          const candidate = pixels.packageDeviceId(logical);
          const existing = devices.find((device) => String(device.id) === String(candidate) ||
            (device.binding && String(device.binding.nodeId || '').toLowerCase() === String(logical).toLowerCase() &&
              (device.binding.route === pixels.P4_ROUTE || device.binding.route === pixels.PIXEL_ROUTE)));
          if (existing) deviceId = existing.id;
          else if (ids.has(String(candidate))) deviceId = candidate;
        }
        if (!deviceId) {
          if (clip.type === 'pixel' && pixels) {
            const ready = pixels.shdoDeviceForNode(logical, {
              name: outputRaw && !pixels.isP4Id(logical) ? `${logical} — ${outputRaw}` : undefined,
              metadata: { derivedFromLegacyRouting: true }
            });
            if (ready) {
              deviceId = ready.id;
              if (!ids.has(deviceId)) {
                devices.push(ready);
                ids.add(deviceId);
              }
              routeMap.set(key, deviceId);
              clipTargets.set(clip.id, deviceId);
              return;
            }
          }
          const base = `device-${slug(nodeId)}-${slug(outputRaw || clip.type, clip.type || 'output')}`;
          deviceId = base;
          let suffix = 2;
          while (ids.has(deviceId)) deviceId = `${base}-${suffix++}`;

          const binding = { route: bindingRouteForClip(clip), nodeId: logical || nodeId };
          if (outputRaw) {
            if (/^\d+$/.test(outputRaw)) binding.output = Number(outputRaw);
            else binding.outputLabel = outputRaw;
          }

          devices.push({
            id: deviceId,
            name: outputRaw ? `${nodeId} · ${outputRaw}` : nodeId,
            type,
            enabled: true,
            binding,
            capabilities: {},
            metadata: { derivedFromLegacyRouting: true }
          });
          ids.add(deviceId);
          routeMap.set(key, deviceId);
        }
        clipTargets.set(clip.id, deviceId);
    });

    return { devices, clipTargets };
  }

  function canonicalScenes(project, clipIds) {
    const scenes = Array.isArray(project.scenes) ? clone(project.scenes) : [];
    const valid = scenes.every((scene) => scene && typeof scene === 'object' && Array.isArray(scene.cues) &&
      scene.cues.every((cue) => cue && Array.isArray(cue.clipIds)));
    if (valid && scenes.length) {
      return scenes.map((scene, sceneIndex) => ({
        id: String(scene.id || `scene-${sceneIndex + 1}`),
        name: String(scene.name || `Scene ${sceneIndex + 1}`),
        description: String(scene.description || ''),
        order: Number.isFinite(Number(scene.order)) ? Math.max(0, Math.round(Number(scene.order))) : sceneIndex,
        cues: scene.cues.map((cue, cueIndex) => ({
          id: String(cue.id || `cue-${sceneIndex + 1}-${cueIndex + 1}`),
          name: String(cue.name || `Cue ${cueIndex + 1}`),
          trigger: String(cue.trigger || 'Timeline'),
          notes: String(cue.notes || ''),
          clipIds: (cue.clipIds || []).filter((id) => clipIds.has(String(id))).map(String)
        }))
      }));
    }

    if (!clipIds.size) return [];
    return [{
      id: 'scene-main-timeline',
      name: 'Main Timeline',
      description: 'Timeline authored in Showduino Studio V4.',
      order: 0,
      cues: [{
        id: 'cue-main-timeline',
        name: 'Timeline',
        trigger: 'Timeline',
        notes: '',
        clipIds: Array.from(clipIds)
      }]
    }];
  }

  function clipAssignments(scenes) {
    const map = new Map();
    scenes.forEach((scene) => {
      scene.cues.forEach((cue) => {
        cue.clipIds.forEach((clipId) => map.set(String(clipId), { sceneId: scene.id, cueId: cue.id }));
      });
    });
    return map;
  }

  function toShdo(input) {
    const source = ensurePackageMetadata(clone(input || {}));
    const now = new Date().toISOString();
    const sourceClips = Array.isArray(source.clips) ? source.clips : [];
    const { devices, clipTargets } = deriveDevices(source, sourceClips);
    const clipIds = new Set(sourceClips.map((clip) => String(clip.id)).filter(Boolean));
    const scenes = canonicalScenes(source, clipIds);
    const assignment = clipAssignments(scenes);

    // Every timeline clip must live in a canonical scene/cue. If imported scene data
    // omitted a clip, preserve it under an explicit imported-timeline cue.
    const orphanIds = Array.from(clipIds).filter((id) => !assignment.has(id));
    if (orphanIds.length) {
      const fallback = {
        id: 'scene-unassigned-timeline',
        name: 'Unassigned Timeline',
        description: 'Clips preserved by Studio because they were not assigned to an authored scene cue.',
        order: scenes.length,
        cues: [{ id: 'cue-unassigned-timeline', name: 'Unassigned Clips', trigger: 'Timeline', notes: '', clipIds: orphanIds }]
      };
      scenes.push(fallback);
      orphanIds.forEach((id) => assignment.set(id, { sceneId: fallback.id, cueId: fallback.cues[0].id }));
    }

    const tracks = (source.tracks || []).map((track, index) => {
      const targetIds = sourceClips
        .filter((clip) => clip.trackId === track.id)
        .map((clip) => clipTargets.get(clip.id))
        .filter(Boolean);
      const uniqueTargets = Array.from(new Set(targetIds));
      return {
        id: String(track.id),
        name: String(track.name || `${track.type || 'Mixed'} Track`),
        type: String(track.type || 'mixed'),
        enabled: track.muted !== true,
        locked: track.locked === true,
        order: Number.isFinite(Number(track.order)) ? Math.max(0, Math.round(Number(track.order))) : index,
        ...(uniqueTargets.length === 1 ? { targetDeviceId: uniqueTargets[0] } : {})
      };
    });

    const clips = sourceClips.map((clip) => {
      const place = assignment.get(String(clip.id)) || { sceneId: 'scene-main-timeline', cueId: 'cue-main-timeline' };
      const targetDeviceId = clipTargets.get(clip.id);
      const device = targetDeviceId ? devices.find((item) => item.id === targetDeviceId) : null;
      return {
        id: String(clip.id),
        trackId: String(clip.trackId),
        sceneId: String(place.sceneId),
        cueId: String(place.cueId),
        name: String(clip.label || clip.type || 'Clip'),
        type: String(clip.type || 'custom'),
        startMs: int(clip.startMs),
        durationMs: int(clip.durationMs),
        enabled: clip.enabled !== false,
        ...(targetDeviceId ? { targetDeviceId } : {}),
        ...(device ? { target: String(device.name) } : {}),
        action: {
          type: String(clip.type || 'custom'),
          label: String(clip.label || clip.type || 'Clip'),
          ...(targetDeviceId ? { targetDeviceId } : {}),
          ...(device ? { target: String(device.name) } : {}),
          delayMs: 0,
          durationMs: int(clip.durationMs),
          value: clip.type === 'audio' ? String(clip.params?.file || '') : String(clip.label || ''),
          notes: String(clip.notes || ''),
          params: clone(clip.params && typeof clip.params === 'object' ? clip.params : {})
        }
      };
    });

    const markers = (source.markers || []).map((marker, index) => ({
      ...clone(marker),
      id: String(marker.id || `marker-${index + 1}`),
      name: String(marker.name || marker.label || `Marker ${index + 1}`),
      timeMs: int(marker.timeMs)
    }));

    return {
      schema: SCHEMA_NAME,
      project: {
        id: String(source.project?.id || ''),
        name: String(source.project?.name || 'Untitled Show'),
        description: String(source.project?.description || ''),
        version: '2.0.0',
        createdAt: String(source.project?.createdAt || now),
        updatedAt: String(source.project?.updatedAt || now),
        bpm: Number(source.project?.bpm) > 0 ? Number(source.project.bpm) : 120,
        duration: Math.max(1, int(source.project?.duration, 300000) || 300000)
      },
      architecture: { ...ARCHITECTURE },
      compatibility: {
        shdoMajor: 2,
        minimumStudioVersion: '2.0.0',
        minimumRuntimeProtocol: 1
      },
      devices,
      tracks,
      clips,
      markers,
      scenes,
      assets: normaliseAssets(source.assets),
      safety: clone(SAFETY),
      globalSettings: clone(source.globalSettings && typeof source.globalSettings === 'object' ? source.globalSettings : {}),
      config: {
        snapEnabled: source.config?.snapEnabled !== false,
        snapMs: Math.max(1, int(source.config?.snapMs, 1000) || 1000),
        gridEnabled: source.config?.gridEnabled !== false,
        zoom: Number(source.config?.zoom) > 0 ? Number(source.config.zoom) : 0.1
      },
      package: {
        format: FORMAT_NAME,
        version: FORMAT_VERSION,
        architecture: 'director-comms-p4-nodes',
        runtimeAuthority: 'esp32-p4-show-engine'
      },
      metadata: {
        ...(clone(source.metadata && typeof source.metadata === 'object' ? source.metadata : {})),
        studioModel: 'timeline-tracks-clips-logical-devices',
        authoringSource: 'showduino-studio-v4'
      }
    };
  }

  function validateShdo(document) {
    const errors = [];
    if (!document || typeof document !== 'object') errors.push('Document is not an object.');
    if (document?.schema !== SCHEMA_NAME) errors.push(`Unsupported SHDO schema ${document?.schema || '(missing)'}.`);
    if (document?.package?.format !== FORMAT_NAME || Number(document?.package?.version) !== FORMAT_VERSION) errors.push('Unsupported Showduino package metadata.');
    if (document?.architecture?.runtimeAuthority !== ARCHITECTURE.runtimeAuthority) errors.push('Runtime authority must remain the ESP32-P4 Show Engine.');
    if (document?.architecture?.nodeAddressing !== 'logical-device-id') errors.push('SHDO v2 requires logical-device-id addressing.');
    if (document?.safety?.runtimeAuthority !== SAFETY.runtimeAuthority || document?.safety?.policy !== SAFETY.policy) errors.push('Safety authority/policy was changed.');
    if (document?.safety?.emergency?.stopTimeline !== true ||
        document?.safety?.emergency?.pixelOverride !== 'all-white' ||
        document?.safety?.emergency?.requiresManualClear !== true ||
        document?.safety?.emergency?.autoResume !== false ||
        document?.safety?.emergency?.productionCannotDisable !== true) errors.push('Emergency policy does not match the firmware-authoritative Showduino contract.');
    ['devices', 'tracks', 'clips', 'markers', 'scenes', 'assets'].forEach((field) => {
      if (!Array.isArray(document?.[field])) errors.push(`${field} must be an array.`);
    });
    return { valid: errors.length === 0, errors };
  }

  function fromShdo(document) {
    if (!document || typeof document !== 'object') throw new Error('This is not a valid Showduino project file.');
    if (document.schema !== SCHEMA_NAME) return ensurePackageMetadata(clone(document));

    const validation = validateShdo(document);
    if (!validation.valid) throw new Error(validation.errors[0]);
    const devices = Array.isArray(document.devices) ? clone(document.devices) : [];
    const deviceById = new Map(devices.map((device) => [String(device.id), device]));

    const tracks = (document.tracks || []).map((track, index) => ({
      id: String(track.id),
      type: String(track.type || 'mixed'),
      name: String(track.name || `${track.type || 'Mixed'} Track`),
      muted: track.enabled === false,
      visible: true,
      locked: track.locked === true,
      color: typeColor(track.type),
      order: Number.isFinite(Number(track.order)) ? Number(track.order) : index,
      ...(track.targetDeviceId ? { targetDeviceId: String(track.targetDeviceId) } : {})
    }));

    const clips = (document.clips || []).map((clip) => {
      const targetDeviceId = clip.action?.targetDeviceId || clip.targetDeviceId || '';
      const device = targetDeviceId ? deviceById.get(String(targetDeviceId)) : null;
      const binding = device?.binding || {};
      const output = binding.outputLabel ?? binding.output ?? '';
      return {
        id: String(clip.id),
        trackId: String(clip.trackId),
        type: String(clip.action?.type || clip.type || 'custom'),
        startMs: int(clip.startMs),
        durationMs: int(clip.durationMs),
        label: String(clip.action?.label || clip.name || clip.type || 'Clip'),
        color: typeColor(clip.action?.type || clip.type),
        enabled: clip.enabled !== false,
        targetDeviceId: targetDeviceId ? String(targetDeviceId) : '',
        routing: {
          nodeId: String(binding.nodeId || ''),
          output: String(output),
          route: String(binding.route || '')
        },
        params: clone(clip.action?.params && typeof clip.action.params === 'object' ? clip.action.params : {}),
        notes: String(clip.action?.notes || '')
      };
    });

    const markers = (document.markers || []).map((marker) => ({
      ...clone(marker),
      id: String(marker.id),
      label: String(marker.name || marker.label || 'Marker'),
      timeMs: int(marker.timeMs)
    }));

    const internal = {
      project: clone(document.project),
      architecture: clone(document.architecture),
      devices,
      tracks,
      clips,
      markers,
      scenes: clone(document.scenes || []),
      assets: clone(document.assets || []),
      globalSettings: clone(document.globalSettings || {}),
      config: clone(document.config || {}),
      metadata: {
        ...(clone(document.metadata || {})),
        importedSchema: SCHEMA_NAME
      },
      package: clone(document.package)
    };
    return ensurePackageMetadata(internal);
  }

  function describe(project) {
    const safe = toShdo(project || {});
    return {
      format: safe.package.format,
      version: safe.package.version,
      architecture: safe.package.architecture,
      runtimeAuthority: safe.package.runtimeAuthority,
      name: safe.project?.name || 'Untitled Show',
      devices: safe.devices.length,
      tracks: safe.tracks.length,
      clips: safe.clips.length,
      pixelClips: safe.clips.filter((clip) => clip.type === 'pixel').length,
      routedClips: safe.clips.filter((clip) => Boolean(clip.targetDeviceId)).length,
      scenes: safe.scenes.length
    };
  }

  window.ShowduinoPackage = Object.freeze({
    FORMAT_NAME,
    FORMAT_VERSION,
    SCHEMA_NAME,
    ARCHITECTURE,
    SAFETY,
    ensurePackageMetadata,
    toShdo,
    fromShdo,
    validateShdo,
    describe
  });
})();
