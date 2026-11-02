import { el } from '../utils.js';
import { Nav, bindMenuToggle } from './Nav.js';
import { startStore, subscribeStore } from '../store.js';
import { postPanic, postEmergencyClear } from '../api.js';
import {
  chipClass, emergencyWord, linkWord, productionLabel, systemHealth
} from '../status.js';

let clearBusy = false;
let latestSnap = null;

function chip(id, label) {
  return el('span', { id, className: 'status-chip unknown', text: label });
}

function setChip(id, label, state) {
  const node = document.getElementById(id);
  if (!node) return;
  node.textContent = label;
  node.className = 'status-chip ' + state;
}

function paintShell(snap) {
  latestSnap = snap;
  const commsWord = snap.commsOnline ? 'ONLINE' : 'OFFLINE';
  const p4Word = snap.comms ? (snap.comms.p4Online ? 'ONLINE' : 'OFFLINE') : 'OFFLINE';
  const directorWord = snap.comms
    ? linkWord(snap.comms.directorOnline, snap.comms.directorSeen)
    : 'OFFLINE';
  const health = systemHealth(snap.comms, snap.system);
  const production = productionLabel(snap.system);
  const emergency = emergencyWord(snap.system);

  setChip('chip-comms', `COMMS ${commsWord}`, chipClass(commsWord));
  setChip('chip-p4', `P4 ${p4Word}`, chipClass(p4Word));
  setChip('chip-director', `DIRECTOR ${directorWord}`, chipClass(directorWord));
  setChip('chip-system', `SYSTEM ${health}`, chipClass(health));
  setChip('chip-production', production, production === 'NO PRODUCTION' ? 'unknown' : 'ok');
  if (emergency === 'EMERGENCY') {
    setChip('chip-emergency', 'EMERGENCY', 'bad');
  } else if (emergency === 'OFFLINE') {
    setChip('chip-emergency', 'EMERGENCY OFFLINE', 'unknown');
  } else {
    setChip('chip-emergency', 'EMERGENCY CLEAR', 'ok');
  }

  document.body.classList.toggle('p4-offline', !snap.p4Online);
  document.body.classList.toggle('emergency-active', emergency === 'EMERGENCY');
  document.body.classList.toggle('comms-offline', !snap.commsOnline);

  const authority = document.getElementById('header-authority');
  if (authority) {
    authority.textContent = emergency === 'EMERGENCY'
      ? 'P4 AUTHORITY · EMERGENCY'
      : (snap.p4Online ? 'P4 RUNTIME AUTHORITY' : 'P4 AUTHORITY OFFLINE');
  }

  const panic = document.getElementById('header-panic');
  if (panic) panic.disabled = !snap.p4Online;
  const clear = document.getElementById('header-clear-emergency');
  if (clear) clear.disabled = clearBusy || !snap.p4Online || emergency !== 'EMERGENCY';
  const result = document.getElementById('header-clear-result');
  if (result && emergency === 'CLEAR' && result.dataset.pending === '1') {
    result.textContent = 'P4 confirms emergency clear. Show remains stopped.';
    result.dataset.pending = '0';
  }
}

export function Layout() {
  const boot = document.getElementById('boot');
  if (boot) boot.remove();

  const app = el('div', { id: 'app' }, [
    el('div', { className: 'layout' }, [
      el('aside', { className: 'sidebar' }, [
        el('div', { className: 'sidebar-brand' }, [
          el('div', { className: 'brand-line' }, [
            el('span', { className: 'brand-dot' }),
            el('span', { text: 'SHOWDUINO' })
          ]),
          el('span', { className: 'brand-role', text: 'Communications Engine' }),
          el('span', { className: 'brand-sub', text: '1.0.0-rc.1 · LOCAL SYSTEM CONSOLE' })
        ]),
        Nav()
      ]),
      el('div', { className: 'main' }, [
        el('header', { className: 'header' }, [
          el('button', { id: 'menu-toggle', className: 'menu-toggle', text: '☰', title: 'Open system navigation' }),
          el('h1', { id: 'page-title', className: 'header-title', text: 'Overview' }),
          el('div', { id: 'header-authority', className: 'header-authority', text: 'P4 RUNTIME AUTHORITY' }),
          el('div', { className: 'header-status', id: 'status-shell' }, [
            chip('chip-comms', 'COMMS …'),
            chip('chip-p4', 'P4 …'),
            chip('chip-director', 'DIRECTOR …'),
            chip('chip-system', 'SYSTEM …'),
            chip('chip-production', 'NO PRODUCTION'),
            chip('chip-emergency', 'EMERGENCY …'),
            el('button', {
              id: 'header-panic',
              className: 'btn-panic',
              text: 'EMERGENCY STOP',
              title: 'PANIC latches P4 emergency stop',
              onClick: async () => {
                try {
                  const data = await postPanic();
                  if (data && data.error === 'p4_offline') {
                    alert('P4 OFFLINE — PANIC did not reach the Show Engine.');
                  }
                } catch (err) {
                  alert(err.message);
                }
              }
            }),
            el('button', {
              id: 'header-clear-emergency', className: 'btn-cancel',
              text: 'CLEAR EMERGENCY', disabled: true,
              onClick: async () => {
                if (clearBusy || !latestSnap?.p4Online || emergencyWord(latestSnap.system) !== 'EMERGENCY') return;
                clearBusy = true;
                paintShell(latestSnap);
                const result = document.getElementById('header-clear-result');
                result.textContent = 'Requesting clearance from P4…';
                result.dataset.pending = '0';
                try {
                  const sent = await postEmergencyClear(() => window.confirm(
                    'Release all emergency buttons. Clear emergency and leave the show stopped?'));
                  result.textContent = sent ? 'Awaiting P4 confirmation. Do not press Run.' : 'Clear cancelled.';
                  result.dataset.pending = sent ? '1' : '0';
                } catch (err) {
                  result.textContent = 'Clear not confirmed: ' + err.message;
                } finally {
                  clearBusy = false;
                  if (latestSnap) paintShell(latestSnap);
                }
              }
            }),
            el('span', { id: 'header-clear-result', role: 'status', 'aria-live': 'polite' })
          ])
        ]),
        el('main', { id: 'page-content', className: 'content' })
      ])
    ]),
    el('div', { id: 'nav-overlay', className: 'overlay' })
  ]);

  document.body.append(app);
  bindMenuToggle();
  startStore();
  subscribeStore(paintShell);
}
