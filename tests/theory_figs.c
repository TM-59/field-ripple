/* Regenerates every plotted figure of docs/theory and the table of measured values
 * (docs/theory/figures/values.md). Run with `make theory-figs`. Each figure is produced by the
 * same solver (src/field.c) and story layer (src/line.c) the game uses; nothing is drawn by hand. */
#include "../src/field.h"
#include "../src/line.h"
#include "../src/pair.h"
#include "../src/svg.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PI 3.14159265358979323846
#define MAXN 4096

static char dir[512];
static FILE *values;
static float xs[MAXN], ya[MAXN], yb[MAXN], yc[MAXN];

static const char *path(const char *name)
{
    static char buf[640];
    snprintf(buf, sizeof buf, "%s/%s", dir, name);
    return buf;
}

static void plot(const char *name, const char *title, const char *xl, const char *yl, int count, const Series *s, int ns)
{
    if (svg_plot(path(name), title, xl, yl, xs, count, s, ns) != 0) {
        fprintf(stderr, "cannot write %s\n", name);
        exit(1);
    }
}

/* One row of the measured-values table: id | what | measured | theory | relative error. */
static void value(const char *id, const char *what, double measured, double theory)
{
    double err = theory != 0.0 ? fabs(measured / theory - 1.0) : fabs(measured - theory);
    fprintf(values, "| `%s` | %s | %.6g | %.6g | %.2e |\n", id, what, measured, theory, err);
}

static void run(Field *f, double t)
{
    int steps = (int)(t / f->dt + 0.5);
    for (int s = 0; s < steps; s++) field_step(f);
}

static double density(const Field *f, int j)
{
    double pt = (f->cur[j] - f->prev[j]) / f->dt, gx = field_gradient(f, j);
    return 0.5 * pt * pt + 0.5 * gx * gx + (1.0 - cos(f->cur[j]));
}

/* 01: the potential 1 - cos(phi) and the restoring torque -sin(phi). */
static void fig_potential(void)
{
    int n = 401;
    for (int i = 0; i < n; i++) {
        double phi = -PI + 4.0 * PI * i / (n - 1);
        xs[i] = (float)(phi / PI);
        ya[i] = (float)(1.0 - cos(phi));
        yb[i] = (float)(-sin(phi));
    }
    Series s[] = {{"V = 1 - cos(phi)", ya, "#1f77b4"}, {"-sin(phi)", yb, "#d62728"}};
    plot("potential.svg", "Potential and restoring torque", "phi / pi", "value", n, s, 2);
}

/* 02: kink profiles and their energy density at three speeds, taken from field_set_kink. */
static void fig_kink(void)
{
    double speeds[3] = {0.0, 0.6, 0.9};
    float *prof[3] = {ya, yb, yc};
    static float dens[3][MAXN];
    int n = 1601;
    for (int k = 0; k < 3; k++) {
        Field f;
        if (field_init(&f, n, -8.0, 0.01, 0.005, 0)) exit(1);
        field_set_kink(&f, 0.0, speeds[k], 1);
        for (int j = 0; j < n; j++) {
            xs[j] = (float)field_x(&f, j);
            prof[k][j] = (float)(f.cur[j] / PI);
            dens[k][j] = (float)density(&f, j);
        }
        char id[32], what[96];
        snprintf(id, sizeof id, "kink-energy-v%.1f", speeds[k]);
        snprintf(what, sizeof what, "energy of the initial kink at v = %.1f (theory 8 gamma)", speeds[k]);
        value(id, what, field_energy(&f), 8.0 / sqrt(1.0 - speeds[k] * speeds[k]));
        snprintf(id, sizeof id, "kink-charge-v%.1f", speeds[k]);
        snprintf(what, sizeof what, "topological charge (phi(right) - phi(left)) / 2 pi at v = %.1f", speeds[k]);
        value(id, what, (f.cur[n - 1] - f.cur[0]) / (2.0 * PI), 1.0);
        field_free(&f);
    }
    Series s[] = {{"v = 0", ya, "#1f77b4"}, {"v = 0.6", yb, "#2ca02c"}, {"v = 0.9", yc, "#d62728"}};
    plot("kink-profiles.svg", "Kink profile phi(x) at three speeds", "x", "phi / pi", n, s, 3);
    Series d[] = {{"v = 0", dens[0], "#1f77b4"}, {"v = 0.6", dens[1], "#2ca02c"}, {"v = 0.9", dens[2], "#d62728"}};
    plot("energy-density.svg", "Energy density of the kink", "x", "energy density", n, d, 3);
}

