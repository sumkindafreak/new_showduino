/**
 * Showduino Studio – Diagnostics
 *
 * Unified diagnostics view with tabs:
 *   Commands | Capabilities | Routing | Time
 *
 * All views use shared runtime state — no independent polling.
 */

import { el, formatTimestamp, severityClass, statRow, makeCleanupGroup } from '../utils.js';
import { subscribe, getState } from '../state/runtime.js';
import {
  fetchCommands, postCommand, cancelCommand,
  fetchCapabilities, fetchDeviceCapabilities, fetchRoutes, postRouteTest,
  fetchTime, fetchTimeStatus,
} from '../api.js';
import {
  upsertCommand, bumpCapabilityTick,
} from '../state/runtime.js';

const TABS = ['Commands', 'Capabilities', 'Routing', 'Time'];

export function DiagnosticsPage(container) {
  const cleanup = makeCleanupGroup();

  // ── Tab bar ──────────────────────────────────────────────────────────────
  const tabBar = el('div', { className: 'tab-bar' });
  const tabContent = el('div', { className: 'tab-content' });
  container.append(tabBar, tabContent);

  let activeTab = 'Commands';
  let tabCleanup = null;

  const tabBtns = {};
  for (const tab of TABS) {
    const btn = el('button', {
      className: 'tab-btn' + (tab === activeTab ? ' active' : ''),
      text: tab,
      onClick: () => switchTab(tab),
    });
    tabBtns[tab] = btn;
    tabBar.append(btn);
  }

  function switchTab(tab) {
    if (tabCleanup) { try { tabCleanup(); } catch (_) {} tabCleanup = null; }
    tabContent.innerHTML = '';
    activeTab = tab;
    for (const [t, btn] of Object.entries(tabBtns)) {
      btn.classList.toggle('active', t === tab);
    }

    const builders = {
      Commands:     buildCommandsTab,
      Capabilities: buildCapabilitiesTab,
      Routing:      buildRoutingTab,
      Time:         buildTimeTab,
    };

    const result = builders[tab]?.(tabContent);
    if (typeof result === 'function') tabCleanup = result;
    else if (result instanceof Promise) result.then((fn) => { if (typeof fn === 'function') tabCleanup = fn; });
  }

  cleanup.add(() => { if (tabCleanup) { try { tabCleanup(); } catch (_) {} } });

  switchTab('Commands');

  return () => cleanup.run();
}

// ─── Commands tab ─────────────────────────────────────────────────────────────

function cmdRow(cmd, { cancellable = false } = {}) {
  const row = el('tr', { 'data-id': cmd.id || '' }, [
    el('td', { text: cmd.priority || '—' }),
    el('td', { text: cmd.status || '—' }),
    el('td', { text: cmd.source || '—' }),
    el('td', { text: cmd.destination || '—' }),
    el('td', { text: cmd.category || '—' }),
    el('td', { text: cmd.action || '—' }),
    el('td', { className: 'mono', text: cmd.executionTimeMs != null ? `${cmd.executionTimeMs} ms` : '—' }),
    el('td', { className: 'mono text-dim', text: (cmd.id || '').slice(0, 8) }),
  ]);
  if (cancellable && cmd.id) {
    const td = el('td', {});
    td.append(el('button', {
      className: 'btn-cancel',
      text: 'Cancel',
      onClick: async () => { try { await cancelCommand(cmd.id); } catch (err) { alert(err.message); } },
    }));
    row.append(td);
  } else {
    row.append(el('td', {}));
  }
  return row;
}

