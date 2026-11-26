// Local-only browser check of the music (M63 #34, parent TM-59/project-context#598). Not in CI.
//   make web && node scripts/bgm-smoke.mjs   (port FR_BGM_PORT, default 22081; docs/bgm/smoke.json)
// On the built app it matches screen -> track -> sound at the output (RMS):
//  1) nothing is fetched before the first press; Free observation starts the ballad (observatory)
//  2) Start the night opens the first scene: story music at 0.7x; answering it runs the night
//     (line), a paused night ducks to 0.4x
//  3) mute during the crossfade silences the fading track too (<= -100 dBFS after 0.8 s,
//     TM-59/project-context#668); the ♪ button is 44 px and mutes and restores
//  4) the cue of the heavy scenes, the dawn and the entry (pure, from web/bgm.mjs)
//  5) the size of dist (10 MB)

import assert from 'node:assert/strict';
import { existsSync } from 'node:fs';
import { readdir, readFile, stat, writeFile } from 'node:fs/promises';
import { createServer } from 'node:http';
import os from 'node:os';
import path from 'node:path';
import { chromium } from 'playwright';
import { cue, DUCK, STORY_GAIN } from '../web/bgm.mjs';

const libs = path.join(os.homedir(), '.local/lib/chromium-deps/usr/lib/x86_64-linux-gnu');
if (existsSync(libs))
  process.env.LD_LIBRARY_PATH =
    libs + (process.env.LD_LIBRARY_PATH ? `:${process.env.LD_LIBRARY_PATH}` : '');

const port = Number(process.env.FR_BGM_PORT || 22081);
const server = createServer(async (req, res) => {
  try {
    const pathname = new URL(req.url ?? '/', 'http://localhost').pathname;
    const file = path.resolve('dist', decodeURIComponent(pathname.slice(1)) || 'index.html');
    assert(file.startsWith(path.resolve('dist') + path.sep));
    const type = file.endsWith('.wasm')
      ? 'application/wasm'
      : /\.m?js$/.test(file)
        ? 'text/javascript'
        : '';
    res.writeHead(200, type ? { 'Content-Type': type } : {});
    res.end(await readFile(file));
  } catch {
    if (!res.headersSent) res.writeHead(404);
    res.end();
  }
});
await new Promise((resolve) => server.listen(port, '127.0.0.1', () => resolve(undefined)));

