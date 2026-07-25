import { subscribeRuntime } from '../state/runtimeStore.js';
import { formatDurationMs } from '../utils.js';
import { comingSoonCard, infoBanner, keyValueTable } from './pagePrimitives.js';

export function ShowsPage(container) {
  container.append(infoBanner('Show metadata and runtime ownership are unified with the Director terminology.'));

  const card = document.createElement('section');
  card.className = 'card';
  card.innerHTML = '<h2>Current Show Context</h2>';
  const host = document.createElement('div');
  card.append(host);
  container.append(card);

  container.append(comingSoonCard(
    'Show Browser',
    'Show package browse/load controls are hidden until project-index and safe-load API endpoints are finalised.'
  ));

  const unsub = subscribeRuntime((state) => {
    host.innerHTML = '';
    host.append(keyValueTable([
      ['Current Show', state.runtimeStatus.currentShow],
      ['Runtime State', state.runtimeStatus.runtime],
      ['Current Cue', state.runtimeStatus.cue],
      ['Elapsed', formatDurationMs(state.runtimeStatus.elapsedMs)],
      ['Remaining', formatDurationMs(state.runtimeStatus.remainingMs)],
      ['Shows Path', state.assetCollection.showsPath],
      ['Packages Path', state.assetCollection.showPackagesPath]
    ]));
  });

  return () => unsub();
}
ShowsPage.title = 'Shows';
ShowsPage.subtitle = 'Current show context and package ownership.';
