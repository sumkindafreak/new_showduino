/**
 * Showduino Studio – Coming Soon page factory
 *
 * Returns a page constructor for features not yet implemented.
 * Pages are clearly marked rather than hidden, per Phase 1 requirements.
 */

import { el } from '../utils.js';

const DESCRIPTIONS = {
  Timeline: 'The Timeline editor will allow building and editing full show sequences with millisecond-precision cue placement, crossfades, and automation tracks.',
  Audio:    'Dual MP3 deck control (players A and B), volume sliders, and play/pause/stop transport. Audio hardware runs on the Brain node; commands are sent via ESP-NOW.',
  Lighting: 'Relay grid (8 tactile toggles, expandable via SX1509) and per-pixel LED brightness control. Relay states are commanded to the Brain and confirmed via ESP-NOW feedback.',
  Assets:   'Show package and asset management: upload, organise and validate show.json, timeline.json, and associated media files on the SD card.',
  'Node Config': 'Per-node configuration: assign roles, set ESP-NOW channels, manage paired devices, and configure capability priorities.',
};

export function comingSoonPage(name, description) {
  function Page(container) {
    container.append(el('div', { className: 'coming-soon' }, [
      el('div', { className: 'coming-soon-icon', text: '⚙' }),
      el('h2', { text: name }),
      el('p', { text: description || DESCRIPTIONS[name] || `${name} is not yet implemented in this release.` }),
      el('p', { className: 'text-dim', text: 'This feature is planned for a future phase.' }),
    ]));
  }
  Page.title = name;
  return Page;
}

export const TimelinePage   = comingSoonPage('Timeline');
export const AudioPage      = comingSoonPage('Audio');
export const LightingPage   = comingSoonPage('Lighting');
export const AssetsPage     = comingSoonPage('Assets');
export const NodeConfigPage = comingSoonPage('Node Config');
