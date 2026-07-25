/**
 * Showduino Studio – Application Shell
 *
 * The permanent shell never reloads during navigation.
 * It owns: status bar, navigation, page container.
 * All pages mount inside #page-content.
 */

import { el } from '../utils.js';
import { Nav, bindMenuToggle } from './Nav.js';
import { StatusBar } from './StatusBar.js';
import { ToastContainer } from './Toast.js';

export function Layout() {
  const statusBar = StatusBar();
  const toastContainer = ToastContainer();

  const app = el('div', { id: 'app' }, [
    statusBar,
    el('div', { className: 'layout' }, [
      el('aside', { className: 'sidebar' }, [
        el('div', { className: 'sidebar-brand' }, [
          el('span', { className: 'sb-logo-text', text: 'SHOWDUINO' }),
          el('span', { className: 'sb-logo-sub', text: 'Studio WebUI' }),
        ]),
        Nav(),
      ]),
      el('div', { className: 'main' }, [
        el('header', { className: 'topbar' }, [
          el('button', { id: 'menu-toggle', className: 'menu-toggle', text: '☰', title: 'Menu' }),
          el('h1', { id: 'page-title', className: 'topbar-title', text: 'Dashboard' }),
        ]),
        el('main', { id: 'page-content', className: 'content' }),
      ]),
    ]),
    el('div', { id: 'nav-overlay', className: 'overlay' }),
    toastContainer,
  ]);

  document.body.append(app);
  bindMenuToggle();
}
