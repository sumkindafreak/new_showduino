import { subscribeRuntime } from '../state/runtimeStore.js';
import { comingSoonCard, infoBanner, keyValueTable } from './pagePrimitives.js';

export function AssetsPage(container) {
  container.append(infoBanner('Asset ownership paths are exposed from shared runtime state.'));

  const card = document.createElement('section');
  card.className = 'card';
  card.innerHTML = '<h2>Asset Paths</h2>';
  const host = document.createElement('div');
  card.append(host);
  container.append(card);

  container.append(comingSoonCard(
    'Asset Manager',
    'Upload/browse operations are hidden until authoritative project APIs are available from the Show Engine.'
  ));

  const unsub = subscribeRuntime((state) => {
    host.innerHTML = '';
    host.append(keyValueTable([
      ['WebUI Root', state.assetCollection.webuiPath],
      ['Shows Root', state.assetCollection.showsPath],
      ['Show Packages', state.assetCollection.showPackagesPath],
      ['Storage Status', state.assetCollection.status]
    ]));
  });

  return () => unsub();
}

AssetsPage.title = 'Assets';
AssetsPage.subtitle = 'Storage ownership and future asset operations.';
