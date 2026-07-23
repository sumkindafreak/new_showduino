import { el } from '../utils.js';
import { Nav, bindMenuToggle } from './Nav.js';

export function Layout() {
  const app = el('div', { id: 'app' }, [
    el('div', { className: 'layout' }, [
      el('aside', { className: 'sidebar' }, [
        el('div', { className: 'sidebar-brand' }, [
          'SHOWDUINO',
          el('span', { text: 'Studio WebUI' })
        ]),
        Nav()
      ]),
      el('div', { className: 'main' }, [
        el('header', { className: 'header' }, [
          el('button', { id: 'menu-toggle', className: 'menu-toggle', text: '☰' }),
          el('h1', { id: 'page-title', className: 'header-title', text: 'Home' })
        ]),
        el('main', { id: 'page-content', className: 'content' })
      ])
    ]),
    el('div', { id: 'nav-overlay', className: 'overlay' })
  ]);
  document.body.append(app);
  bindMenuToggle();
}
