import { subscribeStore } from '../store.js';
import { el, formatPlayhead, statRow, p4OfflineBanner } from '../utils.js';
import {
  chipClass, emergencyWord, linkWord, loopWord, productionLabel,
  showDisplayState, systemHealth
} from '../status.js';

function tile(label, word) {
  return el('div', { className: `dash-tile ${chipClass(word)}` }, [
    el('div', { className: 'dash-kicker', text: label }),
    el('div', { className: 'dash-value', text: word })
  ]);
}

export async function HomePage(container) {
  container.append(el('p', {
    className: 'info-panel',
    text: 'System summary from the Communications S3 and the authoritative P4 Show Engine. This is not the Director operator desk.'
  }));

  const host = el('div', {});
  container.append(host);

  function paint(snap) {
    host.innerHTML = '';
    const comms = snap.comms;
    const sys = snap.system;
    const commsWord = snap.commsOnline ? 'ONLINE' : 'OFFLINE';
    const p4Word = comms && comms.p4Online ? 'ONLINE' : 'OFFLINE';
    const directorWord = comms ? linkWord(comms.directorOnline, comms.directorSeen) : 'OFFLINE';
    const health = systemHealth(comms, sys);
    const showState = showDisplayState(sys);
    const emergency = emergencyWord(sys);

    host.append(el('div', { className: 'dash-status' }, [
      tile('COMMS', commsWord),
      tile('P4', p4Word),
      tile('DIRECTOR', directorWord),
      tile('SYSTEM', health)
    ]));

    if (!snap.p4Online) host.append(p4OfflineBanner());

    const production = el('div', { className: 'card' });
    production.append(el('h2', { text: 'Current production' }));
    production.append(el('div', { className: 'value', text: productionLabel(sys) }));
    production.append(el('div', { className: 'sub', text: snap.p4Online
      ? (sys && sys.productionId ? `ID ${sys.productionId}` : 'No production loaded on the P4')
      : 'Production name withheld while P4 is offline' }));
    host.append(production);

    const runtime = el('div', { className: showState === 'EMERGENCY' ? 'card danger-card' : 'card' });
    runtime.append(el('h2', { text: 'Runtime' }));
    runtime.append(el('div', { className: 'value', text: showState }));
    if (snap.p4Online && sys) {
      runtime.append(statRow('P4 state', sys.showState || '—'));
      runtime.append(statRow('Cue', `${sys.currentCue ?? 0} / ${sys.totalCues ?? 0}`));
      runtime.append(statRow('Elapsed', formatPlayhead(sys.showElapsedMs)));
    } else {
      runtime.append(el('p', { className: 'sub', text: 'No authoritative runtime while P4 is offline.' }));
    }
    host.append(runtime);

    const safety = el('div', { className: emergency === 'EMERGENCY' ? 'card danger-card' : 'card' });
    safety.append(el('h2', { text: 'Emergency' }));
    safety.append(el('div', { className: 'value', text: emergency === 'EMERGENCY' ? 'EMERGENCY ACTIVE' : 'CLEAR' }));
    if (snap.p4Online && sys) {
      safety.append(statRow('Physical loop', loopWord(sys)));
      safety.append(statRow('Clear request', sys.emergencyPendingClear ? 'PENDING' : 'None'));
      safety.append(statRow('Latch source', sys.emergencySource || '—'));
    }
    host.append(safety);

    const bus = el('div', { className: 'card' });
    bus.append(el('h2', { text: 'Plug-in Bus' }));
    if (snap.p4Online && sys && sys.pluginBus) {
      const pb = sys.pluginBus;
      bus.append(statRow('Devices', pb.total ?? 0));
      bus.append(statRow('Configured', pb.configuredPlugins ?? 0));
      bus.append(statRow('Unconfigured', pb.unconfiguredPlugins ?? 0));
    } else {
      bus.append(el('p', { className: 'sub', text: 'Bus summary unavailable.' }));
    }
    host.append(bus);

    const net = el('div', { className: 'card' });
    net.append(el('h2', { text: 'Show network' }));
    if (snap.p4Online && sys) {
      const eth = sys.ethernet || {};
      const e131 = sys.e131 || {};
      net.append(statRow('Ethernet', eth.ip ? `${eth.link || 'UP'} ${eth.ip}` : (eth.link || 'OFFLINE')));
      net.append(statRow('E1.31 test', e131.state || 'UNAVAILABLE'));
      net.append(el('p', { className: 'sub', text: 'Configuration lives on the Network page. The Director does not set Ethernet or universes.' }));
    } else {
      net.append(el('p', { className: 'sub', text: 'P4 network summary unavailable.' }));
    }
    host.append(net);

    const storage = el('div', { className: 'card' });
    storage.append(el('h2', { text: 'Storage' }));
    if (snap.p4Online && sys) {
      const st = sys.storage || {};
      storage.append(statRow('State', st.state || sys.storageState || (sys.storageReady ? 'ONLINE' : 'OFFLINE')));
      storage.append(statRow('Card', st.cardType || sys.storageCardType || '—'));
      storage.append(el('p', { className: 'sub', text: 'SD is the persistent backbone, not the safety backbone. Details are on System.' }));
    } else {
      storage.append(el('p', { className: 'sub', text: 'P4 storage summary unavailable.' }));
    }
    host.append(storage);

    const nodes = el('div', { className: 'card' });
    nodes.append(el('h2', { text: 'Showduino Nodes' }));
    const an = (sys && sys.audioNode) || {};
    if (snap.p4Online && (an.seen || an.online)) {
      nodes.append(statRow('Audio Node', an.online ? (an.state || 'ONLINE') : 'OFFLINE'));
    } else {
      nodes.append(el('p', { className: 'sub', text: 'No Showduino Nodes detected' }));
      nodes.append(el('p', { className: 'sub', text: 'Audio Node appears here after ESP-NOW announce. Relay, MOSFET, LED, and DMX remain future.' }));
    }
    host.append(nodes);

    const softap = el('div', { className: 'card' });
    softap.append(el('h2', { text: 'Network' }));
    if (comms) {
      softap.append(statRow('WebUI host', 'COMMS S3'));
      softap.append(statRow('SSID', comms.ssid || 'Showduino'));
      softap.append(statRow('Channel', comms.radioChannel ?? comms.espnowChannel ?? '—'));
      softap.append(statRow('P4 Ethernet', 'NOT CONFIGURED'));
    } else {
      softap.append(el('p', { className: 'sub', text: 'Comms status unavailable.' }));
    }
    host.append(softap);
  }

  return subscribeStore(paint);
}
HomePage.title = 'Home';
