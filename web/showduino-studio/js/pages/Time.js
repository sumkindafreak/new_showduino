import { el } from '../utils.js';
import { initializeRuntimeStore, subscribeRuntime } from '../runtimeStore.js';

function row(label, value) {
  return el('tr', {}, [el('th', { text: label }), el('td', { text: value == null || value === '' ? '—' : String(value) })]);
}

export function TimePage(container) {
  initializeRuntimeStore();
  container.append(el('p', {
    className: 'info-panel',
    text: 'SUE Time Service from runtimeStore (DS3231 + WebSocket updates).'
  }));

  const status = el('div', { className: 'live-status', text: 'Connecting…' });
  const clock = el('div', { className: 'time-clock', text: '--:--:--' });
  const dateLine = el('div', { className: 'time-date', text: '————' });
  const card = el('div', { className: 'card time-card' });
  card.append(clock, dateLine);
  const detail = el('div', { className: 'card' });
  detail.append(el('h2', { text: 'Clock Details' }));
  const table = el('table', { className: 'log-table' });
  const tbody = el('tbody', {});
  table.append(tbody);
  detail.append(table);
  container.append(status, card, detail);

  function paint(time, st, conn) {
    if (!time) {
      status.textContent = conn.connected
        ? 'Live · waiting for time payload'
        : `Disconnected · reconnect in ${conn.reconnectInSec || 0}s`;
      return;
    }
    clock.textContent = time.time || '--:--:--';
    dateLine.textContent = `${time.date || ''} · ${time.dayOfWeek || ''} · ${time.timezone || 'UTC'}`;
    tbody.innerHTML = '';
    const rows = [
      ['ISO', time.iso],
      ['Unix Epoch', time.epoch],
      ['Timezone', time.timezone],
      ['DST', time.dst ? 'active' : (time.dstEnabled ? 'enabled' : 'off')],
      ['RTC Status', time.rtcStatus || st?.health],
      ['RTC Temperature', time.rtcTemperature != null ? `${time.rtcTemperature} °C` : '—'],
      ['Battery', time.battery || st?.battery],
      ['Time Source', time.source],
      ['System Uptime', time.uptime],
      ['Last Synchronisation', st?.lastSynchronisation],
      ['Drift (ms)', st?.driftMs],
      ['Firmware Build', time.firmwareBuild]
    ];
    for (const [k, v] of rows) tbody.append(row(k, v));
    status.textContent = conn.connected
      ? `Live · source ${time.source || '?'} · rtc ${time.rtcStatus || '?'}`
      : `Disconnected · reconnect in ${conn.reconnectInSec || 0}s`;
  }

  const unsub = subscribeRuntime((snap) => paint(snap.time, snap.timeStatus || null, snap.connection));

  return () => unsub();
}
TimePage.title = 'Time';