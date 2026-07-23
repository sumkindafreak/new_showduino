import { fetchSystem } from '../api.js';
import { el, archBlock } from '../utils.js';

export async function ScenesPage(container) {
  container.append(el('p', { className: 'info-panel', text: 'Scenes map to show packages and timeline cues. The Stage Engine executes scene commands; the Director builds and uploads timelines from SD.' }));

  const panel = el('div', { className: 'card' });
  panel.append(el('h2', { text: 'Scene Architecture' }));

  try {
    const sys = await fetchSystem();
    panel.append(archBlock('Timeline Source', sys.showPackagesPath + '/<show-id>/timeline.json'));
    panel.append(archBlock('Show Metadata', sys.showPackagesPath + '/<show-id>/show.json'));
    panel.append(archBlock('Show Index', sys.showIndexPath));
    panel.append(archBlock('Execution Path', 'Director → ESP-NOW → C3 Bridge → Stage Engine (P4)'));
    panel.append(el('p', { className: 'sub', text: 'Scene transitions are defined as timeline cues with millisecond timestamps. Stage owns playback authority; Director mirrors runtime via SHOW:RUNTIME packets.' }));
  } catch (err) {
    panel.append(el('p', { text: err.message }));
  }

  container.append(panel);
}
ScenesPage.title = 'Scenes';
