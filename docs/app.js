'use strict';
(async function () {
  const REPO = 'seajaysec/garden-op1f';
  const KNOBS = ['blue', 'tan', 'gray', 'orange'];
  const W = {DOWN: 1, DOWN_SHIFT: 2, UP: 3, TURN: 4};
  const K = {help: 0, t: 6, numbers: [45, 46, 47, 16, 17, 18, 19, 20], click: 54, press: 61};
  const NOTE = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
  const PAGES = ['play', 'rhythm', 'palette', 'gardens'];
  const FIELD_CC = {turn: {1: 0, 2: 1, 3: 2, 4: 3}, press: {66: 0, 67: 1, 68: 2, 69: 3}, pulse: {59: 0, 60: 1, 61: 2, 62: 3},
    t: {11: 0, 12: 1, 13: 2, 14: 3}, numbers: {50: 0, 51: 1, 52: 2, 21: 3, 22: 4, 23: 5, 24: 6, 25: 7},
    help: 5, synth: 7, seq: 26, left: 41, right: 42};
  const $ = id => document.getElementById(id);
  const store = {
    get(k) { try { return localStorage.getItem('garden.' + k); } catch (e) { return null; } },
    set(k, v) { try { localStorage.setItem('garden.' + k, v); } catch (e) {} },
    clear() { try { for (const k of Object.keys(localStorage)) if (k.startsWith('garden.')) localStorage.removeItem(k); } catch (e) {} },
  };
  const b64 = {
    from(bytes) { let s = ''; for (const b of bytes) s += String.fromCharCode(b); return btoa(s).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, ''); },
    to(text) { const s = atob(text.replace(/-/g, '+').replace(/_/g, '/')); return Uint8Array.from(s, c => c.charCodeAt(0)); },
  };
  const noteName = n => NOTE[((n % 12) + 12) % 12] + (Math.floor(n / 12) - 1);

  if ('serviceWorker' in navigator && location.protocol !== 'file:') {
    const had = !!navigator.serviceWorker.controller;
    navigator.serviceWorker.addEventListener('controllerchange', () => { if (had) location.reload(); });
    navigator.serviceWorker.register('sw.js').catch(() => {});
  }

  const build = await fetch('build.json', {cache: 'no-cache'}).then(r => r.json()).catch(() => ({garden: '?', pack: ''}));
  $('version').textContent = 'Garden ' + build.garden + (build.pack ? ' · pack ' + build.pack.slice(0, 8) : '');

  const hash = new URLSearchParams(location.hash.slice(1));
  const synth = new GardenSynth();
  let synthSaved = hash.get('s') || store.get('synth2');
  if (synthSaved) { try { synth.load(JSON.parse(atob(synthSaved))); } catch (e) {} }
  let bpm = +(hash.get('bpm') || store.get('bpm') || 120);
  let view = hash.get('v') || store.get('view') || 'garden';
  let seqOn = (hash.get('on') || store.get('on') || '1') === '1';
  let octave = 0;

  const M = await GardenEngine();
  const fileBytes = hash.get('g') || store.get('file');
  if (fileBytes) {
    try { const b = b64.to(fileBytes); M.HEAPU8.set(b, M._gw_file()); M._gw_file_set(b.length); } catch (e) {}
  }
  const SETTINGS = M._gw_settings_bytes();
  let fileWrites = 0;
  M._gw_mono(synth.mono() ? 1 : 0);
  M._gw_show(view === 'garden' ? 1 : 0);

  const svgText = await fetch('screen.svg', {cache: 'no-cache'}).then(r => r.text());
  $('screen').innerHTML = svgText;
  const svg = $('screen').querySelector('svg');
  const PARTS = M._gw_part_count(), TEXTS = M._gw_text_count();
  const partEl = [], partWas = [];
  for (let i = 0; i < PARTS; ++i) {
    const el = svg.getElementById ? svg.getElementById('n' + i) : svg.querySelector('#n' + i);
    partEl.push(el); partWas.push('');
    if (el) el.setAttribute('display', 'none');
  }
  const textEl = [], textWas = [];
  for (let i = 0; i < TEXTS; ++i) {
    const g = svg.querySelector('#t' + i);
    let t = null;
    if (g) {
      const colour = (g.querySelector('path') || g).getAttribute('stroke') || '#fff';
      t = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      t.setAttribute('fill', colour); t.setAttribute('font-size', '14'); t.setAttribute('font-family', 'Helvetica, Arial, sans-serif');
      g.replaceWith(t);
    }
    textEl.push(t); textWas.push('');
  }
  function paint() {
    const base = M._gw_parts() >> 2, h = M.HEAP32;
    for (let i = 0; i < PARTS; ++i) {
      const el = partEl[i]; if (!el) continue;
      const o = base + i * 7, on = h[o];
      const key = on ? h[o + 1] + ',' + h[o + 2] + ',' + h[o + 3] + ',' + h[o + 4] + ',' + h[o + 5] + ',' + h[o + 6] : '';
      if (key === partWas[i]) continue;
      partWas[i] = key;
      if (!on) { el.setAttribute('display', 'none'); continue; }
      el.removeAttribute('display');
      el.setAttribute('transform', 'matrix(' + [1, 2, 3, 4, 5, 6].map(k => h[o + k] / 1024).join(' ') + ')');
    }
    for (let i = 0; i < TEXTS; ++i) {
      const t = textEl[i]; if (!t) continue;
      const s = M.UTF8ToString(M._gw_text(i)), x = M._gw_text_x(i), y = M._gw_text_y(i), a = M._gw_text_align(i);
      const key = s + '|' + x + '|' + y + '|' + a;
      if (key === textWas[i]) continue;
      textWas[i] = key;
      t.textContent = s; t.setAttribute('x', x); t.setAttribute('y', y);
      t.setAttribute('text-anchor', ['start', 'middle', 'end'][a] || 'start');
    }
  }

  const roll = [];
  const garden = new Set(), played = new Map();
  function drain(time) {
    const n = M._gw_notes();
    for (let i = 0; i < n; ++i) {
      const w = M._gw_note(i), note = w & 255, vel = (w >> 8) & 255, on = (w >> 16) & 1;
      if (on) { synth.noteOn(note, vel, time); garden.add(note); roll.push({note, t0: time, t1: 0, src: 'g', vel}); }
      else {
        synth.noteOff(note, time); garden.delete(note);
        for (let j = roll.length - 1; j >= 0; --j) if (roll[j].src === 'g' && roll[j].note === note && !roll[j].t1) { roll[j].t1 = time; break; }
      }
    }
    if (n) { M._gw_notes_clear(); lights(); }
  }

  let nextTick = 0, ticking = false;
  const LOOK = 0.035;
  function pump() {
    if (!ticking) return;
    const now = synth.now();
    if (nextTick < now - 0.2) nextTick = now + 0.005;
    const us = Math.round(60e6 / (bpm * 96));
    while (nextTick < now + LOOK) {
      M._gw_tick(bpm * 100, us, 0);
      drain(nextTick);
      nextTick += us / 1e6;
    }
  }
  let worker = null;
  try {
    worker = new Worker(URL.createObjectURL(new Blob(['setInterval(()=>postMessage(0),8)'], {type: 'text/javascript'})));
    worker.onmessage = pump;
  } catch (e) { setInterval(pump, 8); }

  const shiftBy = {kbd: false, panel: false, field: false, armed: false};
  const shift = () => shiftBy.kbd || shiftBy.panel || shiftBy.field || shiftBy.armed;
  function shiftShown() {
    $('shift-badge').classList.toggle('on', shift());
    $('shift-key').classList.toggle('down', shiftBy.panel);
    $('shift-key').classList.toggle('latched', shiftBy.armed && !shiftBy.panel);
    lanes();
  }
  let fieldHelpUsed = false, panelUsed = false, liftPending = false, liftTimer = 0, dragging = 0;
  function used() {
    if (shiftBy.field) fieldHelpUsed = true;
    if (shiftBy.panel) panelUsed = true;
    if (shiftBy.armed) liftPending = true;
  }
  function lift() { clearTimeout(liftTimer); if (liftPending) { liftPending = false; shiftBy.armed = false; shiftShown(); } }
  function liftLater() { if (!dragging && liftPending) { clearTimeout(liftTimer); liftTimer = setTimeout(lift, 450); } }

  const log = [];
  function note(s) { log.push(s); if (log.length > 48) log.shift(); }
  function input(what, key, turn) { const r = M._gw_input(what, key, turn || 0, shift() ? 1 : 0); drain(synth.now()); return r; }
  const down = () => shift() ? W.DOWN_SHIFT : W.DOWN;

  let synthPage = 0;
  const pressed = [0, 0, 0, 0], turned = [0, 0, 0, 0];
  function turnKnob(k, steps) {
    if (!steps) return; used();
    if (pressed[k]) turned[k] = 1;
    if (view === 'garden') { input(W.TURN, k, steps * 1024); note((shift() ? 'shift+' : '') + KNOBS[k] + (steps > 0 ? ' +' : ' ') + steps); }
    else {
      const key = SYNTH_PAGES[synthPage].keys[k], d = SYNTH_PARAMS[key];
      synth.set(key, synth.p[key] + steps * (d.steps ? 1 : 2));
      if (key === 'mode') M._gw_mono(synth.mono() ? 1 : 0);
      store.set('synth2', btoa(JSON.stringify(synth.p)));
      note('synth ' + key + ' ' + synth.p[key]);
    }
    spin(k, steps); lanes(); drawSynth(); liftLater();
  }
  function pressKnob(k) {
    used(); pressed[k] = 1; turned[k] = 0;
    $$knob[k].classList.add('down');
    if (view === 'garden') input(down(), K.press + k);
  }
  function releaseKnob(k) {
    if (!pressed[k]) return;
    pressed[k] = 0; $$knob[k].classList.remove('down');
    if (view === 'garden') {
      input(W.UP, K.press + k);
      if (!turned[k]) { input(down(), K.click + k); input(W.UP, K.click + k); note((shift() ? 'shift+' : '') + 'push ' + KNOBS[k]); }
    }
    lift();
  }
  function tKey(i, isDown) {
    used();
    if (view === 'garden') { input(isDown ? down() : W.UP, K.t + i); if (isDown) note((shift() ? 'shift+' : '') + 'T' + (i + 1)); }
    else if (isDown) { synthPage = i === 0 ? 0 : i === 1 ? (shift() ? 2 : 1) : i + 1; lanes(); drawSynth(); }
    if (!isDown) lift();
  }
  function numberKey(i, isDown) {
    used();
    $$num[i].classList.toggle('down', isDown);
    if (view === 'garden') { input(isDown ? down() : W.UP, K.numbers[i]); if (isDown) note((shift() ? 'shift+' : '') + (i + 1)); }
    if (!isDown) lift();
  }
  function helpKey(isDown) {
    used();
    if (!isDown) setTimeout(lift, 0);
    if (view !== 'garden') return;
    input(isDown ? down() : W.UP, K.help);
    if (isDown) note('help');
  }

  const taken = new Map(), passed = new Map();
  function keyDown(id, k, n, vel, midi) {
    used();
    if (!ticking) return;
    if (taken.has(id) || passed.has(id)) keyUp(id);
    const r = M._gw_key(1, k, n, vel, shift() ? 1 : 0, midi ? 1 : 0);
    drain(synth.now());
    if (r) taken.set(id, {k, n});
    else { synth.noteOn(n, vel, synth.now()); passed.set(id, n); roll.push({note: n, t0: synth.now(), t1: 0, src: 'i', vel}); }
    note('key ' + noteName(n) + (r ? '' : ' (played by the synth)'));
    lights();
  }
  function keyUp(id) {
    if (taken.has(id)) { const e = taken.get(id); taken.delete(id); M._gw_key(0, e.k, e.n, 0, 0, 0); drain(synth.now()); }
    else if (passed.has(id)) {
      const n = passed.get(id); passed.delete(id); synth.noteOff(n, synth.now());
      for (let j = roll.length - 1; j >= 0; --j) if (roll[j].src === 'i' && roll[j].note === n && !roll[j].t1) { roll[j].t1 = synth.now(); break; }
    }
    lights(); lift();
  }

  function capture(el, e) { try { el.setPointerCapture(e.pointerId); } catch (x) {} }
  const $$knob = [...document.querySelectorAll('[data-knob]')];
  const knobAngle = [0, 0, 0, 0];
  function spin(k, steps) { knobAngle[k] += steps * 12; $$knob[k].querySelector('.tick').style.transform = 'rotate(' + knobAngle[k] + 'deg)'; }
  $$knob.forEach((el, k) => {
    let y0 = 0, done = 0, id = null, moved = false;
    el.addEventListener('pointerdown', e => {
      e.preventDefault(); capture(el, e); id = e.pointerId; y0 = e.clientY; done = 0; moved = false; ++dragging;
    });
    el.addEventListener('pointermove', e => {
      if (e.pointerId !== id) return;
      const steps = Math.trunc((y0 - e.clientY) / 9) - done;
      if (steps) { moved = true; done += steps; turnKnob(k, steps); }
    });
    const end = e => {
      if (e.pointerId !== id) return; id = null; --dragging;
      if (!moved) { pressKnob(k); releaseKnob(k); } else lift();
    };
    el.addEventListener('pointerup', end); el.addEventListener('pointercancel', e => { if (e.pointerId === id) { id = null; --dragging; lift(); } });
    el.addEventListener('wheel', e => { e.preventDefault(); turnKnob(k, e.deltaY < 0 ? 1 : -1); }, {passive: false});
    el.addEventListener('keydown', e => {
      if (e.key === 'ArrowUp' || e.key === 'ArrowRight') { e.preventDefault(); turnKnob(k, 1); }
      else if (e.key === 'ArrowDown' || e.key === 'ArrowLeft') { e.preventDefault(); turnKnob(k, -1); }
      else if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); e.stopPropagation(); pressKnob(k); releaseKnob(k); }
    });
  });
  function hold(el, on, off) {
    el.addEventListener('pointerdown', e => { e.preventDefault(); capture(el, e); el.classList.add('down'); on(); });
    const up = () => { if (!el.classList.contains('down')) return; el.classList.remove('down'); off(); };
    el.addEventListener('pointerup', up); el.addEventListener('pointercancel', up);
  }
  document.querySelectorAll('[data-control^="t"]').forEach(el => { const i = +el.dataset.control.slice(1); hold(el, () => tKey(i, true), () => tKey(i, false)); });
  hold(document.querySelector('[data-control="help"]'), () => helpKey(true), () => helpKey(false));
  hold($('shift-key'), () => { shiftBy.panel = true; panelUsed = false; shiftShown(); },
    () => { shiftBy.panel = false; if (!panelUsed) { shiftBy.armed = !shiftBy.armed; liftPending = false; } shiftShown(); });

  const numbers = $('numbers'), $$num = [];
  for (let i = 0; i < 8; ++i) {
    const b = document.createElement('button');
    b.className = 'num'; b.innerHTML = '<b>' + (i + 1) + '</b><small></small>';
    numbers.appendChild(b); $$num.push(b);
    b.addEventListener('pointerdown', e => { e.preventDefault(); capture(b, e); numberKey(i, true); });
    const up = () => { if (b.classList.contains('down')) numberKey(i, false); };
    b.addEventListener('pointerup', up); b.addEventListener('pointercancel', up);
  }

  const piano = $('piano'), $$key = [];
  const BLACK = [1, 3, 6, 8, 10];
  (function () {
    const whites = [];
    for (let k = 0; k < 24; ++k) if (!BLACK.includes((k + 5) % 12)) whites.push(k);
    for (let k = 0; k < 24; ++k) {
      const pc = (k + 5) % 12, el = document.createElement('div');
      if (BLACK.includes(pc)) { el.className = 'b'; const w = whites.indexOf(k - 1); el.style.left = 'calc(' + ((w + 1) / whites.length * 100) + '% - 3.2%)'; el.style.width = '6.4%'; }
      else { el.className = 'w'; if (pc === 0) el.innerHTML = '<span class="c-mark"></span>'; }
      el.dataset.k = k; piano.appendChild(el); $$key[k] = el;
    }
    const fingers = new Map();
    const keyAt = (x, y) => { const el = document.elementFromPoint(x, y); return el && el.closest && el.closest('.piano > div'); };
    const press = (pid, el) => { const k = +el.dataset.k; fingers.set(pid, k); el.classList.add('down'); keyDown('p' + k, k, 53 + k + 12 * octave, 100, false); };
    const lift = pid => { const k = fingers.get(pid); if (k === undefined) return; fingers.delete(pid); if (![...fingers.values()].includes(k)) { $$key[k].classList.remove('down'); keyUp('p' + k); } };
    piano.addEventListener('pointerdown', e => { e.preventDefault(); const el = keyAt(e.clientX, e.clientY); if (el) { capture(piano, e); press(e.pointerId, el); } });
    piano.addEventListener('pointermove', e => {
      if (!fingers.has(e.pointerId)) return;
      const el = keyAt(e.clientX, e.clientY);
      if (el && +el.dataset.k !== fingers.get(e.pointerId)) { lift(e.pointerId); press(e.pointerId, el); }
    });
    piano.addEventListener('pointerup', e => lift(e.pointerId)); piano.addEventListener('pointercancel', e => lift(e.pointerId));
  })();
  function octaveMarks() { $$key.forEach((el, k) => { const m = el.querySelector('.c-mark'); if (m) m.textContent = noteName(53 + k + 12 * octave); }); }
  function setOctave(o) { octave = Math.max(-3, Math.min(3, o)); octaveMarks(); lights(); }
  $('oct-down').onclick = () => setOctave(octave - 1); $('oct-up').onclick = () => setOctave(octave + 1);
  octaveMarks();
  function lights() {
    for (let k = 0; k < 24; ++k) $$key[k].classList.toggle('lit', garden.has(53 + k + 12 * octave));
  }

  const PIANO = 'awsedftgyhujkolp;';
  const kbdHeld = new Set();
  window.addEventListener('keydown', e => {
    if (e.target.closest && e.target.closest('textarea,input,dialog')) return;
    if (e.key === 'Shift') { shiftBy.kbd = true; shiftShown(); return; }
    if (e.repeat || e.metaKey || e.ctrlKey || e.altKey) return;
    const k = e.key.toLowerCase();
    if (/^[1-8]$/.test(k)) { numberKey(+k - 1, true); return; }
    if ('zxcv'.includes(k) && k) { tKey('zxcv'.indexOf(k), true); return; }
    if (k === '/' || k === '?') { helpKey(true); return; }
    if (e.key === 'ArrowLeft' && !e.target.closest('.knob')) { setOctave(octave - 1); return; }
    if (e.key === 'ArrowRight' && !e.target.closest('.knob')) { setOctave(octave + 1); return; }
    const i = PIANO.indexOf(k);
    if (i >= 0 && !kbdHeld.has(k)) { kbdHeld.add(k); const key = 7 + i; $$key[key].classList.add('down'); keyDown('c' + k, key, 53 + key + 12 * octave, 100, false); }
  });
  window.addEventListener('keyup', e => {
    if (e.key === 'Shift') { shiftBy.kbd = false; shiftShown(); return; }
    const k = e.key.toLowerCase();
    if (/^[1-8]$/.test(k)) { numberKey(+k - 1, false); return; }
    if ('zxcv'.includes(k) && k) { tKey('zxcv'.indexOf(k), false); return; }
    if (k === '/' || k === '?') { helpKey(false); return; }
    const i = PIANO.indexOf(k);
    if (i >= 0 && kbdHeld.has(k)) { kbdHeld.delete(k); $$key[7 + i].classList.remove('down'); keyUp('c' + k); }
  });
  window.addEventListener('blur', () => { shiftBy.kbd = false; shiftShown(); });

  const fieldPress = [0, 0, 0, 0];
  function midiMessage(port, e) {
    const [st, d1, d2] = e.data, type = st & 0xf0;
    const field = /op-?1/i.test(port.name || '');
    if (type === 0x90 || type === 0x80) {
      const on = type === 0x90 && d2 > 0, id = 'm' + port.id + ':' + d1, k = d1 - 53;
      if (on) {
        if (!field && seqOn && (k < 0 || k > 23)) { hint("That note is outside Garden's 24 keys (F3 to E5): the unit drops it too."); return; }
        keyDown(id, k, d1, d2, !field);
      } else keyUp(id);
      return;
    }
    if (type !== 0xb0 || !field) return;
    const v = d2, isDown = v >= 64;
    if (d1 in FIELD_CC.turn) { const d = v === 64 ? 0 : v < 64 ? v : v - 128; turnKnob(FIELD_CC.turn[d1], d); return; }
    if (d1 in FIELD_CC.press) { const k = FIELD_CC.press[d1]; if (isDown) { fieldPress[k] = performance.now(); pressKnob(k); } else releaseKnob(k); return; }
    if (d1 in FIELD_CC.pulse) { const k = FIELD_CC.pulse[d1]; if (isDown && performance.now() - fieldPress[k] > 1500 && !pressed[k]) { pressKnob(k); releaseKnob(k); } return; }
    if (d1 in FIELD_CC.t) { tKey(FIELD_CC.t[d1], isDown); return; }
    if (d1 in FIELD_CC.numbers) { numberKey(FIELD_CC.numbers[d1], isDown); return; }
    if (d1 === FIELD_CC.help) {
      if (isDown) { shiftBy.field = true; fieldHelpUsed = false; shiftShown(); }
      else { shiftBy.field = false; shiftShown(); if (!fieldHelpUsed) { helpKey(true); helpKey(false); } }
      return;
    }
    if (d1 === FIELD_CC.synth && isDown) { setView(view === 'synth' ? 'garden' : 'synth'); return; }
    if (d1 === FIELD_CC.seq && isDown) { setSeq(!seqOn); setView('garden'); return; }
    if (d1 === FIELD_CC.left && isDown) { setOctave(octave - 1); return; }
    if (d1 === FIELD_CC.right && isDown) { setOctave(octave + 1); }
  }
  async function midi() {
    if (!navigator.requestMIDIAccess) { $('midi').textContent = 'No MIDI in this browser: play on screen, or open in Chrome to use a Field or a keyboard.'; return; }
    try {
      const access = await navigator.requestMIDIAccess();
      const listen = () => {
        const names = [];
        for (const port of access.inputs.values()) { port.onmidimessage = e => midiMessage(port, e); names.push(port.name); }
        const field = names.some(n => /op-?1/i.test(n));
        $('midi').textContent = names.length ? 'MIDI: ' + names.join(', ') + (field ? ' · Field help key = shift, a tap = help' : '') : 'MIDI: nothing connected';
        $('midi').classList.toggle('ok', names.length > 0);
      };
      access.onstatechange = listen; listen();
    } catch (e) { $('midi').textContent = 'MIDI not allowed: play on screen.'; }
  }

  function setView(v) {
    view = v; store.set('view', v);
    document.querySelectorAll('.view').forEach(b => b.classList.toggle('on', b.dataset.view === v));
    $('screen').hidden = v !== 'garden'; $('synth-screen').hidden = v !== 'synth';
    M._gw_show(v === 'garden' ? 1 : 0);
    hint(v === 'garden' ? '' : 'T1 sound · T2 envelope · shift+T2 play · T3 fx · T4 lfo');
    lanes(); drawSynth();
  }
  function setSeq(on) {
    seqOn = on; store.set('on', on ? '1' : '0');
    if (!on) { for (const id of [...taken.keys()]) keyUp(id); }
    M._gw_run(on ? 1 : 0); drain(synth.now());
    $('seq').classList.toggle('on', on); $('seq').setAttribute('aria-pressed', String(on));
    $('seq-word').textContent = on ? 'on' : 'off';
    note('sequencer ' + (on ? 'on' : 'off'));
  }
  document.querySelectorAll('.view').forEach(b => b.onclick = () => setView(b.dataset.view));
  $('seq').onclick = () => setSeq(!seqOn);
  $('bpm').value = bpm; $('bpm-out').textContent = bpm;
  $('bpm').oninput = () => { bpm = +$('bpm').value; $('bpm-out').textContent = bpm; synth.tempo(bpm); store.set('bpm', bpm); };
  synth.tempo(bpm);

  let hintTimer = 0;
  function hint(s) { $('hint').textContent = s; clearTimeout(hintTimer); if (s && view === 'garden') hintTimer = setTimeout(() => { $('hint').textContent = ''; }, 3500); }

  let S = null;
  const pct = v => v + '%';
  function knobValue(k) {
    if (!S) return '';
    const g = S.g, n = S.names, sh = shift();
    if (S.help) return '';
    if (S.page === 0) {
      if (k === 0) return sh ? n.echo : String(g.cascade);
      if (k === 1) return sh ? n.rate : n.style;
      if (k === 2) { const t = S.turn[2]; return t === 'phrase' ? n.phrase : t === 'arp order' ? n.order : n.voicing; }
      return sh ? n.pulse : n.boost;
    }
    if (S.page === 1) return [g.loop ? 'loop on' : 'loop off', pct(g.length), pct(g.swing), pct(g.human)][k];
    if (S.page === 3 && !sh) return k === 0 ? 'garden ' + (g.pad_sel + 1) : '';
    if (S.page === 3 && sh) return k === 0 ? n.walk_len : k === 1 ? n.walk_order : '';
    return '';
  }
  function lanes() {
    const el = $('lanes'); let html = '';
    for (let k = 0; k < 4; ++k) {
      let turn = '', push = '', val = '';
      if (view === 'garden') { if (S) { turn = S.turn[k]; push = S.push[k]; val = knobValue(k); } }
      else { const key = SYNTH_PAGES[synthPage].keys[k], d = SYNTH_PARAMS[key]; turn = d.label; val = d.text ? d.text(synth.p[key]) : String(synth.p[key]); }
      html += '<div class="lane ' + KNOBS[k] + (turn ? '' : ' blank') + '"><span class="sw"></span><div><div class="turn">' + (turn || '—') +
        '</div>' + (val ? '<div class="val">' + val + '</div>' : '') + (push ? '<div class="push">' + push + '</div>' : '') + '</div></div>';
    }
    if (el.innerHTML !== html) el.innerHTML = html;
  }
  function explain() {
    S = JSON.parse(M.UTF8ToString(M._gw_state(shift() ? 1 : 0)));
    lanes();
    const g = S.g, n = S.names, v = S.view;
    const kind = n.kinds[v.playing ? v.kind : g.quality] || '';
    const ext = n.pollen.filter((_, e) => ((v.playing ? v.mask : g.extensions) >> e) & 1).join(' ');
    const word = (v.playing ? NOTE[v.root % 12] + ' ' : '') + kind + (ext ? ' ' + ext : '');
    $('chord-word').textContent = word || '—'; $('chord-word').classList.toggle('quiet', !v.playing);
    let chips = '';
    for (let i = 0; i < 4; ++i) { const e = g.extsets[g.pf * 4 + i]; chips += '<span class="chip' + ((g.extensions >> e) & 1 ? ' on' : '') + '">' + n.pollen[e] + '</span>'; }
    for (const x of v.notes) chips += '<span class="chip note">' + noteName(x) + '</span>';
    if ($('chips').innerHTML !== chips) $('chips').innerHTML = chips;
    for (let i = 0; i < 8; ++i) {
      let label = '', live = false;
      if (S.page === 3 && !S.help) { const used = g.pad_used[i] && g.seed_q[i] < 12; label = used ? NOTE[g.pad_root[i] % 12] + ' ' + n.kinds[g.seed_q[i]] : 'empty'; live = used; }
      else if (i < 4) { const q = g.kinds[g.fl * 4 + i]; label = n.kinds[q]; live = q === g.quality; }
      else { const e = g.extsets[g.pf * 4 + i - 4]; label = n.pollen[e]; live = !!((g.extensions >> e) & 1); }
      const s = $$num[i].querySelector('small');
      if (s.textContent !== label) s.textContent = label;
      $$num[i].classList.toggle('live', live);
    }
    const rows = [
      ['page', S.help ? 'help' : PAGES[S.page]], ['sequencer', S.on ? 'on' : 'off'], ['synth plays', synth.mono() ? 'mono' : 'poly'],
      ['style', n.style], ['rate', n.rate], ['cascade', g.cascade], ['echo', n.echo], ['voicing', n.voicing], ['arp order', n.order],
      ['phrase', n.phrase], ['boost', n.boost], ['boost pulse', n.pulse], ['voice leading', g.lead ? 'on' : 'off'], ['hold', g.hold ? 'on' : 'off'],
      ['loop', g.loop ? 'on' : 'off'], ['length', pct(g.length)], ['swing', pct(g.swing)], ['human', pct(g.human)],
      ['kind', n.kinds[g.quality]], ['chord bank', g.fl ? 'b' : 'a'], ['extension bank', g.pf ? 'b' : 'a'],
      ['extensions', n.pollen.filter((_, e) => (g.extensions >> e) & 1).join(', ') || 'none'],
      ['chord bank a', g.kinds.slice(0, 4).map(q => n.kinds[q]).join(', ')], ['chord bank b', g.kinds.slice(4).map(q => n.kinds[q]).join(', ')],
      ['extension bank a', g.extsets.slice(0, 4).map(e => n.pollen[e]).join(', ')], ['extension bank b', g.extsets.slice(4).map(e => n.pollen[e]).join(', ')],
      ['garden', g.pad_sel + 1], ['extension square', g.square + 1], ['walk length', n.walk_len], ['walk order', n.walk_order],
    ];
    const html = rows.map(r => '<dt>' + r[0] + '</dt><dd>' + r[1] + '</dd>').join('');
    if ($('settings').innerHTML !== html) $('settings').innerHTML = html;
  }

  const rc = $('roll'), rx = rc.getContext('2d');
  function drawRoll() {
    const now = synth.now(), span = 2 * 4 * 60 / bpm, t0 = now - span, W2 = rc.width, H = rc.height;
    while (roll.length && roll[0].t1 && roll[0].t1 < t0) roll.shift();
    if (roll.length > 600) roll.splice(0, roll.length - 600);
    rx.fillStyle = '#141618'; rx.fillRect(0, 0, W2, H);
    let lo = 127, hi = 0;
    for (const r of roll) { lo = Math.min(lo, r.note); hi = Math.max(hi, r.note); }
    if (lo > hi) { lo = 48; hi = 72; }
    lo = Math.min(lo, hi - 12) - 2; hi += 2;
    const beat = 60 / bpm, first = Math.ceil(t0 / beat) * beat;
    for (let t = first, i = Math.round(first / beat); t < now; t += beat, ++i) {
      const x = (t - t0) / span * W2;
      rx.fillStyle = i % 4 === 0 ? '#3a3f44' : '#24282b'; rx.fillRect(x, 0, i % 4 === 0 ? 2 : 1, H);
    }
    const rowH = Math.max(3, Math.min(14, (H - 8) / (hi - lo + 1)));
    for (const r of roll) {
      const a = Math.max(r.t0, t0), b = r.t1 || now; if (b < t0) continue;
      const x = (a - t0) / span * W2, w = Math.max(3, (b - a) / span * W2);
      const y = H - 4 - (r.note - lo + 1) * (H - 8) / (hi - lo + 1);
      if (r.src === 'g') { rx.fillStyle = 'rgba(229,40,23,' + (0.45 + 0.55 * r.vel / 127) + ')'; rx.fillRect(x, y, w, rowH - 1); }
      else { rx.strokeStyle = '#cdd0c2'; rx.lineWidth = 2; rx.strokeRect(x + 1, y + 1, w - 2, rowH - 3); }
    }
  }

  const sc = $('synth-screen'), sx = sc.getContext('2d');
  const KCOL = ['#3C6087', '#8A653C', '#CDD0C2', '#E52817'], KINK = ['#7aa6d2', '#c9a273', '#e2e4da', '#ff6a57'];
  function drawSynth() {
    if (view !== 'synth') return;
    const P = SYNTH_PAGES[synthPage], p = synth.p;
    sx.fillStyle = '#000'; sx.fillRect(0, 0, 960, 440);
    sx.fillStyle = '#fff'; sx.font = '700 44px system-ui, sans-serif'; sx.textBaseline = 'top'; sx.fillText(P.name, 32, 24);
    if (P.name === 'envelope') {
      const a = envTime(p.attack), d = envTime(p.decay), r = envTime(p.release), s = p.sustain / 100, tot = a + d + r + 0.6;
      const x0 = 40, w = 880, y0 = 380, h = 250, X = t => x0 + t / tot * w;
      sx.lineWidth = 6; sx.lineJoin = 'round';
      const pts = [[0, 0], [a, 1], [a + d, s], [a + d + 0.6, s], [tot, 0]];
      for (let i = 0; i < 4; ++i) { sx.strokeStyle = KCOL[i]; sx.beginPath(); sx.moveTo(X(pts[i][0]), y0 - pts[i][1] * h); sx.lineTo(X(pts[i + 1][0]), y0 - pts[i + 1][1] * h); sx.stroke(); }
      return;
    }
    for (let k = 0; k < 4; ++k) {
      const key = P.keys[k], d = SYNTH_PARAMS[key], x = 32 + k * 228, v = p[key];
      sx.fillStyle = KCOL[k];
      if (d.steps) {
        sx.fillStyle = KINK[k]; sx.font = '700 40px system-ui, sans-serif'; sx.fillText(d.text(v), x, 210); sx.fillStyle = KCOL[k];
        for (let i = 0; i < d.steps; ++i) { sx.globalAlpha = i === v ? 1 : 0.3; sx.fillRect(x + i * 30, 300, 22, 22); }
        sx.globalAlpha = 1;
      } else {
        const f = v / d.max; sx.globalAlpha = 0.25; sx.fillRect(x, 130, 60, 230); sx.globalAlpha = 1; sx.fillRect(x, 130 + 230 * (1 - f), 60, 230 * f);
      }
      sx.fillStyle = '#a3a7a0'; sx.font = '400 26px system-ui, sans-serif'; sx.fillText(d.label, x, 384);
    }
  }

  let frames = 0;
  function frame(t) {
    M._gw_frame(Math.floor(t) >>> 0, shift() ? 1 : 0);
    drain(synth.now());
    if (view === 'garden') paint();
    if ((frames++ & 3) === 0) explain();
    drawRoll();
    const w = M._gw_file_writes();
    if (w !== fileWrites) {
      fileWrites = w;
      const len = M._gw_file_len();
      if (len > 0) store.set('file', b64.from(M.HEAPU8.subarray(M._gw_file(), M._gw_file() + len)));
    }
    requestAnimationFrame(frame);
  }

  function stateLink() {
    const g = b64.from(M.HEAPU8.subarray(M._gw_g(), M._gw_g() + SETTINGS));
    const q = new URLSearchParams({g, s: btoa(JSON.stringify(synth.p)), bpm: String(bpm), on: seqOn ? '1' : '0', v: view});
    return location.origin + location.pathname + '#' + q.toString();
  }
  function toast(s) { $('toast').textContent = s; setTimeout(() => { if ($('toast').textContent === s) $('toast').textContent = ''; }, 4000); }
  $('share').onclick = async () => {
    const link = stateLink();
    try { await navigator.clipboard.writeText(link); toast('Link copied.'); } catch (e) { prompt('Copy this link', link); }
  };
  $('reset').onclick = () => { store.clear(); location.replace(location.pathname); };
  $('report').onclick = () => $('report-dlg').showModal();
  $('report-form').addEventListener('submit', e => {
    if (e.submitter && e.submitter.value !== 'open') return;
    const n = S ? S.names : {}, g = S ? S.g : {};
    const did = $('r-did').value.trim(), web = $('r-web').value.trim(), unit = $('r-unit').value.trim();
    const lines = [
      '**Garden ' + build.garden + '** · pack `' + (build.pack || '').slice(0, 12) + '` · ' + navigator.userAgent.replace(/[()]/g, ''),
      '', '**What I played**', did || '_not said_', '', '**On the web**', web || '_not said_', '', '**On the unit**', unit || '_not said_', '',
      '**State**', '', '[Play this state on the web](' + stateLink() + ')', '',
      '- page ' + (S ? (S.help ? 'help' : PAGES[S.page]) : '?') + ', sequencer ' + (seqOn ? 'on' : 'off') + ', tempo ' + bpm,
      '- ' + [n.style, n.rate, 'cascade ' + g.cascade, n.echo, n.voicing, n.order, n.phrase, n.boost, n.pulse].join(', '),
      '- hold ' + (g.hold ? 'on' : 'off') + ', voice leading ' + (g.lead ? 'on' : 'off') + ', loop ' + (g.loop ? 'on' : 'off') +
        ', length ' + g.length + '%, swing ' + g.swing + '%, human ' + g.human + '%',
      '- synth ' + SYNTH_PARAMS.mode.text(synth.p.mode) + ', portamento ' + SYNTH_PARAMS.portamento.text(synth.p.portamento),
      '- garden.bin `' + b64.from(M.HEAPU8.subarray(M._gw_g(), M._gw_g() + SETTINGS)) + '`',
      '', '<details><summary>Last inputs</summary>', '', log.slice(-40).join(' · '), '', '</details>',
    ];
    let body = lines.join('\n');
    if (body.length > 6500) body = body.slice(0, 6500);
    const title = 'Web and unit differ: ' + (did || 'Garden').slice(0, 60);
    window.open('https://github.com/' + REPO + '/issues/new?title=' + encodeURIComponent(title) + '&body=' + encodeURIComponent(body), '_blank', 'noopener');
  });

  async function begin() {
    $('start').hidden = true;
    try { if (navigator.audioSession) navigator.audioSession.type = 'playback'; } catch (e) {}
    await synth.start();
    try { Tone.getContext().lookAhead = 0.02; } catch (e) {}
    nextTick = synth.now() + 0.05; ticking = true;
    M._gw_run(seqOn ? 1 : 0);
    setSeq(seqOn);
  }
  $('start-btn').onclick = begin;
  setView(view);
  midi();
  requestAnimationFrame(frame);
  window.GARDEN = {M, synth, state: () => S, link: stateLink, input, keyDown, keyUp, begin, midi: (name, bytes) => midiMessage({name, id: name}, {data: bytes})};
})();
