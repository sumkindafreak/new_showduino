/**
 * Showduino Studio – Toast Notification System
 *
 * Renders transient notification toasts driven by runtimeStore.
 * Toasts appear in the top-right corner and auto-dismiss.
 */

import { el } from '../utils.js';
import { subscribeRuntime, dismissRuntimeNotification } from '../state/runtimeStore.js';

let _container = null;
let _rendered  = new Map(); // id → element

export function ToastContainer() {
  _container = el('div', { id: 'toast-container', className: 'toast-container' });
  const unsub = subscribeRuntime((state) => syncToasts(state.notifications));
  _container._cleanup = () => { unsub(); _container = null; _rendered.clear(); };
  return _container;
}

function syncToasts(notifications) {
  if (!_container) return;
  const currentIds = new Set(notifications.map((n) => n.id));

  for (const [id, node] of _rendered) {
    if (!currentIds.has(id)) {
      node.classList.add('toast-exit');
      setTimeout(() => node.remove(), 300);
      _rendered.delete(id);
    }
  }

  for (const notif of notifications) {
    if (!_rendered.has(notif.id)) {
      const toast = buildToast(notif);
      _container.prepend(toast);
      _rendered.set(notif.id, toast);
    }
  }
}

function buildToast(notif) {
  const close = el('button', {
    className: 'toast-close',
    text: '✕',
    onClick: () => dismissRuntimeNotification(notif.id),
  });
  return el('div', {
    className: `toast toast-${notif.level || 'info'}`,
    'data-id': String(notif.id),
  }, [
    el('span', { className: 'toast-msg', text: notif.message }),
    close,
  ]);
}
