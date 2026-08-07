import {
  fetchNetwork,
  fetchLanConfig,
  scanLanNetworks,
  saveLanConfig,
  reconnectLan,
  clearLanConfig,
  networkSetupUrl
} from '../api.js';
import { el, statRow } from '../utils.js';
import { connectLive, subscribeLive } from '../live.js';

function makeButton(text, onClick, primary = false) {
  return el('button', {
    className: primary ? 'btn-primary' : 'btn-cancel',
    text,
    onClick
  });
}

export async function NetworkPage(container) {
  container.append(el('p', {
    className: 'info-panel',
    text: 'SUE is the Showduino network gateway. The Showduino-Studio access point always stays available; an optional local LAN connection can run alongside it.'
  }));

  const grid = el('div', { className: 'page-grid' });

  const healthCard = el('div', { className: 'card network-health-card' });
  healthCard.append(el('h2', { text: 'Showduino Fabric' }));
  const healthBody = el('div', { className: 'network-body' });
  healthCard.append(healthBody);

  const wifiCard = el('div', { className: 'card' });
  wifiCard.append(el('h2', { text: 'SUE Network Gateway' }));
  const wifiBody = el('div', { className: 'wifi-body' });
  wifiCard.append(wifiBody);

  const setupCard = el('div', { className: 'card command-form' });
  setupCard.append(el('h2', { text: 'Local Network Setup' }));
  setupCard.append(el('p', {
    className: 'sub',
    text: 'The fallback Showduino-Studio AP remains active even after a LAN is configured.'
  }));

  const channelWarning = el('p', {
    className: 'info-panel',
    text: 'Important: SUE, Director and ESP-NOW nodes currently share Wi-Fi channel 1. Only a local router on channel 1 can be joined without breaking the Showduino control fabric.'
  });
  setupCard.append(channelWarning);

  const networkSelect = el('select', {
    style: 'width:100%;background:var(--bg);border:1px solid var(--border);color:var(--text);padding:.45rem .5rem;border-radius:4px;'
  }, [el('option', { value: '', text: 'Press Scan Networks first' })]);
  const passwordInput = el('input', { type: 'password', placeholder: 'Wi-Fi password' });
  const setupStatus = el('div', { className: 'live-status', text: 'Reading SUE network status…' });

  setupCard.append(
    el('label', { className: 'cmd-field' }, ['Local Wi-Fi ', networkSelect]),
    el('label', { className: 'cmd-field' }, ['Password ', passwordInput]),
    setupStatus
  );

  const actions = el('div', {
    style: 'display:flex;flex-wrap:wrap;gap:.5rem;margin-top:.75rem;'
  });
  setupCard.append(actions);

  const topologyCard = el('div', { className: 'card' });
  topologyCard.append(el('h2', { text: 'Network Path' }));
  topologyCard.append(el('p', {
    className: 'sub',
    text: 'Browser → LAN or Showduino-Studio AP → SUE (C3) → UART → Stage Engine (P4). Internet access is not required.'
  }));

  grid.append(healthCard, wifiCard, setupCard, topologyCard);
  container.append(grid);

  function renderFabric(net) {
    if (!net) return;
    healthBody.innerHTML = '';
    healthBody.append(statRow('Total Devices', net.deviceCount));
    healthBody.append(statRow('Online', net.onlineCount));
    healthBody.append(statRow('Warning', net.warningCount));
    healthBody.append(statRow('Offline', net.offlineCount));
    healthBody.append(statRow('Average Signal', net.averageRssi != null ? `${net.averageRssi} dBm` : '—'));
    healthBody.append(statRow('Heartbeat Rate', net.heartbeatRate != null ? `${net.heartbeatRate} / min` : '—'));
    healthBody.append(statRow('Network Health', net.networkHealth || net.health || '—'));
  }

  function renderGateway(status) {
    const ap = status?.ap || {};
    const sta = status?.sta || {};
    wifiBody.innerHTML = '';
    wifiBody.append(statRow('Mode', status?.mode === 'hybrid' ? 'AP + Local LAN' : 'Showduino AP only'));
    wifiBody.append(statRow('Fabric Channel', status?.fabricChannel ?? '—'));
    wifiBody.append(statRow('Fallback AP', ap.enabled ? ap.ssid : 'Disabled'));
    wifiBody.append(statRow('AP Address', ap.ip || '192.168.4.1'));
    wifiBody.append(statRow('AP Clients', ap.clients ?? 0));
    wifiBody.append(statRow('LAN Configured', sta.configured ? sta.ssid : 'No'));
    wifiBody.append(statRow('LAN Status', sta.connected ? 'CONNECTED' : (sta.enabled ? 'DISCONNECTED' : 'OFF')));
    wifiBody.append(statRow('LAN Address', sta.connected ? sta.ip : '—'));
    wifiBody.append(statRow('LAN Signal', sta.connected ? `${sta.rssi} dBm` : '—'));
    wifiBody.append(statRow('LAN Channel', sta.connected ? sta.channel : '—'));
    wifiBody.append(statRow('Hostname', `${status?.hostname || 'showduino-studio'}.local`));
    wifiBody.append(statRow('Provisioning', networkSetupUrl()));

    if (status?.lastError) {
      setupStatus.textContent = status.lastError;
      setupStatus.className = 'live-status sev-warn';
    } else if (sta.connected) {
      setupStatus.textContent = `Connected to ${sta.ssid} at ${sta.ip}`;
      setupStatus.className = 'live-status sev-info';
    } else if (sta.enabled) {
      setupStatus.textContent = `Configured for ${sta.ssid}; waiting for connection`;
      setupStatus.className = 'live-status';
    } else {
      setupStatus.textContent = 'AP-only mode. Configure a channel-1 LAN below if required.';
      setupStatus.className = 'live-status';
    }
  }

  async function refreshGateway() {
    try {
      renderGateway(await fetchLanConfig());
    } catch (err) {
      setupStatus.textContent = `Network provisioning service unavailable: ${err.message}`;
      setupStatus.className = 'live-status sev-error';
    }
  }

  actions.append(
    makeButton('Scan Networks', async () => {
      setupStatus.textContent = 'Scanning 2.4 GHz Wi-Fi…';
      try {
        const data = await scanLanNetworks();
        networkSelect.innerHTML = '';
        const networks = data.networks || [];
        const compatible = networks.filter((n) => n.compatible);
        if (!compatible.length) {
          networkSelect.append(el('option', { value: '', text: 'No compatible channel-1 networks found' }));
          setupStatus.textContent = 'No compatible networks found. Set the local router 2.4 GHz channel to 1, then scan again.';
          return;
        }
        for (const network of compatible) {
          networkSelect.append(el('option', {
            value: network.ssid,
            text: `${network.ssid} · ${network.rssi} dBm · ch ${network.channel}${network.secure ? ' · secured' : ''}`
          }));
        }
        setupStatus.textContent = `${compatible.length} compatible network${compatible.length === 1 ? '' : 's'} found.`;
      } catch (err) {
        setupStatus.textContent = err.message;
      }
    }),
    makeButton('Save & Connect', async () => {
      if (!networkSelect.value) {
        setupStatus.textContent = 'Choose a compatible network first.';
        return;
      }
      setupStatus.textContent = `Saving ${networkSelect.value}…`;
      try {
        await saveLanConfig(networkSelect.value, passwordInput.value);
        passwordInput.value = '';
        setupStatus.textContent = 'Saved. SUE is connecting while the fallback AP stays online.';
        window.setTimeout(refreshGateway, 1500);
      } catch (err) {
        setupStatus.textContent = err.message;
      }
    }, true),
    makeButton('Reconnect', async () => {
      try {
        await reconnectLan();
        setupStatus.textContent = 'Reconnect requested.';
        window.setTimeout(refreshGateway, 1500);
      } catch (err) {
        setupStatus.textContent = err.message;
      }
    }),
    makeButton('AP Only', async () => {
      try {
        await clearLanConfig();
        passwordInput.value = '';
        setupStatus.textContent = 'Local LAN credentials cleared. Showduino-Studio remains available.';
        await refreshGateway();
      } catch (err) {
        setupStatus.textContent = err.message;
      }
    })
  );

  connectLive();
  const unsub = subscribeLive((snap) => renderFabric(snap.network));

  try {
    renderFabric(await fetchNetwork());
  } catch (err) {
    healthBody.append(el('p', { text: err.message }));
  }

  await refreshGateway();
  const statusTimer = window.setInterval(refreshGateway, 5000);

  return () => {
    window.clearInterval(statusTimer);
    unsub();
  };
}

NetworkPage.title = 'Network';
