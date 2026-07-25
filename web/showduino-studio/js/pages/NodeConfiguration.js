import { subscribeRuntime } from '../state/runtimeStore.js';
import { comingSoonCard, infoBanner, keyValueTable } from './pagePrimitives.js';

export function NodeConfigurationPage(container) {
  container.append(infoBanner('Node selection is global; configuration edits are intentionally gated for safety in this phase.'));

  const card = document.createElement('section');
  card.className = 'card';
  card.innerHTML = '<h2>Selected Node</h2>';
  const host = document.createElement('div');
  card.append(host);
  container.append(card);

  container.append(comingSoonCard(
    'Node Configuration Editor',
    'Renaming, priority and capability mutations are hidden until transactional update APIs are in place.'
  ));

  const unsub = subscribeRuntime((state) => {
    const node = state.nodeCollection.nodes.find((entry) => entry.id === state.nodeCollection.selectedNodeId);
    host.innerHTML = '';
    if (!node) {
      host.append(keyValueTable([['Selected Node', 'None']]));
      return;
    }
    host.append(keyValueTable([
      ['ID', node.id],
      ['Name', node.friendlyName || node.name || '—'],
      ['Board', node.boardType || node.board || '—'],
      ['Presence', node.presence || '—'],
      ['Freshness', node.freshness || '—'],
      ['Last Seen', node.lastSeenLabel || 'Not reported'],
      ['Priority', node.priority ?? '—'],
      ['Preferred', node.preferred ? 'yes' : 'no']
    ]));
  });

  return () => unsub();
}

NodeConfigurationPage.title = 'Node Configuration';
NodeConfigurationPage.subtitle = 'Global selection context and guarded configuration.';
