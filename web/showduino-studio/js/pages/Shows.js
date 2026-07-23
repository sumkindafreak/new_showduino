import { fetchSystem } from '../api.js';
import { el, archBlock } from '../utils.js';

export async function ShowsPage(container) {
  container.append(el('p', { className: 'info-panel', text: 'Show packages are stored on SD and loaded by the Director. Timeline cues are uploaded to the Stage Engine via ESP-NOW for execution.' }));

  const panel = el('div', { className: 'card' });
  panel.append(el('h2', { text: 'Storage Architecture' }));

  try {
    const sys = await fetchSystem();
    panel.append(archBlock('Shows Root', sys.showsPath));
    panel.append(archBlock('Show Index', sys.showIndexPath));
    panel.append(archBlock('Show Packages', sys.showPackagesPath));
    panel.append(archBlock('Show Trash', sys.showTrashPath));
    panel.append(archBlock('Favourites', sys.showFavouritesPath));
    panel.append(archBlock('Recent Shows', sys.showRecentPath));
    panel.append(archBlock('SD Ready', sys.storageReady ? 'Mounted' : 'Recovery mode'));
    panel.append(el('p', { className: 'sub', text: 'Each show package contains show.json, timeline.json, and optional assets. The Director parses timelines locally and uploads cue commands to Stage.' }));
  } catch (err) {
    panel.append(el('p', { text: err.message }));
  }

  container.append(panel);
}
ShowsPage.title = 'Shows';
