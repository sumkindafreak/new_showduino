import { Layout } from './components/Layout.js';
import { registerRoute, startRouter } from './router.js';
import { HomePage } from './pages/Home.js';
import { ProductionsPage } from './pages/Productions.js';
import { LivePage } from './pages/Live.js';
import { OutputsPage } from './pages/Outputs.js';
import { DevicesPage } from './pages/Devices.js';
import { NetworkPage } from './pages/Network.js';
import { SystemPage } from './pages/System.js';
import { SettingsPage } from './pages/Settings.js';

Layout();

registerRoute('/', HomePage);
registerRoute('/productions', ProductionsPage);
registerRoute('/live', LivePage);
registerRoute('/outputs', OutputsPage);
registerRoute('/devices', DevicesPage);
registerRoute('/network', NetworkPage);
registerRoute('/system', SystemPage);
registerRoute('/settings', SettingsPage);

registerRoute('/shows', ProductionsPage);
registerRoute('/commands', LivePage);
registerRoute('/scenes', LivePage);
registerRoute('/audio', OutputsPage);
registerRoute('/lighting', OutputsPage);
registerRoute('/logs', SystemPage);
registerRoute('/capabilities', SystemPage);
registerRoute('/routing', SystemPage);
registerRoute('/time', SystemPage);

startRouter();
