import { comingSoonCard, infoBanner } from './pagePrimitives.js';

export function ScenesPage(container) {
  container.append(infoBanner('Scene tools are staged under the Timeline/Shows roadmap.'));
  container.append(comingSoonCard(
    'Scene Inspector',
    'Scene-level editing and review are intentionally held until timeline authoring endpoints are available.'
  ));
}

ScenesPage.title = 'Scenes';
