#include <math.h>
#include <stdio.h>

#include "../src/field.h"
#include "../src/render.h"

static int failures;

#define CHECK(cond, ...)                                      \
    do {                                                      \
        if (!(cond)) {                                        \
            failures++;                                       \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);       \
            printf(__VA_ARGS__);                              \
            printf("\n");                                     \
        }                                                     \
    } while (0)

static void run(Field *f, double t)
{
    int steps = (int)lround(t / f->dt);
    for (int k = 0; k < steps; k++) field_step(f);
}

static void test_static_kink_stays_put_with_energy_eight(void)
{
    Field f;
    CHECK(field_init(&f, 1000, -50.0, 0.1, 0.05, 0) == 0, "allocation failed");
    field_set_kink(&f, 0.0, 0.0, 1);
    CHECK(fabs(field_energy(&f) - 8.0) < 0.01, "rest energy %.4f", field_energy(&f));
    run(&f, 50.0);
    CHECK(fabs(field_kink_position(&f)) < 0.01, "kink drifted to %.4f", field_kink_position(&f));
    CHECK(fabs(field_energy(&f) - 8.0) < 0.01, "energy changed to %.4f", field_energy(&f));
    field_free(&f);
}

static void test_moving_kink_follows_its_worldline(void)
{
    Field f;
    double v = 0.6;
    CHECK(field_init(&f, 2000, -100.0, 0.1, 0.05, 0) == 0, "allocation failed");
    field_set_kink(&f, -20.0, v, 1);
    run(&f, 60.0);
    CHECK(fabs(field_kink_position(&f) - (-20.0 + v * 60.0)) < 0.05, "position %.3f vs %.3f",
          field_kink_position(&f), -20.0 + v * 60.0);
    field_free(&f);
}

static void test_energy_is_gamma_times_rest_energy(void)
{
    Field f;
    double v = 0.8, gamma = 1.0 / sqrt(1.0 - v * v);
    CHECK(field_init(&f, 2000, -100.0, 0.1, 0.05, 0) == 0, "allocation failed");
    field_set_kink(&f, -20.0, v, 1);
    double e0 = field_energy(&f);
    CHECK(fabs(e0 / (8.0 * gamma) - 1.0) < 0.01, "E %.4f vs 8 gamma %.4f", e0, 8.0 * gamma);
    run(&f, 40.0);
    CHECK(fabs(field_energy(&f) / e0 - 1.0) < 1e-3, "energy drift %.2e", field_energy(&f) / e0 - 1.0);
    field_free(&f);
}

static void test_kink_is_lorentz_contracted(void)
{
    double speeds[] = {0.0, 0.6, 0.9};
    for (int i = 0; i < 3; i++) {
        Field f;
        double gamma = 1.0 / sqrt(1.0 - speeds[i] * speeds[i]);
        CHECK(field_init(&f, 2000, -100.0, 0.1, 0.05, 0) == 0, "allocation failed");
        field_set_kink(&f, -20.0, speeds[i], 1);
        run(&f, 20.0);
        /* phi_x = 2 gamma sech(gamma (x - v t)): the peak grows like gamma as the width shrinks */
        CHECK(fabs(field_max_gradient(&f) / (2.0 * gamma) - 1.0) < 0.02, "v=%.1f peak %.4f vs %.4f", speeds[i],
              field_max_gradient(&f), 2.0 * gamma);
        field_free(&f);
    }
}

static void test_small_waves_follow_the_massive_dispersion(void)
{
    Field f;
    double k = 1.0, length = 2.0 * 3.14159265358979 * 16.0, amp = 1e-3, omega = sqrt(k * k + 1.0);
    int n = 1024;
    CHECK(field_init(&f, n, 0.0, length / n, 0.02, 1) == 0, "allocation failed");
    field_set_wave(&f, amp, k);
    int steps = (int)(3.14159265358979 / omega / f.dt);
    for (int s = 0; s < steps; s++) field_step(&f);
    double expected = amp * cos(omega * steps * f.dt);
    CHECK(fabs(f.cur[0] - expected) < 0.02 * amp, "phi(0) %.6e vs %.6e", f.cur[0], expected);
    field_free(&f);
}

static void test_bias_pushes_the_kink_like_a_particle(void)
{
    Field f;
    double eps = 0.01, t = 100.0, force = 2.0 * 3.14159265358979 * eps;
    CHECK(field_init(&f, 2000, -100.0, 0.1, 0.05, 0) == 0, "allocation failed");
    f.eps = eps;
    field_set_kink(&f, 0.0, 0.0, 1);
    double x1 = 0.0, x2 = 0.0;
    run(&f, t - 5.0);
    x1 = field_kink_position(&f);
    run(&f, 5.0);
    x2 = field_kink_position(&f);
    double v = (x2 - x1) / 5.0, p = force * t, expected = p / sqrt(p * p + 64.0);
    CHECK(fabs(x2) > 1.0, "the bias should move the kink");
    CHECK(fabs(fabs(v) / expected - 1.0) < 0.1, "speed %.3f vs p/E %.3f", fabs(v), expected);
    field_free(&f);
}

static void test_render_fits_the_screen(void)
{
    Field f;
    CHECK(field_init(&f, 1000, -50.0, 0.1, 0.05, 0) == 0, "allocation failed");
    field_set_kink(&f, 0.0, 0.0, 1);
    static char buf[RENDER_COLS * RENDER_ROWS * 2];
    size_t used = render_frame(&f, 0.0, 3.0, buf, sizeof buf);
    int lines = 0;
    for (size_t i = 0; i < used; i++) lines += buf[i] == '\n';
    CHECK(lines == RENDER_ROWS, "expected %d lines, got %d", RENDER_ROWS, lines);
    CHECK(render_frame(&f, 0.0, 3.0, buf, 8) == 0, "tiny buffer must be refused");
    field_free(&f);
}

int main(void)
{
    test_static_kink_stays_put_with_energy_eight();
    test_moving_kink_follows_its_worldline();
    test_energy_is_gamma_times_rest_energy();
    test_kink_is_lorentz_contracted();
    test_small_waves_follow_the_massive_dispersion();
    test_bias_pushes_the_kink_like_a_particle();
    test_render_fits_the_screen();
    if (failures) {
        printf("%d check(s) failed\n", failures);
        return 1;
    }
    puts("all tests passed");
    return 0;
}
