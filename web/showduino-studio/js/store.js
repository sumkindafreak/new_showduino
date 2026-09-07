import { fetchComms, fetchSystem } from './api.js';
import { isP4DataOffline } from './status.js';

const listeners = new Set();
let comms = null;
let system = null;
let commsError = null;
let commsTimer = null;
let systemTimer = null;
let commsBusy = false;
let systemBusy = false;

export function getStore() {
  return {
    comms,
    system,
    commsError,
    commsOnline: !commsError && !!comms,
    p4Online: !!(comms && comms.p4Online) && !(system && isP4DataOffline(system))
  };
}

export function subscribeStore(fn) {
  listeners.add(fn);
  fn(getStore());
  return () => listeners.delete(fn);
}

function emit() {
  const snap = getStore();
  for (const fn of listeners) fn(snap);
}

async function pollComms() {
  if (commsBusy) return;
  commsBusy = true;
  try {
    comms = await fetchComms();
    commsError = null;
    if (!comms.p4Online) {
      system = { p4Online: false, error: 'p4_offline' };
    }
  } catch (err) {
    commsError = err.message || 'comms_unreachable';
    comms = null;
  } finally {
    commsBusy = false;
  }
  emit();
}

async function pollSystem() {
  if (systemBusy) return;
  if (!comms || !comms.p4Online) return;
  systemBusy = true;
  try {
    system = await fetchSystem();
  } catch (err) {
    system = { p4Online: false, error: 'p4_offline', note: err.message };
  } finally {
    systemBusy = false;
  }
  emit();
}

export function startStore() {
  if (commsTimer) return;
  pollComms().then(pollSystem);
  commsTimer = setInterval(pollComms, 2000);
  systemTimer = setInterval(pollSystem, 2000);
}

export function refreshSystemNow() {
  return pollSystem();
}
