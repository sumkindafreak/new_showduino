/* Showduino Studio — UX pass 1
 *
 * This layer deliberately changes vocabulary and interaction affordances only.
 * It does not alter SHDO v2 data, runtime authority, safety behaviour or the P4.
 *
 * Goal: let attraction users think in terms of what they want to happen
 * (sound, lighting, prop, pixels) before they ever need to think about hardware.
 */
(function () {
  'use strict';

  const STYLE_ID = 'showduino-ux-pass1-style';
  const ENHANCED = 'data-showduino-ux-pass1';

  function injectStyles() {
    if (document.getElementById(STYLE_ID)) return;

    const style = document.createElement('style');
    style.id = STYLE_ID;
    style.textContent = `
      /* Friendly cue chooser */
      .sm-picker-sheet[data-showduino-ux-pass1="true"] .sm-picker-head {
        align-items:flex-start;
      }

      .sm-ux-intro {
        margin:-.18rem 0 .72rem;
        color:#7f929c;
        font-size:.64rem;
        line-height:1.45;
      }

      .sm-ux-primary-grid {
        display:grid;
        grid-template-columns:repeat(2,minmax(0,1fr));
        gap:.5rem;
      }

      .sm-ux-choice {
        min-height:92px;
        padding:.72rem;
        border:1px solid #2a404b;
        border-radius:12px;
        background:#0b151a;
        color:#edf4f6;
        text-align:left;
      }

      .sm-ux-choice:active {
        border-color:rgba(0,255,200,.45);
        transform:scale(.992);
      }

      .sm-ux-choice b {
        display:block;
        color:#00ffc8;
        font-size:1.05rem;
        font-weight:650;
      }

      .sm-ux-choice strong {
        display:block;
        margin-top:.38rem;
        font-size:.77rem;
      }

      .sm-ux-choice span {
        display:block;
        margin-top:.16rem;
        color:#7f929c;
        font-size:.57rem;
        line-height:1.35;
      }

      .sm-ux-secondary {
        margin-top:.75rem;
        padding-top:.7rem;
        border-top:1px solid #1b2a32;
      }

      .sm-ux-secondary-title {
        display:block;
        margin-bottom:.46rem;
        color:#60747e;
        font:850 .49rem/1 ui-monospace,SFMono-Regular,Menlo,monospace;
        letter-spacing:.08em;
      }

      .sm-ux-secondary-row {
        display:grid;
        grid-template-columns:repeat(2,minmax(0,1fr));
        gap:.45rem;
      }

      .sm-ux-secondary button {
        min-height:48px;
        padding:.55rem .65rem;
        border:1px solid #263943;
        border-radius:10px;
        background:#0a1217;
        color:#cbd7db;
        text-align:left;
        font-size:.62rem;
        font-weight:800;
      }

      .sm-ux-hardware-hint {
        display:block;
        margin-top:.16rem;
        color:#617681;
        font-size:.5rem;
        font-weight:650;
      }

      /* Desktop: make the mental model obvious before users meet the DAW. */
      .studio-v4 .studio-ux-guide {
        margin:0 0 1rem;
        padding:.9rem 1rem;
        border:1px solid rgba(0,255,200,.2);
        border-radius:12px;
        background:linear-gradient(135deg,rgba(0,255,200,.055),rgba(0,0,0,.12));
      }

      .studio-v4 .studio-ux-guide strong {
        display:block;
        color:#e9f6f3;
        font-size:.82rem;
      }

      .studio-v4 .studio-ux-guide p {
        margin:.3rem 0 0;
        color:#7f929c;
        font-size:.68rem;
        line-height:1.45;
      }

      .studio-v4 .studio-ux-guide-actions {
        display:flex;
        flex-wrap:wrap;
        gap:.45rem;
        margin-top:.65rem;
      }

      .studio-v4 .studio-ux-guide-actions button {
        min-height:38px;
        padding:.48rem .68rem;
        border:1px solid #2a414c;
        border-radius:9px;
        background:#0b1419;
        color:#dce8eb;
        font-size:.62rem;
        font-weight:800;
      }

      .studio-v4 .studio-ux-guide-actions button:first-child {
        border-color:rgba(0,255,200,.4);
        background:rgba(0,255,200,.07);
        color:#baffed;
      }

      @media (max-width:760px) {
        .studio-ux-guide { display:none !important; }
      }
    `;
    document.head.appendChild(style);
  }

  function clickExistingType(type) {
    const button = document.querySelector(`.sm-picker [data-sm-add="${type}"]`);
    if (!button) return false;
    button.click();
    return true;
  }

  function makeChoice(icon, title, description, onClick) {
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'sm-ux-choice';
    button.innerHTML = `<b>${icon}</b><strong>${title}</strong><span>${description}</span>`;
    button.addEventListener('click', onClick);
    return button;
  }

  function enhanceMobilePicker() {
    const sheet = document.querySelector('.sm-picker-sheet');
    if (!sheet || sheet.getAttribute(ENHANCED) === 'true') return;

    const oldGrid = sheet.querySelector('.sm-type-grid');
    if (!oldGrid) return;

    sheet.setAttribute(ENHANCED, 'true');

    const intro = document.createElement('p');
    intro.className = 'sm-ux-intro';
    intro.textContent = 'Choose what you want the attraction to do. Showduino can worry about the hardware underneath.';

    const primary = document.createElement('div');
    primary.className = 'sm-ux-primary-grid';

    primary.appendChild(makeChoice('♪', 'Sound', 'Play a scream, ambience, music or other audio.', () => {
      clickExistingType('audio');
    }));

    primary.appendChild(makeChoice('☀', 'Lighting', 'Turn a light on, dim it, pulse it or run an LED effect.', () => {
      showLightingChoices(sheet);
    }));

    primary.appendChild(makeChoice('⚙', 'Prop / Switch', 'Fire a prop, solenoid, contactor or switched effect.', () => {
      clickExistingType('relay');
    }));

    primary.appendChild(makeChoice('✦', 'Pixels / LED FX', 'Run colour and animation effects on addressable LEDs.', () => {
      clickExistingType('pixel');
    }));

    const secondary = document.createElement('div');
    secondary.className = 'sm-ux-secondary';
    secondary.innerHTML = '<span class="sm-ux-secondary-title">MORE ACTIONS</span>';

    const secondaryRow = document.createElement('div');
    secondaryRow.className = 'sm-ux-secondary-row';

    const trigger = document.createElement('button');
    trigger.type = 'button';
    trigger.innerHTML = 'Trigger / Event<span class="sm-ux-hardware-hint">Logical event</span>';
    trigger.addEventListener('click', () => clickExistingType('trigger'));

    const other = document.createElement('button');
    other.type = 'button';
    other.innerHTML = 'Other Effect<span class="sm-ux-hardware-hint">Fog, air, motor, servo…</span>';
    other.addEventListener('click', () => clickExistingType('fx'));

    secondaryRow.append(trigger, other);
    secondary.appendChild(secondaryRow);

    oldGrid.hidden = true;
    oldGrid.before(intro, primary, secondary);
  }

  function showLightingChoices(sheet) {
    let panel = sheet.querySelector('.sm-ux-lighting-panel');
    if (panel) {
      panel.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
      return;
    }

    panel = document.createElement('div');
    panel.className = 'sm-ux-secondary sm-ux-lighting-panel';
    panel.innerHTML = '<span class="sm-ux-secondary-title">WHAT KIND OF LIGHT?</span>';

    const row = document.createElement('div');
    row.className = 'sm-ux-secondary-row';

    const simple = document.createElement('button');
    simple.type = 'button';
    simple.innerHTML = 'Simple / Dimmed Light<span class="sm-ux-hardware-hint">12/24V output · MOSFET underneath</span>';
    simple.addEventListener('click', () => clickExistingType('mosfet'));

    const pixels = document.createElement('button');
    pixels.type = 'button';
    pixels.innerHTML = 'Addressable LEDs<span class="sm-ux-hardware-hint">Pixels / animated lighting</span>';
    pixels.addEventListener('click', () => clickExistingType('pixel'));

    row.append(simple, pixels);
    panel.appendChild(row);

    const oldGrid = sheet.querySelector('.sm-type-grid');
    (oldGrid?.nextElementSibling || sheet.lastElementChild)?.before?.(panel);
    panel.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
  }

  function friendlyEditorLabels() {
    const editor = document.querySelector('.sm-editor:not([hidden])');
    if (!editor) return;

    editor.querySelectorAll('h3').forEach((heading) => {
      if (heading.textContent.trim() === 'Relay') heading.textContent = 'Prop / switched output';
      if (heading.textContent.trim() === 'Powered output') heading.textContent = 'Lighting / powered output';
      if (heading.textContent.trim() === 'Pixels') heading.textContent = 'Pixels / LED effect';
    });

    editor.querySelectorAll('label').forEach((label) => {
      const text = label.textContent.trim();
      if (text === 'DEVICE') label.textContent = 'WHICH DEVICE?';
      if (text === 'OUTPUT') label.textContent = 'WHICH OUTPUT?';
      if (text.startsWith('START TIME')) label.textContent = 'WHEN SHOULD THIS HAPPEN? · TYPE 12.5 OR 0:12.500';
      if (text === 'CUE NAME') label.textContent = 'WHAT SHOULD WE CALL THIS?';
    });
  }

  function addDesktopGuide() {
    if (window.matchMedia('(max-width:760px)').matches) return;

    const workspace = document.querySelector('.workspace');
    if (!workspace || workspace.querySelector('.studio-ux-guide')) return;

    const guide = document.createElement('section');
    guide.className = 'studio-ux-guide';
    guide.innerHTML = `
      <strong>Build the show by deciding what happens next.</strong>
      <p>You do not need to think in relays, MOSFETs or board numbers first. Start with the effect — sound, lighting, prop or pixels — then choose the device that performs it.</p>
      <div class="studio-ux-guide-actions">
        <button type="button" data-ux-open-timeline>Build show</button>
        <button type="button" data-ux-open-audio>Add sound</button>
        <button type="button" data-ux-open-nodes>See devices</button>
      </div>
    `;

    workspace.prepend(guide);

    guide.querySelector('[data-ux-open-timeline]')?.addEventListener('click', () => window.loadPanel?.('timeline-editor'));
    guide.querySelector('[data-ux-open-audio]')?.addEventListener('click', () => window.loadPanel?.('audio-manager'));
    guide.querySelector('[data-ux-open-nodes]')?.addEventListener('click', () => window.loadPanel?.('devices'));
  }

  function relabelDesktopNavigation() {
    document.querySelectorAll('.sidebar-nav li').forEach((item) => {
      const panel = item.getAttribute('data-panel');
      const label = item.querySelector('span:last-child');
      if (!label) return;

      if (panel === 'timeline-editor') label.textContent = 'Build Show';
      if (panel === 'playback') label.textContent = 'Lighting FX';
      if (panel === 'audio-manager') label.textContent = 'Sounds';
      if (panel === 'devices') label.textContent = 'Devices';
      if (panel === 'connect') label.textContent = 'Send to Showduino';
    });
  }

  function refresh() {
    enhanceMobilePicker();
    friendlyEditorLabels();
    relabelDesktopNavigation();
    addDesktopGuide();
  }

  function boot() {
    injectStyles();
    refresh();

    const observer = new MutationObserver(() => {
      window.requestAnimationFrame(refresh);
    });

    observer.observe(document.body, { childList: true, subtree: true, attributes: true, attributeFilter: ['hidden', 'class'] });

    window.addEventListener('resize', refresh);
    window.addEventListener('showduino:v4-saved', refresh);
    window.addEventListener('showduino:project-saved', refresh);
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', boot, { once: true });
  } else {
    boot();
  }
})();
