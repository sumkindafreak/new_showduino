import { subscribeRuntime } from '../state/runtimeStore.js';
import { infoBanner, keyValueTable } from './pagePrimitives.js';

function sourceRows(sources) {
  const rows = [];
  const entries = Object.entries(sources || {});
  for (const [name, data] of entries.sort(([a], [b]) => a.localeCompare(b))) {
    rows.push([
      name,
      data.ok
        ? `ok · ${data.latencyMs ?? '—'}ms`
        : `error · ${data.error || 'unknown'}`
    ]);
  }
  return rows;
}

export function DiagnosticsPage(container) {
  container.append(infoBanner('Connection lifecycle and source-health diagnostics powered by the shared runtime store.'));

  const lifeCard = document.createElement('section');
  lifeCard.className = 'card';
  lifeCard.innerHTML = '<h2>Connection Lifecycle</h2>';
  const lifeHost = document.createElement('div');
  lifeCard.append(lifeHost);

  const sourceCard = document.createElement('section');
  sourceCard.className = 'card';
  sourceCard.innerHTML = '<h2>Source Health</h2>';
  const sourceHost = document.createElement('div');
  sourceCard.append(sourceHost);

  const grid = document.createElement('div');
  grid.className = 'page-grid';
  grid.append(lifeCard, sourceCard);
  container.append(grid);

  const unsub = subscribeRuntime((state) => {
    lifeHost.innerHTML = '';
    lifeHost.append(keyValueTable([
      ['Lifecycle', state.connectionStatus.lifecycle],
      ['Retries', state.connectionStatus.retries],
      ['Last Reason', state.connectionStatus.reason || '—'],
      ['Last Connected', state.connectionStatus.lastConnectedAt ? new Date(state.connectionStatus.lastConnectedAt).toLocaleTimeString() : 'Never'],
      ['Last Message', state.connectionStatus.lastMessageAt ? new Date(state.connectionStatus.lastMessageAt).toLocaleTimeString() : 'Never']
    ]));

    sourceHost.innerHTML = '';
    sourceHost.append(keyValueTable(sourceRows(state.diagnostics.sources)));
  });

  return () => unsub();
}

DiagnosticsPage.title = 'Diagnostics';
DiagnosticsPage.subtitle = 'Lifecycle, source health and reconnect telemetry.';
