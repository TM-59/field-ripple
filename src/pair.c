#include "pair.h"

#include <math.h>

#define PI 3.14159265358979323846

double sg_pair_exact(double x, double t, double v)
{
    double g = 1.0 / sqrt(1.0 - v * v);
    return 4.0 * atan(sinh(g * v * t) / (v * cosh(g * x)));
}

double sg_breather_exact(double x, double t, double omega)
{
    double nu = sqrt(1.0 - omega * omega);
    return 4.0 * atan(nu * sin(omega * t) / (omega * cosh(nu * x)));
}

double sg_pair_shift(double v) { return 2.0 * sqrt(1.0 - v * v) * log(1.0 / v); }
double sg_pair_energy(double v) { return 16.0 / sqrt(1.0 - v * v); }
double sg_breather_energy(double omega) { return 16.0 * sqrt(1.0 - omega * omega); }

void field_set_pair(Field *f, double v, double t0)
{
    for (int j = 0; j < f->n; j++) {
        double x = field_x(f, j);
        f->cur[j] = sg_pair_exact(x, t0, v);
        f->prev[j] = sg_pair_exact(x, t0 - f->dt, v);
    }
}

void field_set_breather(Field *f, double omega, double t0, double centre)
{
    for (int j = 0; j < f->n; j++) {
        double x = field_x(f, j) - centre;
        f->cur[j] = sg_breather_exact(x, t0, omega);
        f->prev[j] = sg_breather_exact(x, t0 - f->dt, omega);
    }
}

/* A free kink 4 atan(exp(gamma (x - x0 - v t))). */
static double free_kink(double x, double t, double x0, double v)
{
    double g = 1.0 / sqrt(1.0 - v * v);
    return 4.0 * atan(exp(g * (x - x0 - v * t)));
}

void field_set_superposed_pair(Field *f, double a, double v)
{
    /* kink 0 -> 2 pi at -a moving +v, plus antikink 2 pi -> 0 at +a moving -v, minus 2 pi: 0 -> 2 pi -> 0. */
    for (int j = 0; j < f->n; j++) {
        double x = field_x(f, j);
        f->cur[j] = free_kink(x, 0.0, -a, v) - free_kink(x, 0.0, a, -v);
        f->prev[j] = free_kink(x, -f->dt, -a, v) - free_kink(x, -f->dt, a, -v);
    }
}

static double grad(const Field *f, const double *a, int j)
{
    int l = j > 0 ? j - 1 : (f->periodic ? f->n - 1 : 0);
    int r = j < f->n - 1 ? j + 1 : (f->periodic ? 0 : f->n - 1);
    return (a[r] - a[l]) / (2.0 * f->dx);
}

double field_momentum(const Field *f)
{
    /* phi_t between the two stored levels, phi_x averaged over the same two levels. */
    double p = 0.0;
    for (int j = 0; j < f->n; j++) {
        double pt = (f->cur[j] - f->prev[j]) / f->dt;
        double px = 0.5 * (grad(f, f->cur, j) + grad(f, f->prev, j));
        p -= pt * px;
    }
    return p * f->dx;
}

int field_fronts(const Field *f, double *xs, int max)
{
    int count = 0;
    for (int j = 0; j < f->n - 1; j++) {
        double a = fabs(f->cur[j]) - PI, b = fabs(f->cur[j + 1]) - PI;
        if ((a < 0.0 && b >= 0.0) || (a >= 0.0 && b < 0.0)) {
            if (count < max) xs[count] = field_x(f, j) + f->dx * a / (a - b);
            count++;
        }
    }
    return count;
}

double field_value_at(const Field *f, double x)
{
    double u = (x - f->x0) / f->dx;
    if (u <= 0.0) return f->cur[0];
    if (u >= f->n - 1) return f->cur[f->n - 1];
    int j = (int)floor(u);
    double a = u - j;
    return f->cur[j] + a * (f->cur[j + 1] - f->cur[j]);
}

double sg_breather_separation(double omega)
{
    double nu = sqrt(1.0 - omega * omega);
    return nu > omega ? 2.0 * acosh(nu / omega) / nu : 0.0;
}

