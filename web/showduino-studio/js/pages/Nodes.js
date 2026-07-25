import { DeviceCard } from '../components/DeviceCard.js';
import { subscribeRuntime, selectNode } from '../state/runtimeStore.js';
import { infoBanner, keyValueTable } from './pagePrimitives.js';

export function NodesPage(container) {
  container.append(infoBanner('Unified node inventory with shared freshness tracking and runtime-derived health.'));

  const summary = document.createElement('section');
  summary.className = 'card';
  summary.innerHTML = '<h2>Node Fabric</h2>';
  const summaryHost = document.createElement('div');
  summary.append(summaryHost);

  const grid = document.createElement('div');
  grid.className = 'page-grid';
  container.append(summary, grid);

  const unsubscribe = subscribeRuntime((state) => {
    const counts = state.nodeCollection.counts;
    summaryHost.innerHTML = '';
    summaryHost.append(keyValueTable([
      ['Total Nodes', counts.total],
      ['Online', counts.online],
      ['Warning', counts.warning],
      ['Offline', counts.offline],
      ['Connected Controllers', state.nodeCollection.connectedControllers],
      ['Selected Node', state.nodeCollection.selectedNodeId || 'none']
    ]));

    grid.innerHTML = '';
    if (!state.nodeCollection.nodes.length) {
      const empty = document.createElement('section');
      empty.className = 'card';
      empty.textContent = 'No nodes discovered yet.';
      grid.append(empty);
      return;
    }

    for (const node of state.nodeCollection.nodes) {
      const card = DeviceCard(node);
      card.addEventListener('click', () => selectNode(node.id));
      if (state.nodeCollection.selectedNodeId === node.id) card.classList.add('selected');
      grid.append(card);
    }
  });

  return () => unsubscribe();
}

NodesPage.title = 'Nodes';
NodesPage.subtitle = 'Live node status, freshness and roles.';
