/**
 * Showduino Studio – Persistent Status Bar
 *
 * Always visible at the top of the application shell.
 * Reflects connection state, runtime state, emergency, node count, current
 * show, system clock and notification count.
 */

import { el } from '../utils.js';
import { subscribe, getState, ConnectionState } from '../state/runtime.js';
import { navigate } from '../router.js';

const CONN_LABEL = {
  [ConnectionState.CONNECTING]:  { text: 'Connecting…', cls: 'conn-connecting' },
  [ConnectionState.CONNECTED]:   { text: 'Connected',   cls: 'conn-connected' },
  [ConnectionState.DEGRADED]:    { text: 'Degraded',    cls: 'conn-degraded' },
  [ConnectionState.RECONNECTING]:{ text: 'Reconnecting',cls: 'conn-reconnecting' },
  [ConnectionState.OFFLINE]:     { text: 'Offline',     cls: 'conn-offline' },
};

function pad(n) { return String(n).padStart(2, '0'); }

function formatClock() {
  const d = new Date();
  return `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`;
}

export function StatusBar() {
  // ── Elements ──────────────────────────────────────────────────────────────
  const connDot  = el('span', { className: 'sb-dot' });
  const connText = el('span', { className: 'sb-conn-text', text: 'Connecting…' });
  const connChip = el('div', { className: 'sb-chip sb-chip--conn conn-connecting' }, [connDot, connText]);

  const runtimeChip = el('div', { className: 'sb-chip sb-chip--runtime', text: 'IDLE' });

  const emergencyChip = el('div', { className: 'sb-chip sb-chip--emergency sb-emergency--safe', text: 'SAFE' });

  const nodeChip = el('div', { className: 'sb-chip sb-chip--nodes', text: '0 nodes' });

  const showChip = el('div', { className: 'sb-chip sb-chip--show', text: 'No show' });

  const clockEl = el('div', { className: 'sb-clock', text: formatClock() });

  const notifBadge = el('span', { className: 'sb-notif-badge', text: '' });
  notifBadge.style.display = 'none';
  const notifBtn = el('button', {
    className: 'sb-notif-btn',
    title: 'Notifications',
    onClick: () => navigate('/logs'),
  }, ['🔔', notifBadge]);

  const bar = el('div', { id: 'status-bar', className: 'status-bar' }, [
    el('div', { className: 'sb-brand' }, [
      el('span', { className: 'sb-brand-name', text: 'SHOWDUINO' }),
      el('span', { className: 'sb-brand-sub', text: 'Studio' }),
    ]),
    el('div', { className: 'sb-left' }, [connChip]),
    el('div', { className: 'sb-center' }, [runtimeChip, emergencyChip]),
    el('div', { className: 'sb-right' }, [nodeChip, showChip, clockEl, notifBtn]),
  ]);

  // ── Clock tick ────────────────────────────────────────────────────────────
  let clockInterval = null;

  function startClock() {
    clockInterval = setInterval(() => {
      const s = getState();
      // If authoritative time from device, use it; else use local clock
      if (s.time && s.time.time) {
        clockEl.textContent = s.time.time;
      } else {
        clockEl.textContent = formatClock();
      }
    }, 1000);
  }

  // ── State subscription ────────────────────────────────────────────────────
  function paint(state) {
    // Connection
    const connInfo = CONN_LABEL[state.connection] || CONN_LABEL[ConnectionState.OFFLINE];
    connChip.className = `sb-chip sb-chip--conn ${connInfo.cls}`;
    connText.textContent = connInfo.text;

    // Runtime
    const rState = state.runtime?.state || 'idle';
    runtimeChip.textContent = rState.toUpperCase();
    runtimeChip.className = `sb-chip sb-chip--runtime rt-${rState}`;

    // Emergency
    if (state.emergency?.active) {
      emergencyChip.textContent = state.emergency.level === 'critical' ? '⚠ EMERGENCY' : '⚠ WARNING';
      emergencyChip.className = 'sb-chip sb-chip--emergency sb-emergency--active';
    } else {
      emergencyChip.textContent = 'SAFE';
      emergencyChip.className = 'sb-chip sb-chip--emergency sb-emergency--safe';
    }

    // Nodes
    const onlineNodes = state.nodes.filter((n) => n.online !== false && n.presence !== 'offline').length;
    const totalNodes = state.nodes.length;
    nodeChip.textContent = totalNodes > 0 ? `${onlineNodes}/${totalNodes} nodes` : '0 nodes';
    nodeChip.className = `sb-chip sb-chip--nodes${totalNodes > 0 && onlineNodes < totalNodes ? ' nodes-warn' : ''}`;

    // Current show
    const showName = state.runtime?.currentShow?.name || null;
    showChip.textContent = showName || 'No show';
    showChip.title = showName || '';

    // Notifications
    const notifCount = state.notifications.length;
    if (notifCount > 0) {
      notifBadge.textContent = String(notifCount);
      notifBadge.style.display = '';
    } else {
      notifBadge.style.display = 'none';
    }
  }

  const unsub = subscribe(paint);
  startClock();

  // Attach cleanup to bar element for shell teardown
  bar._statusBarCleanup = () => {
    if (clockInterval) clearInterval(clockInterval);
    unsub();
  };

  return bar;
}
