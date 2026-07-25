import { comingSoonCard, infoBanner } from './pagePrimitives.js';

export function RoutingPage(container) {
  container.append(infoBanner('Legacy route-test tooling is no longer exposed from the production operation shell.'));
  container.append(comingSoonCard(
    'Routing Workbench',
    'Advanced route simulation is paused while baseline operator pages focus on live runtime reliability.'
  ));
}

RoutingPage.title = 'Routing';