/* 03: after running the solver, width and energy against speed (Lorentz contraction, E = 8 gamma). */
static void fig_contraction(void)
{
    int n = 19;
    for (int i = 0; i < n; i++) {
        double v = 0.05 * i, gamma = 1.0 / sqrt(1.0 - v * v);
        Field f;
        if (field_init(&f, 2000, -100.0, 0.1, 0.05, 0)) exit(1);
        field_set_kink(&f, -20.0, v, 1);
        run(&f, 20.0);
        xs[i] = (float)v;
        ya[i] = (float)(2.0 / field_max_gradient(&f));   /* width 2 / max phi_x = 1 / gamma */
        yb[i] = (float)(1.0 / gamma);
        yc[i] = (float)(field_energy(&f) / 8.0);
        if (i == 12) {
            value("width-v0.6", "width 2 / max(phi_x) after t = 20 at v = 0.6 (theory 1 / gamma)", ya[i], 1.0 / gamma);
            value("energy-v0.6", "energy after t = 20 at v = 0.6 (theory 8 gamma)", field_energy(&f), 8.0 * gamma);
            value("worldline-v0.6", "kink position after t = 20 at v = 0.6 (theory x0 + v t)", field_kink_position(&f), -20.0 + v * 20.0);
        }
        if (i == 18) value("width-v0.9", "width 2 / max(phi_x) after t = 20 at v = 0.9 (theory 1 / gamma)", ya[i], 1.0 / gamma);
        field_free(&f);
    }
    Series s[] = {{"measured", ya, "#1f77b4"}, {"sqrt(1 - v^2)", yb, "#d62728"}};
    plot("contraction.svg", "Lorentz contraction of the kink", "speed v (c = 1)", "width", n, s, 2);
    for (int i = 0; i < n; i++) yb[i] = (float)(1.0 / sqrt(1.0 - xs[i] * xs[i]));
    Series e[] = {{"measured E / 8", yc, "#1f77b4"}, {"gamma", yb, "#d62728"}};
    plot("energy-gamma.svg", "Energy of the moving kink", "speed v (c = 1)", "E / 8", n, e, 2);
}

/* Angular frequency of a small standing wave amp cos(k x), from the time of its first zero at x = 0. */
static double measured_omega(double k, int n, double dx, double dt)
{
    Field f;
    if (field_init(&f, n, 0.0, dx, dt, 1)) exit(1);
    field_set_wave(&f, 1e-4, k);
    double t = 0.0, before = f.cur[0], quarter = NAN;
    for (int s = 0; s < 200000 && isnan(quarter); s++) {
        field_step(&f);
        t += dt;
        if (before > 0.0 && f.cur[0] <= 0.0) quarter = t - dt * (0.0 - f.cur[0]) / (before - f.cur[0]);
        before = f.cur[0];
    }
    field_free(&f);
    return PI / (2.0 * quarter);
}

/* 04: dispersion omega^2 = 1 + k^2, the grid's own dispersion, and the measured points. */
static void fig_dispersion(void)
{
    int n = 400, count = 40;
    double dx = 0.1, dt = 0.05, length = n * dx;
    for (int m = 1; m <= count; m++) {
        double k = 2.0 * PI * (4 * m) / length;        /* k from 0.63 to 25.1: up to 0.8 of the grid's Nyquist pi/dx */
        double grid = 4.0 / (dx * dx) * pow(sin(0.5 * k * dx), 2.0) + 1.0;   /* linearised right-hand side */
        double disc = 2.0 / dt * asin(fmin(1.0, 0.5 * dt * sqrt(grid)));
        double w = measured_omega(k, n, dx, dt);
        xs[m - 1] = (float)k;
        ya[m - 1] = (float)sqrt(1.0 + k * k);
        yb[m - 1] = (float)disc;
        yc[m - 1] = (float)w;
        if (m == 1) value("dispersion-k0.63", "omega of a small wave at k = 0.628 (theory sqrt(1 + k^2))", w, sqrt(1.0 + k * k));
        if (m == 20) value("dispersion-grid-k12.6", "omega at k = 12.57 against the grid's own dispersion relation", w, disc);
    }
    Series s[] = {{"sqrt(1 + k^2)", ya, "#d62728"}, {"grid relation", yb, "#2ca02c"}, {"measured", yc, "#1f77b4"}};
    plot("dispersion.svg", "Dispersion of small waves", "wavenumber k", "omega", count, s, 3);
}

/* 05: relative energy error of a moving kink on the game's grid and on a grid twice as fine in x and t
 * (a second-order scheme: the error should fall by about four). */
