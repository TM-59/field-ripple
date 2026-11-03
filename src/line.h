#ifndef LINE_H
#define LINE_H

#include "field.h"

/* "The Flood Line": a story layer over the unchanged solver in field.c.
 * The Turn Line is a long chain of pendulums coupled by torsion springs, the
 * mechanical model of the sine-Gordon equation. One full turn of the chain (the
 * kink) runs along it; the relay keeper holds the winding motor, which puts a
 * uniform torque on every pendulum (the bias eps), and parks the turn on the next
 * mark, where a catch arm rings that mark's bell. Five legs in the night of the
 * flood. Story scenes open at fixed progress points (the start of the night and
 * the end of each leg) and change game numbers only: the charge in the motor's
 * cell, which mark is next, how much the speed readout wobbles, a cell that bleeds
 * in the wet and what a lost turn costs. Positive x is down the valley. */

enum { L_LEGS = 5 };
#define LINE_RAMP 6.283185307179586  /* t the motor takes to spin from stopped to full (one period of the line's slowest ringing) */
#define LINE_DT 0.05
#define LINE_SUBSTEPS 4
#define LINE_FRAME (LINE_DT * LINE_SUBSTEPS)       /* t per frame: four solver steps of 0.05 t */
#define LINE_RED_LINE 0.30   /* the cell gauge turns red below this charge */
#define LINE_LEG_LIMIT 200.0 /* t on one leg without a held mark: the turn is lost and relaunched */

enum {
    LS_NONE = -1,
    LS_HUT,     /* Ada winds the cell and sets the rule; Kit at the village end will call the brake */
    LS_STONES,  /* choice: the mill bell (a short span) or the church bell (a long span; the sexton winds back) */
    LS_BELL,    /* the bell rings and the low road starts up the hill; Kit puts her board's cell into the line */
    LS_SPILL,   /* the spillway goes over: the relay hut takes water (heavy: shown with no banner) */
    LS_FORD,    /* the school bus is out on the ford road; choice: the ford bell under the water line, or the hill siren */
    LS_CUT,     /* the river takes the lower posts; Kit's end of the line goes quiet */
    LS_DAWN,    /* first light; ending */
    LS_COUNT
};

/* Numeric story effects, one bit each, so the bench can remove them one by one. */
enum {
    LE_HUT = 1u << 0,     /* Ada winds the cell past the line, and Kit's call steadies the readout */
    LE_ROUTE = 1u << 1,   /* the mill: a short span; the church: a long span, and the sexton winds charge back */
    LE_BOARD = 1u << 2,   /* Kit's board cell goes into the line */
    LE_SPILL = 1u << 3,   /* the spill: in leg 4 the wet cell bleeds and the readout wobbles more */
    LE_FORD = 1u << 4,    /* the ford: leg 4 goes past the water line to the ford bell; hold the turn while it rings */
    LE_CUT = 1u << 5,     /* Kit's call is gone for the last leg, and a dry cell now loses the turn */
    LE_ALL = (1u << 6) - 1u
};

/* Commands: coast, run the motor down the valley (+x), run it up the valley (-x). */
enum { LC_COAST, LC_DOWN, LC_UP };
enum { LB_NONE, LB_HELD, LB_LOST, LB_DRY, LB_KNOCK };

typedef struct {
    Field field;
    int ready;
    unsigned effects;
    int leg;              /* 0..4; L_LEGS when the night is over */
    int scene;            /* LS_* open now, or LS_NONE */
    int route;            /* -1 until chosen; 0 = the mill bell, 1 = the church bell */
    int ford;             /* -1 until chosen; 0 = the ford bell, 1 = the hill siren */
    int count;            /* 1 while Kit reads the turn at her end and calls the brake */
    int command;          /* LC_* applied in the last frame */
    double time;          /* night clock, t; every unit is run through the solver, no flat waits */
    double leg_time;
    double x, v;          /* the turn's position and its speed over the last frame */
    double parked;        /* t the turn has stood still on the mark with the motor off */
    double held;          /* t the turn has been held on a ringing church or ford bell */
    double drift;         /* t left until the motor turns again (a dry cell being cranked, a jammed arm) */
    int refuel;           /* the drift ends with Kit's crank charge (a dry cell), not just a freed arm */
    int knocked;          /* the catch arm was struck once this leg */
    double charge, red;   /* cell charge; t run with the cell gauge in the red */
    double from_x, target_x;
    int fails;
    int lost, dry, knocks;    /* fails by kind: turns lost and relaunched, dry cells cranked, catch arms struck */
    int fails_leg[L_LEGS];
    int crisis_leg[L_LEGS];   /* 1 if the cell dropped below the red line or the leg had a failure */
    int banner;           /* LB_* shown on screen */
    double banner_left;
    int loud;             /* number of times a "mark held" banner was raised */
    long frames;
    int order[LS_COUNT];
    int order_len;
} Line;

enum LineValue {
    LV_X, LV_V, LV_V_READ, LV_TIME, LV_CHARGE, LV_RED, LV_LEG, LV_SCENE, LV_BANNER, LV_FAILS,
    LV_FROM, LV_TARGET, LV_REMAINING, LV_ROUTE, LV_FORD, LV_COUNT_ON, LV_WOBBLE, LV_ENDING, LV_LOUD,
    LV_HEAVY, LV_COMMAND, LV_RED_LINE, LV_HELD, LV_HOLD_NEED, LV_LEAK, LV_DRIFT, LV_CUE, LV_PARKED,
    LV_ENERGY, LV_WIDTH, LV_STOP_DISTANCE, LV_MOTOR, LV_COAST_V, LV_FIELDS
};

void line_init(Line *l, unsigned effects);
void line_free(Line *l);
/* frames of four 0.05 t steps, under one command (LC_*). Does nothing while a scene is open or after the end. */
void line_tick(Line *l, int frames, int command);
/* Close the open scene. Choice scenes take option 0 or 1; others take 0. Returns 0 when accepted. */
int line_answer(Line *l, int option);
int line_options(int scene);
/* 1 for scenes that must be shown without a "mark held" banner (the heavy scenes). */
int line_heavy(int scene);
/* 0 = rang the ford bell, 1 = rang the hill siren; -1 before dawn. */
int line_ending(const Line *l);
/* Absolute wobble of the speed readout right now (0 when the night is over). */
double line_wobble(const Line *l);
/* t the turn must be held on the church or ford bell before the leg ends; 0 for an ordinary leg. */
double line_hold_need(const Line *l);
/* Fastest a turn may meet the catch arm on the mark before it jams the motor. */
double line_arm_limit(const Line *l);
/* Kit's call while her end is alive: 1 = brake now, 2 = it's on my mark, let it go, 0 = nothing. */
int line_cue(const Line *l);
/* Charge the wet cell bleeds per t right now (the spill in leg 4). */
double line_leak(const Line *l);
/* Distance the motor needs to stop a turn moving at speed v (particle approximation: 8 (gamma - 1) / F). */
double line_stop_distance(const Line *l, double v);
/* The motor's full torque right now (|eps| at full dial). */
double line_torque(const Line *l);
/* The fastest speed from which the motor can still stop the turn within distance d (the inverse of the above). */
double line_brake_speed(const Line *l, double d);
/* The motor dial: -1 (full up the valley) .. +1 (full down). */
double line_motor(const Line *l);
/* The speed the turn will have once the motor has spun down from where the dial is now (shown beside the speed). */
double line_coast_speed(const Line *l, double v);
double line_value(const Line *l, int field);

#endif
