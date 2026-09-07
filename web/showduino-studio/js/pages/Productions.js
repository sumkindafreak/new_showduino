import { fetchProductions, postCommand, isP4Offline } from '../api.js';
import { subscribeStore } from '../store.js';
import { el, emptyState, p4OfflineBanner, statRow } from '../utils.js';

export async function ProductionsPage(container) {
  container.append(el('p', {
    className: 'info-panel',
    text: 'Persistent P4 productions on SD. Load asks the Show Engine. The WebUI does not invent outputs or node dependencies.'
  }));

  const banner = el('div', {});
  const result = el('div', { className: 'reply-box', text: 'No load result yet.' });
  const host = el('div', { className: 'page-stack' });
  container.append(banner, host, el('div', { className: 'card' }, [
    el('h2', { text: 'Last P4 result' }),
    result
  ]));

  let productions = null;
  let pending = null;
  let lastSnap = { p4Online: false, system: null };

  function loadedId() {
    return (productions && productions.loadedId) ||
      (lastSnap.system && lastSnap.system.productionId) || '';
  }

  async function send(cmd) {
    pending = cmd;
    result.textContent = `${cmd} … waiting for P4`;
    paint();
    try {
      const data = await postCommand(cmd);
      if (isP4Offline(data)) {
        result.textContent = 'P4 OFFLINE — command did not reach the Show Engine.';
      } else {
        result.textContent = data.replies || JSON.stringify(data);
      }
    } catch (err) {
      result.textContent = err.message;
    }
    pending = null;
    await poll();
  }

  function paint() {
    banner.innerHTML = '';
    host.innerHTML = '';
    if (!lastSnap.p4Online) banner.append(p4OfflineBanner());

    const listCard = el('div', { className: 'card' });
    listCard.append(el('h2', { text: 'Discovered productions' }));
    if (productions && productions.path) {
      listCard.append(el('p', { className: 'sub', text: productions.path }));
    }
    if (productions && productions.error) {
      listCard.append(statRow('P4 store error', productions.error));
    }

    const items = (productions && productions.productions) || [];
    const current = loadedId();
    if (!lastSnap.p4Online) {
      listCard.append(emptyState('P4 OFFLINE', 'Production inventory is owned by the Show Engine.'));
    } else if (!items.length) {
      const why = productions && productions.storageReady === false
        ? 'SD is not mounted.'
        : 'No valid productions found.';
      listCard.append(emptyState('No productions', why));
    } else {
      for (const p of items) {
        const loaded = p.id === current;
        const card = el('div', { className: loaded ? 'device-card is-loaded' : 'device-card' });
        card.append(el('div', { className: 'device-name', text: p.name || p.id }));
        card.append(el('div', { className: 'device-role', text: loaded ? 'LOADED' : 'READY' }));
        if (p.description) card.append(el('p', { className: 'sub', text: p.description }));
        card.append(statRow('Production ID', p.id || '—'));
        card.append(statRow('Version', p.revision != null ? String(p.revision) : '—'));
        card.append(statRow('Cue count', loaded && lastSnap.system ? String(lastSnap.system.totalCues ?? '—') : '—'));
        card.append(statRow('Timeline', p.timeline || '—'));
        card.append(el('button', {
          className: 'btn-primary',
          text: pending === ('PRODUCTION:LOAD:' + p.id) ? 'LOAD…' : (loaded ? 'Loaded' : 'Load'),
          disabled: !lastSnap.p4Online || loaded || !!(lastSnap.system && lastSnap.system.emergencyActive) || !!pending,
          onClick: () => send('PRODUCTION:LOAD:' + p.id)
        }));
        listCard.append(card);
      }
    }
    host.append(listCard);

    const actions = el('div', { className: 'card' });
    actions.append(el('h2', { text: 'Loaded production' }));
    actions.append(el('div', { className: 'value', text: (lastSnap.system && lastSnap.system.productionName) || 'None' }));
    actions.append(el('button', {
      className: 'btn-cancel',
      text: pending === 'PRODUCTION:UNLOAD' ? 'UNLOAD…' : 'Unload',
      disabled: !lastSnap.p4Online || !current || !!(lastSnap.system && lastSnap.system.emergencyActive) || !!pending,
      onClick: () => send('PRODUCTION:UNLOAD')
    }));
    host.append(actions);
  }

  async function poll() {
    if (!lastSnap.p4Online) {
      productions = null;
      paint();
      return;
    }
    try {
      const data = await fetchProductions();
      productions = isP4Offline(data) ? null : data;
    } catch (err) {
      result.textContent = err.message;
    }
    paint();
  }

  const unsub = subscribeStore((snap) => {
    lastSnap = snap;
    paint();
  });
  await poll();
  const timer = setInterval(poll, 4000);
  return () => {
    unsub();
    clearInterval(timer);
  };
}
ProductionsPage.title = 'Productions';
