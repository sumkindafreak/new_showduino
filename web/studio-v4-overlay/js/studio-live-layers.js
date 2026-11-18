/* Live inventory creates bound authoring layers only; it never sends commands. */
(function () {
  'use strict';
  let lastSystem = null;
  let busy = false;
  const status = new Map();
  const safeId = value => String(value || '').replace(/[^a-zA-Z0-9_-]/g, '-');

  function descriptors(system) {
    if (!system || system.p4Online === false || system.error || system.stageLink === 'offline') return [];
    const rows = [];
    const add = (nodeId, output, type, name, route, extra = {}) => rows.push({
      id: 'live-' + safeId(nodeId) + '-' + safeId(output), name, type,
      binding: { nodeId, output, outputLabel: output, route, ...extra },
      metadata: { liveLayer: true, layerType: type }
    });
    if (system.capabilities?.pixels === 'ready') add('p4', 'show-pixels', 'pixel', 'P4 · Show pixels', 'p4-show-pixels');
    add('p4-ambience', 'ambience', 'audio', 'P4 - Ambience', 'p4-ambience');
    const audio = system.audioNode;
    if (audio?.online === true) {
      add('audio-node', 'audio', 'audio', 'Audio Node · Playback', 'audio-node');
      if (audio.pixel?.capable && audio.pixel?.ready) add('audio-node-pixels', 'gpio22', 'pixel', 'Audio Node · Pixels', 'audio-node-pixels', {pixelCount: audio.pixel.count, parentNodeId: 'audio-node'});
    }
    const lamp = system.lampNode;
    if (lamp?.online === true) add(lamp.id || 'LAMP-01', 'flame', 'fx', (lamp.name || 'Lamp Node') + ' · Flame', 'lamp-node');
    for (const node of (system.pixelNodes || [])) {
      if (node.online !== true || !node.id) continue;
      add(node.id, 'pixels', 'pixel', (node.name || node.id) + ' · Pixels', 'pixel-node', {pixelCount: node.count || node.pixelCount});
    }
    for (const node of (system.mosfetNodes || [])) {
      if (node.online !== true || !node.id) continue;
      for (const output of (node.outputs || [])) {
        if (!/^out[1-4]$/i.test(output.id || '')) continue;
        add(node.id, output.id.toLowerCase(), 'mosfet', (node.name || node.id) + ' · ' + (output.name || output.id.toUpperCase()), 'mosfet-node');
      }
    }
    return rows;
  }

  function sync(project, system, model) {
    if (!project || !model || !system || system.p4Online === false || system.error || system.stageLink === 'offline') return 0;
    project.tracks ||= [];
    project.devices ||= [];
    const rows = descriptors(system);
    const online = new Set(rows.map(row => row.id));
    for (const device of project.devices) if (device.metadata?.liveLayer) status.set(device.id, online.has(device.id));
    let added = 0;
    for (const row of rows) {
      let device = project.devices.find(item => item.id === row.id ||
        (item.binding?.nodeId === row.binding.nodeId && item.binding?.route === row.binding.route &&
         String(item.binding?.outputLabel || item.binding?.output || '') === row.binding.output));
      if (!device) {
        device = {...row, type: row.type === 'mosfet' ? 'mosfet-node' : row.type === 'audio' ? 'audio-node' : row.type === 'fx' ? 'lamp-node' : row.binding.route === 'p4-show-pixels' ? 'p4-pixel-line' : row.binding.route};
        project.devices.push(device);
      }
      status.set(device.id, true);
      if (project.tracks.some(track => track.targetDeviceId === device.id)) continue;
      const track = model.createTrack(row.type, row.name, project.tracks.length);
      track.targetDeviceId = device.id;
      project.tracks.push(track);
      added++;
    }
    return added;
  }

  function trackForClip(clip, project) {
    const node = clip.routing?.nodeId;
    if (!node) return null; // Never guess between multiple live devices.
    return (project.tracks || []).find(track => {
      const device = project.devices?.find(item => item.id === track.targetDeviceId);
      if (!device || track.locked || track.type !== clip.type || device.binding?.nodeId !== node) return false;
      return clip.type !== 'mosfet' || device.binding.output === String(clip.params?.out || clip.routing?.output || 'out1').toLowerCase();
    });
  }

  function bindClip(clip, project) {
    const track = project.tracks?.find(item => item.id === clip.trackId);
    const device = project.devices?.find(item => item.id === track?.targetDeviceId);
    if (!device || track.type !== clip.type) return clip;
    const binding = device.binding || {};
    // A bound layer is authoritative for newly created clips on that layer.
    clip.targetDeviceId = device.id;
    clip.routing = {nodeId: binding.nodeId, output: binding.outputLabel || binding.output || '', route: binding.route};
    clip.params ||= {};
    if (clip.type === 'mosfet') clip.params.out = binding.output;
    if (clip.type === 'pixel' && Number(binding.pixelCount) > 0) clip.params.length = Number(binding.pixelCount);
    return clip;
  }

  async function refresh() {
    if (busy || !window.state?.project || !window.SHDOModel || window.location?.protocol !== 'http:') return;
    busy = true;
    const controller = new AbortController();
    const timer = window.setTimeout(() => controller.abort(), 3500);
    try {
      const response = await fetch('/api/system', {cache: 'no-store', signal: controller.signal});
      if (!response.ok) throw new Error('P4 inventory unavailable');
      const system = await response.json();
      if (system.p4Online === false || system.error || system.stageLink === 'offline') throw new Error('P4 offline');
      lastSystem = system;
      const added = sync(window.state.project, system, window.SHDOModel);
      if (added) {
        const timeline = window.timelineEditor;
        if (timeline?._trackListEl) timeline._renderTracks();
        window.ShowduinoMobileStudio?.render?.();
        window.ShowduinoStudioV4?.refreshProjectStats?.();
        window.studioLog?.('Connected hardware: added ' + added + ' bound layers.', 'INFO');
        await window.ShowduinoProjects?.saveCurrentProject?.({cloud: false});
      }
      window.dispatchEvent(new CustomEvent('showduino:live-layers', {detail: {added, devices: descriptors(system)}}));
    } catch (_) {
      // Loss of inventory never deletes or reassigns existing authoring data.
      for (const id of status.keys()) status.set(id, false);
    } finally {
      window.clearTimeout(timer);
      busy = false;
    }
  }

  window.ShowduinoLiveLayers = {descriptors, sync, bindClip, trackForClip, refresh, status, getSystem: () => lastSystem};
  if (typeof document !== 'undefined') document.addEventListener('DOMContentLoaded', () => {
    refresh();
    window.setInterval(refresh, 8000);
  });
})();
