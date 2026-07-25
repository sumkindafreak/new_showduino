/**
 * Showduino Studio – Shows Library
 *
 * Operational show browser. Show list is fetched once on mount (the store
 * does not cache the full library). Storage info comes from runtimeStore.
 */

import { el, archBlock, makeCleanupGroup } from '../utils.js';
import { fetchShows } from '../api.js';
import { subscribeRuntime, getRuntimeState } from '../state/runtimeStore.js';

export async function ShowsPage(container) {
  const cleanup = makeCleanupGroup();

  const status      = el('div', { className: 'live-status', text: 'Loading shows…' });
  const listCard    = el('div', { className: 'card', style: 'grid-column: 1 / -1;' });
  listCard.append(el('h2', { text: 'Show Library' }));
  const showList    = el('div', { className: 'shows-list' });
  listCard.append(showList);

  const storageCard = el('div', { className: 'card' });
  storageCard.append(el('h2', { text: 'Storage' }));
  const storageBody = el('div', {});
  storageCard.append(storageBody);

  container.append(status, el('div', { className: 'page-grid', style: 'grid-template-columns: 2fr 1fr;' }, [listCard, storageCard]));

  // Track current show name to highlight active show
  cleanup.add(subscribeRuntime((state) => {
    const currentShow = state.runtimeStatus.currentShow || '';
    document.querySelectorAll('.show-item').forEach((item) => {
      item.classList.toggle('active', item.dataset.showName === currentShow);
    });

    // Render storage info from shared state
    const assets = state.assetCollection;
    const sys    = state.systemState;
    storageBody.innerHTML = '';
    if (assets.showsPath) storageBody.append(archBlock('Shows Root', assets.showsPath));
    if (assets.showPackagesPath) storageBody.append(archBlock('Packages', assets.showPackagesPath));
    storageBody.append(archBlock('Storage Status', assets.status || '—'));
    if (sys.storageReady != null) storageBody.append(archBlock('SD Ready', sys.storageReady ? 'Mounted' : 'Recovery mode'));
    if (sys.storageTotalMb != null) storageBody.append(archBlock('Free / Total', `${sys.storageFreeMb || 0} / ${sys.storageTotalMb} MB`));
  }));

  // One-time load of the show library (the store does not poll this endpoint)
  try {
    const data  = await fetchShows();
    const shows = Array.isArray(data) ? data : (data?.shows || []);
    if (shows.length === 0) {
      showList.append(el('div', { className: 'sub text-muted', text: 'No shows found on SD. Upload a show package to /showduino/shows/packages/' }));
      status.textContent = 'No shows found';
    } else {
      status.textContent = `${shows.length} show(s)`;
      const currentShow = getRuntimeState().runtimeStatus.currentShow || '';
      for (const show of shows) {
        showList.append(buildShowItem(show, currentShow));
      }
    }
  } catch (_) {
    showList.append(el('div', { className: 'info-panel', text: 'Show library API not available on this firmware. Shows are loaded from the Director touchscreen or via SD card.' }));
    status.textContent = 'Shows API unavailable';
  }

  return () => cleanup.run();
}

function buildShowItem(show, currentShow) {
  const isActive = show.name === currentShow;
  const info = el('div', { className: 'show-item-info' }, [
    el('div', { className: 'show-item-name', text: show.name || show.id || '—' }),
    el('div', { className: 'show-item-meta', text: [
      show.description || '',
      show.duration ? `${Math.round(show.duration / 1000)}s` : '',
      show.cueCount  ? `${show.cueCount} cues` : '',
    ].filter(Boolean).join(' · ') || '—' }),
  ]);
  return el('div', {
    className: `show-item${isActive ? ' active' : ''}`,
    'data-show-name': show.name || '',
  }, [info]);
}

ShowsPage.title = 'Shows';