static void fig_energy_error(void)
{
    double dxs[2] = {0.1, 0.05}, worst[2] = {0.0, 0.0};
    float *out[2] = {ya, yb};
    int samples = 201;
    for (int k = 0; k < 2; k++) {
        Field f;
        if (field_init(&f, (int)(200.0 / dxs[k]), -100.0, dxs[k], 0.5 * dxs[k], 0)) exit(1);
        field_set_kink(&f, -20.0, 0.5, 1);
        double exact = 8.0 / sqrt(1.0 - 0.25);
        for (int i = 0; i < samples; i++) {
            if (i) run(&f, 0.2);
            double rel = field_energy(&f) / exact - 1.0;
            xs[i] = (float)(0.2 * i);
            out[k][i] = (float)(rel * 1e3);
            if (fabs(rel) > worst[k]) worst[k] = fabs(rel);
        }
        field_free(&f);
    }
    value("energy-error-game-grid", "largest relative error of the energy against 8 gamma over t = 40, v = 0.5, dx = 0.1, dt = 0.05 (ideal 0)", worst[0], 0.0);
    value("energy-error-ratio", "ratio of that error to the error on a grid twice as fine in x and t (second order: 4)", worst[0] / worst[1], 4.0);
    Series s[] = {{"dx = 0.1", ya, "#d62728"}, {"dx = 0.05", yb, "#1f77b4"}};
    plot("energy-error.svg", "Energy error against the exact 8 gamma", "time t", "relative error x 1000", samples, s, 2);
}

/* 06: growth of the shortest grid wave after 400 steps against dt/dx (the CFL limit). */
static void fig_cfl(void)
{
    int count = 61, n = 200;
    double dx = 0.1, limit = 2.0 / sqrt(4.0 + dx * dx);
    for (int i = 0; i < count; i++) {
        double ratio = 0.5 + 0.01 * i;
        Field f;
        if (field_init(&f, n, 0.0, dx, ratio * dx, 1)) exit(1);
        for (int j = 0; j < n; j++) f.cur[j] = f.prev[j] = (j % 2 ? -1e-9 : 1e-9);   /* the sawtooth mode k = pi/dx */
        for (int s = 0; s < 400; s++) field_step(&f);
        double m = 0.0;
        for (int j = 0; j < n; j++) if (fabs(f.cur[j]) > m) m = fabs(f.cur[j]);
        xs[i] = (float)ratio;
        ya[i] = (float)log10(fmin(m, 1e30) / 1e-9);
        yb[i] = ratio > limit ? ya[i] : 0.0f;
        field_free(&f);
    }
    double first = NAN;
    for (int i = 0; i < count && isnan(first); i++) if (ya[i] > 1.0f) first = xs[i];
    value("cfl-limit", "smallest dt/dx on the 0.01 grid of ratios at which the sawtooth mode grows tenfold (theory: first ratio above 2 / sqrt(4 + dx^2))",
          first, ceil(limit * 100.0) / 100.0 > limit ? ceil(limit * 100.0) / 100.0 : limit + 0.01);
    Series s[] = {{"growth", ya, "#1f77b4"}, {"past the limit", yb, "#d62728"}};
    plot("cfl.svg", "Stability of the scheme against dt/dx", "dt / dx", "log10 (amplitude / initial)", count, s, 2);
}

/* 07: momentum of a kink pushed by a uniform bias, against F t with F = 2 pi eps. */
static void fig_push(void)
{
    double eps = 0.01, force = 2.0 * PI * eps;
    int count = 101;
    Field f;
    if (field_init(&f, 3000, -200.0, 0.1, 0.05, 0)) exit(1);
    f.eps = eps;
    field_set_kink(&f, 60.0, 0.0, 1);   /* a positive bias pushes a rising kink towards -x */
    double before = field_kink_position(&f);
    xs[0] = ya[0] = yb[0] = yc[0] = 0.0f;
    for (int i = 1; i < count; i++) {
        run(&f, 2.0);
        double x = field_kink_position(&f), v = (x - before) / 2.0, t = 2.0 * i - 1.0;   /* speed at the middle of the interval */
        double p = force * t, theory = p / sqrt(p * p + 64.0);
        before = x;
        xs[i] = (float)t;
        ya[i] = (float)fabs(v);
        yb[i] = (float)theory;
        yc[i] = (float)(8.0 * fabs(v) / sqrt(1.0 - v * v));
        if (i == 50) {
            value("push-speed-t99", "speed of the pushed kink at t = 99, eps = 0.01 (theory p / sqrt(p^2 + 64), p = 2 pi eps t)", fabs(v), theory);
            value("push-momentum-t99", "momentum 8 gamma v at t = 99 (theory 2 pi eps t)", yc[i], p);
        }
    }
    field_free(&f);
    Series s[] = {{"measured", ya, "#1f77b4"}, {"p / sqrt(p^2+64)", yb, "#d62728"}};
    plot("push-speed.svg", "A uniform bias pushes the kink like a relativistic particle", "time t", "speed |v|", count, s, 2);
    for (int i = 0; i < count; i++) yb[i] = (float)(force * xs[i]);
    Series m[] = {{"8 gamma v", yc, "#1f77b4"}, {"F t", yb, "#d62728"}};
    plot("push-momentum.svg", "Momentum grows linearly under a constant force", "time t", "momentum |p|", count, m, 2);
}