/** @type {{ checks: object[], ok?: boolean, error?: string, seconds?: number, when?: string, fetched?: string[] }} */
const out = { checks: [] };
const t0 = Date.now();
/** @param {string} dir @returns {Promise<number>} */
async function size(dir) {
  let n = 0;
  for (const f of await readdir(dir)) {
    const p = path.join(dir, f);
    const s = await stat(p);
    n += s.isDirectory() ? await size(p) : s.size;
  }
  return n;
}
const browser = await chromium.launch({
  headless: true,
  args: ['--no-sandbox', '--autoplay-policy=user-gesture-required'],
});
try {
  /** @type {string[]} */
  const errors = [];
  /** @type {string[]} */
  const fetched = [];
  const page = await browser.newPage({ viewport: { width: 1280, height: 800 } });
  page.on('pageerror', (e) => errors.push(String(e)));
  page.on('request', (r) => {
    if (r.url().includes('/bgm/')) fetched.push(r.url().split('/').pop() ?? '');
  });
  await page.goto(`http://127.0.0.1:${port}/`);
  await page.waitForFunction(() => window.lineView && window.musicView);
  const music = async () => {
    const m = await page.evaluate(() => window.musicView);
    if (!m) throw Error('no music view');
    return m;
  };
  /** @param {string} track */
  const audible = async (track) => {
    await page.waitForFunction(
      (t) => {
        const m = window.musicView;
        return m?.playing === t && m.loaded.includes(t);
      },
      track,
      { timeout: 60000, polling: 100 },
    );
    await page.waitForTimeout(2800); // the crossfade (2.5 s) is over
    let s = await music();
    for (let i = 0; i < 10; i++) {
      await page.waitForTimeout(250);
      const n = await music();
      if (n.level > s.level) s = n;
    }
    out.checks.push({
      what: `${track} plays`,
      cue: s.cue,
      level: +s.level.toFixed(1),
      gain: s.gain,
    });
    assert(s.running, 'AudioContext running');
    assert(s.level > -45, `${track}: ${s.level.toFixed(1)} dBFS`);
    return s;
  };

  // 1) entry -> observatory; nothing before the first press
  await page.waitForFunction(() => !document.querySelector('#start')?.hasAttribute('disabled'));
  await page.waitForTimeout(1000);
  assert.equal(fetched.length, 0, 'no music is fetched before the first press');
  out.checks.push({ what: 'nothing fetched before the first press', fetched: fetched.length });
  await page.click('#start');
  const o0 = await audible('observatory');
  assert.equal(o0.cue.track, 'observatory');

  // 2) the first scene of the night -> story; answering it runs the night (line)
  await page.click('#back-line');
  await page.click('#line-start');
  await page.waitForFunction(() => window.lineView?.scene === 'hut');
  const s0 = await audible('story');
  assert.equal(s0.cue.gain, STORY_GAIN);
  await page.click('#scene-options button');
  await page.waitForFunction(() => window.lineView?.scene === null && !window.lineView.paused);
  await page.waitForFunction(() => window.musicView?.playing === 'line', null, {
    timeout: 60000,
    polling: 50,
  });

  // 3) mute in the middle of the crossfade: the fading story track goes silent too
  await page.keyboard.press('m');
  await page.waitForTimeout(800);
  const xm = await music();
  out.checks.push({
    what: 'mute during a crossfade silences the fading track too',
    level: +xm.level.toFixed(1),
  });
  assert(xm.muted && xm.level <= -100, `mute during crossfade: ${xm.level.toFixed(1)} dBFS`);
  await page.keyboard.press('m');
  const l0 = await audible('line');
  await page.click('#line-pause');
  await page.waitForFunction(() => window.lineView?.paused);
  await page.waitForTimeout(800);
  const l1 = await music();
  assert(Math.abs(l1.gain / l0.gain - DUCK) < 0.01, 'a paused night ducks to 0.4x');
  assert(l1.level > -60 && l1.level < l0.level, 'still audible, quieter');
  out.checks.push({
    what: 'pause ducks',
    gain: [l0.gain, l1.gain],
    level: [+l0.level.toFixed(1), +l1.level.toFixed(1)],
  });
  const box = await page.locator('#mute').boundingBox();
  assert(box && box.width >= 44 && box.height >= 44, `♪ is 44 px: ${JSON.stringify(box)}`);
  await page.click('#mute');
  await page.waitForTimeout(800);
  const bm = await music();
  assert(bm.muted && bm.level <= -100, `♪ mutes: ${bm.level.toFixed(1)}`);
  await page.click('#mute');
  let bu = await music();
  for (let i = 0; i < 16 && bu.level <= -60; i++) {
    await page.waitForTimeout(250);
    bu = await music();
  }
  assert(!bu.muted && bu.level > -60, 'the ♪ button restores the music');
  out.checks.push({
    what: '♪ mutes and restores',
    size: box,
    level: [+bm.level.toFixed(1), +bu.level.toFixed(1)],
  });

  // 4) the cues of the scenes the check does not swim to
  const view = { mode: 'line', paused: false, heavy: false, scene: null };
  for (const scene of ['spill', 'ford', 'cut'])
    assert.deepEqual(cue({ ...view, scene, heavy: true }), { track: 'story', gain: DUCK });
  assert.deepEqual(cue({ ...view, scene: 'dawn' }), { track: 'dawn', gain: 1 });
  assert.deepEqual(cue({ ...view, mode: 'entry', paused: true }), {
    track: 'observatory',
    gain: 1,
  });
  assert.deepEqual(cue({ ...view, mode: 'free', paused: true }), { track: 'observatory', gain: 1 });
  out.checks.push({
    what: 'cues: heavy scenes story 0.4x, dawn, entry and observatory (paused or not) observatory',
  });

  // 5) dist
  const bytes = await size('dist');
  out.checks.push({ what: 'dist bytes', bytes });
  assert(bytes <= 10_000_000, `dist ${bytes} > 10 MB`);
  out.fetched = [...new Set(fetched)];
  assert.deepEqual(errors, [], 'no page errors');
  out.ok = true;
  console.log('bgm-smoke OK', JSON.stringify(out.checks));
} catch (e) {
  out.ok = false;
  out.error = String(e instanceof Error ? e.stack : e);
  console.error(e);
  process.exitCode = 1;
} finally {
  out.seconds = +((Date.now() - t0) / 1000).toFixed(1);
  out.when = new Date().toISOString();
  await writeFile('docs/bgm/smoke.json', `${JSON.stringify(out, null, 1)}\n`);
  await browser.close();
  server.close();
}
