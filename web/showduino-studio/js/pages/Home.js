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

function topologyNode(role, name, detail, extraClass = '') {
  return el('div', { className: `system-topology-node ${extraClass}`.trim() }, [
    el('span', { className: 'topology-role', text: role }),
    el('strong', { text: name }),
    el('span', { text: detail })
  ]);
}

function sectionLabel(text) {
  return el('div', { className: 'system-section-label', text });
}

export async function HomePage(container) {
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

    host.append(el('section', { className: 'system-console-hero' }, [
      el('div', { className: 'system-console-kicker', text: 'LOCAL SHOWDUINO SYSTEM CONSOLE' }),
      el('h2', { text: 'Communications Engine' }),
      el('p', {
        text: 'This interface is hosted by the ESP32-S3 Communications Controller. It exposes transport health, commissioning and P4-authoritative system state. It does not become the Show Engine.'
      }),
      el('div', {
        className: 'system-console-rule',
        text: 'S3 transports · P4 decides · Nodes actuate · Director operates'
      })
    ]));

    host.append(sectionLabel('CORE STATUS'));
    const gw = (comms && comms.gateway) || {};
    const homeWifi = gw.staState === 'connected' ? 'ONLINE' : (gw.staState === 'connecting' ? 'CONNECTING' : 'OFF');
    const internet = gw.internet === 'online' ? 'ONLINE' : (gw.internet === 'offline' ? 'OFFLINE' : 'UNKNOWN');
    host.append(el('div', { className: 'dash-status' }, [
      tile('COMMS S3', commsWord),
      tile('P4 SHOW ENGINE', p4Word),
      tile('DIRECTOR', directorWord),
      tile('SYSTEM HEALTH', health)
    ]));
    host.append(el('div', { className: 'dash-status' }, [
      tile('SHOWDUINO LINK', directorWord),
      tile('HOME NETWORK', homeWifi),
      tile('INTERNET', internet),
      tile('RADIO CH', String(gw.radioChannel ?? comms?.radioChannel ?? comms?.espnowChannel ?? '—'))
    ]));
    host.append(el('p', {
      className: 'sub',
      text: 'Internet loss is not SHOWDUINO CONNECTION LOST. Home Wi-Fi is optional and is not required to run a loaded production.'
    }));

    if (!snap.p4Online) host.append(p4OfflineBanner());

    host.append(sectionLabel('SYSTEM PATH'));
    host.append(el('div', { className: 'system-topology' }, [
      topologyNode('OPERATOR', 'Director', directorWord === 'ONLINE' ? 'ESP-NOW operator link online' : `ESP-NOW operator link ${directorWord.toLowerCase()}`),
      topologyNode('TRANSPORT', 'S3 Comms', snap.commsOnline ? 'WebUI host + ESP-NOW ↔ UART transport' : 'Communications controller unavailable'),
      topologyNode('AUTHORITY', 'P4 Show Engine', snap.p4Online ? 'Timeline · state · safety · cue dispatch' : 'Authoritative runtime unavailable', 'authority'),
      topologyNode('ACTION', 'Specialist Nodes', 'Audio · Lantern · Pixel · MOSFET execution')
    ]));

    host.append(sectionLabel('AUTHORITATIVE RUNTIME'));
    const runtimeGrid = el('div', { className: 'page-grid' });

    const production = el('div', { className: 'card' });
    production.append(el('h2', { text: 'Loaded production' }));
    production.append(el('div', { className: 'value', text: productionLabel(sys) }));
    production.append(el('div', { className: 'sub', text: snap.p4Online
      ? (sys && sys.productionId ? `P4 production ID · ${sys.productionId}` : 'No production loaded on the P4')
      : 'Withheld while P4 is offline' }));
    runtimeGrid.append(production);

    const runtime = el('div', { className: showState === 'EMERGENCY' ? 'card danger-card' : 'card' });
    runtime.append(el('h2', { text: 'Runtime state' }));
    runtime.append(el('div', { className: 'value', text: showState }));
    if (snap.p4Online && sys) {
      runtime.append(statRow('P4 state', sys.showState || '—'));
      runtime.append(statRow('Cue', `${sys.currentCue ?? 0} / ${sys.totalCues ?? 0}`));
      runtime.append(statRow('Elapsed', formatPlayhead(sys.showElapsedMs)));
    } else {
      runtime.append(el('p', { className: 'sub', text: 'No authoritative runtime while P4 is offline.' }));
    }
    runtimeGrid.append(runtime);

    const safety = el('div', { className: emergency === 'EMERGENCY' ? 'card danger-card' : 'card' });
    safety.append(el('h2', { text: 'Safety state' }));
    safety.append(el('div', { className: 'value', text: emergency === 'EMERGENCY' ? 'EMERGENCY ACTIVE' : (emergency === 'OFFLINE' ? 'P4 OFFLINE' : 'CLEAR') }));
    if (snap.p4Online && sys) {
      safety.append(statRow('Physical loop', loopWord(sys)));
      safety.append(statRow('Clear request', sys.emergencyPendingClear ? 'PENDING' : 'None'));
      safety.append(statRow('Latch source', sys.emergencySource || '—'));
    }
    runtimeGrid.append(safety);

    host.append(runtimeGrid);

    host.append(sectionLabel('HARDWARE + TRANSPORT'));
    const hardwareGrid = el('div', { className: 'page-grid' });

    const commsCard = el('div', { className: 'card' });
    commsCard.append(el('h2', { text: 'Communications S3' }));
    if (comms) {
      commsCard.append(statRow('Role', 'TRANSPORT'));
      commsCard.append(statRow('WebUI host', 'LOCAL PROGMEM'));
      commsCard.append(statRow('SSID', comms.ssid || 'Showduino'));
      commsCard.append(statRow('ESP-NOW channel', comms.radioChannel ?? comms.espnowChannel ?? '—'));
      commsCard.append(statRow('Home Wi-Fi', (comms.gateway && comms.gateway.staState) ? comms.gateway.staState.toUpperCase() : 'IDLE'));
      commsCard.append(statRow('Internet', (comms.gateway && comms.gateway.internet) ? comms.gateway.internet.toUpperCase() : 'UNKNOWN'));
      commsCard.append(statRow('P4 UART', comms.p4Online ? 'ONLINE' : 'OFFLINE'));
    } else {
      commsCard.append(el('p', { className: 'sub', text: 'Communications status unavailable.' }));
    }
    hardwareGrid.append(commsCard);

    const bus = el('div', { className: 'card' });
    bus.append(el('h2', { text: 'P4 Plug-in Bus' }));
    if (snap.p4Online && sys && sys.pluginBus) {
      const pb = sys.pluginBus;
      bus.append(statRow('Devices', pb.total ?? 0));
      bus.append(statRow('Configured', pb.configuredPlugins ?? 0));
      bus.append(statRow('Unconfigured', pb.unconfiguredPlugins ?? 0));
    } else {
      bus.append(el('p', { className: 'sub', text: 'Plug-in Bus summary unavailable.' }));
    }
    hardwareGrid.append(bus);

    const nodes = el('div', { className: 'card' });
    nodes.append(el('h2', { text: 'Specialist Nodes' }));
    const an = (sys && sys.audioNode) || {};
    const ln = (sys && sys.lampNode) || {};
    if (snap.p4Online && (an.seen || an.online)) {
      nodes.append(statRow('Audio Node', an.online ? (an.state || 'ONLINE') : 'OFFLINE'));
    } else {
      nodes.append(statRow('Audio Node', 'NOT DETECTED'));
    }
    if (snap.p4Online && (ln.seen || ln.online)) {
      nodes.append(statRow('Lamp Node', ln.online ? (ln.state || 'ONLINE') : 'OFFLINE'));
    } else {
      nodes.append(statRow('Lamp Node', 'NOT DETECTED'));
    }
    nodes.append(el('p', {
      className: 'sub',
      text: 'Current rollout: Audio Node → C3 Lamp Node → C3 Pixel Node → MOSFET Node. Relay is retired. DMX is parked.'
    }));
    hardwareGrid.append(nodes);

    const network = el('div', { className: 'card' });
    network.append(el('h2', { text: 'P4 Network' }));
    if (snap.p4Online && sys) {
      const eth = sys.ethernet || {};
      network.append(statRow('Ethernet', eth.ip ? `${eth.link || 'UP'} ${eth.ip}` : (eth.link || 'OFFLINE')));
      network.append(statRow('Configuration owner', 'P4'));
    } else {
      network.append(el('p', { className: 'sub', text: 'P4 network summary unavailable.' }));
    }
    hardwareGrid.append(network);

    const storage = el('div', { className: 'card' });
    storage.append(el('h2', { text: 'P4 Storage' }));
    if (snap.p4Online && sys) {
      const st = sys.storage || {};
      storage.append(statRow('State', st.state || sys.storageState || (sys.storageReady ? 'ONLINE' : 'OFFLINE')));
      storage.append(statRow('Card', st.cardType || sys.storageCardType || '—'));
      storage.append(statRow('Role', 'PERSISTENT DATA'));
    } else {
      storage.append(el('p', { className: 'sub', text: 'P4 storage summary unavailable.' }));
    }
    hardwareGrid.append(storage);

    host.append(hardwareGrid);
  }

  return subscribeStore(paint);
}
HomePage.title = 'Overview';
