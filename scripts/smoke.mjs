// Local only. Screenshots capture the real C/WASM observatory under a URL subpath.
import {chromium} from 'playwright';
import {createServer} from 'node:http';
import {readFile, mkdir, writeFile, stat} from 'node:fs/promises';
import {existsSync} from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import assert from 'node:assert/strict';

const libs = path.join(os.homedir(),'.local/lib/chromium-deps/usr/lib/x86_64-linux-gnu');
if(existsSync(libs))process.env.LD_LIBRARY_PATH=libs+(process.env.LD_LIBRARY_PATH?`:${process.env.LD_LIBRARY_PATH}`:'');
const types={'.html':'text/html','.mjs':'text/javascript','.css':'text/css','.wasm':'application/wasm','.jpg':'image/jpeg'};
const root=path.resolve('dist');
const server=createServer(async(req,res)=>{
  try {
    const pathname=new URL(req.url,'http://localhost').pathname;
    assert(pathname.startsWith('/ripple/'));
    const file=path.resolve(root,decodeURIComponent(pathname.slice(8))||'index.html');
    assert(file.startsWith(root+path.sep));
    res.writeHead(200,{'Content-Type':types[path.extname(file)]||'application/octet-stream'});
    res.end(await readFile(file));
  } catch {if(!res.headersSent)res.writeHead(404);res.end();}
});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
const url=`http://127.0.0.1:${server.address().port}/ripple/`;
let browser;
const checks=[],errors=[],external=[],failedResponses=[],consoleErrors=[];
const inspect=page=>page.evaluate(()=>window.fieldView);
const waitTime=(page,time)=>page.waitForFunction(t=>window.fieldView?.time>=t,time);
const track=(page,{fallback=false}={})=>{
  page.on('pageerror',e=>errors.push(String(e)));
  page.on('request',req=>{if(new URL(req.url()).origin!==new URL(url).origin)external.push(req.url());});
  if(!fallback){
    page.on('response',r=>{if(r.status()>=400)failedResponses.push(`${r.status()} ${r.url()}`);});
    page.on('console',message=>{if(message.type()==='error')consoleErrors.push(message.text());});
  }
};
await mkdir('docs/images',{recursive:true});await mkdir('out',{recursive:true});
const shot=(page,name)=>page.screenshot({path:`docs/images/${name}.jpg`,type:'jpeg',quality:85,fullPage:true});
try {
  browser=await chromium.launch({headless:true,args:['--no-sandbox']});
  const page=await browser.newPage({viewport:{width:1280,height:900}});track(page);
  await page.goto(url);await page.waitForFunction(()=>window.fieldView?.artLoaded);
  assert.equal((await inspect(page)).started,false);
  await shot(page,'title');checks.push('subpath: WASM and original art load; entry stays still');
  await page.locator('#start').click();
  assert.equal((await inspect(page)).paused,true);
  assert(Math.abs((await inspect(page)).energy-8)<.01);
  checks.push('entry opens the measured field at rest, with E≈8');

  for(const speed of [.6,.9]) {
    await page.locator(`[data-speed="${speed}"]`).click();
    let s=await inspect(page);
    assert.equal(s.time,0);assert.equal(s.initialSpeed,speed);
    assert(Math.abs(s.width-Math.sqrt(1-speed*speed))<.02);
    assert(Math.abs(s.energy/(8/Math.sqrt(1-speed*speed))-1)<.01);
    await page.locator('#pause').click();await waitTime(page,3);
    await page.locator('#pause').click();s=await inspect(page);
    assert(Math.abs(s.position-speed*s.time)<.1);
    const frozen=await inspect(page);await page.waitForTimeout(180);
    assert.deepEqual(await inspect(page),frozen);
    await shot(page,speed===.6?'coasting':'contracted');
  }
  checks.push('0.6c and 0.9c: Lorentz width, 8γ energy, x≈vt; pause freezes state');
  await page.locator('#reset').click();
  assert.equal((await inspect(page)).time,0);assert((await inspect(page)).paused);
  await page.locator('[data-speed="0"]').click();
  await page.keyboard.down('d');await waitTime(page,6);
  await page.keyboard.up('d');let s=await inspect(page);
  assert(s.velocity>.15&&s.position>.4);
  await page.waitForFunction(()=>window.fieldView.force===0);
  await waitTime(page,s.time+1);s=await inspect(page);
  assert(s.velocity>.15);checks.push('keyboard right push accelerates C field; release coasts');
  await page.keyboard.down('a');await waitTime(page,s.time+6);
  await page.keyboard.up('a');s=await inspect(page);
  assert(Math.abs(s.velocity)<.1);checks.push('opposing keyboard force brakes the kink');
  await page.keyboard.down('a');await waitTime(page,s.time+.5);
  await page.evaluate(()=>window.dispatchEvent(new Event('blur')));
  await page.keyboard.up('a');s=await inspect(page);
  assert(s.paused&&s.held.length===0&&s.force===0);
  checks.push('focus loss pauses and clears input');

  // Real touch events, including cancellation, rather than a synthetic click.
  const mobile=await browser.newContext({viewport:{width:390,height:844},deviceScaleFactor:1,isMobile:true,hasTouch:true});
  const touch=await mobile.newPage();track(touch);await touch.goto(url);
  await touch.locator('#start').tap();await touch.locator('#left').scrollIntoViewIfNeeded();
  const box=await touch.locator('#left').boundingBox();const cdp=await mobile.newCDPSession(touch);
  await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x:box.x+box.width/2,y:box.y+box.height/2,id:1,radiusX:3,radiusY:3,force:1}]});
  await waitTime(touch,5);
  await cdp.send('Input.dispatchTouchEvent',{type:'touchCancel',touchPoints:[]});
  await touch.waitForFunction(()=>window.fieldView.held.length===0&&window.fieldView.force===0);
  s=await inspect(touch);assert(s.velocity<-.15&&s.position<-.3);
  await touch.locator('#pause').tap();
  assert(await touch.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
  await shot(touch,'mobile');await mobile.close();
  checks.push('390px: real left touch, cancellation and pause; horizontal overflow 0');

  await page.locator('[data-speed="0.9"]').click();await page.locator('#pause').click();
  // Render-frame time is not altered. Wait for the actual finite-domain stop.
  await page.waitForFunction(()=>window.fieldView.stopped,null,{timeout:30000});
  s=await inspect(page);assert(s.paused&&s.position>=75&&s.held.length===0);
  assert(await page.locator('#pause').isDisabled());assert(await page.locator('#limit-note').isVisible());
  await page.locator('#reset').click();assert.equal((await inspect(page)).time,0);
  assert.equal((await inspect(page)).stopped,false);checks.push('finite-domain limit stops and explains completion; reset recovers');

  // The Flood Line: two whole nights with real keys and clicks, one per ending. A simple driver reads only what the
  // page shows (the speed needle after spin-down and the distance to the mark) and holds A / D.
  const line=page=>page.evaluate(()=>window.lineView);
  async function night(route,ford,shots){
    const p=await browser.newPage({viewport:{width:1280,height:900}});track(p);
    await p.goto(url+'?rate=3');await p.waitForFunction(()=>!document.getElementById('line-start').disabled);
    await p.locator('#line-start').click();
    const seen=[],quiet=[];let held=null,heavyOpen=null,legShot=false;
    const deadline=Date.now()+170000;
    for(;;){
      assert(Date.now()<deadline,'night did not finish');
      const v=await line(p);
      if(v.scene){
        if(held){await p.keyboard.up(held);held=null;}
        if(seen.at(-1)!==v.scene)seen.push(v.scene);
        if(['spill','ford','cut'].includes(v.scene)){assert(!v.bannerShown,`banner over ${v.scene}`);heavyOpen=v.scene;}
        if(v.scene==='stones'&&shots)await shot(p,'line-choice');
        if(v.scene==='cut'&&shots)await shot(p,'line-bottom');
        if(v.scene==='dawn'){if(shots)await shot(p,ford?'line-ending-siren':'line-ending-ford');break;}
        const option=v.scene==='stones'?route:v.scene==='ford'?ford:0;
        await p.locator('#scene-options button').nth(option).click();
        if(heavyOpen){
          // The first moments after a heavy scene: no "mark held" banner.
          for(let k=0;k<4;k++){const w=await line(p);assert(!/MARK HELD/.test(w.bannerShown?w.bannerText:''));await p.waitForTimeout(60);}
          quiet.push(heavyOpen);heavyOpen=null;
        }
        continue;
      }
      assert(await p.evaluate(()=>document.getElementById('l-lost').textContent===String(window.lineView.fails)));
      if(v.leg===3&&ford===0) {
        assert.equal(v.target,49);
        assert.match(await p.locator('#line-goal').textContent(), /ford|motor|winding|catch arm/i);
      }
      if(shots&&!legShot&&v.leg===1&&Math.abs(v.remaining)<8){await shot(p,'line-play');legShot=true;}
      const dir=v.remaining>=0?1:-1,dist=Math.abs(v.remaining),speed=v.coastV*dir;
      const aim=dist<0.8?0:Math.min(0.42,0.22*Math.sqrt(Math.max(0,dist-0.8)));
      const want=speed<aim-0.04?(dir>0?'d':'a'):speed>aim+0.04?(dir>0?'a':'d'):null;
      if(want!==held){if(held)await p.keyboard.up(held);if(want)await p.keyboard.down(want);held=want;}
      await p.waitForTimeout(40);
    }
    const v=await line(p);await p.close();
    return {seen,quiet,ending:v.sceneText,fails:v.fails,time:v.time};
  }
  const nightA=await night(1,0,true),nightB=await night(0,1,true);
  for(const n of [nightA,nightB]){
    assert.deepEqual(n.seen,['hut','stones','bell','spill','ford','cut','dawn']);
    assert.deepEqual(n.quiet,['spill','ford','cut']);
  }
  assert(/heard a bell where no bell should be/.test(nightA.ending)&&/never rang/.test(nightB.ending));
  checks.push(`flood line: two nights with real keys (church + ford bell, ${nightA.time.toFixed(0)} t, ${nightA.fails} failures; mill + siren, ${nightB.time.toFixed(0)} t, ${nightB.fails} failures); 7 scenes in order, both endings, heavy scenes with no banner`);

  // Phone, upright and sideways: touch only. Words 14 CSS px or more, targets 44 px or more, the plot over half the screen.
  for(const [w,h,name] of [[375,812,'line-mobile'],[812,375,null]]){
    const ctx=await browser.newContext({viewport:{width:w,height:h},deviceScaleFactor:2,isMobile:true,hasTouch:true});
    const p=await ctx.newPage();track(p);await p.goto(url);
    await p.waitForFunction(()=>!document.getElementById('line-start').disabled);
    await p.locator('#line-start').tap();await p.locator('#scene-options button').first().tap();
    const b=await p.locator('[data-motor=down]').boundingBox();const cdp=await ctx.newCDPSession(p);
    await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{x:b.x+b.width/2,y:b.y+b.height/2,id:1,radiusX:3,radiusY:3,force:1}]});
    await p.waitForTimeout(2500);
    await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});
    await p.waitForTimeout(600);
    const v=await line(p);assert(v.x>0.3&&v.v>0.05,'touch did not move the turn');
    const m=await p.evaluate(()=>{
      const small=[],tiny=[];
      const vis=e=>{const r=e.getBoundingClientRect();const st=getComputedStyle(e);return r.width>0&&r.height>0&&st.visibility!=='hidden'&&r.bottom>0&&r.top<innerHeight;};
      for(const e of document.querySelectorAll('body *')){if(!vis(e))continue;for(const n of e.childNodes)if(n.nodeType===3&&n.textContent.trim()&&parseFloat(getComputedStyle(e).fontSize)<14)small.push(n.textContent.trim().slice(0,20));}
      for(const e of document.querySelectorAll('button,a[href],input,select')){if(!vis(e))continue;const r=e.getBoundingClientRect();if(r.width<44||r.height<44)tiny.push(e.id||e.textContent.trim().slice(0,20));}
      const c=document.getElementById('line-plot').getBoundingClientRect();
      return {small,tiny,area:Math.min(c.width,innerWidth)*Math.min(c.height,innerHeight)/(innerWidth*innerHeight),overflow:document.documentElement.scrollWidth>innerWidth};
    });
    assert.deepEqual(m.small,[]);assert.deepEqual(m.tiny,[]);assert(m.area>=0.5,`plot share ${m.area}`);assert(!m.overflow);
    if(name)await shot(p,name);
    await ctx.close();
    checks.push(`${w}x${h} touch: hold moves the turn; words >=14 px, targets >=44 px, plot ${(m.area*100).toFixed(0)} % of the screen`);
  }

  const fallback=await browser.newPage({viewport:{width:1280,height:900}});track(fallback,{fallback:true});
  await fallback.route('**/assets/**',route=>route.fulfill({status:404,body:''}));
  await fallback.goto(url);await fallback.waitForFunction(()=>window.fieldView);
  assert(await fallback.locator('#art-warning').isVisible());assert.equal((await inspect(fallback)).artLoaded,false);
  await fallback.locator('#start').click();await fallback.keyboard.down('d');await waitTime(fallback,3);
  await fallback.keyboard.up('d');assert((await inspect(fallback)).position>.1);
  await fallback.close();checks.push('missing illustration keeps readable controls and measured physics');
  assert.deepEqual(errors,[]);assert.deepEqual(consoleErrors,[]);assert.deepEqual(failedResponses,[]);assert.deepEqual(external,[]);
  checks.push('page errors 0; console errors 0; unexpected HTTP failures 0; external requests 0');
  const images=[];
  for(const name of ['before.png','title.jpg','coasting.jpg','contracted.jpg','mobile.jpg','line-play.jpg','line-choice.jpg','line-bottom.jpg','line-ending-ford.jpg','line-ending-siren.jpg','line-mobile.jpg']) {
    const file=`docs/images/${name}`;images.push({file,bytes:(await stat(file)).size});
  }
  const summary={checks,errors,consoleErrors,failedResponses,externalRequests:external,images};
  await writeFile('out/smoke.json',JSON.stringify(summary,null,2)+'\n');
  console.log(JSON.stringify(summary,null,2));
} finally {if(browser)await browser.close();await new Promise(resolve=>server.close(resolve));}
