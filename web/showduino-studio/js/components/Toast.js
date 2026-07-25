/**
 * Showduino Studio – Toast Notification System
 *
 * Renders transient notification toasts driven by the runtime state.
 * Toasts appear in the top-right corner and auto-dismiss.
 */

import { el } from '../utils.js';
import { subscribe, dismissNotification } from '../state/runtime.js';

let _container = null;
let _rendered = new Map(); // id → element

export function ToastContainer() {
  _container = el('div', { id: 'toast-container', className: 'toast-container' });
  const unsub = subscribe((state) => syncToasts(state.notifications));
  _container._cleanup = () => { unsub(); _container = null; _rendered.clear(); };
  return _container;
}

function syncToasts(notifications) {
  if (!_container) return;

  const currentIds = new Set(notifications.map((n) => n.id));

  // Remove dismissed toasts
  for (const [id, el] of _rendered) {
    if (!currentIds.has(id)) {
      el.classList.add('toast-exit');
      setTimeout(() => el.remove(), 300);
      _rendered.delete(id);
    }
  }

  // Add new toasts (prepend so newest is on top)
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
    onClick: () => dismissNotification(notif.id),
  });

  const toast = el('div', {
    className: `toast toast-${notif.level || 'info'}`,
    'data-id': String(notif.id),
  }, [
    el('span', { className: 'toast-msg', text: notif.message }),
    close,
  ]);

  return toast;
}
