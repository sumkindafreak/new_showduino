import { subscribeRuntime } from '../state/runtimeStore.js';
import { formatDurationMs } from '../utils.js';
import { comingSoonCard, infoBanner, keyValueTable } from './pagePrimitives.js';

export function TimelinePage(container) {
  container.append(infoBanner('Timeline editing is staged for the next phase; runtime timing shown here is authoritative.'));
  const current = document.createElement('section');
  current.className = 'card';
  current.innerHTML = '<h2>Current Timeline State</h2>';
  const host = document.createElement('div');
  current.append(host);
  container.append(current);
  container.append(comingSoonCard('Timeline Editor', 'Cue stack editing and drag timeline controls are intentionally hidden until runtime-safe editing is complete.'));

  const unsubscribe = subscribeRuntime((state) => {
    host.innerHTML = '';
    host.append(keyValueTable([
      ['Runtime', state.runtimeStatus.runtime],
      ['Current Show', state.runtimeStatus.currentShow],
      ['Cue', state.runtimeStatus.cue],
      ['Elapsed', formatDurationMs(state.runtimeStatus.elapsedMs)],
      ['Remaining', formatDurationMs(state.runtimeStatus.remainingMs)],
      ['Transport Health', state.runtimeStatus.transportHealth]
    ]));
  });

  return () => unsubscribe();
}

TimelinePage.title = 'Timeline';
TimelinePage.subtitle = 'Runtime timing and upcoming timeline tools.';
