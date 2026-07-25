import { Layout, setPageHeader } from './components/Layout.js';
import { bindStatusSurface } from './components/StatusSurface.js';
import { registerRoute, startRouter } from './router.js';
import { DashboardPage } from './pages/Dashboard.js';
import { LivePage } from './pages/Live.js';
import { TimelinePage } from './pages/Timeline.js';
import { ShowsPage } from './pages/Shows.js';
import { NodesPage } from './pages/Nodes.js';
import { LogsPage } from './pages/Logs.js';
import { DiagnosticsPage } from './pages/Diagnostics.js';
import { NetworkPage } from './pages/Network.js';
import { AudioPage } from './pages/Audio.js';
import { LightingPage } from './pages/Lighting.js';
import { AssetsPage } from './pages/Assets.js';
import { NodeConfigurationPage } from './pages/NodeConfiguration.js';
import { SettingsPage } from './pages/Settings.js';
import { setNavigationRoute, startRuntimeStore } from './state/runtimeStore.js';

const NAV_SECTIONS = [
  {
    title: 'OPERATE',
    items: [
      { route: '/dashboard', label: 'Dashboard', icon: '◉' },
      { route: '/live', label: 'Live', icon: '▶' },
      { route: '/timeline', label: 'Timeline', icon: '◷' },
      { route: '/shows', label: 'Shows', icon: '◫' },
      { route: '/nodes', label: 'Nodes', icon: '⬡' },
      { route: '/logs', label: 'Logs', icon: '≡' },
      { route: '/diagnostics', label: 'Diagnostics', icon: '⌁' }
    ]
  },
  {
    title: 'CONFIGURE',
    items: [
      { route: '/network', label: 'Network', icon: '⌂' },
      { route: '/lighting', label: 'Lighting', icon: '☀' },
      { route: '/audio', label: 'Audio', icon: '♪' },
      { route: '/assets', label: 'Assets', icon: '▣' },
      { route: '/node-configuration', label: 'Node Configuration', icon: '⚡' },
      { route: '/settings', label: 'Settings', icon: '⚙' }
    ]
  }
];

Layout(NAV_SECTIONS);
bindStatusSurface();
startRuntimeStore();

registerRoute('/', DashboardPage);
registerRoute('/dashboard', DashboardPage);
registerRoute('/live', LivePage);
registerRoute('/timeline', TimelinePage);
registerRoute('/shows', ShowsPage);
registerRoute('/nodes', NodesPage);
registerRoute('/logs', LogsPage);
registerRoute('/diagnostics', DiagnosticsPage);
registerRoute('/network', NetworkPage);
registerRoute('/audio', AudioPage);
registerRoute('/lighting', LightingPage);
registerRoute('/assets', AssetsPage);
registerRoute('/node-configuration', NodeConfigurationPage);
registerRoute('/settings', SettingsPage);

startRouter({
  onRouteChange: ({ path, handler }) => {
    setNavigationRoute(path);
    setPageHeader(handler.title || 'Showduino Studio', handler.subtitle || '');
  }
});
