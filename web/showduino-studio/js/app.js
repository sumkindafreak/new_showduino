/**
 * Showduino Studio – Application Entry Point
 *
 * Starts the single runtime store, builds the permanent shell, and registers
 * all routes. The shell never reloads during navigation; pages mount inside
 * #page-content.
 */

import { startRuntimeStore } from './state/runtimeStore.js';
import { Layout } from './components/Layout.js';
import { registerRoute, startRouter } from './router.js';

// ── OPERATE pages ─────────────────────────────────────────────────────────────
import { DashboardPage }   from './pages/Dashboard.js';
import { LivePage }        from './pages/Live.js';
import { TimelinePage }    from './pages/Timeline.js';
import { ShowsPage }       from './pages/Shows.js';
import { NodesPage }       from './pages/Nodes.js';
import { LogsPage }        from './pages/Logs.js';
import { DiagnosticsPage } from './pages/Diagnostics.js';

// ── CONFIGURE pages ───────────────────────────────────────────────────────────
import { NetworkPage }           from './pages/Network.js';
import { LightingPage }          from './pages/Lighting.js';
import { AudioPage }             from './pages/Audio.js';
import { AssetsPage }            from './pages/Assets.js';
import { NodeConfigurationPage } from './pages/NodeConfiguration.js';
import { SettingsPage }          from './pages/Settings.js';

// ── Start runtime store (WebSocket + REST polling + watchdog) ─────────────────
startRuntimeStore();

// ── Build permanent shell ─────────────────────────────────────────────────────
Layout();

// ── OPERATE routes ────────────────────────────────────────────────────────────
registerRoute('/',            DashboardPage);
registerRoute('/dashboard',   DashboardPage);
registerRoute('/live',        LivePage);
registerRoute('/timeline',    TimelinePage);
registerRoute('/shows',       ShowsPage);
registerRoute('/nodes',       NodesPage);
registerRoute('/logs',        LogsPage);
registerRoute('/diagnostics', DiagnosticsPage);

// ── CONFIGURE routes ──────────────────────────────────────────────────────────
registerRoute('/network',     NetworkPage);
registerRoute('/lighting',    LightingPage);
registerRoute('/audio',       AudioPage);
registerRoute('/assets',      AssetsPage);
registerRoute('/nodeconfig',  NodeConfigurationPage);
registerRoute('/settings',    SettingsPage);

// ── Start router ──────────────────────────────────────────────────────────────
startRouter();
