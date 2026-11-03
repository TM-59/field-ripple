/* Browser entry points for "The Flood Line" (src/line.c). The page reads every value through these; nothing about
 * the night is computed in JavaScript. */
#include "line.h"

#include <math.h>

#define LW_SAMPLES 301

static Line line;
static double samples[LW_SAMPLES * 2];

void lw_reset(void)
{
    line_free(&line);
    line_init(&line, LE_ALL);
}

void lw_tick(int frames, int command) { line_tick(&line, frames, command); }
int lw_answer(int option) { return line_answer(&line, option); }
double lw_value(int field) { return line_value(&line, field); }
int lw_count(void) { return LW_SAMPLES; }

static double density(const Field *f, int j)
{
    double vt = (f->cur[j] - f->prev[j]) / f->dt, vx = field_gradient(f, j);
    return 0.5 * (vt * vt + vx * vx) + 1.0 - cos(f->cur[j]);
}

/* Interleaved phi and energy density over [x - 12, x + 12] around the turn. */
const double *lw_samples(void)
{
    const Field *f = &line.field;
    if (!line.ready) return samples;
    for (int i = 0; i < LW_SAMPLES; i++) {
        double x = line.x - 12.0 + 24.0 * i / (LW_SAMPLES - 1);
        double u = (x - f->x0) / f->dx;
        int j = (int)floor(u);
        if (j < 1) j = 1;
        if (j > f->n - 3) j = f->n - 3;
        double a = fmax(0.0, fmin(1.0, u - j));
        samples[2 * i] = f->cur[j] + a * (f->cur[j + 1] - f->cur[j]);
        samples[2 * i + 1] = density(f, j) + a * (density(f, j + 1) - density(f, j));
    }
    return samples;
}
