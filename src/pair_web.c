/* Browser entry points for the collision and breather panel (src/pair.c over the unchanged src/field.c).
 * The page reads every number and every drawn curve through these; nothing physical is computed in JavaScript.
 * Mode 0: an exact kink-antikink pair at speed v (0.05..0.95), fronts starting at x = -20 and +20.
 * Mode 1: an exact breather at angular frequency omega (0.1..0.95), four periods. */
#include "pair.h"

#include <math.h>
#include <stddef.h>

#define PI 3.14159265358979323846
#define PW_SAMPLES 301
#define PW_COLS 151
#define PW_ROWS 241
#define PW_TRACK 4096
#define PW_HALF 30.0   /* the drawn window is x in [-30, 30] */

enum {
    PW_TIME, PW_START, PW_END, PW_ENERGY, PW_ENERGY_THEORY, PW_MOMENTUM, PW_FRONTS, PW_LEFT, PW_RIGHT,
    PW_SHIFT, PW_SHIFT_THEORY, PW_OMEGA, PW_OMEGA_THEORY, PW_ERROR, PW_ROWS_DONE, PW_DONE, PW_MODE, PW_PARAM,
    PW_SPEED_IN, PW_SPEED_OUT, PW_SEPARATION, PW_SEPARATION_THEORY, PW_VALUES
};

static Field field;
static int ready, mode, rows, done, steps_done, steps_total, crossings, nb, na;
static double param, t_start, t_end, worst, widest, first_cross, last_cross, before0;
static double samples[PW_SAMPLES * 3], history[PW_ROWS * PW_COLS];
static double tb[PW_TRACK], xb[PW_TRACK], ta[PW_TRACK], xa[PW_TRACK];

static double now(void) { return t_start + steps_done * field.dt; }

static double exact(double x, double t)
{
    return mode == 0 ? sg_pair_exact(x, t, param) : sg_breather_exact(x, t, param);
}

/* One row of the space-time map and the largest gap to the exact solution over the whole grid. */
static void record(void)
{
    double t = now();
    for (int c = 0; c < PW_COLS && rows < PW_ROWS; c++)
        history[rows * PW_COLS + c] = field_value_at(&field, -PW_HALF + 2.0 * PW_HALF * c / (PW_COLS - 1));
    rows++;
    for (int j = 0; j < field.n; j++) {
        double gap = fabs(field.cur[j] - exact(field_x(&field, j), t));
        if (gap > worst) worst = gap;
    }
}

int pw_reset(int m, double p)
{
    if (m < 0 || m > 1 || !isfinite(p)) return 1;
    if (m == 0 && (p < 0.05 || p > 0.95)) return 1;
    if (m == 1 && (p < 0.1 || p > 0.95)) return 1;
    if (ready) field_free(&field);
    ready = 0;
    if (field_init(&field, 1201, -60.0, 0.1, 0.05, 0)) return 1;
    ready = 1;
    mode = m;
    param = p;
    if (mode == 0) {
        double gamma = 1.0 / sqrt(1.0 - p * p), lag = log(1.0 / p) / gamma;   /* asymptote |x| = v |t| + lag */
        t_start = -(20.0 - lag) / p;
        t_end = -t_start;
        field_set_pair(&field, p, t_start);
    } else {
        t_start = 0.0;
        t_end = 4.0 * 2.0 * PI / p;
        field_set_breather(&field, p, 0.0, 0.0);
    }
    steps_total = (int)lround((t_end - t_start) / field.dt);
    rows = done = steps_done = crossings = nb = na = 0;
    worst = widest = 0.0;
    first_cross = last_cross = NAN;
    before0 = field_value_at(&field, 0.0);
    for (int i = 0; i < PW_ROWS * PW_COLS; i++) history[i] = 0.0;
    record();
    return 0;
}

static void track(void)
{
    double xs[2], t = now();
    if (field_fronts(&field, xs, 2) != 2) return;
    if (xs[1] - xs[0] > widest) widest = xs[1] - xs[0];
    if (mode != 0 || xs[1] - xs[0] < 16.0) return;
    /* The soliton that enters from the left leaves on the right. */
    if (t < 0.0 && nb < PW_TRACK) { tb[nb] = t; xb[nb++] = xs[0]; }
    if (t > 0.0 && na < PW_TRACK) { ta[na] = t; xa[na++] = xs[1]; }
}

