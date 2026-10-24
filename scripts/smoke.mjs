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
  for(const name of ['before.png','title.jpg','coasting.jpg','contracted.jpg','mobile.jpg']) {
    const file=`docs/images/${name}`;images.push({file,bytes:(await stat(file)).size});
  }
  const summary={checks,errors,consoleErrors,failedResponses,externalRequests:external,images};
  await writeFile('out/smoke.json',JSON.stringify(summary,null,2)+'\n');
  console.log(JSON.stringify(summary,null,2));
} finally {if(browser)await browser.close();await new Promise(resolve=>server.close(resolve));}
