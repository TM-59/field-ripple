#ifndef LINE_RUN_H
#define LINE_RUN_H

/* One whole night with the human-like keeper; shared by the story test and bench. */
#include "line_policy.h"

#include <string.h>

enum { RUN_GUARD_FRAMES = 6000 };  /* 1,200 t on the night clock: counted as "did not finish" */

typedef struct {
    double seconds, red;
    int fails, completed, ending;
    int crisis[L_LEGS], failure[L_LEGS];
    int order[LS_COUNT], order_len;
    int quiet[LS_COUNT];  /* heavy scene shown with no "mark held" banner, before or after it */
} RunResult;

static const char *const SCENE_ID[LS_COUNT] = {"hut", "stones", "bell", "spill", "ford", "cut", "dawn"};

static inline void run_line(uint64_t key, unsigned effects, int route, int ford, RunResult *out)
{
    Line l;
    Keeper p;
    memset(out, 0, sizeof *out);
    out->ending = -1;
    line_init(&l, effects);
    keeper_init(&p, key);
    int frames = 0;
    while (l.leg < L_LEGS && frames < RUN_GUARD_FRAMES) {
        if (l.scene != LS_NONE) {
            int scene = l.scene, loud = l.loud, calm = l.banner == LB_NONE;
            int option = scene == LS_STONES ? route : scene == LS_FORD ? ford : 0;
            line_answer(&l, option);
            if (line_heavy(scene)) {
                line_tick(&l, 1, keeper_command(&p, &l));  /* the first frame after the scene closes */
                frames++;
                out->quiet[scene] = calm && l.loud == loud && l.banner != LB_HELD;
            }
            continue;
        }
        line_tick(&l, 1, keeper_command(&p, &l));
        frames++;
    }
    out->completed = l.leg >= L_LEGS;
    out->seconds = l.time;
    out->red = l.red;
    out->fails = l.fails;
    out->ending = line_ending(&l);
    for (int k = 0; k < L_LEGS; k++) { out->crisis[k] = l.crisis_leg[k]; out->failure[k] = l.fails_leg[k]; }
    out->order_len = l.order_len;
    memcpy(out->order, l.order, sizeof l.order);
    line_free(&l);
}

/* Seeds: every row and run gets its own stirred key (no XOR or sums of raw keys). */
#define STORY_BASE 20261103ULL
enum { SALT_SEED = 1, SALT_PLAY = 2, SALT_ABLATION = 3, SALT_TENSION = 4 };
static inline uint64_t story_seed(int i) { return sub_seed(STORY_BASE, (uint64_t)i, SALT_SEED); }
static inline uint64_t play_key(int i) { return sub_seed(story_seed(i), 0, SALT_PLAY); }
static inline uint64_t ablation_key(int i) { return sub_seed(story_seed(i), 0, SALT_ABLATION); }
static inline uint64_t tension_key(int i, int run) { return sub_seed(story_seed(i), (uint64_t)run, SALT_TENSION); }

enum { PLAY_SEEDS = 12, ABLATION_SEEDS = 24, TENSION_SEEDS = 16, TENSION_RUNS = 12 };
/* Default choices for the ablation rows: the church bell, and the hill siren. */
enum { DEFAULT_ROUTE = 1, DEFAULT_FORD = 1 };
static inline void play_choices(int i, int *route, int *ford) { *route = i % 2; *ford = (i / 2) % 2; }
static inline void tension_choices(int run, int *route, int *ford) { *route = run % 2; *ford = (run / 2) % 2; }

#endif
