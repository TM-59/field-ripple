import createField from './field.mjs';

const $ = id => document.getElementById(id);
const keys = new Set(), pointers = new Map();
let field, paused = true, started = false, artLoaded = false, initialSpeed = 0, force = 0, mode = 'entry';
let lastFrame = 0, accumulator = 0;
const canvas = $('field-plot'), ctx = canvas.getContext('2d');
const pi = Math.PI;
const art = new Image();
art.src = new URL('./assets/observatory.jpg', import.meta.url).href;
art.decode().then(() => { artLoaded = true; }).catch(() => { $('art-warning').hidden = false; });

function clearInput() {
  keys.clear(); pointers.clear(); force = 0;
  $('left').classList.remove('active'); $('right').classList.remove('active');
}
function setPaused(value) { paused = value; clearInput(); accumulator = 0; update(); }
function reset() {
  clearInput(); accumulator = 0;
  field._fr_reset(initialSpeed);
  paused = true;
  update();
}
function start() {
  started = true; mode = 'free';
  $('line').hidden = true; $('observatory').hidden = false;
  reset();
  $('pause').focus({preventScroll:true});
}
function selectSpeed(speed) {
  initialSpeed = speed; $('speed').value = speed;
  $('speed-label').value = `${speed.toFixed(2)} c`;
  reset();
}
function plot() {
  const width = canvas.clientWidth, height = canvas.clientHeight;
  if (!width || !height) return;
  const dpr = Math.min(devicePixelRatio || 1, 2);
  if (canvas.width !== Math.round(width*dpr) || canvas.height !== Math.round(height*dpr)) {
    canvas.width = Math.round(width*dpr); canvas.height = Math.round(height*dpr);
  }
  ctx.setTransform(dpr,0,0,dpr,0,0);
  ctx.clearRect(0,0,width,height);
  const count = field._fr_count(), ptr = field._fr_samples();
  const samples = field.HEAPF64.subarray(ptr/8, ptr/8 + count*2);
  const left = 39, right = width-12, plotWidth = right-left;
  const top = 20, profileBottom = height*.56, densityTop = height*.68, bottom = height-25;
  ctx.font = '14px system-ui'; ctx.textBaseline = 'middle';
  function label(text,x,y,color='#afc0cc',align='right') {
    ctx.fillStyle=color;ctx.textAlign=align;ctx.fillText(text,x,y);
  }
  function line(y) {ctx.beginPath();ctx.moveTo(left,y);ctx.lineTo(right,y);ctx.stroke();}
  ctx.strokeStyle='#2b4153'; ctx.lineWidth=1;
  for (const [f,t] of [[0,'2π'],[.5,'π'],[1,'0']]) {
    const y=top+f*(profileBottom-top);line(y);label(t,left-10,y);
  }
  for (const x of [-12,-6,0,6,12]) {
    const px=left+(x+12)/24*plotWidth;
    ctx.beginPath();ctx.moveTo(px,top);ctx.lineTo(px,bottom);ctx.stroke();
    label(String(x),px,bottom+15,'#afc0cc','center');
  }
  let maxDensity=4;
  for(let i=0;i<count;i++) maxDensity=Math.max(maxDensity,samples[2*i+1]);
  maxDensity=Math.ceil(maxDensity/2)*2;
  line(bottom);line(densityTop);
  label('ρ',left,densityTop-12,'#f4b968','left');
  label(String(maxDensity),left-10,densityTop,'#f4b968');label('0',left-10,bottom,'#f4b968');
  function curve(value,yOf,color,dash=[]) {
    ctx.beginPath();ctx.setLineDash(dash);ctx.strokeStyle=color;ctx.lineWidth=2;
    for(let i=0;i<count;i++) {
      const px=left+i/(count-1)*plotWidth, py=yOf(value(i));
      if(i===0)ctx.moveTo(px,py);else ctx.lineTo(px,py);
    }
    ctx.stroke();ctx.setLineDash([]);
  }
  const phiY=value=>profileBottom-value/(2*pi)*(profileBottom-top);
  curve(i=>4*Math.atan(Math.exp(-12+24*i/(count-1))),phiY,'#a7b5c5',[5,5]);
  curve(i=>samples[2*i],phiY,'#81e1f4');
  curve(i=>samples[2*i+1],value=>bottom-value/maxDensity*(bottom-densityTop),'#f4b968');
}
function update() {
  if(!field)return;
  const stopped=!!field._fr_stopped();
  if(stopped) {paused=true;clearInput();}
  const state={time:field._fr_time(),position:field._fr_position(),velocity:field._fr_velocity(),energy:field._fr_energy(),width:field._fr_width()};
  $('time').value=state.time.toFixed(2);$('velocity').value=state.velocity.toFixed(3);
  $('energy').value=state.energy.toFixed(3);$('width').value=state.width.toFixed(3);
  $('position-label').value=`x = ${state.position.toFixed(2)}`;
  $('position-marker').style.left=`${50+state.position/150*100}%`;
  $('force-label').textContent=force ? `${force>0?'pushing right':'pushing left'} · ε = ${(-.05*force).toFixed(2)}` : 'coasting · ε = 0';
  const stateLabel=stopped?'Complete':paused?'Paused':'Observing';
  if($('state-label').textContent!==stateLabel)$('state-label').textContent=stateLabel;
  $('pause').textContent=paused?'Resume':'Pause';$('pause').disabled=stopped;
  $('left').disabled=stopped;$('right').disabled=stopped;
  $('limit-note').hidden=!stopped;
  document.body.classList.toggle('running',started&&!paused);
  for(const button of document.querySelectorAll('[data-speed]'))button.classList.toggle('active',Number(button.dataset.speed)===initialSpeed);
  window.fieldView={...state,paused,started,stopped,initialSpeed,force,artLoaded,held:[...keys,...pointers.values()]};
  if(started&&mode==='free')plot();
}
function direction() {
  const held=new Set([...keys,...pointers.values()]);
  return Number(held.has('right'))-Number(held.has('left'));
}
function frame(now) {
  const seconds=lastFrame?Math.min((now-lastFrame)/1000,.1):0;lastFrame=now;
  if(mode==='line'||mode==='entry') lineFrame(seconds);
  if(field&&mode==='free'&&started&&!paused&&!document.hidden) {
    // 5 simulation units per real second. Fixed 0.05 steps, independent of fps.
    accumulator+=seconds*5;
    const steps=Math.min(Math.floor(accumulator/.05),10);
    force=direction();
    if(steps) {field._fr_step(force,steps);accumulator-=steps*.05;}
  }
  update();requestAnimationFrame(frame);
}
for(const name of ['left','right']) {
  const button=$(name);
  button.addEventListener('pointerdown',e=>{
    if(!started||!field||field._fr_stopped()||e.button!==0)return;
    e.preventDefault();button.setPointerCapture(e.pointerId);
    if(paused)setPaused(false);
    pointers.set(e.pointerId,name);button.classList.add('active');
  });
  for(const event of ['pointerup','pointercancel','lostpointercapture'])button.addEventListener(event,e=>{
    pointers.delete(e.pointerId);button.classList.toggle('active',[...pointers.values()].includes(name));
  });
}
document.addEventListener('keydown',e=>{
  if(mode==='line'){lineKey(e,true);return;}
  if(!field||!started||mode!=='free'||e.target.matches('input,select,textarea'))return;
  const key=e.key.toLowerCase();
  if(['a','d','arrowleft','arrowright'].includes(key)) {
    e.preventDefault();if(field._fr_stopped())return;
    if(paused)setPaused(false);
    keys.add(key==='a'||key==='arrowleft'?'left':'right');
  } else if(!e.repeat&&key===' '&&!e.target.matches('button,a')) {e.preventDefault();if(!field._fr_stopped())setPaused(!paused);}
  else if(!e.repeat&&key==='r')reset();
});
document.addEventListener('keyup',e=>{
  if(mode==='line'){lineKey(e,false);return;}
  const key=e.key.toLowerCase();
  if(key==='a'||key==='arrowleft')keys.delete('left');
  if(key==='d'||key==='arrowright')keys.delete('right');
});
window.addEventListener('blur',()=>{if(field&&mode==='free')setPaused(true);if(mode==='line')linePause(true);});
document.addEventListener('visibilitychange',()=>{if(document.hidden&&field&&mode==='free')setPaused(true);if(document.hidden&&mode==='line')linePause(true);});
$('start').addEventListener('click',start);
$('line-start').addEventListener('click',()=>lineStart());
$('pause').addEventListener('click',()=>setPaused(!paused));
$('reset').addEventListener('click',reset);
$('speed').addEventListener('input',e=>selectSpeed(Number(e.target.value)));
for(const button of document.querySelectorAll('[data-speed]'))button.addEventListener('click',()=>selectSpeed(Number(button.dataset.speed)));
// ------------------------------------------------------------------ The Flood Line (src/line.c)
// Every number of the night comes from C through _lw_value; only the words and the drawing live here.
const lineFields = ['x','v','vRead','time','charge','red','leg','scene','banner','fails','from','target','remaining',
  'route','ford','count','wobble','ending','loud','heavy','command','redLine','held','holdNeed','leak','drift','cue',
  'parked','energy','width','stopDistance','motor','coastV'];
