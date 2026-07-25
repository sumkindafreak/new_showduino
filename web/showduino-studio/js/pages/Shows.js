/**
 * Showduino Studio – Shows Library
 *
 * Operational show browser. Shows packages stored on SD.
 * Falls back gracefully when the shows API is unavailable.
 */

import { el, archBlock, makeCleanupGroup } from '../utils.js';
import { fetchShows, fetchSystem } from '../api.js';
import { subscribe, getState } from '../state/runtime.js';

export async function ShowsPage(container) {
  const cleanup = makeCleanupGroup();

  const status = el('div', { className: 'live-status', text: 'Loading shows…' });
  const grid   = el('div', { className: 'page-grid' });

  // Show browser list
  const listCard = el('div', { className: 'card', style: 'grid-column: 1 / -1;' });
  listCard.append(el('h2', { text: 'Show Library' }));
  const showList = el('div', { className: 'shows-list' });
  listCard.append(showList);

  // Storage info card
  const storageCard = el('div', { className: 'card' });
  storageCard.append(el('h2', { text: 'Storage Architecture' }));

  container.append(status, el('div', { className: 'page-grid', style: 'grid-template-columns: 2fr 1fr;' }, [listCard, storageCard]));

  // Current show from runtime
  cleanup.add(subscribe((state) => {
    const currentId = state.runtime?.currentShow?.id;
    document.querySelectorAll('.show-item').forEach((item) => {
      item.classList.toggle('active', item.dataset.showId === currentId);
    });
  }));

  // Load storage architecture info
  try {
    const sys = await fetchSystem();
    storageCard.append(archBlock('Shows Root', sys.showsPath));
    storageCard.append(archBlock('Show Index', sys.showIndexPath));
    storageCard.append(archBlock('Packages', sys.showPackagesPath));
    storageCard.append(archBlock('Favourites', sys.showFavouritesPath));
    storageCard.append(archBlock('Recent', sys.showRecentPath));
    storageCard.append(archBlock('SD Ready', sys.storageReady ? 'Mounted' : 'Recovery mode'));
  } catch (_) {
    storageCard.append(el('p', { className: 'sub text-muted', text: 'System API unavailable' }));
  }

  // Load show library
  try {
    const data = await fetchShows();
    const shows = Array.isArray(data) ? data : (data?.shows || []);

    if (shows.length === 0) {
      showList.append(el('div', { className: 'sub text-muted', text: 'No shows found on SD. Upload a show package to /showduino/shows/packages/' }));
      status.textContent = 'No shows found';
    } else {
      status.textContent = `${shows.length} show(s)`;
      for (const show of shows) {
        const item = buildShowItem(show);
        showList.append(item);
      }
    }
  } catch (_) {
    // Shows API not yet implemented or unavailable
    showList.append(el('div', { className: 'info-panel', text: 'Show library API not available on this firmware. Shows are loaded from the Director touchscreen or via SD card.' }));
    status.textContent = 'Shows API unavailable';
  }

  return () => cleanup.run();
}

function buildShowItem(show) {
  const currentId = getState().runtime?.currentShow?.id;
  const isActive  = show.id === currentId;

  const info = el('div', { className: 'show-item-info' }, [
    el('div', { className: 'show-item-name', text: show.name || show.id || '—' }),
    el('div', { className: 'show-item-meta', text: [
      show.description || '',
      show.duration ? `${Math.round(show.duration / 1000)}s` : '',
      show.cueCount  ? `${show.cueCount} cues` : '',
    ].filter(Boolean).join(' · ') || '—' }),
  ]);

  const item = el('div', {
    className: `show-item${isActive ? ' active' : ''}`,
    'data-show-id': show.id || '',
  }, [info]);

  return item;
}

ShowsPage.title = 'Shows';
