const routes = new Map();
let currentCleanup = null;

const CANONICAL = {
  '/shows': '/productions',
  '/commands': '/live',
  '/scenes': '/live',
  '/audio': '/outputs',
  '/lighting': '/outputs',
  '/logs': '/system',
  '/capabilities': '/system',
  '/routing': '/system',
  '/time': '/system'
};

export function registerRoute(path, handler) {
  routes.set(path, handler);
}

function getPath() {
  const hash = location.hash.slice(1) || '/';
  return hash.startsWith('/') ? hash : '/' + hash;
}

export function canonicalRoute(path) {
  return CANONICAL[path] || path;
}

export async function navigate(path) {
  if (!path.startsWith('/')) path = '/' + path;
  location.hash = canonicalRoute(path);
}

async function render() {
  const path = getPath();
  const canon = canonicalRoute(path);
  if (canon !== path) {
    location.hash = canon;
    return;
  }
  const handler = routes.get(path) || routes.get('/');
  const container = document.getElementById('page-content');
  if (!container || !handler) return;

  if (typeof currentCleanup === 'function') {
    currentCleanup();
    currentCleanup = null;
  }

  container.innerHTML = '';
  const result = await handler(container);
  if (typeof result === 'function') currentCleanup = result;

  const active = canonicalRoute(path);
  document.querySelectorAll('.nav-link').forEach((link) => {
    link.classList.toggle('active', link.dataset.route === active);
  });

  const titleEl = document.getElementById('page-title');
  if (titleEl) titleEl.textContent = handler.title || 'Showduino';
}

export function startRouter() {
  window.addEventListener('hashchange', render);
  render();
}

export function currentRoute() {
  return getPath();
}