const lineCanvas = $('line-plot'), lctx = lineCanvas.getContext('2d');
const lineHeld = new Set(), linePointers = new Map(), heavyLog = [];
let ls = null, linePaused = true, lineAcc = 0, shownScene = -2;
// ?rate=2..4 runs the night faster (for the local smoke check); the solver steps are the same.
const lineRate = Math.max(1, Math.min(4, Math.floor(Number(new URLSearchParams(location.search).get('rate')) || 1)));
const lineRead = () => Object.fromEntries(lineFields.map((name, i) => [name, field._lw_value(i)]));
const lineDone = () => ls && ls.leg >= 5;

// Scenes open where the C night says (src/line.c); index = LS_* in src/line.h.
const SCENES = [
  {id: 'hut', title: 'The relay hut, ten at night', lines: [
    ['Ada', "Dam's talking. Hear it? That groan is the spillway gate."],
    ['Ada', "I've wound your cell past the red line. That's the last of the hut's charge, so don't waste it on me."],
    ['Ada', "One push to send a turn. One push to stop it. A third push is charge you haven't got."],
    ['Kit', "I'm on the catch arm at the weir, I've got my hand right on it, and if your turn comes in too fast I'll shout. You'll hear me. Everybody always hears me."]],
    note: 'Hold → (Down the valley) to run the motor that way and ← to run it back. The motor takes a breath to spin up and to spin down. Bring the turn to the weir mark and let it stand still: the catch arm takes it and rings the bell.',
    options: ['Send the first turn']},
  {id: 'stones', title: 'Two bells', lines: [
    ['Kit', "Weir bell rang! I heard it from here. The river's at the second stone, Ada. The second stone, and it hasn't stopped raining since six."],
    ['Kit', 'The low road is full of carts. And the late bus brings the choir home over the ford at one. Mr Hale drives it, and he always takes the ford.'],
    ['Ada', "The mill bell is a short run. The church bell is a long one, but the sexton has a crank. Ring his and he'll wind charge back to you."],
    ['Ada', "Pick one. The river won't wait for me to finish."]],
    options: ['Ring the mill bell (short span)', 'Ring the church bell (long span, the sexton cranks back)']},
  {id: 'bell', title: 'The bell', lines: [
    ['Kit', "It's ringing! They heard it, the carts are turning, the whole low road is turning up the hill past the quarry, I can see them going—"],
    ['Kit', "I'm taking the cell out of my board. It's going in the line. Don't argue, Ada. I know you're going to argue."],
    ['Ada', "I wasn't going to argue."]],
    note: "Kit's board cell goes into yours. With it, your cell is less likely to run dry once the night turns wet.",
    options: ["Take Kit's cell"]},
  {id: 'spill', title: 'The spillway', heavy: true, lines: [
    ['Ada', 'Stop talking, both of you. Listen.'],
    ['', 'Under the rain there is a new sound, wide and steady, like a road full of lorries.'],
    ['Ada', "The spillway's gone over."],
    ['Kit', 'They said it would hold till May. They stood on the bridge and said May!'],
    ['Ada', "Being right won't lower it, Kit."],
    ['Ada', "When the water reaches your door, your cell will bleed and the speed needle will shake. Get the next turn up to the gorge while you're still dry."]],
    options: ['Send it back up to the gorge']},
  {id: 'ford', title: 'One o’clock', heavy: true, lines: [
    ['Kit', "The late bus isn't back. Ada, it isn't back. Mr Hale always takes the ford, I told you he always takes it—"],
    ['Ada', 'The ford bell’s post is under the water line. A turn can reach it, but it has to sit on the arm until the bell has rung, and that arm is low in the water.'],
    ['Ada', 'Or ring the hill siren. Dry posts. The whole town hears it, and the line is still ours afterwards.'],
    ['Kit', 'There are twenty-two kids on that bus.'],
    ['Ada', 'I can count, Kit.']],
    options: ['Send it down to the ford bell (past the water line, then hold it while the bell rings)', 'Ring the hill siren (dry posts, the town hears it)']},
  {id: 'cut', title: 'The lower posts', heavy: true, lines: [
    ['Kit', "The siren's going, the whole hill can hear it. I can't see the ford road from here, I can't see anything past the— the posts. The posts below me are going over one after another, the arm's tipping, I can't—"],
    ['', 'The line goes slack below the weir. The voice wire hums. Nobody shouts the brake.'],
    ['Ada', 'Bring the last one up to my arm. When it rings, I’ll come down.'],
    ['You', 'One push to send it. One push to stop it.']],
    fordLines: [
    ['Kit', "It's ringing under the water, Ada, I can hear it, it sounds like— the posts. The posts below me are going over one after another, the arm's tipping, I can't—"],
    ['', 'The line goes slack below the weir. The voice wire hums. Nobody shouts the brake.'],
    ['Ada', 'Bring the last one up to my arm. When it rings, I’ll come down.'],
    ['You', 'One push to send it. One push to stop it.']],
    note: "Nobody reads the turn at the far end now: the speed needle shakes. Ada's old arm at the dam road jams under a fast turn, and a dry cell has no crank any more: the turn coasts off and Ada winds a fresh one at your last mark.",
    options: ['Carry the last turn up to the dam road']},
  {id: 'dawn', title: 'First light', endings: [
    ['First light. The late bus is parked on the hill road with twenty-two coats drying over its seats.',
     'Mr Hale says he heard a bell where no bell should be, out in the brown water, and turned round at the ford.',
     'Ada comes down the dam road at five with her crank on her back.',
     'The ford bell went under with the lower posts. Kit sits on the bus step and asks who is going to cast another.'],
    ['First light. The town spent the night on the hill, and the siren is still going because nobody has thought to stop it.',
     'The bus comes in at noon by the quarry road. Mr Hale waited all night in the chapel at the ford for a bell that never rang; the choir slept in the pews.',
     'Ada comes down the dam road at five with her crank on her back.',
     "Kit walks down the hill to meet the bus and doesn't say anything for a long time."]],
    options: ['Start the night again']}
];
const LEG_NAMES = ['LEG 1 / 5 · TO THE WEIR', 'LEG 2 / 5 · TO THE BELL', 'LEG 3 / 5 · BACK TO THE GORGE',
  'LEG 4 / 5 · THE WATER IN THE HUT', 'LEG 5 / 5 · UP TO THE DAM ROAD'];
