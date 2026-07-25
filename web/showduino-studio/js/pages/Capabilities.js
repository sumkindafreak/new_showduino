import { comingSoonCard, infoBanner } from './pagePrimitives.js';

export function CapabilitiesPage(container) {
  container.append(infoBanner('Capability drill-down is moved to Diagnostics for this phase.'));
  container.append(comingSoonCard(
    'Capability Matrix',
    'A full capability matrix will return once node configuration workflows are promoted into production routes.'
  ));
}

CapabilitiesPage.title = 'Capabilities';