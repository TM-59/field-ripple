#ifndef LINE_POLICY_H
#define LINE_POLICY_H

/* Shared by the story test and the story bench: seeds and a human-like relay keeper. */
#include "../src/line.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static inline uint64_t mix64(uint64_t z)
{
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

/* Seed and index are stirred separately before they are combined (no XOR or sums of raw keys). */
static inline uint64_t sub_seed(uint64_t seed, uint64_t index, uint64_t salt)
{
    return mix64(mix64(seed + salt * 0x632be59bd9b4e019ULL) + (index + 1u) * 0x9e3779b97f4a7c15ULL);
}

typedef struct { uint64_t s; } Rng;
static inline double rng_unit(Rng *r)
{
    r->s += 0x9e3779b97f4a7c15ULL;
    return (double)(mix64(r->s) >> 11) / 9007199254740992.0;
}

/* The keeper's hand. tests/story_bench.c writes the policy text in docs/narrative/tension.json from these same numbers. */
#define KEEPER_CRUISE_LO 0.34   /* speed it aims for on a span: lo .. lo + span, c */
#define KEEPER_CRUISE_SPAN 0.26
#define KEEPER_MARGIN_LO 0.05   /* it aims this fraction below the speed it could still stop from */
#define KEEPER_MARGIN_SPAN 0.30
#define KEEPER_BAND_LO 0.02     /* it coasts while the readout is this close to its aim, c */
#define KEEPER_BAND_SPAN 0.04
#define KEEPER_HESITATE_LO 1    /* a new command takes after lo + 1 .. lo + steps frames of wanting it */
#define KEEPER_HESITATE_STEPS 4
#define KEEPER_CRAWL_LO 0.04    /* speed it creeps at near the mark, c */
#define KEEPER_CRAWL_SPAN 0.06
#define KEEPER_NEAR 2.0         /* within this distance of the mark it reads the position marker, not the readout */
#define KEEPER_STOP 0.8         /* within this distance it lets the turn stop */

/* A keeper who reads the gauges, not the numbers in the code, and does not hear Kit's call: her reading reaches it only
 * as a steadier speed readout (LE_HUT). Far from the mark it reads the speed readout (which wobbles unless Kit is reading
 * the turn at her end) and keeps the turn near a speed it can still brake from: below its cruising speed and below a
 * rule-of-thumb braking speed for the distance left, with a safety margin. It runs the motor towards the mark when the
 * readout is too slow, against the turn when it is too fast, and coasts inside a dead band. Its thumb is slow: a new
 * command waits a few frames. Near the mark it stops trusting the readout and reads the position marker instead (it does
 * not wobble), creeping in and waiting for the catch arm. */
typedef struct {
    Rng rng;
    uint64_t key;
    int leg, fails, command, pending, delay;
    double cruise, margin, band, crawl;
    int hesitate;
} Keeper;

static inline void keeper_plan(Keeper *p)
{
    p->cruise = KEEPER_CRUISE_LO + KEEPER_CRUISE_SPAN * rng_unit(&p->rng);
    p->margin = KEEPER_MARGIN_LO + KEEPER_MARGIN_SPAN * rng_unit(&p->rng);
    p->band = KEEPER_BAND_LO + KEEPER_BAND_SPAN * rng_unit(&p->rng);
    p->hesitate = KEEPER_HESITATE_LO + (int)(KEEPER_HESITATE_STEPS * rng_unit(&p->rng));
    (void)rng_unit(&p->rng); /* Preserve the original stream: the old, unused marker-watch draw. */
    p->crawl = KEEPER_CRAWL_LO + KEEPER_CRAWL_SPAN * rng_unit(&p->rng);
}

/* The policy as written into tension.json: every number comes from the macros above and from line.h. */
static inline void keeper_describe(char *buf, size_t cap)
{
    snprintf(buf, cap,
        "A relay keeper who reads the gauges, not the code, and does not hear Kit's call (her reading reaches it only as "
        "a steadier speed readout). Far from the mark it reads the wobbling speed readout as the speed the turn will "
        "coast to once the motor spins down, and aims for the lower of its cruising speed (%.2f-%.2f c) and the speed "
        "it could still brake from over the distance left, less a margin of %.0f-%.0f %%; it runs the motor towards the "
        "mark below that aim, against the turn above it, and coasts within %.2f-%.2f c of it. A new command takes after "
        "%d-%d frames (%.1f-%.1f t). Within %.1f units of the mark it reads the position marker instead (it does not "
        "wobble) and creeps in at %.2f-%.2f c; within %.1f it lets the turn stop and waits for the catch arm. On a held "
        "bell it cuts the motor and waits. Its hand is re-drawn from the night's own stirred seed for every leg and "
        "after every failure. Choices follow the path field.",
        KEEPER_CRUISE_LO, KEEPER_CRUISE_LO + KEEPER_CRUISE_SPAN, 100.0 * KEEPER_MARGIN_LO,
        100.0 * (KEEPER_MARGIN_LO + KEEPER_MARGIN_SPAN), KEEPER_BAND_LO, KEEPER_BAND_LO + KEEPER_BAND_SPAN,
        KEEPER_HESITATE_LO + 1, KEEPER_HESITATE_LO + KEEPER_HESITATE_STEPS,
        (KEEPER_HESITATE_LO + 1) * LINE_FRAME, (KEEPER_HESITATE_LO + KEEPER_HESITATE_STEPS) * LINE_FRAME, KEEPER_NEAR,
        KEEPER_CRAWL_LO, KEEPER_CRAWL_LO + KEEPER_CRAWL_SPAN, KEEPER_STOP);
}

static inline void keeper_init(Keeper *p, uint64_t key)
{
    memset(p, 0, sizeof *p);
    p->key = key;
    p->leg = -1;
    p->rng.s = key;
}

static inline int keeper_command(Keeper *p, const Line *l)
{
    /* Each leg gets its own stirred stream, so a choice changes the legs it touches and not, by chance, every later one. */
    if (l->leg != p->leg || l->fails != p->fails) {
        if (l->leg != p->leg) p->rng.s = sub_seed(p->key, (uint64_t)l->leg, 5);
        p->leg = l->leg;
        p->fails = l->fails;
        keeper_plan(p);
        p->command = LC_COAST;
        p->pending = -1;
    }
    /* Holding on the ford bell: motor off and wait. */
    if (l->held > 0.0) return LC_COAST;
    double rem = l->target_x - l->x, dist = fabs(rem), dir = rem >= 0.0 ? 1.0 : -1.0;
    int toward = dir > 0.0 ? LC_DOWN : LC_UP, away = dir > 0.0 ? LC_UP : LC_DOWN;
    double speed, aim, band = p->band;
    if (dist < KEEPER_NEAR) {
        /* Near the mark: speed from the marker (it does not wobble). */
        speed = line_coast_speed(l, l->v) * dir;
        aim = dist < KEEPER_STOP ? 0.0 : fmin(p->crawl, line_brake_speed(l, dist - fmax(speed, 0.0) * LINE_RAMP * 0.8) * (1.0 - p->margin));
        band = dist < KEEPER_STOP ? 0.05 : 0.03;
    } else {
        speed = line_value(l, LV_COAST_V) * dir;
        aim = fmin(p->cruise, line_brake_speed(l, dist - 0.5 - fmax(speed, 0.0) * LINE_RAMP * 0.8) * (1.0 - p->margin));
    }
    int want = speed < aim - band ? toward : speed > aim + band ? away : LC_COAST;
    if (want != p->command) {
        if (want != p->pending) { p->pending = want; p->delay = 0; }
        if (++p->delay > p->hesitate) { p->command = want; p->pending = -1; }
    } else {
        p->pending = -1;
    }
    return p->command;
}

#endif
