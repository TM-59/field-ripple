#include "field.h"

#include <math.h>
#include <stdlib.h>

#define PI 3.14159265358979323846

int field_init(Field *f, int n, double x0, double dx, double dt, int periodic)
{
    f->n = n; f->x0 = x0; f->dx = dx; f->dt = dt; f->periodic = periodic; f->eps = 0.0;
    f->prev = calloc(n, sizeof *f->prev);
    f->cur = calloc(n, sizeof *f->cur);
    f->next = calloc(n, sizeof *f->next);
    if (!f->prev || !f->cur || !f->next) { field_free(f); return 1; }
    return 0;
}

void field_free(Field *f)
{
    free(f->prev); free(f->cur); free(f->next);
    f->prev = f->cur = f->next = NULL;
}

double field_x(const Field *f, int j)
{
    return f->x0 + j * f->dx;
}

static double at(const Field *f, const double *a, int j)
{
    if (f->periodic) return a[(j + f->n) % f->n];
    return a[j < 0 ? 0 : j >= f->n ? f->n - 1 : j];
}

static double kink(double x, double t, double v, int sign)
{
    double g = 1.0 / sqrt(1.0 - v * v);
    double p = 4.0 * atan(exp(g * (x - v * t)));
    return sign > 0 ? p : 2.0 * PI - p;
}

void field_set_kink(Field *f, double centre, double v, int sign)
{
    for (int j = 0; j < f->n; j++) {
        double x = field_x(f, j) - centre;
        f->cur[j] = kink(x, 0.0, v, sign);
        f->prev[j] = kink(x, -f->dt, v, sign);
    }
}

void field_set_wave(Field *f, double amp, double k)
{
    for (int j = 0; j < f->n; j++) f->cur[j] = amp * cos(k * field_x(f, j));
    /* phi(-dt) = phi + dt^2/2 phi_tt for a field starting at rest */
    for (int j = 0; j < f->n; j++) {
        double lap = (at(f, f->cur, j + 1) - 2.0 * f->cur[j] + at(f, f->cur, j - 1)) / (f->dx * f->dx);
        f->prev[j] = f->cur[j] + 0.5 * f->dt * f->dt * (lap - sin(f->cur[j]) + f->eps);
    }
}

void field_step(Field *f)
{
    double dt2 = f->dt * f->dt, inv = 1.0 / (f->dx * f->dx);
    for (int j = 0; j < f->n; j++) {
        double lap = (at(f, f->cur, j + 1) - 2.0 * f->cur[j] + at(f, f->cur, j - 1)) * inv;
        f->next[j] = 2.0 * f->cur[j] - f->prev[j] + dt2 * (lap - sin(f->cur[j]) + f->eps);
    }
    double *old = f->prev;
    f->prev = f->cur;
    f->cur = f->next;
    f->next = old;
}

double field_gradient(const Field *f, int j)
{
    return (at(f, f->cur, j + 1) - at(f, f->cur, j - 1)) / (2.0 * f->dx);
}

double field_energy(const Field *f)
{
    double e = 0.0;
    for (int j = 0; j < f->n; j++) {
        double pt = (f->cur[j] - f->prev[j]) / f->dt;
        double gx = field_gradient(f, j), gp = (at(f, f->prev, j + 1) - at(f, f->prev, j - 1)) / (2.0 * f->dx);
        double pot = 0.5 * ((1.0 - cos(f->cur[j])) + (1.0 - cos(f->prev[j])));
        e += 0.5 * pt * pt + 0.25 * (gx * gx + gp * gp) + pot;
    }
    return e * f->dx;
}

double field_kink_position(const Field *f)
{
    for (int j = 0; j < f->n - 1; j++) {
        double a = f->cur[j] - PI, b = f->cur[j + 1] - PI;
        if (a < 0.0 && b >= 0.0) return field_x(f, j) + f->dx * (-a) / (b - a);
    }
    return NAN;
}

double field_max_gradient(const Field *f)
{
    double m = 0.0;
    for (int j = 0; j < f->n; j++) {
        double g = fabs(field_gradient(f, j));
        if (g > m) m = g;
    }
    return m;
}
