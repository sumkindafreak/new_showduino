// AudioBrowser — Modal for selecting audio files from device or uploading
// Assigns selected file to the currently selected audio clip in the timeline

class AudioBrowser {
  constructor(api, timelineEditor) {
    this._api      = api;
    this._tl       = timelineEditor;
    this._modal    = null;
    this._files    = [];
  }

  createModal() {
    if (this._modal) return;

    const overlay = document.createElement('div');
    overlay.id = 'audio-browser-modal';
    overlay.style.cssText = 'display:none;position:fixed;inset:0;background:rgba(0,0,0,0.75);align-items:center;justify-content:center;z-index:8000;';

    const box = document.createElement('div');
    box.style.cssText = 'background:#2a2a2a;border:1px solid #444;border-radius:8px;width:520px;max-width:96vw;max-height:80vh;display:flex;flex-direction:column;box-shadow:0 8px 32px rgba(0,0,0,0.6);';

    // Header
    const hdr = document.createElement('div');
    hdr.style.cssText = 'padding:14px 18px;border-bottom:1px solid #444;display:flex;align-items:center;justify-content:space-between;';
    hdr.innerHTML = `<h3 style="margin:0;color:#00ffcc;font-size:15px;">🎵 Audio Browser</h3>`;
    const closeBtn = document.createElement('button');
    closeBtn.textContent = '✕';
    closeBtn.style.cssText = 'background:none;border:none;color:#888;font-size:18px;cursor:pointer;padding:0 4px;';
    closeBtn.addEventListener('click', () => this.close());
    hdr.appendChild(closeBtn);
    box.appendChild(hdr);

    // Toolbar
    const toolbar = document.createElement('div');
    toolbar.style.cssText = 'padding:10px 18px;border-bottom:1px solid #333;display:flex;gap:8px;align-items:center;';

    const refreshBtn = document.createElement('button');
    refreshBtn.textContent = '🔄 Refresh';
    refreshBtn.className = 'btn-toolbar';
    refreshBtn.style.fontSize = '12px';
    refreshBtn.addEventListener('click', () => this._loadFiles());

    const uploadBtn = document.createElement('button');
    uploadBtn.textContent = '⬆ Upload';
    uploadBtn.className = 'btn-primary';
    uploadBtn.style.cssText = 'font-size:12px;padding:4px 10px;';
    uploadBtn.disabled = true;
    uploadBtn.title = 'Copy files to the Audio Node SD card; upload through Comms is not implemented.';

    const filterInput = document.createElement('input');
    filterInput.type = 'text';
    filterInput.placeholder = 'Filter files…';
    filterInput.id = 'audio-browser-filter';
    filterInput.style.cssText = 'flex:1;background:#111;border:1px solid #555;color:#eee;padding:4px 8px;border-radius:4px;font-size:12px;';
    filterInput.addEventListener('input', () => this._applyFilter(filterInput.value));

    toolbar.appendChild(refreshBtn);
    toolbar.appendChild(uploadBtn);
    toolbar.appendChild(filterInput);
    box.appendChild(toolbar);

    // Status bar
    const status = document.createElement('div');
    status.id = 'audio-browser-status';
    status.style.cssText = 'padding:4px 18px;font-size:11px;color:#888;border-bottom:1px solid #333;';
    status.textContent = 'Loading…';
    box.appendChild(status);

    // File list
    const listWrap = document.createElement('div');
    listWrap.style.cssText = 'flex:1;overflow-y:auto;padding:8px 0;min-height:200px;';
    const list = document.createElement('div');
    list.id = 'audio-browser-list';
    listWrap.appendChild(list);
    box.appendChild(listWrap);

    // Footer
    const footer = document.createElement('div');
    footer.style.cssText = 'padding:12px 18px;border-top:1px solid #444;display:flex;gap:8px;justify-content:flex-end;';
    const cancelBtn = document.createElement('button');
    cancelBtn.textContent = 'Cancel';
    cancelBtn.className = 'btn-secondary';
    cancelBtn.addEventListener('click', () => this.close());
    footer.appendChild(cancelBtn);
    box.appendChild(footer);

    overlay.appendChild(box);
    document.body.appendChild(overlay);
    overlay.addEventListener('click', e => { if (e.target === overlay) this.close(); });

    this._modal = overlay;
  }

  async open() {
    if (!this._modal) this.createModal();
    this._modal.style.display = 'flex';
    await this._loadFiles();
  }

  close() {
    if (this._modal) this._modal.style.display = 'none';
  }

