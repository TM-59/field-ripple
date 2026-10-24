# M4 validation — 2026-10-24

## Scope

The C11 solver in `src/field.c` is unchanged. The new adapter exposes that solver to a browser, and the presentation adds the original laboratory illustration, measured plots, controls and explicit dimensionless units. There is no story, character roster, persistence, random generator, deployment or platform submission in this change.

## Native physics

`make test` passed all three executables: physical field tests, GIF encoding/round-trip tests, and browser-adapter tests. The adapter's position and energy match a separately stepped native field to within 1e-10 after 20 time units. Sample finiteness, the midpoint, width, invalid inputs, both force directions, automatic stopping and reset are checked.

`make bench` passed. The free runs use 800 steps, reaching t = 40:

| Initial speed | Position | Measured speed | Total energy | Width / rest width | Relative energy drift |
|---:|---:|---:|---:|---:|---:|
| 0.0 | 0.000000 | −0.000000 | 7.995556 | 1.000794 | 0.00000098 |
| 0.6 | 23.984791 | 0.599275 | 9.991248 | 0.800965 | 0.00000267 |
| 0.9 | 35.931658 | 0.901312 | 18.297071 | 0.436744 | 0.00004617 |

The expected widths are 1, 0.8 and approximately 0.4359. Maximum measured relative energy drift is 0.004617%. A 30-unit right push reaches v = 0.749365, then a 30-unit opposing push brakes to v = −0.015998, at x = 27.674374. Thresholds for displacement, drift, width and braking are enforced by the bench executable.

This host has no system `cc`. Native validation used an existing temporary Clang toolchain wrapper targeting x86-64 Linux; no system compiler was installed or changed. CI uses its native C compiler. Python is 3.14.4 and Emscripten is 6.0.12.

## Web output

`make web` passed, with no compiler warnings. All output files, rather than only the WASM file, are counted by `scripts/check-size.py`:

| File | Bytes |
|---|---:|
| `dist/app.mjs` | 7,624 |
| `dist/assets/observatory.jpg` | 249,674 |
| `dist/field.mjs` | 6,954 |
| `dist/field.wasm` | 18,782 |
| `dist/index.html` | 6,090 |
| `dist/style.css` | 8,402 |
| **Total** | **297,526** |

The total is below the 10,000,000-byte budget. Build outputs, Node dependencies and caches are ignored. Runtime assets use relative URLs; the application does not request external resources. The illustration is one accepted original image, not an external stock asset.

## Real browser input

`make smoke` passed ten check groups using Playwright 1.64.0 and headless Chromium, serving the built app under `/ripple/`:

1. Entry, decoded original art and C/WASM module load under the subpath.
2. Starting the observation displays the measured stationary field with E approximately 8.
3. Presets at 0.6 c and 0.9 c have the expected width and energy, follow x approximately vt and freeze exactly on pause.
4. Holding D accelerates right; release coasts.
5. Opposing A input brakes.
6. Focus loss pauses and clears held input.
7. At 390 × 844, actual touch input accelerates left; touch cancellation clears it, pause works and horizontal overflow is zero.
8. The actual simulation reaches the finite-domain limit, stops and explains the state; reset recovers.
9. An intentional missing-art response leaves the measured field and controls operational.
10. Page errors, console errors, unexpected HTTP failures and external requests are all zero. The intentional fallback's missing-art response is excluded from unexpected HTTP failures.

The local detail output is `out/smoke.json` (ignored). The four browser screenshots were inspected visually, including the narrow layout. The readouts avoid live-announcement spam, input states are cleared on focus loss and curve styles supplement colour differences. These checks are not a full accessibility audit.

## Actual screenshots

| Path | Dimensions | Bytes |
|---|---|---:|
| `docs/images/before.png` | 1404 × 486 | 5,640 |
| `docs/images/title.jpg` | 1280 × 900 | 144,450 |
| `docs/images/coasting.jpg` | 1280 × 1204 | 140,194 |
| `docs/images/contracted.jpg` | 1280 × 1204 | 140,764 |
| `docs/images/mobile.jpg` | 390 × 1957 | 109,170 |

`before.png` is the retained terminal field rendering. The four after images are full-page captures of the application, not UI mockups. The original artwork and every displayed screenshot were inspected directly.

## Compatibility and limits

`make plots` regenerated `docs/run.csv`, `docs/profile.svg`, `docs/push.svg`, `docs/energy.svg` and `docs/demo.gif`. All five were byte-identical to the pre-change outputs. Existing terminal controls and results are preserved. No saved data exists to migrate.

Human first-use impressions, physical mobile hardware, Firefox/WebKit, real-GPU performance, long sessions and a comprehensive accessibility audit are **unverified / 未検証**. This work does not claim to complete them. No deployment or external submission was performed.
