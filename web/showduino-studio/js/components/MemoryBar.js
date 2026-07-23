import { el } from '../utils.js';

export function MemoryBar({ label, used, total, variant = 'heap' }) {
  const pct = total > 0 ? Math.min(100, Math.round((used / total) * 100)) : 0;
  const free = total - used;
  return el('div', { className: 'memory-bar' }, [
    el('div', { className: 'memory-bar-label' }, [
      el('span', { text: label }),
      el('span', { text: `${pct}% used · ${formatFree(free, total)} free` })
    ]),
    el('div', { className: 'memory-bar-track' }, [
      el('div', { className: `memory-bar-fill ${variant}`, style: `width:${pct}%` })
    ])
  ]);
}

function formatFree(free, total) {
  if (total >= 1048576) return (free / 1048576).toFixed(1) + ' MB';
  if (total >= 1024) return (free / 1024).toFixed(1) + ' KB';
  return free + ' B';
}
