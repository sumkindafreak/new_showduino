import { subscribeRuntime } from '../state/runtimeStore.js';
import { formatDurationMs } from '../utils.js';
import { infoBanner, keyValueTable, listCard, metricCard } from './pagePrimitives.js';

export function LivePage(container) {
  container.append(infoBanner('Live mirrors Director authority: progress, cue state, transport health and execution activity.'));

  const grid = document.createElement('div');
  grid.className = 'page-grid';
  container.append(grid);

  const progressCard = metricCard('Show Progress', '0%');
  const cueCard = metricCard('Cue', 'Not reported');
  const actionsCard = metricCard('Executing Actions', '0');
  const transportCard = metricCard('Transport Health', 'unknown');
  const latencyCard = metricCard('Network Latency', '—');
  const nodeActivityCard = metricCard('Node Activity', '0 online');
  const audioCard = metricCard('Audio Status', 'Not reported');
  const lightingCard = metricCard('Lighting Status', 'Not reported');
  grid.append(
    progressCard,
    cueCard,
    actionsCard,
    transportCard,
    latencyCard,
    nodeActivityCard,
    audioCard,
    lightingCard
  );

  const detailGrid = document.createElement('div');
  detailGrid.className = 'page-grid';
  container.append(detailGrid);

  const timelineCard = document.createElement('section');
  timelineCard.className = 'card';
  timelineCard.innerHTML = '<h2>Timeline</h2>';
  const timelineHost = document.createElement('div');
  timelineCard.append(timelineHost);

  const actionsHost = document.createElement('div');
  const warningsHost = document.createElement('div');
  detailGrid.append(timelineCard, actionsHost, warningsHost);

  const updateValue = (card, value, detailText) => {
    const valueEl = card.querySelector('.value');
    const subEl = card.querySelector('.sub');
    if (valueEl) valueEl.textContent = String(value ?? '—');
    if (subEl) subEl.textContent = detailText || '';
  };

  const unsubscribe = subscribeRuntime((state) => {
    const percent = Math.round((state.runtimeStatus.progress || 0) * 100);
    const actions = state.runtimeStatus.executingActions || [];

    updateValue(progressCard, `${percent}%`, `${formatDurationMs(state.runtimeStatus.elapsedMs)} / ${formatDurationMs(state.runtimeStatus.timelineLengthMs)}`);
    updateValue(cueCard, state.runtimeStatus.cue || 'Not reported', `Mode ${state.runtimeStatus.mode}`);
    updateValue(actionsCard, actions.length, actions.length ? actions[0].action : 'Queue idle');
    updateValue(transportCard, state.runtimeStatus.transportHealth, `Link ${state.connectionStatus.lifecycle}`);
    updateValue(latencyCard, state.connectionStatus.latencyMs != null ? `${state.connectionStatus.latencyMs} ms` : 'Not reported');
    updateValue(nodeActivityCard, `${state.nodeCollection.counts.online} online`, `${state.nodeCollection.counts.warning} warning · ${state.nodeCollection.counts.offline} offline`);
    updateValue(audioCard, state.runtimeStatus.audio.summary);
    updateValue(lightingCard, state.runtimeStatus.lighting.summary);

    timelineHost.innerHTML = '';
    timelineHost.append(keyValueTable([
      ['Current Show', state.runtimeStatus.currentShow],
      ['Elapsed', formatDurationMs(state.runtimeStatus.elapsedMs)],
      ['Remaining', formatDurationMs(state.runtimeStatus.remainingMs)],
      ['Timeline Length', formatDurationMs(state.runtimeStatus.timelineLengthMs)],
      ['Current Cue', state.runtimeStatus.cue || 'Not reported'],
      ['Runtime State', state.runtimeStatus.runtime]
    ]));

    actionsHost.innerHTML = '';
    actionsHost.append(listCard(
      'Executing Actions',
      actions.map((entry) => `${entry.action} → ${entry.destination} (${entry.status})`),
      'No active actions.'
    ));

    warningsHost.innerHTML = '';
    warningsHost.append(listCard('Warnings', state.runtimeStatus.warnings, 'No active warnings.'));
  });

  return () => unsubscribe();
}

LivePage.title = 'Live';
LivePage.subtitle = 'Realtime show execution mirror.';
