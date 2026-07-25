/**
 * Showduino Studio – Hash Router
 *
 * Hash-based client-side router.
 * Pages mount inside #page-content; the shell is never rebuilt.
 */

const routes = new Map();
let currentCleanup = null;

export function registerRoute(path, handler) {
  routes.set(path, handler);
}

function getPath() {
  const hash = location.hash.slice(1) || '/';
  return hash.startsWith('/') ? hash : '/' + hash;
}

export async function navigate(path) {
  if (!path.startsWith('/')) path = '/' + path;
  location.hash = path;
}

async function render() {
  let path = getPath();

  // Redirect bare root to /dashboard
  if (path === '/') path = '/dashboard';

  const handler = routes.get(path) || routes.get('/dashboard') || routes.get('/');
  const container = document.getElementById('page-content');
  if (!container || !handler) return;

  if (typeof currentCleanup === 'function') {
    try { currentCleanup(); } catch (_) {}
    currentCleanup = null;
  }

  container.innerHTML = '';

  // Scroll content back to top on navigation
  container.scrollTop = 0;

  const result = handler(container);
  if (result instanceof Promise) {
    const resolved = await result;
    if (typeof resolved === 'function') currentCleanup = resolved;
  } else if (typeof result === 'function') {
    currentCleanup = result;
  }

  // Update active nav links
  document.querySelectorAll('.nav-link').forEach((link) => {
    link.classList.toggle('active', link.dataset.route === path);
  });

  // Update page title
  const titleEl = document.getElementById('page-title');
  if (titleEl) titleEl.textContent = handler.title || 'Showduino Studio';
}

export function startRouter() {
  window.addEventListener('hashchange', render);
  render();
}

export function currentRoute() {
  return getPath();
}