/* 08: the same small pulse (it travels at the group velocity, below c) after reflecting from a clamped end and after wrapping a periodic box. */
static void fig_boundary(void)
{
    int n = 400;
    float *out[2] = {ya, yb};
    for (int k = 0; k < 2; k++) {
        Field f;
        if (field_init(&f, n, 0.0, 0.1, 0.05, k)) exit(1);
        for (int j = 0; j < n; j++) {
            double x = field_x(&f, j);
            f.cur[j] = 1e-3 * exp(-pow(x - 32.0, 2.0) / 2.0);
            f.prev[j] = 1e-3 * exp(-pow(x - 32.0 + f.dt, 2.0) / 2.0);   /* moving to +x at about c */
        }
        run(&f, 24.0);
        for (int j = 0; j < n; j++) {
            xs[j] = (float)field_x(&f, j);
            out[k][j] = (float)(f.cur[j] * 1e3);
        }
        field_free(&f);
    }
    Series s[] = {{"clamped end", ya, "#d62728"}, {"periodic", yb, "#1f77b4"}};
    plot("boundary.svg", "A pulse 24 t after leaving x = 32 (the grid ends at x = 40)", "x", "phi x 1000", n, s, 2);
}

/* 09: where phi crosses pi: the exact kink, the grid samples and the interpolated position. */
static void fig_position(void)
{
    int n = 161;
    Field coarse;
    double centre = 0.137;
    if (field_init(&coarse, 41, -10.0, 0.5, 0.05, 0)) exit(1);
    field_set_kink(&coarse, centre, 0.0, 1);
    double found = field_kink_position(&coarse);
    for (int i = 0; i < n; i++) {
        double x = -2.0 + 4.0 * i / (n - 1), g = (x + 10.0) / 0.5;
        int j = (int)floor(g);
        xs[i] = (float)x;
        ya[i] = (float)(4.0 * atan(exp(x - centre)) / PI);
        yb[i] = (float)((coarse.cur[j] + (g - j) * (coarse.cur[j + 1] - coarse.cur[j])) / PI);   /* the grid's straight segments */
        yc[i] = 1.0f;
    }
    value("position-dx0.5", "interpolated kink position on a coarse grid dx = 0.5 (true centre 0.137)", found, centre);
    field_free(&coarse);
    Field fine;
    if (field_init(&fine, 2000, -100.0, 0.1, 0.05, 0)) exit(1);
    field_set_kink(&fine, centre, 0.0, 1);
    value("position-dx0.1", "interpolated kink position on the game's grid dx = 0.1 (true centre 0.137)", field_kink_position(&fine), centre);
    field_free(&fine);
    Series s[] = {{"exact kink", ya, "#1f77b4"}, {"grid dx = 0.5", yb, "#d62728"}, {"phi = pi", yc, "#888888"}};
    plot("position.svg", "Locating the kink: where phi crosses pi", "x", "phi / pi", n, s, 3);
}

/* 10: The Flood Line: stopping distance against speed, with the full and the slack torque. */
static void fig_stop(void)
{
    Line full, slack;
    line_init(&full, LE_ALL);
    line_init(&slack, LE_ALL);
    slack.leg = 4;
    slack.count = 0;
    int n = 91;
    for (int i = 0; i < n; i++) {
        double v = 0.01 * i;
        xs[i] = (float)v;
        ya[i] = (float)line_stop_distance(&full, v);
        yb[i] = (float)line_stop_distance(&slack, v);
    }
    double v = 0.5, d = line_stop_distance(&full, v);
    value("stop-distance-v0.5", "stopping distance from v = 0.5 at full torque (theory 8 (gamma - 1) / (2 pi 0.08))", d,
          8.0 * (1.0 / sqrt(0.75) - 1.0) / (2.0 * PI * 0.08));
    value("brake-speed-inverse", "line_brake_speed applied to that distance (theory 0.5)", line_brake_speed(&full, d), v);
    value("torque-slack", "motor torque in the last leg after the cut, relative to full (theory 0.65)", line_torque(&slack) / line_torque(&full), 0.65);
    line_free(&full);
    line_free(&slack);
    Series s[] = {{"full torque", ya, "#1f77b4"}, {"slack (x0.65)", yb, "#d62728"}};
    plot("stop-distance.svg", "Distance the motor needs to stop a turn", "speed v (c = 1)", "stopping distance", n, s, 2);
}

