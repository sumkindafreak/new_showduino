import { fetchSystem } from '../api.js';
import { el, formatBytes, formatUptime, statRow } from '../utils.js';
import { MemoryBar } from '../components/MemoryBar.js';

export async function HomePage(container) {
  container.append(el('p', { className: 'info-panel', text: 'Live telemetry from the Show Engine (ESP32-P4), via the C3 Wi-Fi front door.' }));

  const grid = el('div', { className: 'page-grid' });
  container.append(grid);

  async function refresh() {
    try {
      const sys = await fetchSystem();
      grid.innerHTML = '';

      const overview = el('div', { className: 'card' });
      overview.append(el('h2', { text: 'System' }));
      overview.append(el('div', { className: 'value', text: sys.boardName || 'Show Engine' }));
      overview.append(el('div', { className: 'sub', text: `Firmware ${sys.firmwareVersion}` }));
      grid.append(overview);

      const uptime = el('div', { className: 'card' });
      uptime.append(el('h2', { text: 'Uptime' }));
      uptime.append(el('div', { className: 'value', text: formatUptime(sys.uptime) }));
      uptime.append(el('div', { className: 'sub', text: `CPU ${sys.cpuMhz} MHz` }));
      grid.append(uptime);

      const mem = el('div', { className: 'card' });
      mem.append(el('h2', { text: 'Memory' }));
      mem.append(MemoryBar({ label: 'Heap', used: sys.heapTotal - sys.heapFree, total: sys.heapTotal, variant: 'heap' }));
      mem.append(MemoryBar({ label: 'PSRAM', used: sys.psramTotal - sys.psramFree, total: sys.psramTotal, variant: 'psram' }));
      grid.append(mem);

      const storage = el('div', { className: 'card' });
      storage.append(el('h2', { text: 'Storage' }));
      storage.append(statRow('SD Ready', sys.storageReady ? 'Yes' : 'No'));
      storage.append(statRow('Writable', sys.storageWritable ? 'Yes' : 'No'));
      storage.append(statRow('WebUI on SD', sys.storageHasWww ? 'Yes' : 'No'));
      storage.append(statRow('Card', sys.storageCardType || '-'));
      if (sys.storageTotalMb != null) {
        storage.append(statRow('Free / Total', `${sys.storageFreeMb || 0} / ${sys.storageTotalMb} MB`));
      }
      storage.append(statRow('Status', sys.storageMessage || '-'));
      storage.append(statRow('Shows Path', sys.showsPath));
      storage.append(statRow('WebUI Path', sys.webuiPath || '/showduino/www'));
      grid.append(storage);
    } catch (err) {
      grid.innerHTML = '';
      grid.append(el('div', { className: 'card' }, [
        el('h2', { text: 'Connection Error' }),
        el('p', { text: err.message })
      ]));
    }
  }

  await refresh();
  const timer = setInterval(refresh, 3000);
  return () => clearInterval(timer);
}
HomePage.title = 'Home';
