/**
 * Showduino Studio – Persistent Status Bar
 *
 * Always visible at the top of the application shell.
 * Driven entirely by the single runtimeStore — no independent polling.
 */

import { el } from '../utils.js';
import { subscribeRuntime } from '../state/runtimeStore.js';
import { navigate } from '../router.js';

const CONN_LABEL = {
  connecting:  { text: 'Connecting…', cls: 'conn-connecting' },
  connected:   { text: 'Connected',   cls: 'conn-connected' },
  degraded:    { text: 'Degraded',    cls: 'conn-degraded' },
  reconnecting:{ text: 'Reconnecting',cls: 'conn-reconnecting' },
  offline:     { text: 'Offline',     cls: 'conn-offline' },
};

export function StatusBar() {
  const connDot  = el('span', { className: 'sb-dot' });
  const connText = el('span', { className: 'sb-conn-text', text: 'Connecting…' });
  const connChip = el('div', { className: 'sb-chip sb-chip--conn conn-connecting' }, [connDot, connText]);

  const runtimeChip   = el('div', { className: 'sb-chip sb-chip--runtime', text: 'IDLE' });
  const emergencyChip = el('div', { className: 'sb-chip sb-chip--emergency sb-emergency--safe', text: 'SAFE' });
  const nodeChip      = el('div', { className: 'sb-chip sb-chip--nodes', text: '0 nodes' });
  const showChip      = el('div', { className: 'sb-chip sb-chip--show', text: 'No show' });
  const clockEl       = el('div', { className: 'sb-clock', text: '--:--:--' });

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

  function paint(state) {
    // Connection chip
    const lifecycle = state.connectionStatus.lifecycle || 'connecting';
    const connInfo  = CONN_LABEL[lifecycle] || CONN_LABEL.offline;
    connChip.className  = `sb-chip sb-chip--conn ${connInfo.cls}`;
    connText.textContent = connInfo.text;

    // Runtime chip
    const rState = (state.runtimeStatus.runtime || 'idle').toLowerCase();
    runtimeChip.textContent = rState.toUpperCase();
    runtimeChip.className   = `sb-chip sb-chip--runtime rt-${rState}`;

    // Emergency chip
    if (state.emergencyState.active) {
      emergencyChip.textContent = '⚠ EMERGENCY';
      emergencyChip.className   = 'sb-chip sb-chip--emergency sb-emergency--active';
    } else {
      emergencyChip.textContent = 'SAFE';
      emergencyChip.className   = 'sb-chip sb-chip--emergency sb-emergency--safe';
    }

    // Node chip
    const { online = 0, total = 0 } = state.nodeCollection.counts;
    nodeChip.textContent = total > 0 ? `${online}/${total} nodes` : '0 nodes';
    nodeChip.className   = `sb-chip sb-chip--nodes${total > 0 && online < total ? ' nodes-warn' : ''}`;

    // Show chip
    const showName = state.runtimeStatus.currentShow || null;
    const isReported = showName && showName !== 'Not reported';
    showChip.textContent = isReported ? showName : 'No show';
    showChip.title       = isReported ? showName : '';

    // Clock — use device clock when available, local fallback handled by watchdog
    clockEl.textContent = state.clock.label || '--:--:--';

    // Notifications badge
    const notifCount = state.notifications.length;
    if (notifCount > 0) {
      notifBadge.textContent = String(notifCount);
      notifBadge.style.display = '';
    } else {
      notifBadge.style.display = 'none';
    }
  }

  const unsub = subscribeRuntime(paint);
  bar._statusBarCleanup = () => unsub();

  return bar;
}