/* 11: The Flood Line: the first leg under "run down the valley for 12 t, then coast". */
static void fig_motor(void)
{
    Line l;
    line_init(&l, LE_ALL);
    line_answer(&l, 0);                    /* close the opening scene: the first leg starts */
    int n = 121;
    static float charge[MAXN];
    double start = line_value(&l, LV_CHARGE), dial_at_ramp = 0.0;
    for (int i = 0; i < n; i++) {
        if (i) line_tick(&l, 1, i <= 60 ? LC_DOWN : LC_COAST);
        xs[i] = (float)(LINE_FRAME * i);
        ya[i] = (float)line_value(&l, LV_MOTOR);
        yb[i] = (float)line_value(&l, LV_V);
        charge[i] = (float)line_value(&l, LV_CHARGE);
        if (fabs(xs[i] - LINE_RAMP) < 0.5 * LINE_FRAME) dial_at_ramp = ya[i];
    }
    value("motor-ramp", "motor dial one ramp time (2 pi t) after switching on (theory 1: full)", dial_at_ramp, 1.0);
    /* Charge drawn while the dial ramps 0 -> 1 over 2 pi and holds at 1 until t = 12, then ramps down over 2 pi. */
    value("motor-drain", "charge used by that run (theory 0.005 x integral of the dial = 0.005 x 12)", start - charge[n - 1], 0.005 * 12.0);
    line_free(&l);
    Series s[] = {{"motor dial", ya, "#d62728"}, {"speed v", yb, "#1f77b4"}};
    plot("motor-ramp.svg", "The winding motor spins up and down over 2 pi t", "time t", "dial and speed", n, s, 2);
    Series c[] = {{"cell charge", charge, "#2ca02c"}};
    plot("motor-charge.svg", "Charge drawn by the motor in that run", "time t", "charge", n, c, 1);
}

/* 12: The Flood Line: the wobble of the speed readout with and without the call from the far end. */
static void fig_wobble(void)
{
    Line called, alone;
    line_init(&called, LE_ALL);
    line_init(&alone, LE_ALL);
    called.count = 1;
    alone.count = 0;
    int n = 301;
    double peak[2] = {0.0, 0.0};
    for (int i = 0; i < n; i++) {
        called.time = alone.time = 0.05 * i;
        xs[i] = (float)(0.05 * i);
        ya[i] = (float)line_wobble(&alone);
        yb[i] = (float)line_wobble(&called);
        if (fabs(ya[i]) > peak[0]) peak[0] = fabs(ya[i]);
        if (fabs(yb[i]) > peak[1]) peak[1] = fabs(yb[i]);
    }
    value("wobble-call", "largest readout wobble with the call, relative to without it, over t = 15 (theory 0.10)", peak[1] / peak[0], 0.10);
    line_free(&called);
    line_free(&alone);
    Series s[] = {{"no call", ya, "#d62728"}, {"with the call", yb, "#1f77b4"}};
    plot("wobble.svg", "Wobble added to the speed readout", "night clock t", "added to the shown speed", n, s, 2);
}

/* 13: The Flood Line: the three answers of the call from the far end, on states set by hand. */
static void check_cue(void)
{
    Line l;
    line_init(&l, LE_ALL);
    line_answer(&l, 0);                       /* the first leg: mark at 18, the far end alive */
    l.x = 17.5; l.v = 0.0;
    value("cue-on-mark", "call with the turn standing 0.5 from the mark (2 = let it go)", line_cue(&l), 2.0);
    l.x = 14.0; l.v = 0.6;
    value("cue-brake", "call 4.0 from the mark at v = 0.6, where the stopping distance is 3.98 (1 = brake)", line_cue(&l), 1.0);
    l.x = 2.0; l.v = 0.2;
    value("cue-nothing", "call 16 from the mark at v = 0.2 (0 = nothing)", line_cue(&l), 0.0);
    value("cue-estimate", "coast speed shown for v = 0.2 with the motor off (theory 0.2: nothing to add)", line_coast_speed(&l, 0.2), 0.2);
    l.count = 0;
    l.x = 17.5; l.v = 0.0;
    value("cue-silent", "call once the far end is gone (0 = nothing)", line_cue(&l), 0.0);
    line_free(&l);
}

/* 14: kink-antikink pair from the exact solution at t = -40, v = 0.5: the solver against Perring-Skyrme. */
static void fig_pair(void)
{
    double v = 0.5, t0 = -40.0, worst = 0.0;
    static float d[2][MAXN];
    Field f;
    if (field_init(&f, 1201, -60.0, 0.1, 0.05, 0)) exit(1);
    field_set_pair(&f, v, t0);
    int n = 601;
    for (int k = 1; k <= 8; k++) {
        run(&f, 10.0);
        double t = t0 + 10.0 * k;
        for (int j = 0; j < f.n; j++) {
            double g = fabs(f.cur[j] - sg_pair_exact(field_x(&f, j), t, v));
            if (g > worst) worst = g;
        }
        if (k == 3 || k == 5) {   /* t = -10 and t = +10 */
            int m = k == 3 ? 0 : 1;
            for (int i = 0; i < n; i++) {
                double x = -30.0 + 0.1 * i;
                xs[i] = (float)x;
                d[m][i] = (float)(field_value_at(&f, x) / PI);
                (m ? yb : ya)[i] = (float)(sg_pair_exact(x, t, v) / PI);
            }
        }
    }
    field_free(&f);
    value("pair-gap-v0.5", "largest |phi - Perring-Skyrme| (rad) from t = -40 to 40, v = 0.5, dx = 0.1 (ideal 0)", worst, 0.0);
    Series s[] = {{"solver t = -10", d[0], "#1f77b4"}, {"exact t = -10", ya, "#9ecae1"},
                  {"solver t = +10", d[1], "#d62728"}, {"exact t = +10", yb, "#fcae91"}};
    plot("pair-snapshots.svg", "Kink-antikink pair at v = 0.5 before and after it meets", "x", "phi / pi", n, s, 4);
}

