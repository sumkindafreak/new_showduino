// Showduino API Client
// Communicates with Showduino hardware over HTTP (AP or LAN mode)
// Falls back to mock responses when offline

class ShowduinoAPI {
  constructor(baseURL = null) {
    this._baseURL = baseURL || this._detectBaseURL();
    this._timeout = 5000;
    this._mockMode = false;
  }

  _detectBaseURL() {
    try {
      const configured = window.localStorage?.getItem('showduino_target_url');
      if (configured) return configured.replace(/\/$/, '');
    } catch (_) {}
    const host = window.location.hostname;
    if (host === '192.168.4.1' || host.startsWith('192.168.4.')) {
      return `http://${host}`;
    }
    // Public Showduino website is show-duino.com. Never treat it as a local
    // hardware hostname; local hardware falls back to the Showduino AP address.
    if (host !== 'localhost' && host !== '127.0.0.1' && host !== 'show-duino.com' && !host.endsWith('.show-duino.com') && !host.endsWith('.github.io') && host !== 'github.com') {
      return `http://${host}`;
    }
    return 'http://192.168.4.1';
  }

  setBaseURL(url) { this._baseURL = url; }
  setMockMode(enabled) { this._mockMode = enabled; }

  async _request(method, path, body = null) {
    if (this._mockMode) return this._mockResponse(method, path, body);
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), this._timeout);
    try {
      const opts = {
        method,
        signal: controller.signal,
        headers: { 'Content-Type': 'application/json' }
      };
      if (body) opts.body = JSON.stringify(body);
      const resp = await fetch(this._baseURL + path, opts);
      clearTimeout(timer);
      if (!resp.ok) throw new Error(`HTTP ${resp.status}: ${resp.statusText}`);
      return await resp.json();
    } catch (err) {
      clearTimeout(timer);
      if (err.name === 'AbortError') throw new Error('Request timed out');
      throw err;
    }
  }

  _mockResponse(method, path, body) {
    if (path === '/status') {
      return Promise.resolve({
        wifi_connected: false, ip: '192.168.4.1', mac: 'AA:BB:CC:DD:EE:FF',
        firmware: '1.0.0', uptime: 0, heap: 200000, ssid: '',
        relays: { out1: false, out2: false },
        audio: { playing: false, file: '', volume: 80 },
        sd_card: true
      });
    }
    if (path === '/audio/list') return Promise.resolve({ files: [] });
    if (path === '/devices') return Promise.resolve({ devices: [] });
    return Promise.resolve({ ok: true });
  }

  // Status
  async getStatus() { return this._request('GET', '/status'); }

  // LED Lines
  async clearLEDLine(line) {
    return this._request('POST', '/led/clear', { line });
  }
  async setLEDLine(line, r, g, b, brightness = 255) {
    return this._request('POST', '/led/set', { line, r, g, b, brightness });
  }
  async setStatusLED(r, g, b) {
    return this._request('POST', '/led/status', { r, g, b });
  }
  async blinkStatusLED(r, g, b, interval = 500) {
    return this._request('POST', '/led/blink', { r, g, b, interval });
  }
  async statusLEDOff() {
    return this._request('POST', '/led/off', {});
  }

  // Relays
  async setRelay(out, state) {
    return this._request('POST', '/relay/set', { out, state });
  }

  // Audio
  async listAudioFiles() {
    const readNode = async () => {
      const system = await this._request('GET', '/api/system');
      if (system.p4Online === false || system.error || !system.audioNode?.online) throw new Error('Audio Node is offline');
      if (!Number.isInteger(system.audioNode.inventoryRevision) || !Number.isInteger(system.audioNode.inventoryPageSize)) {
        throw new Error('Update P4 firmware for Studio Audio Node inventory');
      }
      return system.audioNode;
    };
    let node = await readNode();
    const files = [];
    let page = 0;
    for (;;) {
      const revision = node.inventoryRevision;
      await this._nodeCommand('INVENTORY:' + page);
      const deadline = Date.now() + 6000;
      let fresh = false;
      while (Date.now() < deadline) {
        await new Promise(resolve => setTimeout(resolve, 250));
        node = await readNode();
        if (node.inventoryRevision !== revision && node.inventoryPage === page) { fresh = true; break; }
      }
      if (!fresh) throw new Error('Audio Node inventory page ' + page + ' timed out. Retry Refresh.');
      if (!Number.isInteger(node.inventoryTotal) || node.inventoryTotal < 0 || node.inventoryTotal > 48 || node.inventoryPageSize < 1) throw new Error('Invalid Audio Node inventory response');
      for (const name of (node.inventory || [])) {
        if (typeof name !== 'string' || !name || name.includes('..') || /[\r\n]/.test(name)) throw new Error('Invalid Audio Node filename');
        if (!files.includes(name)) files.push(name);
      }
      if ((page + 1) * node.inventoryPageSize >= node.inventoryTotal) break;
      page++;
      if (page >= 48) throw new Error('Audio Node inventory exceeded page limit');
    }
    if (files.length !== node.inventoryTotal) throw new Error('Audio inventory changed or was incomplete. Retry Refresh.');
    return {files, source:'audio-node'};
  }
  async _nodeCommand(command) {
    const result = await this._request('POST', '/api/command', {
      cmd:'AUDIO:NODE:' + command,
      requestId:'audio-browser-' + Date.now().toString(36) + '-' + Math.random().toString(36).slice(2,8)
    });
    if (!result || result.ok === false || result.error || result.p4Online === false) throw new Error(result?.replies || result?.error || 'Audio command rejected');
    return result;
  }
  async playAudio(file) { return this._nodeCommand('PLAY:' + file); }
  async stopAudio() { return this._nodeCommand('STOP'); }
  async pauseAudio() { return this._nodeCommand('PAUSE'); }
  async resumeAudio() { return this._nodeCommand('RESUME'); }
  async setAudioVolume(value) { return this._nodeCommand('VOLUME:' + value); }
  async uploadAudioFile(file) {
    const fd = new FormData();
    fd.append('file', file);
    const controller = new AbortController();
    const timer = setTimeout(() => controller.abort(), 30000);
    try {
      const resp = await fetch(this._baseURL + '/audio/upload', {
        method: 'POST', body: fd, signal: controller.signal
      });
      clearTimeout(timer);
      if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
      return await resp.json();
    } catch (err) {
      clearTimeout(timer);
      throw err;
    }
  }

  // WiFi
  async connectWiFi(ssid, password) {
    return this._request('POST', '/wifi/connect', { ssid, password });
  }

  // Projects on SD card
  async saveProject(filename, project) {
    return this._request('POST', '/project/save', { filename, data: JSON.stringify(project) });
  }
  async loadProject(filename) {
    const resp = await this._request('GET', `/project/load?file=${encodeURIComponent(filename)}`);
    if (typeof resp === 'string') return JSON.parse(resp);
    return resp;
  }

  // Devices
  async getDevices() { return this._request('GET', '/devices'); }

  // DMX
  async setDMXChannels(channels) {
    return this._request('POST', '/dmx/set', { channels });
  }
}

window.ShowduinoAPI = ShowduinoAPI;