  async _loadFiles() {
    if (this._loading) return;
    this._loading = true;
    const statusEl = document.getElementById('audio-browser-status');
    const listEl   = document.getElementById('audio-browser-list');
    if (statusEl) statusEl.textContent = 'Reading Audio Node SD inventory…';
    if (listEl)   listEl.innerHTML = '<div style="padding:20px;text-align:center;color:#666;font-size:12px;">Fetching audio files…</div>';

    try {
      const result = await this._api.listAudioFiles();
      this._files = Array.isArray(result.files) ? result.files : (Array.isArray(result) ? result : []);
      if (statusEl) statusEl.textContent = `${this._files.length} file(s) on Audio Node SD`;
      this._applyFilter(document.getElementById('audio-browser-filter')?.value || '');
    } catch (err) {
      if (statusEl) statusEl.textContent = 'Could not read Audio Node files: ' + err.message;
      if (listEl)   listEl.innerHTML = '<div style="padding:20px;text-align:center;color:#ff4444;font-size:12px;">Could not reach device. Are you connected?</div>';
      this._files = [];
    } finally { this._loading = false; }
  }

  _applyFilter(query) {
    const listEl = document.getElementById('audio-browser-list');
    if (!listEl) return;
    const filtered = this._files.filter(f => {
      const name = typeof f === 'string' ? f : (f.name || f.filename || '');
      return name.toLowerCase().includes(query.toLowerCase());
    });
    this._renderList(listEl, filtered);
  }

  _renderList(listEl, files) {
    listEl.innerHTML = '';
    if (files.length === 0) {
      listEl.innerHTML = '<div style="padding:20px;text-align:center;color:#666;font-size:12px;">No audio files reported by Audio Node. Copy supported files to its SD card, then Refresh.</div>';
      return;
    }

    files.forEach(f => {
      const filename = typeof f === 'string' ? f : (f.name || f.filename || String(f));
      const size     = typeof f === 'object' && f.size ? this._fmtSize(f.size) : '';

      const row = document.createElement('div');
      row.style.cssText = 'display:flex;align-items:center;gap:10px;padding:8px 18px;cursor:pointer;border-bottom:1px solid #222;';
      row.addEventListener('mouseenter', () => row.style.background='#333');
      row.addEventListener('mouseleave', () => row.style.background='');

      const icon = document.createElement('span');
      icon.textContent = '🎵';
      icon.style.fontSize = '16px';

      const info = document.createElement('div');
      info.style.flex = '1';
      // W002: file names come from the device or the user's own uploads; render as text.
      const nameEl = document.createElement('div');
      nameEl.style.cssText = 'font-size:13px;color:#eee;';
      nameEl.textContent = String(filename);
      info.appendChild(nameEl);
      if (size) {
        const sizeEl = document.createElement('div');
        sizeEl.style.cssText = 'font-size:10px;color:#666;';
        sizeEl.textContent = String(size);
        info.appendChild(sizeEl);
      }

      const previewBtn = document.createElement('button');
      previewBtn.textContent = '▶';
      previewBtn.title = 'Preview';
      previewBtn.style.cssText = 'background:#333;border:1px solid #555;color:#00ffcc;cursor:pointer;border-radius:3px;padding:2px 7px;font-size:12px;';
      previewBtn.addEventListener('click', e => { e.stopPropagation(); this._preview(filename); });

      const selectBtn = document.createElement('button');
      selectBtn.textContent = 'Select';
      selectBtn.className = 'btn-primary';
      selectBtn.style.cssText = 'font-size:11px;padding:3px 10px;';
      selectBtn.addEventListener('click', e => { e.stopPropagation(); this._selectFile(filename); });

      row.appendChild(icon);
      row.appendChild(info);
      row.appendChild(previewBtn);
      row.appendChild(selectBtn);
      listEl.appendChild(row);
    });
  }

  _selectFile(filename) {
    if (this._tl) {
      this._tl.setSelectedClipFile(filename);
    }
    this.close();
  }

  async _preview(filename) {
    try { await this._api.playAudio(filename); } catch (err) {
      const status = document.getElementById('audio-browser-status');
      if (status) status.textContent = 'Preview not confirmed: ' + err.message;
    }
  }

  _triggerUpload() {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = 'audio/*,.mp3,.wav,.ogg,.flac';
    input.multiple = true;
    input.addEventListener('change', async () => {
      const statusEl = document.getElementById('audio-browser-status');
      for (const file of input.files) {
        try {
          if (statusEl) statusEl.textContent = `Uploading ${file.name}…`;
          await this._api.uploadAudioFile(file);
          if (statusEl) statusEl.textContent = `Uploaded: ${file.name}`;
        } catch (err) {
          if (statusEl) statusEl.textContent = `Upload failed: ${err.message}`;
        }
      }
      await this._loadFiles();
    });
    input.click();
  }

  _fmtSize(bytes) {
    if (bytes >= 1048576) return (bytes / 1048576).toFixed(1) + ' MB';
    if (bytes >= 1024)    return (bytes / 1024).toFixed(1) + ' KB';
    return bytes + ' B';
  }
}

window.AudioBrowser = AudioBrowser;
