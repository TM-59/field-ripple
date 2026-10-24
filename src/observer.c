#include "observer.h"
#include "field.h"

#include <math.h>

static Field field;
static double elapsed, position, velocity;
static int ready, stopped;
static double samples[OBSERVER_SAMPLES * 2];

int fr_reset(double speed)
{
    if (!isfinite(speed) || fabs(speed) > 0.9) return 1;
    if (ready) field_free(&field);
    ready = 0;
    if (field_init(&field, 2000, -100.0, 0.1, 0.05, 0)) return 1;
    ready = 1;
    field_set_kink(&field, 0.0, speed, 1);
    position = elapsed = 0.0;
    velocity = speed;
    stopped = 0;
    return 0;
}

int fr_step(int direction, int steps)
{
    if (!ready || steps < 0 || steps > 120 || direction < -1 || direction > 1) return 1;
    field.eps = -0.05 * direction; /* F = -2 pi epsilon for the rising kink. */
    for (int i = 0; i < steps && !stopped; i++) {
        double previous = position;
        field_step(&field);
        elapsed += field.dt;
        position = field_kink_position(&field);
        velocity = (position - previous) / field.dt;
        /* End the observation before the kink reaches the finite domain edge. */
        stopped = !isfinite(position) || fabs(position) >= 75.0 || elapsed >= 120.0;
    }
    return 0;
}

double fr_time(void) { return elapsed; }
double fr_position(void) { return position; }
double fr_velocity(void) { return velocity; }
double fr_energy(void) { return ready ? field_energy(&field) : 0.0; }
double fr_width(void) { return ready ? 2.0 / field_max_gradient(&field) : 0.0; }
int fr_stopped(void) { return stopped; }
int fr_count(void) { return OBSERVER_SAMPLES; }

static double density(int j)
{
    double vt = (field.cur[j] - field.prev[j]) / field.dt;
    double vx = field_gradient(&field, j);
    return 0.5 * (vt * vt + vx * vx) + 1.0 - cos(field.cur[j]);
}

const double *fr_samples(void)
{
    if (!ready) return samples;
    for (int i = 0; i < OBSERVER_SAMPLES; i++) {
        double x = position - 12.0 + 24.0 * i / (OBSERVER_SAMPLES - 1);
        double u = (x - field.x0) / field.dx;
        int j = (int)floor(u);
        if (j < 1) j = 1;
        if (j > field.n - 3) j = field.n - 3;
        double fraction = fmax(0.0, fmin(1.0, u - j));
        samples[2 * i] = field.cur[j] + fraction * (field.cur[j + 1] - field.cur[j]);
        samples[2 * i + 1] = density(j) + fraction * (density(j + 1) - density(j));
    }
    return samples;
}
