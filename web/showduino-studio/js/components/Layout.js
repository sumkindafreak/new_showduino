import { el } from '../utils.js';
import { Nav, bindMenuToggle } from './Nav.js';

export function Layout(navSections) {
  const app = el('div', { id: 'app' }, [
    el('div', { className: 'layout' }, [
      el('aside', { className: 'sidebar' }, [
        el('div', { className: 'sidebar-brand' }, [
          'SHOWDUINO',
          el('span', { text: 'Studio Operating Console' })
        ]),
        Nav(navSections || [])
      ]),
      el('div', { className: 'main' }, [
        el('header', { className: 'header' }, [
          el('button', { id: 'menu-toggle', className: 'menu-toggle', text: '☰' }),
          el('div', { className: 'header-copy' }, [
            el('h1', { id: 'page-title', className: 'header-title', text: 'Dashboard' }),
            el('p', { id: 'page-subtitle', className: 'header-subtitle', text: 'Unified runtime, safety and system awareness.' })
          ])
        ]),
        el('main', { id: 'page-content', className: 'content' }),
        el('footer', { className: 'status-surface' }, [
          el('div', { className: 'status-brand', text: 'SHOWDUINO' }),
          el('div', { className: 'status-item' }, [
            el('span', { className: 'status-label', text: 'Connection' }),
            el('span', { id: 'status-connection', className: 'status-value', text: 'connecting' })
          ]),
          el('div', { className: 'status-item' }, [
            el('span', { className: 'status-label', text: 'Runtime' }),
            el('span', { id: 'status-runtime', className: 'status-value', text: 'unknown' })
          ]),
          el('div', { className: 'status-item' }, [
            el('span', { className: 'status-label', text: 'Emergency' }),
            el('span', { id: 'status-emergency', className: 'status-value', text: 'clear' })
          ]),
          el('div', { className: 'status-item' }, [
            el('span', { className: 'status-label', text: 'Nodes' }),
            el('span', { id: 'status-nodes', className: 'status-value', text: '0' })
          ]),
          el('div', { className: 'status-item status-item-wide' }, [
            el('span', { className: 'status-label', text: 'Show' }),
            el('span', { id: 'status-show', className: 'status-value', text: 'Not reported' })
          ]),
          el('div', { className: 'status-item status-item-wide' }, [
            el('span', { className: 'status-label', text: 'Notice' }),
            el('span', { id: 'status-notice', className: 'status-value', text: 'System initialising' })
          ]),
          el('div', { className: 'status-item status-clock' }, [
            el('span', { className: 'status-label', text: 'Clock' }),
            el('span', { id: 'status-clock', className: 'status-value', text: '--:--:--' })
          ])
        ])
      ])
    ]),
    el('div', { id: 'nav-overlay', className: 'overlay' })
  ]);
  document.body.append(app);
  bindMenuToggle();
}

export function setPageHeader(title, subtitle) {
  const titleEl = document.getElementById('page-title');
  if (titleEl) titleEl.textContent = title || 'Showduino Studio';
  const subtitleEl = document.getElementById('page-subtitle');
  if (subtitleEl) subtitleEl.textContent = subtitle || '';
}
