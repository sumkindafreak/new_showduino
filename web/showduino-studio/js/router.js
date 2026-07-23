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
  const path = getPath();
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

  document.querySelectorAll('.nav-link').forEach(link => {
    link.classList.toggle('active', link.dataset.route === path);
  });

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