function cmdSection(title, rows, cancellable = false) {
  const card = el('div', { className: 'card' });
  card.append(el('h2', { text: title }));
  const table = el('table', { className: 'log-table cmd-table' });
  const headerCols = ['Priority', 'Status', 'Source', 'Dest', 'Category', 'Action', 'Exec', 'ID', ''];
  table.append(el('thead', {}, [
    el('tr', {}, headerCols.map((t) => el('th', { text: t }))),
  ]));
  const tbody = el('tbody', {});
  if (!rows.length) {
    tbody.append(el('tr', {}, [el('td', { colSpan: '9', className: 'text-muted', text: 'None' })]));
  } else {
    for (const c of rows) tbody.append(cmdRow(c, { cancellable }));
  }
  table.append(tbody);
  card.append(table);
  return { card, tbody };
}

function buildCommandsTab(container) {
  const statusEl = el('div', { className: 'live-status', text: 'Connecting…' });

  // Submit form
  const form = el('div', { className: 'card command-form' });
  form.append(el('h2', { text: 'Submit Command' }));
  const fields = {
    source:      el('input', { value: 'web-studio' }),
    destination: el('input', { value: 'ian', placeholder: 'ian | sue | broadcast | any' }),
    category:    el('input', { value: 'system' }),
    action:      el('input', { value: 'ping' }),
    priority:    el('input', { value: 'normal', placeholder: 'emergency | high | normal | low' }),
    payload:     el('input', { value: '{}', placeholder: 'JSON payload' }),
  };
  for (const [k, input] of Object.entries(fields)) {
    form.append(el('label', { className: 'cmd-field' }, [k + ' ', input]));
  }
  form.append(el('button', {
    className: 'btn-primary',
    text: 'Submit',
    onClick: async () => {
      try {
        const cmd = await postCommand({
          source: fields.source.value,
          destination: fields.destination.value,
          category: fields.category.value,
          action: fields.action.value,
          priority: fields.priority.value,
          payload: fields.payload.value,
        });
        if (cmd) upsertCommand(cmd);
      } catch (err) { alert(err.message); }
    },
  }));
  container.append(form, statusEl);

  const queueHost   = el('div', {});
  const runningHost = el('div', {});
  const doneHost    = el('div', {});
  container.append(queueHost, runningHost, doneHost);

  function paint(state) {
    const cmds = state.commands || {};
    const hist = cmds.history || [];
    queueHost.innerHTML = '';
    runningHost.innerHTML = '';
    doneHost.innerHTML = '';
    const { card: qCard } = cmdSection(`Queue (${cmds.queueDepth ?? (cmds.queue || []).length})`, cmds.queue || [], true);
    const { card: rCard } = cmdSection('Running', cmds.running || []);
    const { card: hCard } = cmdSection('History', hist.slice(0, 50));
    queueHost.append(qCard);
    runningHost.append(rCard);
    doneHost.append(hCard);
    statusEl.textContent = `Live · queue ${cmds.queueDepth ?? 0} · emergency ${cmds.emergencyDepth ?? 0}`;
  }

  const unsub = subscribe(paint);

  // Seed from REST
  fetchCommands().then((data) => {
    if (data) {
      (data.queue || []).forEach((c) => upsertCommand(c));
      (data.running || []).forEach((c) => upsertCommand(c));
      (data.history || []).forEach((c) => upsertCommand(c));
    }
  }).catch(() => {});

  return () => unsub();
}

// ─── Capabilities tab ─────────────────────────────────────────────────────────

