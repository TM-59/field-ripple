// Collisions and breathers (src/pair_web.c over src/pair.c and the unchanged src/field.c).
// Every number and every drawn curve comes from C through the _pw_ entry points; JavaScript only draws and holds the controls.
const $ = id => document.getElementById(id);
const V = {time: 0, start: 1, end: 2, energy: 3, energyTheory: 4, momentum: 5, fronts: 6, left: 7, right: 8, shift: 9,
  shiftTheory: 10, omega: 11, omegaTheory: 12, gap: 13, rows: 14, done: 15, mode: 16, param: 17, speedIn: 18, speedOut: 19,
  separation: 20, separationTheory: 21};
const PRESETS = [[0.1, 0.5, 0.9], [0.2, 0.5, 0.9]];
const RANGE = [[0.05, 0.95], [0.1, 0.95]];

export function initCollide(field) {
  let mode = 0, param = 0.5, running = false, last = 0, carry = 0;
  const plot = $('collide-plot'), map = $('collide-map'), pctx = plot.getContext('2d'), mctx = map.getContext('2d');
  const read = () => Object.fromEntries(Object.entries(V).map(([k, i]) => [k, field._pw_value(i)]));
  const off = document.createElement('canvas'), cols = field._pw_columns(), rows = field._pw_rows();
  off.width = cols; off.height = rows;
  const octx = off.getContext('2d'), image = octx.createImageData(cols, rows);

  // Diverging colours: blue below zero, dark at zero, amber above (same map as the terminal's docs/collision.gif).
  function colour(phi) {
    const u = Math.max(-1, Math.min(1, phi / (2 * Math.PI))), a = Math.abs(u);
    return u > 0 ? [20 + 235 * a, 20 + 165 * a, 20 + 40 * a] : [20 + 40 * a, 20 + 150 * a, 20 + 235 * a];
  }
  function size(c) {
    const w = c.clientWidth, h = c.clientHeight, dpr = Math.min(devicePixelRatio || 1, 2);
    if (c.width !== Math.round(w * dpr) || c.height !== Math.round(h * dpr)) { c.width = Math.round(w * dpr); c.height = Math.round(h * dpr); }
    c.getContext('2d').setTransform(dpr, 0, 0, dpr, 0, 0);
    return [w, h];
  }
  function label(ctx, text, x, y, color = '#afc0cc', align = 'right') { ctx.fillStyle = color; ctx.textAlign = align; ctx.fillText(text, x, y); }

  function drawPlot() {
    const [w, h] = size(plot);
    if (!w || !h) return;
    pctx.clearRect(0, 0, w, h);
    pctx.font = '14px system-ui'; pctx.textBaseline = 'middle';
    const n = field._pw_count(), ptr = field._pw_samples(), s = field.HEAPF64.subarray(ptr / 8, ptr / 8 + n * 3);
    const left = 44, right = w - 10, top = 14, mid = h * 0.56, dTop = h * 0.7, bottom = h - 26, pw = right - left;
    const phiY = p => top + (1 - (p + 2 * Math.PI) / (4 * Math.PI)) * (mid - top);
    pctx.strokeStyle = '#2b4153'; pctx.lineWidth = 1;
    const hline = y => { pctx.beginPath(); pctx.moveTo(left, y); pctx.lineTo(right, y); pctx.stroke(); };
    for (const [p, t] of [[2 * Math.PI, '2π'], [0, '0'], [-2 * Math.PI, '−2π']]) { hline(phiY(p)); label(pctx, t, left - 8, phiY(p)); }
    for (const x of [-30, -15, 0, 15, 30]) {
      const px = left + (x + 30) / 60 * pw;
      pctx.beginPath(); pctx.moveTo(px, top); pctx.lineTo(px, bottom); pctx.stroke();
      label(pctx, String(x), px, bottom + 15, '#afc0cc', 'center');
    }
    let maxD = 2;
    for (let i = 0; i < n; i++) maxD = Math.max(maxD, s[3 * i + 2]);
    maxD = Math.ceil(maxD);
    hline(dTop); hline(bottom);
    label(pctx, String(maxD), left - 8, dTop, '#f4b968'); label(pctx, '0', left - 8, bottom, '#f4b968');
    function curve(k, yOf, color, dash = []) {
      pctx.beginPath(); pctx.setLineDash(dash); pctx.strokeStyle = color; pctx.lineWidth = 2;
      for (let i = 0; i < n; i++) { const px = left + i / (n - 1) * pw, py = yOf(s[3 * i + k]); i ? pctx.lineTo(px, py) : pctx.moveTo(px, py); }
      pctx.stroke(); pctx.setLineDash([]);
    }
    curve(1, phiY, '#a7b5c5', [6, 5]);
    curve(0, phiY, '#81e1f4');
    curve(2, d => bottom - d / maxD * (bottom - dTop), '#f4b968');
  }

  function drawMap(state) {
    const [w, h] = size(map);
    if (!w || !h) return;
    const ptr = field._pw_history(), hist = field.HEAPF64.subarray(ptr / 8, ptr / 8 + rows * cols), filled = state.rows;
    for (let r = 0; r < rows; r++) for (let c = 0; c < cols; c++) {
      const o = 4 * (r * cols + c), rgb = r < filled ? colour(hist[r * cols + c]) : [16, 30, 44];
      image.data[o] = rgb[0]; image.data[o + 1] = rgb[1]; image.data[o + 2] = rgb[2]; image.data[o + 3] = 255;
    }
    octx.putImageData(image, 0, 0);
    mctx.clearRect(0, 0, w, h);
    const left = 44, right = w - 10, top = 10, bottom = h - 26;
    mctx.imageSmoothingEnabled = true;
    mctx.drawImage(off, left, top, right - left, bottom - top);
    mctx.font = '14px system-ui'; mctx.textBaseline = 'middle';
    for (const x of [-30, 0, 30]) label(mctx, String(x), left + (x + 30) / 60 * (right - left), bottom + 15, '#afc0cc', 'center');
    label(mctx, `t ${state.start.toFixed(0)}`, left - 4, top + 8);
    label(mctx, `${state.end.toFixed(0)}`, left - 4, bottom - 8);
    // Where the solver is now.
    const y = top + (state.time - state.start) / (state.end - state.start) * (bottom - top);
    mctx.strokeStyle = '#e6edf2'; mctx.lineWidth = 1; mctx.beginPath(); mctx.moveTo(left, y); mctx.lineTo(right, y); mctx.stroke();
  }

  const fmt = (x, d) => Number.isFinite(x) ? x.toFixed(d) : '—';
  function update() {
    const s = read();
    $('c-clock').textContent = `t = ${s.time.toFixed(1)}`;
    $('c-energy').value = s.energy.toFixed(3);
    $('c-energy-theory').textContent = mode === 0 ? `theory 16γ = ${s.energyTheory.toFixed(3)}` : `theory 16√(1−ω²) = ${s.energyTheory.toFixed(3)}`;
    $('c-momentum').value = Math.abs(s.momentum) < 5e-10 ? '0.000' : s.momentum.toExponential(1);
    $('c-gap').value = s.gap.toFixed(3);
    if (mode === 0) {
      $('c-a-name').textContent = 'SHIFT, MEASURED';
      $('c-a').value = s.done ? fmt(s.shift, 4) : 'after the run';
      $('c-a-theory').textContent = `theory 2√(1−v²) ln(1/v) = ${s.shiftTheory.toFixed(4)}`;
    } else {
      $('c-a-name').textContent = 'ω, MEASURED';
      $('c-a').value = fmt(s.omega, 4);
      $('c-a-theory').textContent = `set ω = ${s.omegaTheory.toFixed(2)}; widest gap ${s.separation.toFixed(2)} (theory ${s.separationTheory.toFixed(2)})`;
    }
    $('collide-run').textContent = s.done ? 'Run again' : running ? 'Pause' : 'Run';
    window.collideView = {...s, running, mode, param};
    drawPlot();
    drawMap(s);
  }

  function reset() {
    running = false; carry = 0;
    field._pw_reset(mode, param);
    update();
  }
  function setParam(p) {
    const [lo, hi] = RANGE[mode];
    param = Math.round(Math.max(lo, Math.min(hi, p)) * 100) / 100;
    $('collide-param').value = param;
    $('collide-param-label').value = param.toFixed(2);
    for (const b of document.querySelectorAll('#collide-presets button')) b.classList.toggle('active', Number(b.dataset.p) === param);
    reset();
  }
  function setMode(m) {
    mode = m;
    for (const b of document.querySelectorAll('[data-collide-mode]')) b.classList.toggle('active', Number(b.dataset.collideMode) === m);
    $('collide-param-name').textContent = m === 0 ? 'Speed of each, v' : 'Angular frequency, ω';
    $('collide-param').min = RANGE[m][0];
    $('collide-param').max = RANGE[m][1];
    $('collide-presets').replaceChildren(...PRESETS[m].map(p => {
      const b = document.createElement('button');
      b.dataset.p = p; b.textContent = m === 0 ? `${p} c` : `ω ${p}`;
      b.addEventListener('click', () => setParam(p));
      return b;
    }));
    setParam(m === 0 ? 0.5 : 0.5);
  }

  function frame(now) {
    const seconds = last ? Math.min((now - last) / 1000, 0.1) : 0; last = now;
    const visible = !$('observatory').hidden && !document.hidden;
    if (running && visible) {
      const s = read(), total = (s.end - s.start) / 0.05;
      carry += seconds * Math.max(60, total / 10);   // a whole run takes about ten seconds
      const steps = Math.min(4000, Math.floor(carry));
      if (steps) { field._pw_step(steps); carry -= steps; }
      if (field._pw_value(V.done)) running = false;
      update();
    }
    requestAnimationFrame(frame);
  }

  for (const b of document.querySelectorAll('[data-collide-mode]')) b.addEventListener('click', () => setMode(Number(b.dataset.collideMode)));
  $('collide-param').addEventListener('input', e => setParam(Number(e.target.value)));
  $('collide-run').addEventListener('click', () => {
    if (field._pw_value(V.done)) { reset(); running = true; } else running = !running;
    update();
  });
  $('collide-reset').addEventListener('click', reset);
  addEventListener('resize', update);
  window.addEventListener('blur', () => { running = false; });
  setMode(0);
  requestAnimationFrame(frame);
  return {update, pause: () => { running = false; update(); }};
}
