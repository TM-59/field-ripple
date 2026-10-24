import createField from './field.mjs';

const $ = id => document.getElementById(id);
const keys = new Set(), pointers = new Map();
let field, paused = true, started = false, artLoaded = false, initialSpeed = 0, force = 0;
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
  started = true;
  $('entry').hidden = true; $('observatory').hidden = false;
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
  ctx.font = '10px system-ui'; ctx.textBaseline = 'middle';
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
  if(started)plot();
}
function direction() {
  const held=new Set([...keys,...pointers.values()]);
  return Number(held.has('right'))-Number(held.has('left'));
}
function frame(now) {
  const seconds=lastFrame?Math.min((now-lastFrame)/1000,.1):0;lastFrame=now;
  if(field&&started&&!paused&&!document.hidden) {
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
  if(!field||!started||e.target.matches('input,select,textarea'))return;
  const key=e.key.toLowerCase();
  if(['a','d','arrowleft','arrowright'].includes(key)) {
    e.preventDefault();if(field._fr_stopped())return;
    if(paused)setPaused(false);
    keys.add(key==='a'||key==='arrowleft'?'left':'right');
  } else if(!e.repeat&&key===' '&&!e.target.matches('button,a')) {e.preventDefault();if(!field._fr_stopped())setPaused(!paused);}
  else if(!e.repeat&&key==='r')reset();
});
document.addEventListener('keyup',e=>{
  const key=e.key.toLowerCase();
  if(key==='a'||key==='arrowleft')keys.delete('left');
  if(key==='d'||key==='arrowright')keys.delete('right');
});
window.addEventListener('blur',()=>{if(field)setPaused(true);});
document.addEventListener('visibilitychange',()=>{if(document.hidden&&field)setPaused(true);});
$('start').addEventListener('click',start);
$('pause').addEventListener('click',()=>setPaused(!paused));
$('reset').addEventListener('click',reset);
$('speed').addEventListener('input',e=>selectSpeed(Number(e.target.value)));
for(const button of document.querySelectorAll('[data-speed]'))button.addEventListener('click',()=>selectSpeed(Number(button.dataset.speed)));
try {
  field=await createField();field._fr_reset(0);
  $('start').textContent='Enter the observatory →';$('start').disabled=false;
  $('load-status').textContent='Ready · keyboard and touch controls';
  requestAnimationFrame(frame);
} catch {
  $('load-status').textContent='Could not load the field. Reload this page to try again.';
  $('start').textContent='Field unavailable';
}