async function buildCapabilitiesTab(container) {
  const statusEl = el('div', { className: 'live-status', text: 'Loading…' });
  const catalogHost = el('div', { className: 'card' });
  const groupsHost  = el('div', {});
  container.append(statusEl, catalogHost, groupsHost);

  function paint(catalog, grouped) {
    catalogHost.innerHTML = '';
    catalogHost.append(el('h2', { text: 'Capability Types' }));
    const chips = el('div', { className: 'cap-chips' });
    for (const c of (catalog.capabilities || [])) {
      chips.append(el('span', { className: 'cap-chip', text: c.name }));
    }
    catalogHost.append(chips);

    groupsHost.innerHTML = '';
    const by = grouped.byCapability || {};
    const names = Object.keys(by).sort();
    if (!names.length) {
      groupsHost.append(el('div', { className: 'card' }, [
        el('p', { className: 'text-muted', text: 'No capability providers online.' }),
      ]));
      return;
    }
    for (const name of names) {
      const card = el('div', { className: 'card' });
      card.append(el('h2', { text: name }));
      const table = el('table', { className: 'log-table' });
      table.append(el('thead', {}, [
        el('tr', {}, ['Device', 'Board', 'Online', 'Firmware', 'Priority', 'Preferred', 'Owner'].map((t) => el('th', { text: t }))),
      ]));
      const tbody = el('tbody', {});
      for (const d of by[name]) {
        tbody.append(el('tr', {}, [
          el('td', { text: d.name || d.id }),
          el('td', { text: d.board || '—' }),
          el('td', {}, [el('span', { className: `badge ${d.online ? 'online' : 'offline'}`, text: d.online ? 'online' : (d.presence || 'offline') })]),
          el('td', { className: 'mono', text: d.firmware || '—' }),
          el('td', { text: String(d.priority ?? '—') }),
          el('td', { text: d.preferred ? 'yes' : 'no' }),
          el('td', { text: d.owner || d.name || '—' }),
        ]));
      }
      table.append(tbody);
      card.append(table);
      groupsHost.append(card);
    }
    statusEl.textContent = `${names.length} capabilities with providers`;
  }

  async function reload() {
    try {
      const [catalog, grouped] = await Promise.all([fetchCapabilities(), fetchDeviceCapabilities()]);
      paint(catalog, grouped);
    } catch (err) {
      statusEl.textContent = err.message;
    }
  }

  const unsub = subscribe((state) => {
    if (state.capabilityTick != null) reload();
  });
  await reload();
  return () => unsub();
}

// ─── Routing tab ──────────────────────────────────────────────────────────────

async function buildRoutingTab(container) {
  const form = el('div', { className: 'card command-form' });
  form.append(el('h2', { text: 'Route Test' }));
  const fields = {
    source:      el('input', { value: 'web-studio' }),
    destination: el('input', { value: 'any', placeholder: 'any | ian | sue | broadcast' }),
    category:    el('input', { value: 'relay' }),
    action:      el('input', { value: 'Set' }),
    priority:    el('input', { value: 'normal' }),
    payload:     el('input', { value: '{}' }),
  };
  for (const [k, input] of Object.entries(fields)) {
    form.append(el('label', { className: 'cmd-field' }, [k + ' ', input]));
  }
  const resultHost = el('div', { className: 'route-result' });
  const status = el('div', { className: 'live-status', text: 'Ready' });
  form.append(el('button', {
    className: 'btn-primary',
    text: 'POST /api/route-test',
    onClick: async () => {
      try {
        status.textContent = 'Resolving…';
        const data = await postRouteTest({
          source: fields.source.value, destination: fields.destination.value,
          category: fields.category.value, action: fields.action.value,
          priority: fields.priority.value, payload: fields.payload.value,
        });
        paintResult(data);
        status.textContent = data.ok ? 'Resolved' : 'Failed';
      } catch (err) {
        status.textContent = err.message;
      }
    },
  }));
  container.append(form, status, resultHost);

  const rulesHost = el('div', { className: 'card' });
  container.append(rulesHost);

  function paintResult(data) {
    resultHost.innerHTML = '';
    const card = el('div', { className: 'card' });
    card.append(el('h2', { text: 'Routing Decision' }));
    const rows = [
      ['Device',    data.resolvedDevice ? `${data.resolvedDevice.name || ''} (${data.resolvedDevice.id || ''})` : '—'],
      ['Board',     data.resolvedDevice?.board || '—'],
      ['Decision',  data.routingDecision || '—'],
      ['Capability',data.capability || '—'],
      ['Path',      data.path || '—'],
      ['Fallback',  data.fallbackUsed ? 'yes' : 'no'],
      ['Reason',    data.reason || '—'],
      ['OK',        data.ok ? 'yes' : 'no'],
    ];
    const table = el('table', { className: 'log-table' });
    const tbody = el('tbody', {});
    for (const [k, v] of rows) tbody.append(el('tr', {}, [el('th', { text: k }), el('td', { text: String(v) })]));
    table.append(tbody);
    card.append(table);
    resultHost.append(card);
  }

  function paintRules(data) {
    rulesHost.innerHTML = '';
    rulesHost.append(el('h2', { text: 'Routing Table' }));
    const table = el('table', { className: 'log-table' });
    table.append(el('thead', {}, [
      el('tr', {}, [el('th', { text: 'Match' }), el('th', { text: 'Policy' }), el('th', { text: 'Capability' })]),
    ]));
    const tbody = el('tbody', {});
    for (const r of (data.rules || [])) {
      tbody.append(el('tr', {}, [el('td', { text: r.match || '—' }), el('td', { text: r.policy || '—' }), el('td', { text: r.capability || '—' })]));
    }
    table.append(tbody);
    rulesHost.append(table);
    if (data.last?.decision) {
      rulesHost.append(el('p', { className: 'live-status', text: `Last: ${data.last.decision} → ${data.last.deviceId || '—'} (${data.last.reason || ''})` }));
    }
  }

  const unsub = subscribe((state) => {
    if (state.lastRoute) paintResult(state.lastRoute);
  });

  try { paintRules(await fetchRoutes()); } catch (err) { status.textContent = err.message; }

  return () => unsub();
}