/* 15: the centres of the pair (|phi| = pi) against time, and the phase shift against speed. */
static void fig_shift(void)
{
    double v = 0.3, gamma = 1.0 / sqrt(1.0 - v * v), lag = log(1.0 / v) / gamma, t0 = -(20.0 - lag) / v;
    static float free_in[MAXN];
    Field f;
    if (field_init(&f, 1201, -60.0, 0.1, 0.05, 0)) exit(1);
    field_set_pair(&f, v, t0);
    int n = 241;
    double span = -2.0 * t0;
    for (int i = 0; i < n; i++) {
        if (i) run(&f, span / (n - 1));
        double t = t0 + span * i / (n - 1), c[2];
        int k = field_fronts(&f, c, 2);
        xs[i] = (float)t;
        ya[i] = (float)(k == 2 ? c[0] : 0.0);   /* 0 while |phi| < pi everywhere: the two have merged at x = 0 */
        yb[i] = (float)(k == 2 ? c[1] : 0.0);
        free_in[i] = (float)(v * t - lag);       /* the left soliton's path had it not met the other */
    }
    field_free(&f);
    Series w[] = {{"left centre", ya, "#1f77b4"}, {"right centre", yb, "#d62728"}, {"free path of the left one", free_in, "#888888"}};
    plot("pair-worldlines.svg", "Centres of the pair at v = 0.3: they pass through", "time t", "x where |phi| = pi", n, w, 3);

    int m = 19;
    for (int i = 0; i < m; i++) {
        double u = 0.05 * (i + 1);
        xs[i] = (float)u;
        ya[i] = (float)sg_pair_shift(u);
        yb[i] = (float)pair_measure_shift(u, 0.1, NULL, NULL);
    }
    double in, out;
    value("shift-v0.1", "forward shift after the collision at v = 0.1 (theory 2 sqrt(1 - v^2) ln(1/v))", pair_measure_shift(0.1, 0.1, NULL, NULL), sg_pair_shift(0.1));
    value("shift-v0.5", "forward shift after the collision at v = 0.5 (theory 2 sqrt(1 - v^2) ln(1/v))", pair_measure_shift(0.5, 0.1, &in, &out), sg_pair_shift(0.5));
    value("speed-out-v0.5", "fitted speed after the collision against before it, v = 0.5 (theory: equal)", out, in);
    value("shift-v0.9", "forward shift after the collision at v = 0.9, dx = 0.1", pair_measure_shift(0.9, 0.1, NULL, NULL), sg_pair_shift(0.9));
    value("shift-v0.95", "forward shift at v = 0.95, dx = 0.1: the width 1/gamma = 0.31 is three grid cells", pair_measure_shift(0.95, 0.1, NULL, NULL), sg_pair_shift(0.95));
    Series s[] = {{"2 sqrt(1-v^2) ln(1/v)", ya, "#d62728"}, {"measured", yb, "#1f77b4"}};
    plot("pair-shift.svg", "Forward shift of each soliton after the collision", "speed v (c = 1)", "shift", m, s, 2);
}

/* 16: energy reading and momentum through a collision of the superposed pair at v = 0.5 (they meet at t = 40). */
static void fig_pair_conserved(void)
{
    Field f;
    if (field_init(&f, 2401, -120.0, 0.1, 0.05, 0)) exit(1);
    field_set_superposed_pair(&f, 20.0, 0.5);
    double e0 = field_energy(&f), worst_p = 0.0;
    int n = 401;
    for (int i = 0; i < n; i++) {
        if (i) run(&f, 0.25);
        double p = field_momentum(&f);
        xs[i] = (float)(0.25 * i);
        ya[i] = (float)((field_energy(&f) / e0 - 1.0) * 1e3);
        yb[i] = (float)(p * 1e10);
        if (fabs(p) > worst_p) worst_p = fabs(p);
    }
    value("pair-energy-v0.5", "energy of the superposed pair at v = 0.5 (theory 16 gamma)", e0, sg_pair_energy(0.5));
    value("pair-energy-after", "energy at t = 100 (60 t after the collision) against the start (theory: equal)", field_energy(&f), e0);
    value("pair-momentum", "largest |P| of the symmetric pair through the collision (theory 0)", worst_p, 0.0);
    field_free(&f);
    Series s[] = {{"(E / E0 - 1) x 1000", ya, "#2ca02c"}, {"P x 1e10", yb, "#1f77b4"}};
    plot("pair-conserved.svg", "Energy reading and momentum through the collision (meet at t = 40)", "time t", "scaled value", n, s, 2);
}

