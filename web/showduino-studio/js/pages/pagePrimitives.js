import { el } from '../utils.js';

export function metricCard(title, value = '—', detail = '') {
  const card = el('section', { className: 'card metric-card' });
  card.append(
    el('h2', { text: title }),
    el('div', { className: 'value', text: String(value ?? '—') })
  );
  if (detail) card.append(el('div', { className: 'sub', text: detail }));
  return card;
}

export function infoBanner(text) {
  return el('p', { className: 'info-panel', text });
}

export function statePill(text, tone = 'neutral') {
  return el('span', { className: `state-pill ${tone}`, text });
}

export function keyValueTable(rows) {
  const table = el('table', { className: 'log-table keyvalue-table' });
  const body = el('tbody');
  for (const [key, value] of rows) {
    body.append(el('tr', {}, [el('th', { text: key }), el('td', { text: String(value ?? '—') })]));
  }
  table.append(body);
  return table;
}

export function listCard(title, items, emptyLabel = 'No data') {
  const card = el('section', { className: 'card' });
  card.append(el('h2', { text: title }));
  const list = el('ul', { className: 'event-list' });
  if (!items || items.length === 0) {
    list.append(el('li', { className: 'event-empty', text: emptyLabel }));
  } else {
    for (const item of items) list.append(el('li', { text: item }));
  }
  card.append(list);
  return card;
}

export function comingSoonCard(title, message) {
  const card = el('section', { className: 'card coming-soon' });
  card.append(
    el('h2', { text: title }),
    el('div', { className: 'coming-soon-label', text: 'Coming Soon' }),
    el('p', { className: 'sub', text: message })
  );
  return card;
}
