import { fetchE131, fetchE131Channels, fetchNetwork, isP4Offline, postCommand, startGatewayScan, fetchGatewayScan, connectGateway, disconnectGateway, forgetGateway, setGatewayMode } from '../api.js';
import { subscribeStore } from '../store.js';
import { el, p4OfflineBanner, statRow } from '../utils.js';
import { linkWord } from '../status.js';

function field(label, attrs) {
  return el('label', { className: 'cmd-field' }, [
    el('span', { text: label }),
    el('input', attrs)
  ]);
}

export async function NetworkPage(container) {
  container.append(el('p', {
    className: 'info-panel',
    text: 'Comms owns the Showduino SoftAP and optional home/venue Wi-Fi. P4 Ethernet is an isolated show LAN. Internet is optional. Losing internet is not SHOWDUINO CONNECTION LOST. The Director does not configure this page.'
  }));

  const host = el('div', { className: 'page-stack' });
  container.append(host);

  let p4net = null;
  let e131 = null;
  let channels = [];
  let chanFrom = 1;
  let pending = '';
  let lastResult = '';
  let lastSnap = { comms: null, p4Online: false };
  let scan = { state: 'idle', networks: [] };
  let gwBusy = '';
  let gwResult = '';

  async function applyNet() {
    if (!p4net || !p4net.saved) return;
    const enabled = host.querySelector('#net-enabled')?.checked;
    const mode = host.querySelector('#net-mode')?.value || 'DHCP';
    const ip = host.querySelector('#net-ip')?.value?.trim() || '';
    const subnet = host.querySelector('#net-subnet')?.value?.trim() || '';
    const gateway = host.querySelector('#net-gw')?.value?.trim() || '';
    const dns = host.querySelector('#net-dns')?.value?.trim() || '';
    const eEn = host.querySelector('#e131-enabled')?.checked;
    const uni = host.querySelector('#e131-universe')?.value?.trim() || '1';
    pending = 'SAVE';
    lastResult = '';
    paint();
    try {
      const cmds = [
        `NET:ENABLE:${enabled ? 1 : 0}`,
        mode === 'STATIC' ? `NET:STATIC:${ip}:${subnet}:${gateway}${dns ? ':' + dns : ''}` : 'NET:MODE:DHCP',
        `E131:ENABLE:${eEn ? 1 : 0}`,
        `E131:UNIVERSE:${uni}`
      ];
      const replies = [];
      for (const cmd of cmds) {
        const data = await postCommand(cmd);
        if (isP4Offline(data)) {
          lastResult = 'P4 OFFLINE — configuration not applied';
          pending = '';
          paint();
          return;
        }
        replies.push(data.replies || cmd);
      }
      lastResult = 'Saved on P4. Live state updates after the Show Engine confirms it.';
    } catch (err) {
      lastResult = err.message;
    }
    pending = '';
    await pollNet();
  }

  function paint() {
    host.innerHTML = '';
    const c = lastSnap.comms;
    if (!lastSnap.p4Online) host.append(p4OfflineBanner());

    const gw = (c && c.gateway) || {};
    const gateway = el('div', { className: 'card' });
    gateway.append(el('h2', { text: 'Showduino network' }));
    gateway.append(el('p', { className: 'sub', text: 'SoftAP SSID Showduino stays up. Home/venue STA is optional. Passwords are never displayed.' }));
    gateway.append(statRow('Product', `${(c && c.productName) || 'Showduino'} ${(c && c.productVersion) || '1.0.0-rc.1'}`));
    gateway.append(statRow('AP', gw.apOnline ? (gw.apSsid || 'Showduino') : 'OFF'));
    gateway.append(statRow('AP IP', gw.apIp || (c && c.ip) || '192.168.4.1'));
    gateway.append(statRow('Mode', (gw.mode || 'ap_only').toUpperCase()));
    gateway.append(statRow('Home Wi-Fi', (gw.staState || 'idle').toUpperCase()));
    gateway.append(statRow('Saved SSID', gw.staSsid || '—'));
    gateway.append(statRow('STA IP', gw.staIp || '—'));
    gateway.append(statRow('RSSI', gw.rssi != null ? String(gw.rssi) : '—'));
    gateway.append(statRow('Radio / ESP-NOW channel', String(gw.radioChannel ?? c?.radioChannel ?? '—')));
    gateway.append(statRow('Internet', (gw.internet || 'unknown').toUpperCase()));
    gateway.append(statRow('Password stored', gw.passwordConfigured ? 'YES' : 'NO'));
    const ssidIn = el('input', { id: 'gw-ssid', value: gw.staSsid || '', placeholder: 'Home / venue SSID' });
    const passIn = el('input', { id: 'gw-pass', type: 'password', placeholder: 'Password (never shown again)' });
    gateway.append(el('label', { className: 'cmd-field' }, [el('span', { text: 'SSID' }), ssidIn]));
    gateway.append(el('label', { className: 'cmd-field' }, [el('span', { text: 'Password' }), passIn]));
    gateway.append(el('div', { className: 'filter-row' }, [
      el('button', {
        className: 'btn-primary',
        text: gwBusy === 'connect' ? 'Connecting…' : 'Connect',
        disabled: !!gwBusy,
        onClick: async () => {
          gwBusy = 'connect';
          gwResult = '';
          paint();
          try {
            const data = await connectGateway(host.querySelector('#gw-ssid')?.value?.trim() || '', host.querySelector('#gw-pass')?.value || '');
            gwResult = data && data.ok === false ? (data.error || 'connect failed') : 'Connecting — SoftAP remains available.';
          } catch (err) {
            gwResult = err.message;
          }
          gwBusy = '';
          paint();
        }
      }),
      el('button', {
        className: 'btn-cancel',
        text: 'Disconnect',
        disabled: !!gwBusy,
        onClick: async () => {
          gwBusy = 'disconnect';
          paint();
          try {
            await disconnectGateway();
            gwResult = 'AP-only. Saved credentials kept.';
          } catch (err) {
            gwResult = err.message;
          }
          gwBusy = '';
          paint();
        }
      }),
      el('button', {
        className: 'btn-cancel',
        text: 'Forget',
        disabled: !!gwBusy,
        onClick: async () => {
          gwBusy = 'forget';
          paint();
          try {
            await forgetGateway();
            gwResult = 'Credentials forgotten.';
          } catch (err) {
            gwResult = err.message;
          }
          gwBusy = '';
          paint();
        }
      }),
      el('button', {
        className: 'btn-cancel',
        text: scan.state === 'scanning' || gwBusy === 'scan' ? 'Scanning…' : 'Scan',
        disabled: !!gwBusy,
        onClick: async () => {
          gwBusy = 'scan';
          paint();
          try {
            await startGatewayScan();
            for (let i = 0; i < 8; i++) {
              await new Promise((r) => setTimeout(r, 500));
              scan = await fetchGatewayScan();
              if (scan && scan.state === 'ready') break;
            }
            gwResult = scan && scan.networks && scan.networks.length
              ? `Found ${scan.networks.length} networks.`
              : 'Scan finished.';
          } catch (err) {
            gwResult = err.message;
          }
          gwBusy = '';
          paint();
        }
      }),
      el('button', {
        className: 'btn-cancel',
        text: 'AP only',
        disabled: !!gwBusy,
        onClick: async () => {
          try { await setGatewayMode('ap_only'); gwResult = 'Mode AP only.'; }
          catch (err) { gwResult = err.message; }
          paint();
        }
      })
    ]));
    if (scan && Array.isArray(scan.networks) && scan.networks.length) {
      for (const net of scan.networks) {
        gateway.append(el('button', {
          className: 'btn-cancel',
          text: `${net.ssid || '(hidden)'}  ch${net.channel}  ${net.rssi} dBm`,
          onClick: () => {
            const field = host.querySelector('#gw-ssid');
            if (field) field.value = net.ssid || '';
          }
        }));
      }
    }
    if (gwResult) gateway.append(el('p', { className: 'sub', text: gwResult }));
    host.append(gateway);

    const comms = el('div', { className: 'card' });
    comms.append(el('h2', { text: 'Communications Network' }));
    if (c) {
      comms.append(statRow('WebUI host', 'ONLINE'));
      comms.append(statRow('AP SSID', c.ssid || 'Showduino'));
      comms.append(statRow('AP channel', c.radioChannel ?? c.espnowChannel ?? '—'));
      comms.append(statRow('S3 MAC', c.mac || '—'));
      comms.append(statRow('Wi-Fi mode', c.wifiMode || '—'));
      comms.append(statRow('IP', c.ip || '—'));
      comms.append(statRow('Director', linkWord(c.directorOnline, c.directorSeen)));
      comms.append(statRow('P4 UART', c.p4Online ? 'ONLINE' : 'OFFLINE'));
      comms.append(statRow('ESP-NOW', c.directorOnline ? 'ONLINE' : (c.directorSeen ? 'DEGRADED' : 'SEARCHING')));
    } else {
      comms.append(el('p', { className: 'sub', text: 'Comms status unavailable.' }));
    }
    host.append(comms);

    const uart = el('div', { className: 'card' });
    uart.append(el('h2', { text: 'P4 UART view' }));
    if (p4net) {
      uart.append(statRow('UART', p4net.uartReady ? 'READY' : 'FAULT'));
      uart.append(statRow('Comms link', p4net.commsLink ? 'ONLINE' : 'OFFLINE'));
      uart.append(statRow('Director traffic seen', p4net.directorTrafficSeen ? 'READY' : 'SEARCHING'));
      uart.append(statRow('Last RX age', `${p4net.lastRxAgeMs || 0} ms`));
    } else {
      uart.append(el('p', { className: 'sub', text: lastSnap.p4Online ? 'Waiting for P4 network status…' : 'Unavailable while P4 is offline.' }));
    }
    host.append(uart);

    const live = (p4net && p4net.ethernet) || {};
    const saved = (p4net && p4net.saved && p4net.saved.ethernet) || {};
    const savedE = (p4net && p4net.saved && p4net.saved.e131) || {};

    const ethLive = el('div', { className: 'card' });
    ethLive.append(el('h2', { text: 'CURRENT LIVE STATE' }));
    if (p4net) {
      ethLive.append(el('p', { className: 'sub', text: 'Reported by the P4. A saved change is not live until this panel updates.' }));
      ethLive.append(statRow('Ethernet enabled', live.enabled ? 'YES' : 'NO'));
      ethLive.append(statRow('Link', live.link || 'DOWN'));
      ethLive.append(statRow('Mode', live.mode || '—'));
      ethLive.append(statRow('MAC', live.mac || '—'));
      ethLive.append(statRow('IP', live.ip || '—'));
      ethLive.append(statRow('Subnet', live.subnet || '—'));
      ethLive.append(statRow('Gateway', live.gateway || '—'));
      ethLive.append(statRow('Link speed', live.speedMbps ? `${live.speedMbps} Mbps` : '—'));
      ethLive.append(statRow('Link age', live.linkAgeMs ? `${live.linkAgeMs} ms` : '—'));
      ethLive.append(statRow('P4 WebUI', live.url || 'offline'));
    } else {
      ethLive.append(el('p', { className: 'sub', text: 'No live Ethernet state while the P4 is offline.' }));
    }
    host.append(ethLive);

    const ethSave = el('div', { className: 'card' });
    ethSave.append(el('h2', { text: 'SAVED CONFIGURATION' }));
    ethSave.append(el('p', { className: 'sub', text: 'Writes /showduino/config/network.json on the P4. Isolated LAN only — no internet test.' }));
    const enabledBox = el('input', { id: 'net-enabled', type: 'checkbox' });
    if (saved.enabled !== false) enabledBox.checked = true;
    ethSave.append(el('label', { className: 'pref-row' }, [enabledBox, el('span', { text: 'Enable Ethernet' })]));
    const modeSel = el('select', { id: 'net-mode' }, [
      el('option', { value: 'DHCP', text: 'DHCP' }),
      el('option', { value: 'STATIC', text: 'STATIC' })
    ]);
    modeSel.value = saved.mode === 'STATIC' ? 'STATIC' : 'DHCP';
    ethSave.append(el('label', { className: 'cmd-field' }, [el('span', { text: 'Mode' }), modeSel]));
    ethSave.append(field('IP', { id: 'net-ip', value: saved.ip || '', placeholder: '192.168.1.120' }));
    ethSave.append(field('Subnet', { id: 'net-subnet', value: saved.subnet || '', placeholder: '255.255.255.0' }));
    ethSave.append(field('Gateway', { id: 'net-gw', value: saved.gateway || '', placeholder: '192.168.1.1' }));
    ethSave.append(field('DNS (optional)', { id: 'net-dns', value: saved.dns || '' }));
    const eBox = el('input', { id: 'e131-enabled', type: 'checkbox' });
    if (savedE.enabled !== false) eBox.checked = true;
    ethSave.append(el('label', { className: 'pref-row' }, [eBox, el('span', { text: 'Enable E1.31 test receiver' })]));
    ethSave.append(field('Test universe', { id: 'e131-universe', value: String(savedE.universe || 1) }));
    ethSave.append(el('button', {
      className: 'btn-primary',
      text: pending ? 'Saving…' : 'Save / apply',
      disabled: !lastSnap.p4Online || !!pending,
      onClick: applyNet
    }));
    if (lastResult) ethSave.append(el('p', { className: 'sub', text: lastResult }));
    host.append(ethSave);

    const mon = el('div', { className: 'card' });
    mon.append(el('h2', { text: 'E1.31 TEST RECEIVER' }));
    if (e131) {
      mon.append(statRow('Status', e131.state || 'UNAVAILABLE'));
      mon.append(statRow('Universe', e131.universe ?? 1));
      mon.append(statRow('Source', e131.source || '—'));
      mon.append(statRow('Priority', e131.priority ?? '—'));
      mon.append(statRow('Packets', e131.packets ?? 0));
      mon.append(statRow('Rate', `${e131.rateFps ?? 0} fps`));
      mon.append(statRow('Last packet', e131.lastPacketAgeMs != null ? `${e131.lastPacketAgeMs} ms` : '—'));
      mon.append(statRow('Rejected', e131.rejected ?? 0));
      mon.append(statRow('Multicast', e131.multicastJoined ? `${e131.multicast} JOINED` : (e131.multicast || '—')));
      mon.append(el('p', { className: 'sub', text: 'Read-only. Received values do not start shows, change pixels, or trip emergency.' }));
    } else {
      mon.append(el('p', { className: 'sub', text: lastSnap.p4Online ? 'Waiting for E1.31 status…' : 'Unavailable while P4 is offline.' }));
    }
    host.append(mon);

    const grid = el('div', { className: 'card' });
    grid.append(el('h2', { text: 'Channel monitor' }));
    const range = el('div', { className: 'filter-row' }, [
      el('button', { className: 'btn-cancel', text: '1–64', onClick: () => { chanFrom = 1; pollNet(); } }),
      el('button', { className: 'btn-cancel', text: '65–128', onClick: () => { chanFrom = 65; pollNet(); } }),
      el('button', { className: 'btn-cancel', text: '129–192', onClick: () => { chanFrom = 129; pollNet(); } }),
      el('button', { className: 'btn-cancel', text: '193–256', onClick: () => { chanFrom = 193; pollNet(); } }),
      el('button', { className: 'btn-cancel', text: '257–320', onClick: () => { chanFrom = 257; pollNet(); } }),
      el('button', { className: 'btn-cancel', text: '321–384', onClick: () => { chanFrom = 321; pollNet(); } }),
      el('button', { className: 'btn-cancel', text: '385–448', onClick: () => { chanFrom = 385; pollNet(); } }),
      el('button', { className: 'btn-cancel', text: '449–512', onClick: () => { chanFrom = 449; pollNet(); } })
    ]);
    grid.append(range);
    const table = el('div', { className: 'e131-grid' });
    if (channels.length) {
      for (const ch of channels) {
        table.append(el('div', { className: 'e131-cell' }, [
          el('span', { className: 'e131-ch', text: String(ch.ch).padStart(3, '0') }),
          el('span', { className: 'e131-val', text: String(ch.v).padStart(3, '0') })
        ]));
      }
    } else {
      table.append(el('p', { className: 'sub', text: 'No channel snapshot yet.' }));
    }
    grid.append(table);
    host.append(grid);
  }

  async function pollNet() {
    if (!lastSnap.p4Online) {
      p4net = null;
      e131 = null;
      channels = [];
      paint();
      return;
    }
    try {
      const data = await fetchNetwork();
      p4net = isP4Offline(data) ? null : data;
    } catch (_) {
      p4net = null;
    }
    try {
      const data = await fetchE131();
      e131 = isP4Offline(data) ? null : data;
    } catch (_) {
      e131 = null;
    }
    try {
      const data = await fetchE131Channels(chanFrom, 64);
      channels = (!data || isP4Offline(data) || !Array.isArray(data.channels)) ? [] : data.channels;
    } catch (_) {
      channels = [];
    }
    paint();
  }

  const unsub = subscribeStore((snap) => {
    lastSnap = snap;
    paint();
  });
  await pollNet();
  const timer = setInterval(pollNet, 2000);
  return () => {
    unsub();
    clearInterval(timer);
  };
}
NetworkPage.title = 'Network';