/* 17: the breather: phi(0, t) against the exact solution, its frequency and its energy. */
static void fig_breather(void)
{
    double w = 0.5;
    Field f;
    if (field_init(&f, 1201, -60.0, 0.1, 0.05, 0)) exit(1);
    field_set_breather(&f, w, 0.0, 0.0);
    int n = 601;
    for (int i = 0; i < n; i++) {
        if (i) run(&f, 0.1);
        xs[i] = (float)(0.1 * i);
        ya[i] = (float)field_value_at(&f, 0.0);
        yb[i] = (float)sg_breather_exact(0.0, 0.1 * i, w);
    }
    field_free(&f);
    Series s[] = {{"solver phi(0, t)", ya, "#1f77b4"}, {"exact", yb, "#d62728"}};
    plot("breather-centre.svg", "Breather at omega = 0.5: phi at its centre", "time t", "phi(0, t)", n, s, 2);

    double drift, swing, fine;
    value("breather-omega-0.3", "measured angular frequency over 20 periods, omega = 0.3", breather_measure_frequency(0.3, 0.1, 20, NULL, NULL, NULL), 0.3);
    value("breather-omega-0.5", "measured angular frequency over 20 periods, omega = 0.5", breather_measure_frequency(0.5, 0.1, 20, &drift, &swing, NULL), 0.5);
    breather_measure_frequency(0.5, 0.05, 20, NULL, &fine, NULL);
    value("breather-omega-0.8", "measured angular frequency over 20 periods, omega = 0.8", breather_measure_frequency(0.8, 0.1, 20, NULL, NULL, NULL), 0.8);
    value("breather-drift-0.5", "largest change of E between breaths (field at rest), omega = 0.5 (ideal 0)", drift, 0.0);
    value("breather-swing-ratio", "within-breath swing of the energy reading, dx = 0.1 over dx = 0.05 (second order: 4)", swing / fine, 4.0);

    int m = 19;
    for (int i = 0; i < m; i++) {
        double u = 0.05 * (i + 1);
        Field g;
        if (field_init(&g, 1201, -60.0, 0.1, 0.05, 0)) exit(1);
        field_set_breather(&g, u, 0.5 * PI / u, 0.0);   /* a peak: the field stands still */
        xs[i] = (float)u;
        ya[i] = (float)sg_breather_energy(u);
        yb[i] = (float)field_energy(&g);
        if (i == 9) value("breather-energy-0.5", "energy of the breather at omega = 0.5 (theory 16 sqrt(1 - omega^2))", field_energy(&g), sg_breather_energy(u));
        field_free(&g);
    }
    Series e[] = {{"16 sqrt(1 - omega^2)", ya, "#d62728"}, {"measured", yb, "#1f77b4"}};
    plot("breather-energy.svg", "Energy of the breather", "angular frequency omega", "energy", m, e, 2);
}

/* 18: bound and unbound: front separation of a breather and of a pair, and the energy threshold 16. */
static void fig_bound(void)
{
    double w = 0.3, v = 0.3;
    Field b, p;
    if (field_init(&b, 1201, -60.0, 0.1, 0.05, 0) || field_init(&p, 1201, -60.0, 0.1, 0.05, 0)) exit(1);
    field_set_breather(&b, w, 0.0, 0.0);
    field_set_pair(&p, v, 0.0);
    int n = 301;
    double wide = 0.0;
    for (int i = 0; i < n; i++) {
        if (i) { run(&b, 0.25); run(&p, 0.25); }
        double c[2];
        xs[i] = (float)(0.25 * i);
        ya[i] = (float)(field_fronts(&b, c, 2) == 2 ? c[1] - c[0] : 0.0);
        if (ya[i] > wide) wide = ya[i];
        yb[i] = (float)(field_fronts(&p, c, 2) == 2 ? c[1] - c[0] : 0.0);
        yc[i] = (float)sg_breather_separation(w);
    }
    field_free(&b);
    field_free(&p);
    value("breather-separation-0.3", "widest front separation of the breather, omega = 0.3 (theory 2 arccosh(nu/omega)/nu)", wide, sg_breather_separation(w));
    Series s[] = {{"breather omega = 0.3", ya, "#1f77b4"}, {"pair v = 0.3", yb, "#d62728"}, {"2 arccosh(nu/omega)/nu", yc, "#888888"}};
    plot("bound-unbound.svg", "A breather stays bound; a pair flies apart", "time t", "distance between the centres", n, s, 3);

    int m = 19;
    for (int i = 0; i < m; i++) {
        double u = 0.05 * (i + 1);
        xs[i] = (float)u;
        ya[i] = (float)sg_pair_energy(u);
        yb[i] = (float)sg_breather_energy(u);
        yc[i] = 16.0f;
    }
    Series e[] = {{"pair 16 gamma(v)", ya, "#d62728"}, {"breather 16 sqrt(1-omega^2)", yb, "#1f77b4"}, {"two kinks at rest", yc, "#888888"}};
    plot("threshold.svg", "Energy of a pair and of a breather against 16", "v (pair) or omega (breather)", "energy", m, e, 3);
}

