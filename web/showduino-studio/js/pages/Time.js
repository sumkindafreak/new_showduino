import { subscribeRuntime } from '../state/runtimeStore.js';
import { infoBanner, keyValueTable } from './pagePrimitives.js';

export function TimePage(container) {
  container.append(infoBanner('Clock data is now part of the shared runtime model and global status surface.'));
  const card = document.createElement('section');
  card.className = 'card';
  card.innerHTML = '<h2>Runtime Clock</h2>';
  const host = document.createElement('div');
  card.append(host);
  container.append(card);

  const unsub = subscribeRuntime((state) => {
    host.innerHTML = '';
    host.append(keyValueTable([
      ['Clock Source', state.clock.source],
      ['Clock Label', state.clock.label],
      ['ISO', state.clock.iso],
      ['Connection', state.connectionStatus.lifecycle]
    ]));
  });

  return () => unsub();
}

TimePage.title = 'Time';