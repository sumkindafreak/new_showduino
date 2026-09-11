import { postCommand, isP4Offline } from '../api.js';
import { subscribeStore } from '../store.js';
import { el, emptyState, p4OfflineBanner, plannedNote, statRow, tableWrap } from '../utils.js';
import {
  pluginConfigWord, pluginDisplayName, pluginRolePhrase, presenceWord
} from '../status.js';

const FUTURE_NODES = [
  'MOSFET Node'
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
    const ln = (lastSnap.system && lastSnap.system.lampNode) || {};
    const pns = (lastSnap.system && Array.isArray(lastSnap.system.pixelNodes))
      ? lastSnap.system.pixelNodes : [];
    const audioSeen = an.seen || an.online;
    const lampSeen = ln.seen || ln.online;
    const pixelSeen = pns.length > 0;
    if (!lastSnap.p4Online) {
      nodes.append(emptyState('P4 OFFLINE', 'Node presence is owned by the Show Engine.'));
    } else if (!audioSeen && !lampSeen && !pixelSeen) {
      nodes.append(emptyState('No Showduino Nodes detected', 'Audio, Lamp and Pixel Nodes appear here after ESP-NOW announce.'));
    } else {
      if (audioSeen) {
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
      }
      if (lampSeen) {
        nodes.append(statRow('Lamp Node', ln.online ? 'ONLINE' : 'OFFLINE'));
        nodes.append(statRow('Lamp state', ln.state || '—'));
        nodes.append(statRow('FX', ln.fx || '—'));
        if (ln.lastError) nodes.append(statRow('Lamp fault', ln.lastError));
        const lampDetails = el('details', {});
        lampDetails.append(el('summary', { text: 'C3 Lamp Node commissioning' }));
        lampDetails.append(statRow('MAC', ln.mac || '—'));
        lampDetails.append(statRow('Firmware', ln.firmware || '—'));
        lampDetails.append(statRow('Brightness', ln.brightness != null ? String(ln.brightness) : '—'));
        lampDetails.append(statRow('Last contact', ln.lastContactMs != null ? (ln.lastContactMs + ' ms') : '—'));
        lampDetails.append(el('p', {
          className: 'sub',
          text: 'Specialist lamp/FX node. Not the superseded C3/SUE Communications Engine.'
        }));
        nodes.append(lampDetails);
      }
      if (pixelSeen) {
        nodes.append(el('h3', { text: 'Pixel Nodes' }));
        for (const pn of pns) {
          nodes.append(statRow(pn.id || 'PIXEL', pn.online ? (pn.state || 'ONLINE') : 'OFFLINE'));
          nodes.append(statRow('Name', pn.name || '—'));
          nodes.append(statRow('Line', pn.initialised
            ? `${pn.pixelCount || 0} px / ${pn.segments || 0} seg`
            : 'NOT INITIALISED'));
          const pixDetails = el('details', {});
          pixDetails.append(el('summary', { text: (pn.id || 'PIXEL') + ' commissioning' }));
          pixDetails.append(statRow('MAC', pn.mac || '—'));
          pixDetails.append(statRow('Firmware', pn.firmware || '—'));
          pixDetails.append(statRow('Initialised', pn.initialised ? 'YES' : 'NO'));
          pixDetails.append(statRow('Last contact', pn.lastContactMs != null ? (pn.lastContactMs + ' ms') : '—'));
          if (pn.lastError) pixDetails.append(statRow('Fault', pn.lastError));
          pixDetails.append(el('p', {
            className: 'sub',
            text: 'Remote equivalent of P4 GPIO23. Same segment/FX model. Commission on the node SoftAP, then address by logical ID.'
          }));
          nodes.append(pixDetails);
        }
      }
    }
    nodes.append(plannedNote('Planned next: ' + FUTURE_NODES.join(', ') + '. Relay Node is retired. DMX is parked.'));
    host.append(nodes);
  }

  return subscribeStore((snap) => {
    lastSnap = snap;
    paint();
  });
}
DevicesPage.title = 'Devices';
