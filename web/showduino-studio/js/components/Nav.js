/**
 * Showduino Studio – Navigation
 *
 * Split into two logical sections:
 *   OPERATE  – operational pages used during a running show
 *   CONFIGURE – system configuration and diagnostics
 */

import { el } from '../utils.js';
import { navigate } from '../router.js';

const OPERATE = [
  { route: '/dashboard',   label: 'Dashboard',    icon: '◉', title: 'Operational overview' },
  { route: '/live',        label: 'Live',         icon: '▶', title: 'Show playback control' },
  { route: '/timeline',    label: 'Timeline',     icon: '⇢', title: 'Timeline editor (coming soon)', comingSoon: true },
  { route: '/shows',       label: 'Shows',        icon: '◫', title: 'Show library' },
  { route: '/nodes',       label: 'Nodes',        icon: '⬡', title: 'Node and device status' },
  { route: '/logs',        label: 'Logs',         icon: '≡', title: 'System log ring buffer' },
  { route: '/diagnostics', label: 'Diagnostics',  icon: '◈', title: 'Commands, capabilities, routing, time' },
];

const CONFIGURE = [
  { route: '/network',     label: 'Network',      icon: '⌁', title: 'Wi-Fi and mesh topology' },
  { route: '/lighting',    label: 'Lighting',     icon: '☀', title: 'Lighting configuration (coming soon)', comingSoon: true },
  { route: '/audio',       label: 'Audio',        icon: '♪', title: 'Audio system (coming soon)', comingSoon: true },
  { route: '/assets',      label: 'Assets',       icon: '◪', title: 'Asset management (coming soon)', comingSoon: true },
  { route: '/nodeconfig',  label: 'Node Config',  icon: '⚙', title: 'Node configuration (coming soon)', comingSoon: true },
  { route: '/settings',    label: 'Settings',     icon: '⚙', title: 'System settings' },
];

function closeDrawer() {
  document.querySelector('.sidebar')?.classList.remove('open');
  document.getElementById('nav-overlay')?.classList.remove('open');
}

function navItem(item) {
  const label = el('span', { className: 'nav-label', text: item.label });
  const icon  = el('span', { className: 'nav-icon',  text: item.icon });
  const children = item.comingSoon
    ? [icon, label, el('span', { className: 'nav-soon-badge', text: 'Soon' })]
    : [icon, label];

  const anchor = el('a', {
    className: 'nav-link' + (item.comingSoon ? ' nav-link--soon' : ''),
    href: `#${item.route}`,
    'data-route': item.route,
    title: item.title || item.label,
    onClick: (e) => {
      e.preventDefault();
      navigate(item.route);
      closeDrawer();
    },
  }, children);

  return el('li', {}, [anchor]);
}

export function Nav() {
  const opSection = el('div', { className: 'nav-section' }, [
    el('div', { className: 'nav-section-label', text: 'OPERATE' }),
    el('ul', { className: 'nav-list' }, OPERATE.map(navItem)),
  ]);

  const cfgSection = el('div', { className: 'nav-section' }, [
    el('div', { className: 'nav-section-label', text: 'CONFIGURE' }),
    el('ul', { className: 'nav-list' }, CONFIGURE.map(navItem)),
  ]);

  const nav = el('nav', { className: 'sidebar-nav' }, [opSection, cfgSection]);
  return nav;
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

export { OPERATE, CONFIGURE };
