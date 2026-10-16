#include "render.h"

#include <math.h>

#define UNITS_PER_COL 0.25
#define PI 3.14159265358979323846

size_t render_frame(const Field *f, double centre, double target, char *buf, size_t cap)
{
    if (cap < (size_t)(RENDER_COLS + 1) * RENDER_ROWS + 1) return 0;
    char g[RENDER_ROWS][RENDER_COLS];
    for (int i = 0; i < RENDER_ROWS; i++)
        for (int j = 0; j < RENDER_COLS; j++) g[i][j] = ' ';
    for (int c = 0; c < RENDER_COLS; c++) {
        double x = centre + (c - RENDER_COLS / 2) * UNITS_PER_COL;
        if (fabs(x - target) < UNITS_PER_COL / 2.0)
            for (int i = 0; i < RENDER_ROWS; i++) g[i][c] = '|';
        int j = (int)lround((x - f->x0) / f->dx);
        if (j < 0 || j >= f->n) continue;
        double p = f->cur[j];
        if (p < 0.0) p = 0.0;
        if (p > 2.0 * PI) p = 2.0 * PI;
        g[RENDER_ROWS - 1 - (int)lround(p / (2.0 * PI) * (RENDER_ROWS - 1))][c] = '*';
    }
    size_t used = 0;
    for (int i = 0; i < RENDER_ROWS; i++) {
        for (int j = 0; j < RENDER_COLS; j++) buf[used++] = g[i][j];
        buf[used++] = '\n';
    }
    buf[used] = '\0';
    return used;
}