/* Least-squares line through (t, x): returns the slope, writes x at t_at. */
static double fit(const double *t, const double *x, int n, double t_at, double *x_at)
{
    double st = 0.0, sx = 0.0, stt = 0.0, stx = 0.0;
    for (int i = 0; i < n; i++) { st += t[i]; sx += x[i]; stt += t[i] * t[i]; stx += t[i] * x[i]; }
    double slope = (n * stx - st * sx) / (n * stt - st * st), cut = (sx - slope * st) / n;
    *x_at = cut + slope * t_at;
    return slope;
}

#define FIT_MAX 4096

double pair_measure_shift(double v, double dx, double *speed_in, double *speed_out)
{
    static double tb[FIT_MAX], xb[FIT_MAX], ta[FIT_MAX], xa[FIT_MAX];
    double a = 20.0, dt = 0.5 * dx, meet = a / v;
    double before_end = (a - 8.0) / v, after_start = meet + 8.0 / v, after_end = meet + 25.0 / v;
    int every = (int)ceil(after_end / dt / 1000.0), nb = 0, na = 0;
    Field f;
    if (field_init(&f, (int)lround(240.0 / dx) + 1, -120.0, dx, dt, 0)) return NAN;
    field_set_superposed_pair(&f, a, v);
    int steps = (int)lround(after_end / dt);
    for (int s = 0; s <= steps; s++) {
        double t = s * dt, xs[2];
        if (s % every == 0 && field_fronts(&f, xs, 2) == 2) {
            if (t <= before_end && nb < FIT_MAX) { tb[nb] = t; xb[nb++] = xs[0]; }   /* the kink, moving right */
            if (t >= after_start && na < FIT_MAX) { ta[na] = t; xa[na++] = xs[1]; }  /* now on the right */
        }
        if (s < steps) field_step(&f);
    }
    field_free(&f);
    return pair_shift_from_tracks(tb, xb, nb, ta, xa, na, meet, speed_in, speed_out);
}

double pair_shift_from_tracks(const double *tb, const double *xb, int nb, const double *ta, const double *xa, int na,
                              double meet, double *speed_in, double *speed_out)
{
    if (nb < 3 || na < 3) return NAN;
    double x_in, x_out;
    double u_in = fit(tb, xb, nb, meet, &x_in), u_out = fit(ta, xa, na, meet, &x_out);
    if (speed_in) *speed_in = u_in;
    if (speed_out) *speed_out = u_out;
    return x_out - x_in;
}

double breather_measure_frequency(double omega, double dx, int periods, double *energy_drift, double *swing, double *separation)
{
    double dt = 0.5 * dx, period = 2.0 * PI / omega, first = NAN, last = NAN, drift = 0.0, most = 0.0, wide = 0.0;
    double rise = 0.0, peak_energy = NAN;
    int crossings = 0, steps = (int)lround(periods * period / dt);
    Field f;
    if (field_init(&f, (int)lround(120.0 / dx) + 1, -60.0, dx, dt, 0)) return NAN;
    field_set_breather(&f, omega, 0.0, 0.0);
    double e0 = field_energy(&f), before = field_value_at(&f, 0.0);
    for (int s = 1; s <= steps; s++) {
        field_step(&f);
        double t = s * dt, now = field_value_at(&f, 0.0), xs[2];
        double rel = fabs(field_energy(&f) / e0 - 1.0);
        if ((before < 0.0) != (now < 0.0)) {
            double tc = t - dt * now / (now - before);
            if (isnan(first)) first = tc;
            last = tc;
            crossings++;
        }
        /* |phi(0)| at its peak: the field stands still for an instant, every breath at the same phase. */
        if (rise > 0.0 && fabs(now) <= fabs(before)) {
            double e = field_energy(&f);
            if (isnan(peak_energy)) peak_energy = e;
            if (fabs(e / peak_energy - 1.0) > drift) drift = fabs(e / peak_energy - 1.0);
        }
        rise = fabs(now) - fabs(before);
        before = now;
        if (rel > most) most = rel;
        if (field_fronts(&f, xs, 2) == 2 && xs[1] - xs[0] > wide) wide = xs[1] - xs[0];
    }
    field_free(&f);
    if (energy_drift) *energy_drift = drift;
    if (swing) *swing = most;
    if (separation) *separation = wide;
    return crossings > 1 ? PI * (crossings - 1) / (last - first) : NAN;
}