/* 19: grid limits: error of the shift (v = 0.7) and of the breather frequency (omega = 0.3) against dx. */
static void fig_collision_grid(void)
{
    double dxs[4] = {0.4, 0.2, 0.1, 0.05}, es[4], ew[4];
    for (int i = 0; i < 4; i++) {
        es[i] = fabs(pair_measure_shift(0.7, dxs[i], NULL, NULL) / sg_pair_shift(0.7) - 1.0);
        ew[i] = fabs(breather_measure_frequency(0.3, dxs[i], 10, NULL, NULL, NULL) / 0.3 - 1.0);
        xs[i] = (float)log10(dxs[i]);
        ya[i] = (float)log10(es[i]);
        yb[i] = (float)log10(ew[i]);
        yc[i] = (float)(log10(ew[2]) + 2.0 * (xs[i] - log10(0.1)));   /* slope 2 through the game grid */
    }
    value("shift-order", "error of the v = 0.7 shift on dx = 0.1 over dx = 0.05 (second order: 4)", es[2] / es[3], 4.0);
    value("omega-order", "error of the omega = 0.3 frequency on dx = 0.1 over dx = 0.05 (second order: 4)", ew[2] / ew[3], 4.0);
    value("omega-0.2-dx0.05", "measured frequency of the omega = 0.2 breather on dx = 0.05", breather_measure_frequency(0.2, 0.05, 20, NULL, NULL, NULL), 0.2);
    /* A slow kink on a coarse grid: no Peierls-Nabarro pinning is visible at this resolution. */
    Field f;
    if (field_init(&f, 1000, -200.0, 0.4, 0.2, 0)) exit(1);
    field_set_kink(&f, -50.0, 0.05, 1);
    double x0 = field_kink_position(&f);
    run(&f, 400.0);
    value("slow-kink-dx0.4", "mean speed of a v = 0.05 kink over t = 400 on a coarse grid dx = 0.4", (field_kink_position(&f) - x0) / 400.0, 0.05);
    field_free(&f);
    Series s[] = {{"shift, v = 0.7", ya, "#d62728"}, {"frequency, omega = 0.3", yb, "#1f77b4"}, {"slope 2", yc, "#888888"}};
    plot("collision-grid.svg", "Grid error of collisions and breathers", "log10 dx (dt = dx / 2)", "log10 relative error", 4, s, 3);
}

/* 20: both exact solutions satisfy phi_tt = phi_xx - sin(phi): residual by central differences (step h = 1e-3). */
static void check_residual(void)
{
    double h = 1e-3, worst[2] = {0.0, 0.0};
    for (int k = 0; k < 2; k++)
        for (int i = 0; i <= 200; i++)
            for (int m = 0; m <= 40; m++) {
                double x = -10.0 + 0.1 * i, t = -10.0 + 0.5 * m;
                double (*u)(double, double, double) = k ? sg_breather_exact : sg_pair_exact;
                double p = k ? 0.4 : 0.5, c = u(x, t, p);
                double tt = (u(x, t + h, p) - 2.0 * c + u(x, t - h, p)) / (h * h);
                double xx = (u(x + h, t, p) - 2.0 * c + u(x - h, t, p)) / (h * h);
                double r = fabs(tt - xx + sin(c));
                if (r > worst[k]) worst[k] = r;
            }
    value("pair-residual", "largest |phi_tt - phi_xx + sin phi| of the exact pair, v = 0.5, by differences h = 1e-3 (ideal 0)", worst[0], 0.0);
    value("breather-residual", "same for the exact breather, omega = 0.4 (ideal 0)", worst[1], 0.0);
}

int main(int argc, char **argv)
{
    snprintf(dir, sizeof dir, "%s", argc > 1 ? argv[1] : "docs/theory/figures");
    values = fopen(path("values.md"), "w");
    if (!values) { fprintf(stderr, "cannot write %s (does the directory exist?)\n", path("values.md")); return 1; }
    fprintf(values, "# Measured values\n\nGenerated by `make theory-figs` (`tests/theory_figs.c`). Do not edit by hand.\n"
                    "Units: c = 1, lengths in units of the kink's rest width, energy in units where the rest kink has 8.\n\n"
                    "| id | what | measured | theory | relative error |\n|---|---|---|---|---|\n");
    fig_potential();
    fig_kink();
    fig_contraction();
    fig_dispersion();
    fig_energy_error();
    fig_cfl();
    fig_push();
    fig_boundary();
    fig_position();
    fig_stop();
    fig_motor();
    fig_wobble();
    check_cue();
    fig_pair();
    fig_shift();
    fig_pair_conserved();
    fig_breather();
    fig_bound();
    fig_collision_grid();
    check_residual();
    if (fclose(values) != 0) return 1;
    printf("theory figures written to %s\n", dir);
    return 0;
}
