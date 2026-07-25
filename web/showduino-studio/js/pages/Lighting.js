import { subscribeRuntime } from '../state/runtimeStore.js';
import { comingSoonCard, infoBanner, keyValueTable } from './pagePrimitives.js';

export function LightingPage(container) {
  container.append(infoBanner('Lighting authority remains on execution nodes. This view consumes shared runtime telemetry.'));

  const card = document.createElement('section');
  card.className = 'card';
  card.innerHTML = '<h2>Lighting Runtime Mirror</h2>';
  const host = document.createElement('div');
  card.append(host);
  container.append(card);

  container.append(comingSoonCard(
    'Lighting Patch & Pixel Tools',
    'Patch, fixture and per-pixel editing are hidden until command acceptance and completion telemetry is complete.'
  ));

  const unsub = subscribeRuntime((state) => {
    host.innerHTML = '';
    host.append(keyValueTable([
      ['Summary', state.runtimeStatus.lighting.summary],
      ['Node Online', state.nodeCollection.counts.online],
      ['Node Warning', state.nodeCollection.counts.warning],
      ['Node Offline', state.nodeCollection.counts.offline],
      ['Transport Health', state.runtimeStatus.transportHealth]
    ]));
  });

  return () => unsub();
}
LightingPage.title = 'Lighting';
LightingPage.subtitle = 'Runtime lighting mirror and future tools.';
