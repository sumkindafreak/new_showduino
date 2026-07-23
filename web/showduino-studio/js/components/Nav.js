import { el } from '../utils.js';
import { navigate } from '../router.js';

const NAV_ITEMS = [
  { route: '/', label: 'Home', icon: '◉' },
  { route: '/devices', label: 'Devices', icon: '⬡' },
  { route: '/commands', label: 'Commands', icon: '⇢' },
  { route: '/capabilities', label: 'Capabilities', icon: '◈' },
  { route: '/routing', label: 'Routing', icon: '⤳' },
  { route: '/time', label: 'Time', icon: '◷' },
  { route: '/shows', label: 'Shows', icon: '▶' },
  { route: '/scenes', label: 'Scenes', icon: '◫' },
  { route: '/audio', label: 'Audio', icon: '♪' },
  { route: '/lighting', label: 'Lighting', icon: '☀' },
  { route: '/network', label: 'Network', icon: '⌁' },
  { route: '/logs', label: 'Logs', icon: '≡' },
  { route: '/settings', label: 'Settings', icon: '⚙' }
];

export function Nav() {
  const list = el('ul', { className: 'nav-list' });
  for (const item of NAV_ITEMS) {
    list.append(el('li', {}, [
      el('a', {
        className: 'nav-link',
        href: `#${item.route}`,
        'data-route': item.route,
        onClick: (e) => { e.preventDefault(); navigate(item.route); closeDrawer(); }
      }, [`${item.icon}  ${item.label}`])
    ]));
  }
  return list;
}

function closeDrawer() {
  document.querySelector('.sidebar')?.classList.remove('open');
  document.getElementById('nav-overlay')?.classList.remove('open');
}

export function bindMenuToggle() {
  const btn = document.getElementById('menu-toggle');
  const sidebar = document.querySelector('.sidebar');
  const overlay = document.getElementById('nav-overlay');
  if (!btn || !sidebar) return;
  btn.addEventListener('click', () => {
    sidebar.classList.toggle('open');
    overlay?.classList.toggle('open');
  });
  overlay?.addEventListener('click', closeDrawer);
}
