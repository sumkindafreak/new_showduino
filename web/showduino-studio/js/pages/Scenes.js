import { dispatchOperatorCommand, initializeRuntimeStore, subscribeRuntime } from '../runtimeStore.js';
import { el, statRow } from '../utils.js';

function renderAssetList(title, values) {
  const card = el('div', { className: 'card' });
  card.append(el('h2', { text: title }));
  if (!values.length) {
    card.append(el('p', { className: 'sub', text: 'No entries reported.' }));
    return card;
  }
  const list = el('ul', { className: 'nav-list' });
  for (const value of values) list.append(el('li', {}, [el('span', { text: value })]));
  card.append(list);
  return card;
}

export function ScenesPage(container) {
  initializeRuntimeStore();
  container.append(el('p', {
    className: 'info-panel',
    text: 'Asset browser from firmware. Refresh/delete/upload actions route through runtimeStore and degrade cleanly when endpoints are unavailable.'
  }));

  const status = el('div', { className: 'live-status', text: 'Loading assets…' });
  const controls = el('div', { className: 'card command-form' });
  controls.append(el('h2', { text: 'Asset Actions' }));

  const deletePath = el('input', { value: '', placeholder: 'path/to/asset.ext' });
  const uploadName = el('input', { value: '', placeholder: 'asset name' });
  const uploadContent = el('input', { value: '', placeholder: 'asset content (text payload)' });

  controls.append(el('button', {
    className: 'btn-primary',
    text: 'Refresh Assets',
    onClick: async () => {
      status.textContent = 'Refreshing assets…';
      try {
        await dispatchOperatorCommand('assets.refresh');
      } catch (error) {
        status.textContent = error.message;
      }
    }
  }));

  controls.append(el('label', { className: 'cmd-field' }, ['Delete ', deletePath]));
  controls.append(el('button', {
    className: 'btn-primary',
    text: 'Delete Asset',
    onClick: async () => {
      const path = deletePath.value.trim();
      if (!path) return;
      status.textContent = 'Deleting asset…';
      try {
        await dispatchOperatorCommand('assets.delete', { path });
        deletePath.value = '';
      } catch (error) {
        status.textContent = error.message;
      }
    }
  }));

  controls.append(el('label', { className: 'cmd-field' }, ['Upload Name ', uploadName]));
  controls.append(el('label', { className: 'cmd-field' }, ['Upload Content ', uploadContent]));
  controls.append(el('button', {
    className: 'btn-primary',
    text: 'Upload Asset',
    onClick: async () => {
      const name = uploadName.value.trim();
      const content = uploadContent.value;
      if (!name) return;
      status.textContent = 'Uploading asset…';
      try {
        await dispatchOperatorCommand('assets.upload', { name, content });
      } catch (error) {
        status.textContent = error.message;
      }
    }
  }));

  const supportCard = el('div', { className: 'card' });
  supportCard.append(el('h2', { text: 'Endpoint Support' }));
  const supportBody = el('div');
  supportCard.append(supportBody);

  const grid = el('div', { className: 'page-grid' });
  container.append(status, controls, supportCard, grid);

  const unsub = subscribeRuntime((snap) => {
    const assets = snap.assets;
    status.textContent = snap.connection.connected
      ? `Live · assets ${assets.supported ? 'online' : 'unsupported'}`
      : `Disconnected · reconnect in ${snap.connection.reconnectInSec || 0}s`;

    supportBody.innerHTML = '';
    supportBody.append(statRow('Asset List', assets.supported ? 'supported' : 'unsupported'));
    supportBody.append(statRow('Delete', assets.deleteSupported ? 'supported' : 'unsupported'));
    supportBody.append(statRow('Upload', assets.uploadSupported ? 'supported' : 'unsupported'));
    supportBody.append(statRow('Source', assets.source || 'unknown'));
    supportBody.append(statRow('Status', assets.error || 'ok'));

    grid.innerHTML = '';
    grid.append(renderAssetList('Images', assets.items.images || []));
    grid.append(renderAssetList('Audio', assets.items.audio || []));
    grid.append(renderAssetList('Lighting Files', assets.items.lighting || []));
    grid.append(renderAssetList('Shows', assets.items.shows || []));
  });

  return () => unsub();
}
ScenesPage.title = 'Assets';
