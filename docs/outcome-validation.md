# Flood Line outcome validation — 2026-11-03

Issues #12 and #13 corrected the distinction between a battery reading and a play result.
The outcomes are now elapsed simulation time (`seconds`, the night clock) and failures
(`fails`, shown in the HUD). Each empty cell needing a crank, catch-arm strike, and lost
turn needing a relaunch increments the failure counter and raises its own banner.
`red` remains available as a `resource` diagnostic and is excluded from outcome tests.
Every metric declares where its effect is felt.

The inherited description/metric changes were retained. Removing an unused random draw
would have changed every subsequent keeper hand, so that draw was restored. The keeper's
cruise, reaction time, margins, and decision algorithm retain their original distributions.
It does not follow Kit's verbal brake cue; Kit affects it through the steadier speed readout.
The generated policy now states this explicitly.

## Changes in the playable layer

The charge at the start of the hut scene is 0.10 instead of 0.30; Ada still winds it to 0.45.
Kit's board cell adds 0.45 instead of 0.28. Wet-cell leakage is 0.014 instead of 0.006 per t.
Without Kit, the needle's wobble amplitude is 0.30 instead of 0.10. The cut leaves 65% of
motor torque instead of 70%, and Ada's stiff arm jams above 0.04 c instead of 0.12 c.
Cranking, freeing an arm, and winding a replacement turn take 8 t; replacements previously
had no wait and the other failures took 3 t. The solver runs throughout these waits;
there are no flat additions to the clock.

The ford bell moves from x=44 to x=49. Its dry cell is connected when the player chooses
the branch, adding 0.40 once and bypassing the leaking cell throughout that leg. Previously
it arrived only after reaching the bell. This makes its longer trip trade time for fewer
failures. The church route retains its longer span, 8 t ring and 0.20 charge. The choice
panel explains the ford's supply before selection. Both endings and all seven scenes remain.

The red threshold (0.30), leg limit (200 t), frame duration, and keeper descriptions come
from constants used by the implementation. `make test` compares the generated metric and
policy descriptions verbatim, checks the actual timing/threshold boundaries, and requires
both outcomes to move in the intended direction for each of the four tagged beats.
It also requires opposite measurable gains for the two paths of each choice.

## Paired ablation

96 paired seeds per arm, base `20261103`, up from 24. Integer failures had many ties in the
smaller sample. A measurable difference requires a two-sided sign test p≤0.05 and relative
mean difference ≥5%, treating ties as non-supporting observations. Values below are means;
time uses the game's dimensionless t, not wall-clock seconds.

| Arm | Night clock t | Failures |
|---|---:|---:|
| All story effects, church + siren | 446.15 | 2.594 |
| All effects removed | 420.36 | 7.219 |
| Hut removed | 510.73 | 9.042 |
| Board-cell reward removed | 499.36 | 7.760 |
| Spill removed | 381.93 | 1.125 |
| Cut removed | 381.49 | 1.323 |
| Mill instead of church | 378.23 | 3.573 |
| Ford bell instead of siren | 474.31 | 1.385 |

Each tagged beat changes both outcomes in its expected direction. The mill is faster but
has more failures; the church has fewer failures but takes longer. The ford bell has fewer
failures but takes longer than the siren. Both choices pass the gain/dominance checks in
this recorded sample. Every ablation row ends below the 1,200 t run guard (maximum 758.4 t).

The generated files cover 972 nights: 12 scene-order runs, 8×96 paired-ablation runs, and
192 independent tension runs. All 192 tension runs finish. Crises by leg average
0.00 / 0.83 / 0.06 / 0.95 / 1.56, with a middle valley. The narrative score changes from
92 to 93 (worst and median of 12 scene-order seeds); independence warnings remain zero.

## Separate seed group and limits

An additional group, base `20261104`, was run after choosing the parameters, without
changing them afterwards. Its four tagged beats also pass on both outcomes in the expected
direction; all ablation rows end below the run guard (maximum 805.2 t), and its 192 tension runs finish.

This group exposes a limit to the choice evidence: the church's failure advantage has
p=0.1253 (56/96 seeds supporting it), and the ford's time disadvantage averages 3.4%, below
the 5% threshold. Both gains keep their expected mean signs, but both choices are classified
as quasi-dominated in this separate group. Thus the recorded sample passes the choice
acceptance checks; stability across seed groups and human choice balance are not established.
No broad balance claim is inferred from the score. The machine-readable summary, including
these failed thresholds, is in [outcome-check.json](outcome-check.json).

Reproduce the separate group with the same Debian toolchain:

```sh
mkdir -p out/independent
cc -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -DSTORY_BASE=20261104ULL \
  -o out/independent/story src/field.c src/line.c tests/story_bench.c -lm
out/independent/story out/independent
```

Long waits, narrower keeper variability, and following the verbal cue were investigated
and rejected: they either increased unfinished nights, removed path tradeoffs, or changed
the benchmark policy. Only generated observations were used; no numeric rows were hand-edited.

## Verification

Native checks used extracted official Debian 13 packages: GCC 14.2.0-19 and glibc/libm
2.41-12+deb13u4. No local Docker process or system compiler installation was used. Before
retuning, this toolchain reproduced the inherited original numeric rows byte for byte.
The compiler's floating-point behavior also depends on the C library, as documented by
[GCC](https://gcc.gnu.org/onlinedocs/gcc/Floating-point-implementation.html).

- `make test`, `make story-check`, `make bench`, `make plots`: pass. Full story regeneration matches all three JSON files byte for byte.
- `src/field.c`, `docs/run.csv`, `docs/{profile,push,energy}.svg`, `docs/demo.gif`: 6/6 byte-identical to the starting revision.
- Physical bench: maximum energy drift 0.004617%; width at 0.9 c is 0.436744; push/brake speed 0.749365 → −0.015998.
- Real C/WASM keyboard/touch smoke: 13/13 checks, two complete nights, seven scenes and both endings, three quiet heavy scenes, page/console errors and external requests all 0.
- Phone gate: 7/7, portrait/landscape progress 7.1/7.1 s, minimum text 14 CSS px, minimum target 165×44 CSS px, plot 59%/61% of the screen.
- Initial download: 335,442 B, below 10,000,000 B. Final play/phone/heavy-scene screenshots were inspected directly.

Physical iPhone/Safari, other browser engines, human first-use experience and long interactive
sessions remain unverified. No deployment, submission, new artwork or external requests were added.
