import { subscribeRuntime } from '../state/runtimeStore.js';
import { formatDurationMs } from '../utils.js';
import { infoBanner, keyValueTable, listCard, metricCard } from './pagePrimitives.js';

export function DashboardPage(container) {
  container.append(infoBanner('Operational overview sourced from one shared runtime model. No page-level polling.'));
  const grid = document.createElement('div');
  grid.className = 'page-grid';
  container.append(grid);

  const runtimeCard = metricCard('Runtime', '—');
  const showCard = metricCard('Current Show', '—');
  const elapsedCard = metricCard('Elapsed Time', '—');
  const remainingCard = metricCard('Remaining Time', '—');
  const emergencyCard = metricCard('Emergency', 'clear');
  const nodeCountCard = metricCard('Node Count', '0');
  const controllerCard = metricCard('Connected Controllers', '0');
  const healthCard = metricCard('System Health', 'unknown');

  grid.append(
    runtimeCard,
    showCard,
    elapsedCard,
    remainingCard,
    emergencyCard,
    nodeCountCard,
    controllerCard,
    healthCard
  );

  const detail = document.createElement('div');
  detail.className = 'page-grid';
  container.append(detail);

  const summaryCard = document.createElement('section');
  summaryCard.className = 'card';
  summaryCard.innerHTML = '<h2>Runtime Summary</h2>';
  const summaryHost = document.createElement('div');
  summaryCard.append(summaryHost);
  detail.append(summaryCard);

  const warningsHost = document.createElement('div');
  const eventsHost = document.createElement('div');
  detail.append(warningsHost, eventsHost);

  const updateValue = (card, value, detailText) => {
    const valueEl = card.querySelector('.value');
    const subEl = card.querySelector('.sub');
    if (valueEl) valueEl.textContent = String(value ?? '—');
    if (subEl) subEl.textContent = detailText || '';
  };

  const unsubscribe = subscribeRuntime((state) => {
    updateValue(runtimeCard, state.runtimeStatus.runtime, `Mode ${state.runtimeStatus.mode}`);
    updateValue(showCard, state.runtimeStatus.currentShow);
    updateValue(elapsedCard, formatDurationMs(state.runtimeStatus.elapsedMs), `Cue ${state.runtimeStatus.cue || '—'}`);
    updateValue(remainingCard, formatDurationMs(state.runtimeStatus.remainingMs));
    updateValue(emergencyCard, state.emergencyState.active ? 'ACTIVE' : 'clear');
    updateValue(nodeCountCard, state.nodeCollection.counts.total, `${state.nodeCollection.counts.online} online`);
    updateValue(controllerCard, state.nodeCollection.connectedControllers, 'Director-class nodes online');
    updateValue(healthCard, state.runtimeStatus.transportHealth, `Link ${state.connectionStatus.lifecycle}`);

    summaryHost.innerHTML = '';
    summaryHost.append(keyValueTable([
      ['Firmware', `${state.systemState.boardName} ${state.systemState.firmwareVersion}`],
      ['Protocol', state.systemState.protocolVersion],
      ['Runtime State', state.runtimeStatus.runtime],
      ['Connection State', state.connectionStatus.lifecycle],
      ['Emergency State', state.emergencyState.active ? 'ACTIVE' : 'clear'],
      ['Network Health', state.networkState.health],
      ['Average RSSI', state.networkState.averageRssi != null ? `${state.networkState.averageRssi} dBm` : 'Not reported'],
      ['Heartbeat Rate', state.networkState.heartbeatRate != null ? `${state.networkState.heartbeatRate} / min` : 'Not reported']
    ]));

    warningsHost.innerHTML = '';
    warningsHost.append(listCard('Warnings', state.runtimeStatus.warnings, 'No active warnings.'));
    eventsHost.innerHTML = '';
    eventsHost.append(listCard(
      'Latest Events',
      state.latestEvents.slice(0, 8).map((entry) => `${entry.level.toUpperCase()} · ${entry.source} · ${entry.message}`),
      'No events captured yet.'
    ));
  });

  return () => unsubscribe();
}

DashboardPage.title = 'Dashboard';
DashboardPage.subtitle = 'Runtime, safety and health overview.';