// ─── Time tab ─────────────────────────────────────────────────────────────────

async function buildTimeTab(container) {
  const status = el('div', { className: 'live-status', text: 'Connecting…' });
  const clock  = el('div', { className: 'time-clock', text: '--:--:--' });
  const dateLine = el('div', { className: 'time-date', text: '————' });
  const clockCard = el('div', { className: 'card time-card' }, [clock, dateLine]);
  const detailCard = el('div', { className: 'card' });
  detailCard.append(el('h2', { text: 'Clock Details' }));
  const table = el('table', { className: 'log-table' });
  const tbody = el('tbody', {});
  table.append(tbody);
  detailCard.append(table);
  container.append(status, clockCard, detailCard);

  function paintTime(time, st) {
    if (!time) return;
    clock.textContent = time.time || '--:--:--';
    dateLine.textContent = `${time.date || ''} · ${time.dayOfWeek || ''} · ${time.timezone || 'UTC'}`;
    tbody.innerHTML = '';
    const rows = [
      ['ISO', time.iso], ['Unix Epoch', time.epoch], ['Timezone', time.timezone],
      ['DST', time.dst ? 'active' : (time.dstEnabled ? 'enabled' : 'off')],
      ['RTC Status', time.rtcStatus || st?.health],
      ['RTC Temp', time.rtcTemperature != null ? `${time.rtcTemperature} °C` : '—'],
      ['Battery', time.battery || st?.battery], ['Source', time.source],
      ['System Uptime', time.uptime],
      ['Last Sync', st?.lastSynchronisation], ['Drift', st?.driftMs != null ? `${st.driftMs} ms` : '—'],
      ['Build', time.firmwareBuild],
    ];
    for (const [k, v] of rows) {
      tbody.append(el('tr', {}, [el('th', { text: k }), el('td', { className: 'mono', text: v == null ? '—' : String(v) })]));
    }
    status.textContent = `Live · source ${time.source || '?'} · rtc ${time.rtcStatus || '?'}`;
  }

  const unsub = subscribe((state) => {
    if (state.time) paintTime(state.time, state.timeStatus);
  });

  try {
    const [time, st] = await Promise.all([fetchTime(), fetchTimeStatus()]);
    paintTime(time, st);
  } catch (err) {
    status.textContent = err.message;
  }

  return () => unsub();
}

DiagnosticsPage.title = 'Diagnostics';