int pw_step(int steps)
{
    if (!ready || steps < 0 || steps > 4000) return 1;
    for (int s = 0; s < steps && !done; s++) {
        field_step(&field);
        steps_done++;
        double v0 = field_value_at(&field, 0.0), t = now();
        if ((before0 < 0.0) != (v0 < 0.0)) {
            double tc = t - field.dt * v0 / (v0 - before0);
            if (isnan(first_cross)) first_cross = tc;
            last_cross = tc;
            crossings++;
        }
        before0 = v0;
        track();
        if (steps_done * (PW_ROWS - 1) >= rows * steps_total) record();
        done = steps_done >= steps_total;
    }
    return 0;
}

double pw_value(int k)
{
    double xs[2] = {NAN, NAN};
    int fronts = ready ? field_fronts(&field, xs, 2) : 0;
    switch (k) {
    case PW_TIME: return now();
    case PW_START: return t_start;
    case PW_END: return t_end;
    case PW_ENERGY: return ready ? field_energy(&field) : 0.0;
    case PW_ENERGY_THEORY: return mode == 0 ? sg_pair_energy(param) : sg_breather_energy(param);
    case PW_MOMENTUM: return ready ? field_momentum(&field) : 0.0;
    case PW_FRONTS: return fronts;
    case PW_LEFT: return fronts >= 1 ? xs[0] : NAN;
    case PW_RIGHT: return fronts >= 2 ? xs[1] : NAN;
    case PW_SHIFT: return mode == 0 && done ? pair_shift_from_tracks(tb, xb, nb, ta, xa, na, 0.0, NULL, NULL) : NAN;
    case PW_SHIFT_THEORY: return mode == 0 ? sg_pair_shift(param) : NAN;
    case PW_OMEGA: return mode == 1 && crossings > 1 ? PI * (crossings - 1) / (last_cross - first_cross) : NAN;
    case PW_OMEGA_THEORY: return mode == 1 ? param : NAN;
    case PW_ERROR: return worst;
    case PW_ROWS_DONE: return rows;
    case PW_DONE: return done;
    case PW_MODE: return mode;
    case PW_PARAM: return param;
    case PW_SPEED_IN: {
        double u = NAN;
        if (mode == 0 && done) pair_shift_from_tracks(tb, xb, nb, ta, xa, na, 0.0, &u, NULL);
        return u;
    }
    case PW_SPEED_OUT: {
        double u = NAN;
        if (mode == 0 && done) pair_shift_from_tracks(tb, xb, nb, ta, xa, na, 0.0, NULL, &u);
        return u;
    }
    case PW_SEPARATION: return widest;
    case PW_SEPARATION_THEORY: return mode == 1 ? sg_breather_separation(param) : NAN;
    default: return NAN;
    }
}

int pw_count(void) { return PW_SAMPLES; }
int pw_columns(void) { return PW_COLS; }
int pw_rows(void) { return PW_ROWS; }

static double density(int j)
{
    double vt = (field.cur[j] - field.prev[j]) / field.dt, vx = field_gradient(&field, j);
    return 0.5 * (vt * vt + vx * vx) + 1.0 - cos(field.cur[j]);
}

/* Interleaved phi, exact phi and energy density over x in [-30, 30]. */
const double *pw_samples(void)
{
    if (!ready) return samples;
    double t = now();
    for (int i = 0; i < PW_SAMPLES; i++) {
        double x = -PW_HALF + 2.0 * PW_HALF * i / (PW_SAMPLES - 1), u = (x - field.x0) / field.dx;
        int j = (int)floor(u);
        if (j < 1) j = 1;
        if (j > field.n - 3) j = field.n - 3;
        double a = fmax(0.0, fmin(1.0, u - j));
        samples[3 * i] = field.cur[j] + a * (field.cur[j + 1] - field.cur[j]);
        samples[3 * i + 1] = exact(x, t);
        samples[3 * i + 2] = density(j) + a * (density(j + 1) - density(j));
    }
    return samples;
}

/* PW_ROWS x PW_COLS values of phi, row r at t_start + r (t_end - t_start) / (PW_ROWS - 1); rows not yet reached are 0. */
const double *pw_history(void) { return history; }
