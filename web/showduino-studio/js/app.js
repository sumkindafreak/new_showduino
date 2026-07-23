import { Layout } from './components/Layout.js';
import { registerRoute, startRouter } from './router.js';
import { HomePage } from './pages/Home.js';
import { DevicesPage } from './pages/Devices.js';
import { CommandsPage } from './pages/Commands.js';
import { CapabilitiesPage } from './pages/Capabilities.js';
import { RoutingPage } from './pages/Routing.js';
import { TimePage } from './pages/Time.js';
import { ShowsPage } from './pages/Shows.js';
import { ScenesPage } from './pages/Scenes.js';
import { AudioPage } from './pages/Audio.js';
import { LightingPage } from './pages/Lighting.js';
import { NetworkPage } from './pages/Network.js';
import { LogsPage } from './pages/Logs.js';
import { SettingsPage } from './pages/Settings.js';

Layout();

registerRoute('/', HomePage);
registerRoute('/devices', DevicesPage);
registerRoute('/commands', CommandsPage);
registerRoute('/capabilities', CapabilitiesPage);
registerRoute('/routing', RoutingPage);
registerRoute('/time', TimePage);
registerRoute('/shows', ShowsPage);
registerRoute('/scenes', ScenesPage);
registerRoute('/audio', AudioPage);
registerRoute('/lighting', LightingPage);
registerRoute('/network', NetworkPage);
registerRoute('/logs', LogsPage);
registerRoute('/settings', SettingsPage);

startRouter();
