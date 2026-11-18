// Read-only observation objects the pages put on window for scripts/smoke.mjs and the
// playability gate. Used only by `tsc --checkJs`; nothing is emitted.

/** window.fieldView: the free observatory (src/observer.c values and the page's controls). */
interface FieldView {
  time: number;
  position: number;
  velocity: number;
  energy: number;
  width: number;
  paused: boolean;
  started: boolean;
  stopped: boolean;
  initialSpeed: number;
  force: number;
  artLoaded: boolean;
  held: string[];
}

/** The C values of "The Flood Line" (enum order of src/line.h, read by name in web/app.mjs). */
interface LineState {
  x: number;
  v: number;
  vRead: number;
  time: number;
  charge: number;
  red: number;
  leg: number;
  scene: number;
  banner: number;
  fails: number;
  from: number;
  target: number;
  remaining: number;
  route: number;
  ford: number;
  count: number;
  wobble: number;
  ending: number;
  loud: number;
  heavy: number;
  command: number;
  redLine: number;
  held: number;
  holdNeed: number;
  leak: number;
  drift: number;
  cue: number;
  parked: number;
  energy: number;
  width: number;
  stopDistance: number;
  motor: number;
  coastV: number;
}

/** One heavy scene as it opened: whether a banner or a loud readout was still showing. */
interface HeavyEntry {
  id: string;
  banner: number;
  loud: number;
  bannerShown: boolean;
}

/** window.lineView. */
interface LineView extends Omit<LineState, 'scene'> {
  paused: boolean;
  scene: string | null;
  bannerShown: boolean;
  bannerText: string | null;
  heavyLog: HeavyEntry[];
  sceneText: string | null;
  goalText: string;
  callText: string;
}

/** The C values of the collision lab (src/pair_web.c), by name. */
interface CollideState {
  time: number;
  start: number;
  end: number;
  energy: number;
  energyTheory: number;
  momentum: number;
  fronts: number;
  left: number;
  right: number;
  shift: number;
  shiftTheory: number;
  omega: number;
  omegaTheory: number;
  gap: number;
  rows: number;
  done: number;
  mode: number;
  param: number;
  speedIn: number;
  speedOut: number;
  separation: number;
  separationTheory: number;
}

/** window.collideView. */
interface CollideView extends Omit<CollideState, 'mode' | 'param'> {
  running: boolean;
  mode: number;
  param: number;
}

interface Window {
  fieldView?: FieldView;
  lineView?: LineView;
  collideView?: CollideView;
  /** Common test hook of the playability gate (project-context rules/playability.json). */
  __tmGate?: object;
}
