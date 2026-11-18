/* One Studio editor on phone, tablet and desktop. Touch adapts the same timeline. */
(function () {
  'use strict';
  function install() {
    document.body.classList.remove('studio-mobile-native', 'studio-mobile-advanced');
    document.getElementById('studio-mobile-app')?.remove();
    const css = document.createElement('style');
    css.id = 'studio-responsive-editor-style';
    css.textContent = `
      @media(max-width:900px) {
        :root { --studio-v4-topbar:112px; }
        .studio-v4 .top-bar { display:flex!important; flex-wrap:wrap; height:auto; min-height:112px; align-content:center; }
        .studio-v4 .studio-project-actions { display:flex!important; gap:6px; flex-wrap:wrap; }
        .studio-v4 .transport-controls { display:flex!important; flex-wrap:wrap; }
        .studio-v4 .transport-controls button { display:inline-flex!important; }
        .studio-v4 .timeline-main { display:grid!important; grid-template-columns:130px minmax(0,1fr); grid-template-rows:minmax(220px,1fr) minmax(140px,32%); overflow:auto!important; }
        .studio-v4 .tl-left { width:130px!important; min-width:0!important; }
        .studio-v4 .tl-right { min-width:0; min-height:220px; }
        .studio-v4 .tl-inspector { grid-column:1 / -1; width:100%!important; min-width:0!important; max-height:240px; }
        .studio-v4 .timeline-toolbar { max-height:140px; overflow:auto; }
        .studio-v4 button { min-height:36px; }
      }
      .studio-v4 .tl-clip { touch-action:none; }
    `;
    document.head.appendChild(css);
    let active = null;
    const mouse = (target, type, event) => target.dispatchEvent(new MouseEvent(type, {
      bubbles:true, cancelable:true, view:window, button:0,
      buttons:type === 'mouseup' ? 0 : 1, clientX:event.clientX, clientY:event.clientY,
      shiftKey:event.shiftKey, ctrlKey:event.ctrlKey, altKey:event.altKey
    }));
    document.addEventListener('pointerdown', event => {
      if (event.pointerType !== 'touch' || event.isPrimary === false || active) return;
      if (!event.target.closest?.('.tl-clip')) return;
      event.preventDefault();
      active = {id:event.pointerId, target:event.target};
      event.target.setPointerCapture?.(event.pointerId);
      mouse(event.target, 'mousedown', event);
    }, {passive:false});
    document.addEventListener('pointermove', event => {
      if (active?.id !== event.pointerId) return;
      event.preventDefault();
      mouse(document, 'mousemove', event);
    }, {passive:false});
    const finish = event => {
      if (active?.id !== event.pointerId) return;
      const target = active.target;
      active = null;
      mouse(document, 'mouseup', event);
      if (target.hasPointerCapture?.(event.pointerId)) target.releasePointerCapture(event.pointerId);
    };
    document.addEventListener('pointerup', finish);
    document.addEventListener('pointercancel', finish);
    window.ShowduinoResponsiveEditor = {finish};
  }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded',install,{once:true});
  else install();
})();
