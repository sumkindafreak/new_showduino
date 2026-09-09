import { fetchLogs, postCommand, isP4Offline } from '../api.js';
import { subscribeStore } from '../store.js';
import { MemoryBar } from '../components/MemoryBar.js';
import {
  el, formatBytes, formatTimestamp, formatUptime, p4OfflineBanner,
  severityClass, statRow, tableWrap
} from '../utils.js';
import { emergencyWord, linkWord, loopWord, showDisplayState } from '../status.js';

const FILTERS = ['ALL', 'P4', 'COMMS', 'DIRECTOR', 'PLUGIN', 'EMERGENCY', 'NETWORK', 'PRODUCTION'];

function sourceBucket(source) {
  const s = String(source || '').toUpperCase();
  if (s === 'HTTP' || s === 'WEBUI' || s === 'P4') return 'P4';
  if (s.includes('COMMS')) return 'COMMS';
  if (s.includes('DIRECTOR')) return 'DIRECTOR';
  if (s.includes('PLUGIN')) return 'PLUGIN';
  if (s.includes('EMERGENCY')) return 'EMERGENCY';
  if (s.includes('NET')) return 'NETWORK';
  if (s.includes('PROD') || s.includes('SHOW')) return 'PRODUCTION';
  return 'P4';
}

export async function SystemPage(container) {
  container.append(el('p', {
    className: 'info-panel',
    text: 'Engineering and commissioning. Logs below are the P4 Web API ring buffer only. Comms and Director do not publish a log stream to this UI yet.'
  }));

  const host = el('div', { className: 'page-stack' });
  const logsHost = el('div', { className: 'card' });
  container.append(host, logsHost);

  let logs = [];
  let filter = 'ALL';
  let lastSnap = { comms: null, system: null, p4Online: false };

  function paintMain() {
    host.innerHTML = '';
    if (!lastSnap.p4Online) host.append(p4OfflineBanner());
    const c = lastSnap.comms;
    const s = lastSnap.system;

    const comms = el('div', { className: 'card' });
    comms.append(el('h2', { text: 'Communications S3' }));
    if (c) {
      comms.append(statRow('Firmware', c.firmwareVersion || '—'));
      comms.append(statRow('MAC', c.mac || '—'));
      comms.append(statRow('ESP-NOW', c.directorOnline ? 'ONLINE' : (c.directorSeen ? 'DEGRADED' : 'SEARCHING')));
      comms.append(statRow('Channel', c.radioChannel ?? c.espnowChannel ?? '—'));
      comms.append(statRow('Director last seen', c.directorOnline ? 'ONLINE' : (c.directorSeen ? 'OFFLINE' : 'SEARCHING')));
      comms.append(statRow('ESP-NOW RX / TX', `${c.espnowRx || 0} / ${c.espnowTx || 0}`));
      comms.append(statRow('UART', c.p4Online ? 'ONLINE' : 'OFFLINE'));
      comms.append(statRow('UART RX / TX', `${c.uartRx || 0} / ${c.uartTx || 0}`));
      comms.append(statRow('P4 last seen', c.p4Online ? 'ONLINE' : (c.p4Seen ? 'OFFLINE' : 'SEARCHING')));
      comms.append(statRow('WebUI build', c.webuiBuild || '—'));
      if (c.statusRgb) {
        comms.append(statRow('Status RGB', `${c.statusRgb.state || '—'} · ${c.statusRgb.colour || '—'}`));
        comms.append(statRow('RGB pin', c.statusRgb.pin != null ? String(c.statusRgb.pin) : '—'));
      }
    } else {
      comms.append(el('p', { className: 'sub', text: 'Comms status unavailable.' }));
    }
    host.append(comms);

    const p4 = el('div', { className: 'card' });
    p4.append(el('h2', { text: 'P4 Show Engine' }));
    if (lastSnap.p4Online && s) {
      p4.append(statRow('Firmware', s.firmwareVersion || '—'));
      p4.append(statRow('Uptime', formatUptime(s.uptime)));
      p4.append(statRow('Heap free', formatBytes(s.heapFree)));
      p4.append(statRow('PSRAM free', formatBytes(s.psramFree)));
      p4.append(MemoryBar({ label: 'Heap', used: (s.heapTotal || 0) - (s.heapFree || 0), total: s.heapTotal, variant: 'heap' }));
      p4.append(MemoryBar({ label: 'PSRAM', used: (s.psramTotal || 0) - (s.psramFree || 0), total: s.psramTotal, variant: 'psram' }));
      p4.append(statRow('Storage', (s.storage && s.storage.state) || s.storageState || (s.storageReady ? 'ONLINE' : 'OFFLINE')));
      p4.append(statRow('Production', s.productionName || 'None'));
      p4.append(statRow('Runtime', showDisplayState(s)));
      p4.append(statRow('Plug-in Bus', s.pluginBus && s.pluginBus.ready ? `READY · ${s.pluginBus.total || 0}` : 'SEARCHING'));
      p4.append(statRow('Emergency', emergencyWord(s)));
      p4.append(statRow('Physical loop', loopWord(s)));
      p4.append(statRow('System audio', s.audio && s.audio.codecReady ? (s.audio.state || 'READY') : 'FAULT'));
      p4.append(statRow('Clock', s.timeIso || '—'));
      p4.append(statRow('Clock status', s.timeSynced ? 'READY' : (s.rtcStatus || 'UNSYNCED').toUpperCase()));
      p4.append(statRow('Clock source', (s.timeSource || 'none').toUpperCase()));
      p4.append(statRow('Timezone', s.timezone || 'UTC'));
      const setClock = el('button', {
        className: 'btn-cancel',
        text: 'Set P4 clock from this browser',
        disabled: !lastSnap.p4Online,
        onClick: async () => {
          const epoch = Math.floor(Date.now() / 1000);
          try {
            const data = await postCommand('TIME:SET:' + epoch);
            if (data && data.error) {
              alert(data.error === 'p4_offline'
                ? 'P4 OFFLINE — clock was not set.'
                : (data.note || data.error));
              return;
            }
          } catch (err) {
            alert(err.message);
          }
        }
      });
      p4.append(el('p', { className: 'sub', text: 'Internal P4 RTC. No DS3231. TIME: is published to the Director. Set writes UTC epoch from this browser.' }));
      p4.append(setClock);
    } else {
      p4.append(el('p', { className: 'sub', text: 'P4 diagnostics withheld while the Show Engine is offline.' }));
    }
    host.append(p4);

    const au = (s && s.audio) || {};
    const sysAudio = el('div', { className: 'card' });
    sysAudio.append(el('h2', { text: 'P4 SYSTEM AUDIO' }));
    if (lastSnap.p4Online && s) {
      sysAudio.append(statRow('Codec', au.codec || 'ES8311'));
      sysAudio.append(statRow('State', au.state || (au.codecReady ? 'READY' : 'FAULT')));
      sysAudio.append(statRow('Output', au.output || 'ONBOARD SPEAKER'));
      sysAudio.append(statRow('Current', au.current || 'NONE'));
      sysAudio.append(statRow('I2S', au.i2sReady ? 'READY' : 'FAULT'));
      sysAudio.append(statRow('Amplifier', au.amplifierEnabled ? 'ENABLED' : 'DISABLED'));
      sysAudio.append(statRow('Assets', `${au.assets != null ? au.assets : 0}/${au.assetsTotal || 7}`));
      sysAudio.append(statRow('Last error', au.lastError || '—'));
      sysAudio.append(el('p', {
        className: 'sub',
        text: 'The P4 onboard ES8311 speaker provides Showduino system and safety audio only. Attraction/programme audio is exclusively produced by specialist Audio Nodes.'
      }));
    } else {
      sysAudio.append(el('p', { className: 'sub', text: 'P4 system-audio status withheld while the Show Engine is offline.' }));
    }
    host.append(sysAudio);

    const st = (s && s.storage) || {};
    const storage = el('div', { className: 'card' });
    storage.append(el('h2', { text: 'P4 SD storage' }));
    if (lastSnap.p4Online && s) {
      storage.append(statRow('State', st.state || s.storageState || (s.storageReady ? 'ONLINE' : 'OFFLINE')));
      storage.append(statRow('Mounted', st.mounted === false ? 'NO' : (s.storageReady ? 'YES' : 'NO')));
      storage.append(statRow('Writable', (st.writable ?? s.storageWritable) ? 'YES' : 'NO'));
      storage.append(statRow('Format version', st.formatVersion ?? '—'));
      storage.append(statRow('Card', st.cardType || s.storageCardType || '—'));
      storage.append(statRow('Size', st.totalBytes != null ? formatBytes(st.totalBytes) : `${s.storageTotalMb || 0} MB`));
      storage.append(statRow('Free', st.freeBytes != null ? formatBytes(st.freeBytes) : `${s.storageFreeMb || 0} MB`));
      storage.append(statRow('Used', st.usedBytes != null ? formatBytes(st.usedBytes) : '—'));
      storage.append(statRow('Productions', st.productionCount ?? '—'));
      storage.append(statRow('Config health', st.configHealth || '—'));
      storage.append(statRow('Log usage', st.logBytes != null ? formatBytes(st.logBytes) : '—'));
      storage.append(statRow('Backups', st.backupCount ?? '—'));
      storage.append(statRow('Last error', st.lastError || s.storageMessage || '—'));
      storage.append(el('p', {
        className: 'sub',
        text: 'SD is the persistent backbone, not the safety backbone. Missing or corrupt storage must not stop boot, Comms, or emergency.'
      }));
      const backup = el('button', {
        className: 'btn-cancel',
        text: 'Backup config to SD',
        disabled: !lastSnap.p4Online || st.writable === false,
        onClick: async () => {
          try {
            const data = await postCommand('STORAGE:BACKUP');
            if (data && data.error) {
              alert(data.error === 'p4_offline' ? 'P4 OFFLINE — backup not written.' : (data.note || data.error));
              return;
            }
          } catch (err) {
            alert(err.message);
          }
        }
      });
      storage.append(backup);
    } else {
      storage.append(el('p', { className: 'sub', text: 'Storage status withheld while the Show Engine is offline.' }));
    }
    host.append(storage);

    const director = el('div', { className: 'card' });
    director.append(el('h2', { text: 'Director' }));
    if (c) {
      director.append(statRow('Presence', linkWord(c.directorOnline, c.directorSeen)));
      director.append(statRow('Firmware', 'Not reported by Comms'));
      director.append(statRow('Last seen', c.directorOnline ? 'ONLINE' : (c.directorSeen ? 'OFFLINE' : 'SEARCHING')));
      director.append(el('p', { className: 'sub', text: 'Director is the operator touchscreen. It is not this WebUI.' }));
    } else {
      director.append(el('p', { className: 'sub', text: 'Director presence is reported by Comms ESP-NOW.' }));
    }
    host.append(director);

    const an = (s && s.audioNode) || {};
    const audioNode = el('div', { className: 'card' });
    audioNode.append(el('h2', { text: 'Audio Node' }));
    if (lastSnap.p4Online && (an.seen || an.online)) {
      audioNode.append(statRow('Presence', an.online ? 'ONLINE' : 'OFFLINE'));
      audioNode.append(statRow('State', an.state || '—'));
      audioNode.append(statRow('MAC', an.mac || '—'));
      audioNode.append(statRow('Firmware', an.firmware || '—'));
      audioNode.append(statRow('Codec', an.codec || 'ES8388'));
      audioNode.append(statRow('Storage', an.storage || '—'));
      audioNode.append(statRow('Output', an.output || '—'));
      audioNode.append(statRow('Lifecycle', an.pending ? 'PENDING' : (an.lastLife || '—')));
      audioNode.append(statRow('Asset', an.asset || '—'));
      audioNode.append(statRow('Volume', an.volume != null ? String(an.volume) : '—'));
      audioNode.append(statRow('Last contact', an.lastContactMs != null ? (an.lastContactMs + ' ms') : '—'));
      audioNode.append(statRow('Last error', an.lastError || '—'));
      audioNode.append(statRow('Capabilities', an.capabilities || '—'));
      const si = an.soundInput || {};
      audioNode.append(statRow('Input', si.state || (si.ready ? 'READY' : '—')));
      audioNode.append(statRow('Input level', si.level != null ? String(si.level) : '—'));
      audioNode.append(statRow('Peak', si.peak != null ? String(si.peak) : '—'));
      audioNode.append(statRow('Noise floor', si.noiseFloor != null ? String(si.noiseFloor) : '—'));
      audioNode.append(statRow('Threshold', si.threshold != null ? String(si.threshold) : '—'));
      audioNode.append(statRow('Armed', si.armed ? 'YES' : 'NO'));
      audioNode.append(statRow('Last sound event', si.lastEvent || 'NONE'));
      audioNode.append(el('p', { className: 'sub', text: 'Programme audio only. P4 ES8311 remains system/safety audio. Sound events are logical inputs, not cues.' }));
    } else {
      audioNode.append(el('p', { className: 'sub', text: lastSnap.p4Online
        ? 'Not discovered. Node stays visible on ESP-NOW even if SD or codec is in FAULT.'
        : 'Audio Node diagnostics withheld while P4 is offline.' }));
    }
    host.append(audioNode);

    const ln = (s && s.lampNode) || {};
    const lampNode = el('div', { className: 'card' });
    lampNode.append(el('h2', { text: 'C3 Lamp Node' }));
    if (lastSnap.p4Online && (ln.seen || ln.online)) {
      lampNode.append(statRow('Presence', ln.online ? 'ONLINE' : 'OFFLINE'));
      lampNode.append(statRow('State', ln.state || '—'));
      lampNode.append(statRow('MAC', ln.mac || '—'));
      lampNode.append(statRow('Firmware', ln.firmware || '—'));
      lampNode.append(statRow('FX', ln.fx || '—'));
      lampNode.append(statRow('Brightness', ln.brightness != null ? String(ln.brightness) : '—'));
      lampNode.append(statRow('Last error', ln.lastError || '—'));
      lampNode.append(el('p', { className: 'sub', text: 'Specialist lamp/FX node. Emergency forces lamp pixels bright white. Not SUE/C3 communications.' }));
    } else {
      lampNode.append(el('p', { className: 'sub', text: lastSnap.p4Online
        ? 'Not discovered. The Lamp Node announces over ESP-NOW through the Communications S3.'
        : 'Lamp Node diagnostics withheld while P4 is offline.' }));
    }
    host.append(lampNode);

    const caps = el('div', { className: 'card' });
    caps.append(el('h2', { text: 'Engine capabilities' }));
    const chips = el('div', { className: 'cap-chips' });
    const cset = (s && s.capabilities) || {};
    const rows = [
      ['show-runtime', lastSnap.p4Online ? 'READY' : 'OFFLINE'],
      ['productions', cset.sd || (s && s.storageReady ? 'READY' : 'UNAVAILABLE')],
      ['audio', (cset.audio || 'PLANNED').toUpperCase()],
      ['plugin-bus', (cset.pluginBus || (s && s.pluginBus && s.pluginBus.ready)) ? 'READY' : 'PLANNED'],
      ['emergency', lastSnap.p4Online ? 'READY' : 'OFFLINE'],
      ['rtc', lastSnap.p4Online ? ((s && s.timeSynced) ? 'READY' : 'UNSYNCED') : 'OFFLINE'],
      ['show-pixels', lastSnap.p4Online ? ((cset.pixels || 'searching').toString().toUpperCase()) : 'OFFLINE'],
      ['dmx', 'PARKED'],
      ['audio-node', lastSnap.p4Online ? ((an && an.online) ? 'READY' : 'SEARCHING') : 'OFFLINE'],
      ['lamp-node', lastSnap.p4Online ? ((ln && ln.online) ? 'READY' : 'SEARCHING') : 'OFFLINE'],
      ['espnow-nodes', lastSnap.p4Online ? (((an && an.seen) || (ln && ln.seen)) ? 'READY' : 'SEARCHING') : 'OFFLINE']
    ];
    for (const [name, state] of rows) {
      chips.append(el('span', { className: 'cap-chip', text: `${name}: ${state}` }));
    }
    caps.append(chips);
    host.append(caps);
  }

  function paintLogs() {
    logsHost.innerHTML = '';
    logsHost.append(el('h2', { text: 'P4 logs' }));
    logsHost.append(el('p', { className: 'sub', text: 'Source filters apply to the P4 HTTP ring buffer. COMMS and DIRECTOR streams are not hosted here.' }));
    const row = el('div', { className: 'filter-row' });
    for (const name of FILTERS) {
      row.append(el('button', {
        className: filter === name ? 'btn-primary' : 'btn-cancel',
        text: name,
        onClick: () => { filter = name; paintLogs(); }
      }));
    }
    row.append(el('button', {
      className: 'btn-cancel',
      text: 'Refresh',
      disabled: !lastSnap.p4Online,
      onClick: () => pollLogs()
    }));
    logsHost.append(row);

    const table = el('table', { className: 'log-table' });
    table.append(el('thead', {}, [
      el('tr', {}, [
        el('th', { text: 'Time' }), el('th', { text: 'Level' }),
        el('th', { text: 'Source' }), el('th', { text: 'Message' })
      ])
    ]));
    const tbody = el('tbody', {});
    const filtered = logs.filter((entry) => filter === 'ALL' || sourceBucket(entry.source) === filter);
    if (!lastSnap.p4Online) {
      tbody.append(el('tr', {}, [el('td', { colSpan: '4', text: 'P4 OFFLINE — log buffer unavailable.' })]));
    } else if (!filtered.length) {
      tbody.append(el('tr', {}, [el('td', {
        colSpan: '4',
        text: filter === 'ALL'
          ? 'No P4 log entries yet.'
          : `No P4 log entries tagged ${filter}. Only the P4 HTTP/command ring buffer is available.`
      })]));
    } else {
      for (const entry of filtered.slice().reverse()) {
        tbody.append(el('tr', {}, [
          el('td', { text: formatTimestamp(entry.timestampMs) }),
          el('td', { className: severityClass(entry.severity), text: entry.severity }),
          el('td', { text: entry.source || '—' }),
          el('td', { text: entry.message || '—' })
        ]));
      }
    }
    table.append(tbody);
    logsHost.append(tableWrap(table));
  }

  async function pollLogs() {
    if (!lastSnap.p4Online) {
      logs = [];
      paintLogs();
      return;
    }
    try {
      const data = await fetchLogs();
      logs = isP4Offline(data) ? [] : (data.logs || []);
    } catch (_) {
      logs = [];
    }
    paintLogs();
  }

  const unsub = subscribeStore((snap) => {
    lastSnap = snap;
    paintMain();
  });
  await pollLogs();
  const timer = setInterval(pollLogs, 5000);
  return () => {
    unsub();
    clearInterval(timer);
  };
}
SystemPage.title = 'System';
