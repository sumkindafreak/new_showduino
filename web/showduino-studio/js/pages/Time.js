import { fetchTime, fetchTimeStatus } from '../api.js';
import { el } from '../utils.js';
import { connectLive, subscribeLive } from '../live.js';

function row(label, value) {
  return el('tr', {}, [el('th', { text: label }), el('td', { text: value == null || value === '' ? '—' : String(value) })]);
}

export async function TimePage(container) {
  container.append(el('p', {
    className: 'info-panel',
    text: 'SUE Time Service — authoritative DS3231 clock. Live via WebSocket (1 Hz).'
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

  function paint(time, st) {
    if (!time) return;
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
    status.textContent = `Live · source ${time.source || '?'} · rtc ${time.rtcStatus || '?'}`;
  }

  connectLive();
  const unsub = subscribeLive((snap) => {
    if (snap.time) paint(snap.time, snap.timeStatus || null);
  });

  try {
    const [time, st] = await Promise.all([fetchTime(), fetchTimeStatus()]);
    paint(time, st);
  } catch (err) {
    status.textContent = err.message;
  }

  return () => unsub();
}
TimePage.title = 'Time';