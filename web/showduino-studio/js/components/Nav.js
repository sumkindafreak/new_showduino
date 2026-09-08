import { el } from '../utils.js';
import { navigate } from '../router.js';

const NAV_GROUPS = [
  {
    label: 'SYSTEM',
    items: [
      { route: '/', label: 'Overview', icon: '◉' },
      { route: '/live', label: 'Runtime', icon: '▶' },
      { route: '/productions', label: 'Productions', icon: '▦' }
    ]
  },
  {
    label: 'HARDWARE',
    items: [
      { route: '/outputs', label: 'Outputs', icon: '▣' },
      { route: '/devices', label: 'Nodes & Bus', icon: '⬡' }
    ]
  },
  {
    label: 'TRANSPORT',
    items: [
      { route: '/network', label: 'Network', icon: '⌁' },
      { route: '/system', label: 'Diagnostics', icon: '≡' }
    ]
  },
  {
    label: 'CONFIGURATION',
    items: [
      { route: '/settings', label: 'Settings', icon: '⚙' }
    ]
  }
];

export function Nav() {
  const list = el('ul', { className: 'nav-list' });

  for (const group of NAV_GROUPS) {
    list.append(el('li', { className: 'nav-group-label', text: group.label }));
    for (const item of group.items) {
      list.append(el('li', {}, [
        el('a', {
          className: 'nav-link',
          href: `#${item.route}`,
          'data-route': item.route,
          onClick: (e) => { e.preventDefault(); navigate(item.route); closeDrawer(); }
        }, [
          el('span', { className: 'nav-icon', text: item.icon }),
          el('span', { text: item.label })
        ])
      ]));
    }
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
