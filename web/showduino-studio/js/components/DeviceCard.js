import { el, statRow } from '../utils.js';

function presenceClass(device) {
  const p = (device.presence || (device.online ? 'online' : 'offline')).toLowerCase();
  if (p === 'online') return 'online';
  if (p === 'warning') return 'warning';
  return 'offline';
}

function presenceLabel(device) {
  const p = presenceClass(device);
  if (p === 'online') return 'Online';
  if (p === 'warning') return 'Warning';
  return 'Offline';
}

function formatLastSeen(device) {
  if (device.lastSeenLabel) return device.lastSeenLabel;
  if (device.lastSeenMs == null) return '—';
  if (device.freshnessAgeMs != null) {
    const seconds = Math.max(0, Math.floor(device.freshnessAgeMs / 1000));
    return `${seconds}s ago`;
  }
  return `t+${device.lastSeenMs} ms`;
}

export function DeviceCard(device) {
  const status = presenceClass(device);
  return el('article', { className: 'device-card', 'data-id': device.id || '' }, [
    el('div', { className: 'device-card-header' }, [
      el('div', {}, [
        el('div', { className: 'device-name', text: device.friendlyName || device.name || 'Unknown' }),
        el('div', { className: 'device-role', text: device.boardType || device.board || device.role || 'device' })
      ]),
      el('span', { className: `badge ${status}`, text: presenceLabel(device) })
    ]),
    statRow('Board', device.boardType || device.board),
    statRow('RSSI', device.rssi != null ? `${device.rssi} dBm` : '—'),
    statRow('Firmware', device.firmwareVersion || '—'),
    statRow('Protocol', device.protocolVersion || '—'),
    statRow('Connection', device.connectionType || device.connectionStatus || '—'),
    statRow('Freshness', device.freshness || '—'),
    statRow('Last Seen', formatLastSeen(device)),
    statRow('MAC', device.mac || '—')
  ]);
}