/**
 * Showduino Studio – Application Entry Point
 *
 * Builds the production application shell and registers all routes.
 * The shell never reloads during navigation; pages mount inside #page-content.
 */

import { Layout } from './components/Layout.js';
import { registerRoute, startRouter } from './router.js';

// ── Pages ────────────────────────────────────────────────────────────────────
import { DashboardPage }  from './pages/Dashboard.js';
import { LivePage }       from './pages/Live.js';
import { ShowsPage }      from './pages/Shows.js';
import { NodesPage }      from './pages/Nodes.js';
import { LogsPage }       from './pages/Logs.js';
import { DiagnosticsPage }from './pages/Diagnostics.js';
import { NetworkPage }    from './pages/Network.js';
import { SettingsPage }   from './pages/Settings.js';

import {
  TimelinePage,
  AudioPage,
  LightingPage,
  AssetsPage,
  NodeConfigPage,
} from './pages/ComingSoon.js';

// ── Build shell ───────────────────────────────────────────────────────────────
Layout();

// ── OPERATE routes ────────────────────────────────────────────────────────────
registerRoute('/',           DashboardPage);   // default redirect
registerRoute('/dashboard',  DashboardPage);
registerRoute('/live',       LivePage);
registerRoute('/timeline',   TimelinePage);
registerRoute('/shows',      ShowsPage);
registerRoute('/nodes',      NodesPage);
registerRoute('/logs',       LogsPage);
registerRoute('/diagnostics',DiagnosticsPage);

// ── CONFIGURE routes ──────────────────────────────────────────────────────────
registerRoute('/network',    NetworkPage);
registerRoute('/lighting',   LightingPage);
registerRoute('/audio',      AudioPage);
registerRoute('/assets',     AssetsPage);
registerRoute('/nodeconfig', NodeConfigPage);
registerRoute('/settings',   SettingsPage);

// ── Start ─────────────────────────────────────────────────────────────────────
startRouter();
