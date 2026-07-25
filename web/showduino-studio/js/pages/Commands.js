import { comingSoonCard, infoBanner } from './pagePrimitives.js';

export function CommandsPage(container) {
  container.append(infoBanner('Legacy command-test page is hidden from production nav.'));
  container.append(comingSoonCard(
    'Command Test Console',
    'Direct command-injection tooling is disabled in the production shell to avoid unsafe live-operation actions.'
  ));
}

CommandsPage.title = 'Commands';