import { postCommand, isP4Offline } from '../api.js';
import { subscribeStore } from '../store.js';
import { el, emptyState, p4OfflineBanner, plannedNote, statRow, tableWrap } from '../utils.js';
import {
  pluginConfigWord, pluginDisplayName, pluginRolePhrase, presenceWord
} from '../status.js';

const FUTURE_NODES = [
  'Relay Node',
  'MOSFET Node',
  'LED Node',
  'DMX Node',
  'Input Node',
  'Sensor Node',
  'Servo Node'
];

export async function DevicesPage(container) {
  container.append(el('p', {
    className: 'info-panel',
    text: 'Plug-in Bus identity is not the same as role. No production Node is required. An empty node list is healthy.'
  }));

  const host = el('div', { className: 'page-stack' });
  container.append(host);

  let pending = null;
  let lastResult = '';
  let lastSnap = { p4Online: false, system: null };

  async function scan() {
    pending = 'PLUGIN:SCAN';
    paint();
    try {
      const data = await postCommand('PLUGIN:SCAN');
      lastResult = isP4Offline(data) ? 'P4 OFFLINE' : (data.replies || JSON.stringify(data));
    } catch (err) {
      lastResult = err.message;
    }
    pending = null;
    paint();
  }

  function paint() {
    host.innerHTML = '';
    if (!lastSnap.p4Online) host.append(p4OfflineBanner());
    const bus = lastSnap.system && lastSnap.system.pluginBus;
    const devices = (bus && bus.devices) || [];

    const plugin = el('div', { className: 'card' });
    plugin.append(el('h2', { text: 'P4 Hardware / Plug-in Bus' }));
    if (bus) {
      plugin.append(statRow('Bus', bus.ready ? 'READY' : 'SEARCHING'));
      plugin.append(statRow('Internal', bus.internal ?? 0));
      plugin.append(statRow('Configured', bus.configuredPlugins ?? 0));
      plugin.append(statRow('Unconfigured', bus.unconfiguredPlugins ?? 0));
    }
    plugin.append(el('button', {
      className: 'btn-cancel',
      text: pending === 'PLUGIN:SCAN' ? 'SCAN…' : 'Rescan Plug-in Bus',
      disabled: !lastSnap.p4Online || !!pending,
      onClick: scan
    }));
    if (lastResult) plugin.append(el('p', { className: 'sub', text: lastResult }));

    if (!lastSnap.p4Online) {
      plugin.append(emptyState('P4 OFFLINE', 'Plug-in Bus inventory is owned by the Show Engine.'));
    } else if (!devices.length) {
      plugin.append(emptyState('No plugins discovered', 'The bus can be empty during commissioning.'));
    } else {
      const table = el('table', { className: 'log-table' });
      table.append(el('thead', {}, [
        el('tr', {}, [
          el('th', { text: 'Device' }),
          el('th', { text: 'Address' }),
          el('th', { text: 'Role' }),
          el('th', { text: 'Classification' }),
          el('th', { text: 'Config' }),
          el('th', { text: 'Presence' })
        ])
      ]));
      const tbody = el('tbody', {});
      for (const d of devices) {
        tbody.append(el('tr', {}, [
          el('td', { text: pluginDisplayName(d) }),
          el('td', { text: d.address || '—' }),
          el('td', { text: pluginRolePhrase(d.role) }),
          el('td', { text: d.classification || d.class || '—' }),
          el('td', { text: pluginConfigWord(d) }),
          el('td', { text: presenceWord(d.online) })
        ]));
      }
      table.append(tbody);
      plugin.append(tableWrap(table));
    }
    host.append(plugin);

    const nodes = el('div', { className: 'card' });
    nodes.append(el('h2', { text: 'Showduino Nodes' }));
    const an = (lastSnap.system && lastSnap.system.audioNode) || {};
    if (!lastSnap.p4Online) {
      nodes.append(emptyState('P4 OFFLINE', 'Node presence is owned by the Show Engine.'));
    } else if (an.seen || an.online) {
      nodes.append(statRow('Audio Node', an.online ? 'ONLINE' : 'OFFLINE'));
      nodes.append(statRow('State', an.state || '—'));
      nodes.append(statRow('Current asset', an.asset || '—'));
      nodes.append(statRow('Volume', an.volume != null ? String(an.volume) : '—'));
      if (an.lastError) nodes.append(statRow('Fault', an.lastError));
      const details = el('details', {});
      details.append(el('summary', { text: 'Audio Node commissioning' }));
      details.append(statRow('MAC', an.mac || '—'));
      details.append(statRow('Firmware', an.firmware || '—'));
      details.append(statRow('Protocol', an.protocol || '1.1'));
      details.append(statRow('Codec', an.codec || 'ES8388'));
      details.append(statRow('Storage', an.storage || '—'));
      details.append(statRow('Output', an.output || '—'));
      details.append(statRow('Last contact', an.lastContactMs != null ? (an.lastContactMs + ' ms') : '—'));
      details.append(statRow('Lifecycle', an.pending ? 'PENDING' : (an.lastLife || '—')));
      details.append(statRow('Capabilities', an.capabilities || '—'));
      const si = an.soundInput || {};
      details.append(statRow('Input', si.state || (si.ready ? 'READY' : '—')));
      details.append(statRow('Input level', si.level != null ? String(si.level) : '—'));
      details.append(statRow('Noise floor', si.noiseFloor != null ? String(si.noiseFloor) : '—'));
      details.append(statRow('Last sound event', si.lastEvent || 'NONE'));
      details.append(el('p', {
        className: 'sub',
        text: 'Programme audio only. P4 ES8311 remains system/safety audio.'
      }));
      nodes.append(details);
    } else {
      nodes.append(emptyState('No Showduino Nodes detected', 'Audio Node is implemented. It appears here after ESP-NOW announce.'));
    }
    nodes.append(plannedNote('Future node types: ' + FUTURE_NODES.join(', ') + '.'));
    host.append(nodes);
  }

  return subscribeStore((snap) => {
    lastSnap = snap;
    paint();
  });
}
DevicesPage.title = 'Devices';
