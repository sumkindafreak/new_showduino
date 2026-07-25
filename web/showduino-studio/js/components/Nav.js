import { el } from '../utils.js';
import { navigate } from '../router.js';

export function Nav(sections) {
  const host = el('div', { className: 'nav-sections' });
  for (const section of sections) {
    const block = el('section', { className: 'nav-block' });
    block.append(el('h2', { className: 'nav-section-title', text: section.title }));
    const list = el('ul', { className: 'nav-list' });
    for (const item of section.items) {
      list.append(el('li', {}, [
        el('a', {
          className: 'nav-link',
          href: `#${item.route}`,
          'data-route': item.route,
          onClick: (e) => { e.preventDefault(); navigate(item.route); closeDrawer(); }
        }, [
          el('span', { className: 'nav-icon', text: item.icon || '•' }),
          el('span', { className: 'nav-label', text: item.label })
        ])
      ]));
    }
    block.append(list);
    host.append(block);
  }
  return host;
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