const MARKS = [[18,'WEIR'],[30,'MILL'],[42,'CHURCH'],[20,'GORGE'],[32,'SIREN'],[44,'FORD'],[-10,'DAM ROAD']];
function markName(x) { const m = MARKS.find(([p]) => Math.abs(p - x) < 0.01); return m ? m[1] : 'MARK'; }

function lineStart() {
  mode = 'line';
  $('welcome').hidden = true; $('observatory').hidden = true; $('line').hidden = false;
  lineReset(false);
  if (innerHeight < 560) $('line').scrollIntoView({block: 'start'});
}
function lineReset(run) {
  field._lw_reset(); ls = lineRead(); shownScene = -2; heavyLog.length = 0;
  lineHeld.clear(); linePointers.clear(); lineAcc = 0;
  linePause(!run); showScene(); lineUpdate(); lineDraw();
}
function linePause(value) {
  linePaused = value; lineAcc = 0; lineHeld.clear(); linePointers.clear(); reflectMotor();
  $('line-pause').textContent = lineDone() ? 'Night over' : linePaused ? 'Resume' : 'Pause';
  $('line-pause').disabled = lineDone();
}
const motorWanted = () => {
  const held = new Set([...lineHeld, ...linePointers.values()]);
  return held.has('down') && !held.has('up') ? 1 : held.has('up') && !held.has('down') ? 2 : 0;
};
function reflectMotor() {
  const held = new Set([...lineHeld, ...linePointers.values()]);
  for (const b of document.querySelectorAll('[data-motor]')) b.setAttribute('aria-pressed', String(held.has(b.dataset.motor)));
}
function showScene() {
  const box = $('scene'), index = ls && mode === 'line' ? ls.scene : -1;
  if (index === shownScene) return;
  shownScene = index;
  if (index < 0) { box.hidden = true; return; }
  const s = SCENES[index];
  lineHeld.clear(); linePointers.clear(); reflectMotor();
  box.hidden = false;
  box.className = s.heavy ? 'heavy' : s.endings ? 'ending' : '';
  box.dataset.scene = s.id;
  if (s.heavy) heavyLog.push({id: s.id, banner: ls.banner, loud: ls.loud, bannerShown: !$('line-banner').hidden});
  $('scene-title').textContent = s.title;
  const text = $('scene-text');
  text.replaceChildren();
  const lines = s.endings ? s.endings[ls.ending === 0 ? 0 : 1].map(line => ['', line])
    : s.fordLines && ls.ford === 0 ? s.fordLines : s.lines;
  for (const [who, words] of lines) {
    const p = document.createElement('p');
    if (who) { const b = document.createElement('b'); b.textContent = who; p.append(b, ' '); }
    else p.className = 'narration';
    p.append(words);
    text.append(p);
  }
  if (s.note) { const p = document.createElement('p'); p.className = 'note'; p.textContent = s.note; text.append(p); }
  const options = $('scene-options');
  options.replaceChildren();
  s.options.forEach((label, option) => {
    const button = document.createElement('button');
    button.textContent = label;
    if (!option) button.className = 'primary';
    button.onclick = () => lineAnswer(option);
    options.append(button);
  });
  $('line-banner').hidden = true;
  box.scrollTop = 0;
  options.firstChild.focus({preventScroll: true});
}
function lineAnswer(option) {
  if (!ls || ls.scene < 0) return;
  if (ls.scene === 6) { lineReset(false); return; }
  field._lw_answer(option); ls = lineRead();
  showScene(); linePause(ls.scene >= 0); lineUpdate(); lineDraw();
}
function fmtSigned(value, digits) { return value.toFixed(digits).replace('-', '−'); }
function lineGoal() {
  if (lineDone()) return 'The night is over.';
  const name = markName(ls.target);
  if (ls.drift > 0) return ls.count ? 'Motor dead. Kit is cranking your cell from her end.' : 'Motor jammed. Wait for it.';
  const church = ls.leg === 1;
  if (ls.holdNeed > 0 && ls.held > 0) return church
    ? `The church bell is ringing and the sexton is cranking. Keep the motor off: ${ls.held.toFixed(1)} of ${ls.holdNeed.toFixed(0)} t.`
    : `The ford bell is ringing. Keep the motor off: ${ls.held.toFixed(1)} of ${ls.holdNeed.toFixed(0)} t.`;
  if (ls.holdNeed > 0 && !church) return 'Bring the turn onto the ford bell and let it stand still. The arm is low: come in slowly.';
  return `Bring the turn to the ${name.toLowerCase()} mark and let it stand still.`;
}
function lineUpdate() {
  if (!ls) return;
  const leg = Math.min(ls.leg, 4), red = ls.charge < ls.redLine;
  $('line-leg').textContent = lineDone() ? 'FIRST LIGHT' : LEG_NAMES[leg];
  $('line-title').textContent = lineDone() ? 'The night is over' : 'The Flood Line';
  const goal = lineGoal();
  if ($('line-goal').textContent !== goal) $('line-goal').textContent = goal;
  const call = ls.scene >= 0 || lineDone() ? '' : !ls.count ? (ls.leg === 4 ? 'The voice wire hums.' : '')
    : ls.cue === 1 ? 'Kit: “Brake! It’s coming in too fast!”' : ls.cue === 2 ? 'Kit: “It’s on my mark. Let it go.”' : '';
  if ($('line-call').textContent !== call) $('line-call').textContent = call;
  $('l-charge').textContent = ls.charge.toFixed(2);
  $('cell-row').dataset.red = String(red);
  $('cell-bar').style.width = `${Math.max(0, Math.min(100, ls.charge / 0.8 * 100))}%`;
  $('cell-red').style.left = `${ls.redLine / 0.8 * 100}%`;
  $('l-speed').textContent = fmtSigned(ls.vRead, 3);
  $('l-remaining').textContent = fmtSigned(ls.remaining, 1);
  const m = ls.motor;
  $('l-motor').textContent = Math.abs(m) < 0.02 ? 'stopped' : `${Math.round(Math.abs(m) * 100)} % ${m > 0 ? 'down' : 'up'}`;
  $('l-clock').textContent = ls.time.toFixed(0);
  $('l-red').textContent = ls.red.toFixed(0);
  $('l-lost').textContent = String(ls.fails);
  const banner = $('line-banner');
  banner.hidden = !ls.banner || ls.scene >= 0;
  banner.textContent = ['', 'MARK HELD · THE BELL RINGS', 'TURN LOST · ADA WINDS A FRESH ONE', 'CELL DRY · KIT IS CRANKING',
    'ARM STRUCK · THE MOTOR JAMMED'][ls.banner] || '';
  banner.dataset.kind = ls.banner === 1 ? 'clear' : 'fail';
  window.lineView = {...ls, paused: linePaused, scene: ls.scene >= 0 ? SCENES[ls.scene].id : null, bannerShown: !banner.hidden,
    bannerText: banner.textContent, heavyLog: heavyLog.map(h => ({...h})), sceneText: $('scene').hidden ? '' : $('scene-text').textContent,
    goalText: goal, callText: call};
}
function lineFrame(seconds) {
  if (!ls) return;
  if (mode === 'line' && !linePaused && !lineDone() && ls.scene < 0 && !document.hidden) {
    // One C frame is 0.2 t of the night; 5 t per real second.
    lineAcc += seconds * 25 * lineRate;
    const frames = Math.min(Math.floor(lineAcc), 6 * lineRate);
    lineAcc -= Math.floor(lineAcc);
    if (frames) { field._lw_tick(frames, motorWanted()); ls = lineRead(); }
    if (ls.scene >= 0) { showScene(); linePause(true); }
    else if (lineDone()) linePause(true);
  }
  lineUpdate(); lineDraw();
}
function lineKey(e, down) {
  if (!ls || e.target.matches('input,select,textarea')) return;
  const key = e.key.toLowerCase();
  const name = key === 'a' || key === 'arrowleft' ? 'up' : key === 'd' || key === 'arrowright' ? 'down' : null;
  if (name) {
    e.preventDefault();
    if (!down) { lineHeld.delete(name); reflectMotor(); return; }
    if (ls.scene >= 0 || lineDone()) return;
    if (linePaused) linePause(false);
    lineHeld.add(name); reflectMotor();
  } else if (down && !e.repeat && key === ' ' && !e.target.matches('button,a') && ls.scene < 0 && !lineDone()) {
    e.preventDefault(); linePause(!linePaused);
  }
}
function lineDraw() {
  if (!ls || $('line').hidden) return;
  document.body.dataset.mode = mode;
  const box = lineCanvas.getBoundingClientRect(), dpr = Math.min(devicePixelRatio || 1, 2);
  const w = box.width, h = box.height;
  if (!w || !h) return;
  if (lineCanvas.width !== Math.round(w * dpr) || lineCanvas.height !== Math.round(h * dpr)) {
    lineCanvas.width = Math.round(w * dpr); lineCanvas.height = Math.round(h * dpr);
  }
  const c = lctx;
  c.setTransform(dpr, 0, 0, dpr, 0, 0);
  c.clearRect(0, 0, w, h);
  c.font = '14px system-ui'; c.textBaseline = 'middle';
  const text = (t, x, y, colour = '#c6d3dd', align = 'left') => { c.fillStyle = colour; c.textAlign = align; c.fillText(t, x, y); };
  const seg = (x1, y1, x2, y2, colour, width = 1) => { c.strokeStyle = colour; c.lineWidth = width; c.beginPath(); c.moveTo(x1, y1); c.lineTo(x2, y2); c.stroke(); };
  const topH = h * 0.58, left = 44, right = w - 14, pw = right - left;
  // Upper panel: the field around the turn (phi and energy density from C), with the next mark if it is in view.
  const count = field._lw_count(), ptr = field._lw_samples();
  const sm = field.HEAPF64.subarray(ptr / 8, ptr / 8 + count * 2);
  const ptop = Math.min(topH * 0.55, document.querySelector('.line-top').offsetHeight + 44), pbot = topH - 34;
  const phiY = v => pbot - v / (2 * Math.PI) * (pbot - ptop) * 0.62;
  for (const [f, t] of [[0, '0'], [Math.PI, 'π'], [2 * Math.PI, '2π']]) { const y = phiY(f); seg(left, y, right, y, '#2b4153'); text(t, left - 8, y, '#c6d3dd', 'right'); }
  const xs = i => left + i / (count - 1) * pw;
  let maxD = 2;
  for (let i = 0; i < count; i++) maxD = Math.max(maxD, sm[2 * i + 1]);
  const dY = v => pbot - v / maxD * (pbot - ptop) * 0.3;
  const tx = left + (ls.target - (ls.x - 12)) / 24 * pw;
  if (tx > left && tx < right) {
    c.setLineDash([6, 5]); seg(tx, ptop - 6, tx, pbot, '#9fe0a8', 1.6); c.setLineDash([]);
    text(`${markName(ls.target)} MARK`, Math.min(Math.max(tx, left + 60), right - 60), ptop - 18, '#9fe0a8', 'center');
  }
  c.beginPath(); for (let i = 0; i < count; i++) { const y = dY(sm[2 * i + 1]); if (i) c.lineTo(xs(i), y); else c.moveTo(xs(i), y); }
  c.strokeStyle = '#f4b968'; c.lineWidth = 1.6; c.stroke();
  c.beginPath(); for (let i = 0; i < count; i++) { const y = phiY(sm[2 * i]); if (i) c.lineTo(xs(i), y); else c.moveTo(xs(i), y); }
  c.strokeStyle = '#81e1f4'; c.lineWidth = 2.4; c.stroke();
  text('φ / rad', left, pbot + 18, '#81e1f4'); text('energy density', left + 72, pbot + 18, '#f4b968');
  text(`x = ${fmtSigned(ls.x, 1)}`, right, pbot + 18, '#c6d3dd', 'right');
  // Lower panel: the whole line, its posts and marks; water past the water line once the spill has come.
  const ly = topH + (h - topH) * 0.48, X = x => left + (x + 52) / 104 * pw;
  seg(left, topH + 2, right, topH + 2, '#304252');
  const wet = ls.leg >= 3, cut = ls.leg >= 4;
  if (wet) { c.fillStyle = '#2a5d7a55'; c.fillRect(X(38), ly - 26, X(52) - X(38), 52); text('WATER LINE', X(45), ly + 40, '#8fc4dd', 'center'); }
  for (let x = -52; x <= 52; x += 4) {
    const broken = cut && x > 22;
    seg(X(x), ly - 10, X(x) + (broken ? 6 : 0), ly + 10, broken ? '#56656f' : '#7d8f9c', 1.4);
  }
  seg(X(-52), ly - 10, cut ? X(22) : X(52), ly - 10, '#a9bbc8', 1);
  for (const [p, name] of MARKS) {
    const active = Math.abs(p - ls.target) < 0.01 && !lineDone();
    if (!active && !(p === 18 || p === -10 || p === 20)) continue;
    seg(X(p), ly - 22, X(p), ly + 14, active ? '#9fe0a8' : '#6c8090', active ? 2.5 : 1);
    if (active) text(name, X(p), ly + 28, '#9fe0a8', 'center');
  }
  text('← UP TO THE DAM', left, topH + 22, '#c6d3dd');
  text('LOWFIELD →', right, topH + 22, '#c6d3dd', 'right');
  const px = X(ls.x);
  c.beginPath(); c.arc(px, ly - 10, 8, 0, Math.PI * 2); c.fillStyle = '#81e1f4'; c.fill();
  const arrow = Math.max(-60, Math.min(60, ls.v * 120));
  if (Math.abs(arrow) > 2) { seg(px, ly - 10, px + arrow, ly - 10, '#e6edf2', 2); }
}
for (const button of document.querySelectorAll('[data-motor]')) {
  const name = button.dataset.motor;
  button.addEventListener('pointerdown', e => {
    if (!ls || ls.scene >= 0 || lineDone() || e.button !== 0) return;
    e.preventDefault(); button.setPointerCapture(e.pointerId);
    if (linePaused) linePause(false);
    linePointers.set(e.pointerId, name); reflectMotor();
  });
  for (const ev of ['pointerup', 'pointercancel', 'lostpointercapture']) button.addEventListener(ev, e => { linePointers.delete(e.pointerId); reflectMotor(); });
}
$('line-pause').addEventListener('click', () => { if (ls && ls.scene < 0 && !lineDone()) linePause(!linePaused); });
$('line-restart').addEventListener('click', () => lineReset(false));
$('back-line').addEventListener('click', () => { mode = 'entry'; $('observatory').hidden = true; $('line').hidden = false; $('welcome').hidden = false; shownScene = -2; $('scene').hidden = true; });
$('line-leave').addEventListener('click', () => { mode = 'entry'; linePause(true); $('scene').hidden = true; shownScene = -2; $('welcome').hidden = false; });
// Common read-only test hook for the automated playability check (phone-sized, touch only).
// Logical units are the canvas's CSS pixels. DOM words and buttons are measured by the gate directly.
// The gate maps logical points through the largest visible canvas: the line in the story, the plot in free observation.
const gateCanvas = () => [lineCanvas, canvas].map(c => [c, c.getBoundingClientRect()]).sort((a, b) => b[1].width * b[1].height - a[1].width * a[1].height)[0][0];
function fromCanvas(el) {
  if (!el || el.hidden || !el.getClientRects().length) return null;
  const cb = gateCanvas().getBoundingClientRect(), r = el.getBoundingClientRect();
  return {x: r.left + r.width / 2 - cb.left, y: r.top + r.height / 2 - cb.top};
}
window.__tmGate = {
  version: 1,
  get logical() { const r = gateCanvas().getBoundingClientRect(); return {w: r.width || 1, h: r.height || 1}; },
  scene: () => mode === 'entry' ? 'title' : mode === 'free' ? 'observatory' : !ls ? 'loading'
    : ls.scene >= 0 ? `scene:${SCENES[ls.scene].id}` : lineDone() ? 'done' : linePaused ? 'paused' : 'play',
  // The first goal: the night's first conversation answered (the hut scene closed, the first turn on its way).
  goal: () => Boolean(mode === 'line' && ls && (ls.leg > 0 || (ls.scene < 0 && ls.count > 0))),
  objective: () => mode === 'line' ? $('line-goal').textContent : 'Run the flood line',
  texts: () => [],
  targets: () => [],
  // The turn moves along one axis under a held motor, not under a steered player.
  player: () => null,
  // Where a player who reads the screen would tap next: the entry button, the open scene's first answer, or Resume.
  hint: () => {
    if (mode === 'entry') return fromCanvas($('line-start'));
    if (mode === 'free') return fromCanvas($('back-line'));
    if (mode !== 'line' || !ls) return null;
    if (ls.scene >= 0) return fromCanvas($('scene-options').firstChild);
    if (linePaused && !lineDone()) return fromCanvas($('line-pause'));
    return null;
  },
};
try {
  field=await createField();field._fr_reset(0);
  $('start').disabled=false;$('line-start').disabled=false;
  field._lw_reset();ls=lineRead();
  $('load-status').textContent='Ready · keyboard and touch controls';
  requestAnimationFrame(frame);
} catch {
  $('load-status').textContent='Could not load the field. Reload this page to try again.';
  $('start').textContent='Field unavailable';$('line-start').textContent='Field unavailable';
}
