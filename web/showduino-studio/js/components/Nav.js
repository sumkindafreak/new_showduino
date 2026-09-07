import { el } from '../utils.js';
import { navigate } from '../router.js';

const NAV_ITEMS = [
  { route: '/', label: 'Home', icon: '◉' },
  { route: '/productions', label: 'Productions', icon: '▦' },
  { route: '/live', label: 'Live', icon: '▶' },
  { route: '/outputs', label: 'Outputs', icon: '▣' },
  { route: '/devices', label: 'Devices', icon: '⬡' },
  { route: '/network', label: 'Network', icon: '⌁' },
  { route: '/system', label: 'System', icon: '≡' },
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
