#ifndef LINE_POLICY_H
#define LINE_POLICY_H

/* Shared by the story test and the story bench: seeds and a human-like relay keeper. */
#include "../src/line.h"

#include <math.h>
#include <stdint.h>
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

/* A keeper who reads the gauges, not the numbers in the code. Far from the mark it reads the speed readout (which
 * wobbles unless Kit is reading the turn at her end) and keeps the turn near a speed it can still brake from: below its
 * cruising speed and below a rule-of-thumb braking speed for the distance left, with a safety margin. It runs the motor
 * towards the mark when the readout is too slow, against the turn when it is too fast, and coasts inside a dead band.
 * Its thumb is slow: a new command waits a few frames. Within two units of the mark it stops trusting the readout and
 * watches the position marker instead (it does not wobble), creeping in and waiting for the catch arm. */
typedef struct {
    Rng rng;
    uint64_t key;
    int leg, fails, command, pending, delay, look, watched;
    double cruise, margin, band, mark_x, crawl, seen_v;
    int hesitate;
} Keeper;

static inline void keeper_plan(Keeper *p)
{
    p->cruise = 0.34 + 0.26 * rng_unit(&p->rng);  /* speed the keeper aims for on this span */
    p->margin = 0.05 + 0.30 * rng_unit(&p->rng);  /* brakes this much below the speed it could still stop from */
    p->band = 0.02 + 0.04 * rng_unit(&p->rng);    /* coasts while the readout is this close to its aim */
    p->hesitate = 1 + (int)(4.0 * rng_unit(&p->rng)); /* frames before a new command takes */
    p->look = 3 + (int)(4.0 * rng_unit(&p->rng)); /* frames of watching the marker near the mark */
    p->crawl = 0.04 + 0.06 * rng_unit(&p->rng);   /* speed it creeps at over the last two units */
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
        p->watched = 0;
        p->mark_x = l->x;
        p->seen_v = 0.0;
    }
    /* Holding on the ford bell: motor off and wait. */
    if (l->held > 0.0) return LC_COAST;
    double rem = l->target_x - l->x, dist = fabs(rem), dir = rem >= 0.0 ? 1.0 : -1.0;
    int toward = dir > 0.0 ? LC_DOWN : LC_UP, away = dir > 0.0 ? LC_UP : LC_DOWN;
    double speed, aim, band = p->band;
    if (dist < 2.0) {
        /* Near the mark: speed from the marker (it does not wobble). */
        speed = line_coast_speed(l, l->v) * dir;
        aim = dist < 0.8 ? 0.0 : fmin(p->crawl, line_brake_speed(l, dist - fmax(speed, 0.0) * LINE_RAMP * 0.8) * (1.0 - p->margin));
        band = dist < 0.8 ? 0.05 : 0.03;
    } else {
        p->watched = 0;
        p->mark_x = l->x;
        p->seen_v = l->v;
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
