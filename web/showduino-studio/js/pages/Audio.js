import { subscribeRuntime } from '../state/runtimeStore.js';
import { comingSoonCard, infoBanner, keyValueTable } from './pagePrimitives.js';

export function AudioPage(container) {
  container.append(infoBanner('Audio authority remains on the Brain. This page mirrors shared runtime audio status only.'));

  const card = document.createElement('section');
  card.className = 'card';
  card.innerHTML = '<h2>Audio Runtime Mirror</h2>';
  const host = document.createElement('div');
  card.append(host);
  container.append(card);

  container.append(comingSoonCard(
    'Audio Control Surface',
    'Transport controls are intentionally disabled here until full command-confirmation loops are available in firmware.'
  ));

  const unsub = subscribeRuntime((state) => {
    host.innerHTML = '';
    host.append(keyValueTable([
      ['Summary', state.runtimeStatus.audio.summary],
      ['Runtime', state.runtimeStatus.runtime],
      ['Transport Health', state.runtimeStatus.transportHealth],
      ['Protocol', state.systemState.protocolVersion]
    ]));
  });

  return () => unsub();
}
AudioPage.title = 'Audio';
AudioPage.subtitle = 'Runtime audio mirror and future controls.';
