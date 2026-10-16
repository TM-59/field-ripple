#define _POSIX_C_SOURCE 200809L
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "field.h"
#include "render.h"
#include "svg.h"

#define PI 3.14159265358979323846
#define GRID 2000
#define DX 0.1
#define DT 0.05
#define TARGET 25.0

static float gradient_at(const Field *f, double x)
{
    double u = (x - f->x0) / f->dx;
    int j = (int)floor(u);
    if (j < 1 || j >= f->n - 2) return 0.0f;
    double a = field_gradient(f, j), b = field_gradient(f, j + 1);
    return (float)(a + (b - a) * (u - j));
}

/* Contracted profiles, energy conservation and the constant-force push, as CSV and SVG charts. */
static int headless(const char *dir)
{
    char path[512];
    Field f;
    double speeds[3] = {0.0, 0.6, 0.9};
    enum { M = 241 };
    static float s[M], prof[3][M];
    for (int i = 0; i < M; i++) s[i] = -6.0f + 0.05f * i;
    for (int k = 0; k < 3; k++) {
        if (field_init(&f, GRID, -100.0, DX, DT, 0)) return 1;
        field_set_kink(&f, -20.0, speeds[k], 1);
        for (int n = 0; n < 400; n++) field_step(&f);
        double pos = field_kink_position(&f);
        for (int i = 0; i < M; i++) prof[k][i] = gradient_at(&f, pos + s[i]);
        field_free(&f);
    }
    snprintf(path, sizeof path, "%s/profile.svg", dir);
    Series sp[] = {{"v = 0", prof[0], "#1f77b4"}, {"v = 0.6c", prof[1], "#ff7f0e"}, {"v = 0.9c", prof[2], "#d62728"}};
    if (svg_plot(path, "Kink energy density profile phi_x: Lorentz contraction", "x - X(t)", "phi_x", s, M, sp, 3)) return 1;

    enum { N = 400 };
    static float t[N], de[N], vs[N], vt[N];
    double eps = 0.01, force = 2.0 * PI * eps, e0;
    if (field_init(&f, GRID, -100.0, DX, DT, 0)) return 1;
    field_set_kink(&f, -20.0, 0.8, 1);
    e0 = field_energy(&f);
    for (int i = 0; i < N; i++) {
        t[i] = (float)(i * 0.25);
        de[i] = (float)(field_energy(&f) / e0 - 1.0);
        for (int k = 0; k < 5; k++) field_step(&f);
    }
    field_free(&f);
    snprintf(path, sizeof path, "%s/energy.svg", dir);
    Series se[] = {{"E / E0 - 1", de, "#2ca02c"}};
    if (svg_plot(path, "Energy conservation of a 0.8c kink", "t", "relative drift", t, N, se, 1)) return 1;

    if (field_init(&f, GRID, -100.0, DX, DT, 0)) return 1;
    f.eps = eps;
    field_set_kink(&f, 0.0, 0.0, 1);
    snprintf(path, sizeof path, "%s/run.csv", dir);
    FILE *csv = fopen(path, "w");
    if (!csv) return 1;
    fprintf(csv, "t,x,speed_sim,speed_theory\n");
    double prev = field_kink_position(&f);
    for (int i = 0; i < N; i++) {
        for (int k = 0; k < 5; k++) field_step(&f);
        double tt = (i + 1) * 0.25, x = field_kink_position(&f), p = force * tt;
        t[i] = (float)tt;
        vs[i] = (float)fabs((x - prev) / 0.25);
        vt[i] = (float)(p / sqrt(p * p + 64.0));
        prev = x;
        fprintf(csv, "%.2f,%.4f,%.4f,%.4f\n", tt, x, vs[i], vt[i]);
    }
    fclose(csv);
    field_free(&f);
    snprintf(path, sizeof path, "%s/push.svg", dir);
    Series sv[] = {{"simulation", vs, "#d62728"}, {"theory p/E", vt, "#1f77b4"}};
    if (svg_plot(path, "A constant bias pushes the kink: speed saturates below c", "t", "v", t, N, sv, 2)) return 1;
    printf("v(t=100): sim %.3f, theory %.3f\n", vs[399], vt[399]);
    return 0;
}

static int snapshot(void)
{
    Field f;
    if (field_init(&f, GRID, -100.0, DX, DT, 0)) return 1;
    field_set_kink(&f, 0.0, 0.8, 1);
    for (int n = 0; n < 200; n++) field_step(&f);
    static char buf[RENDER_COLS * RENDER_ROWS * 4];
    int ok = render_frame(&f, field_kink_position(&f), TARGET, buf, sizeof buf) != 0;
    if (ok) fputs(buf, stdout);
    field_free(&f);
    return ok ? 0 : 1;
}

static struct termios saved;
static void restore(void) { tcsetattr(STDIN_FILENO, TCSANOW, &saved); fputs("\033[?25h", stdout); }

static int play(void)
{
    if (!isatty(STDIN_FILENO)) { fputs("interactive mode needs a terminal; try --snapshot\n", stderr); return 1; }
    Field f;
    if (field_init(&f, GRID, -100.0, DX, DT, 0)) return 1;
    field_set_kink(&f, 0.0, 0.0, 1);
    tcgetattr(STDIN_FILENO, &saved);
    struct termios raw = saved;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    atexit(restore);
    fputs("\033[2J\033[?25l", stdout);

    static char buf[RENDER_COLS * RENDER_ROWS * 4];
    struct timespec tick = {0, 40 * 1000000L};
    double t = 0.0, prev_x = 0.0, v = 0.0;
    int paused = 0, frames = 0;
    for (;;) {
        char c;
        f.eps = 0.0;
        while (read(STDIN_FILENO, &c, 1) == 1) {
            if (c == 'q') return 0;
            if (c == 'p') paused = !paused;
            if (c == 'd') f.eps = -0.01;   /* a bias eps pushes the kink with force -2 pi eps */
            if (c == 'a') f.eps = 0.01;
        }
        if (!paused) {
            for (int k = 0; k < 6; k++) field_step(&f);
            t += 6 * DT;
        }
        double x = field_kink_position(&f);
        if (!isnan(x) && frames++ % 5 == 0) {
            v = (x - prev_x) / (5 * 6 * DT);
            prev_x = x;
        }
        if (isnan(x)) x = 0.0;
        double gamma = fabs(v) < 0.999 ? 1.0 / sqrt(1.0 - v * v) : 1.0;
        render_frame(&f, x, TARGET, buf, sizeof buf);
        printf("\033[H%s", buf);
        printf("t %6.1f  kink at %6.2f  v %+.3f  gamma %.2f  width ~ %.2f    \n", t, x, v, gamma, 1.0 / gamma);
        puts("a/d push left/right  p pause  q quit    park the kink on the | mark");
        fflush(stdout);
        if (fabs(x - TARGET) < 0.5 && fabs(v) < 0.03) { puts("\nParked on target!"); return 0; }
        nanosleep(&tick, NULL);
    }
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--headless") == 0) return headless(argc > 2 ? argv[2] : ".");
    if (argc > 1 && strcmp(argv[1], "--snapshot") == 0) return snapshot();
    return play();
}
