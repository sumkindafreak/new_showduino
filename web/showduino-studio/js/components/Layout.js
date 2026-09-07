import { el } from '../utils.js';
import { Nav, bindMenuToggle } from './Nav.js';
import { startStore, subscribeStore } from '../store.js';
import { postPanic } from '../api.js';
import {
  chipClass, emergencyWord, linkWord, productionLabel, systemHealth
} from '../status.js';

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
  const panic = document.getElementById('header-panic');
  if (panic) panic.disabled = !snap.p4Online;
}

export function Layout() {
  const boot = document.getElementById('boot');
  if (boot) boot.remove();

  const app = el('div', { id: 'app' }, [
    el('div', { className: 'layout' }, [
      el('aside', { className: 'sidebar' }, [
        el('div', { className: 'sidebar-brand' }, [
          'SHOWDUINO',
          el('span', { text: 'Configuration WebUI' })
        ]),
        Nav()
      ]),
      el('div', { className: 'main' }, [
        el('header', { className: 'header' }, [
          el('button', { id: 'menu-toggle', className: 'menu-toggle', text: '☰' }),
          el('h1', { id: 'page-title', className: 'header-title', text: 'Home' }),
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
              text: 'PANIC',
              title: 'PANIC latches P4 emergency stop',
              onClick: async () => {
                if (!window.confirm('PANIC = P4 EMERGENCY STOP. Latch the Show Engine now?')) return;
                try {
                  const data = await postPanic();
                  if (data && data.error === 'p4_offline') {
                    alert('P4 OFFLINE — PANIC did not reach the Show Engine.');
                  }
                } catch (err) {
                  alert(err.message);
                }
              }
            })
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
