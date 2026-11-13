/* Kink-antikink collisions and breathers (src/pair.c) against their exact solutions, and the browser adapter
 * (src/pair_web.c). The solver is the unchanged src/field.c on the game's grid dx = 0.1, dt = 0.05 unless stated. */
#include <math.h>
#include <stddef.h>
#include <stdio.h>

#include "../src/field.h"
#include "../src/pair.h"

#define PI 3.14159265358979323846

static int failures, checks;

#define CHECK(cond, ...)                                      \
    do {                                                      \
        checks++;                                             \
        if (!(cond)) {                                        \
            failures++;                                       \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);       \
            printf(__VA_ARGS__);                              \
            printf("\n");                                     \
        }                                                     \
    } while (0)

/* pair_web.c has no header of its own (like line_web.c); these are its entry points. */
int pw_reset(int mode, double param);
int pw_step(int steps);
double pw_value(int k);
int pw_count(void);
int pw_columns(void);
int pw_rows(void);
const double *pw_samples(void);
const double *pw_history(void);

static double gap_to_exact(const Field *f, double t, double v)
{
    double worst = 0.0;
    for (int j = 0; j < f->n; j++) {
        double g = fabs(f->cur[j] - sg_pair_exact(field_x(f, j), t, v));
        if (g > worst) worst = g;
    }
    return worst;
}

/* Largest |phi - phi_exact| of an exact pair started at t = -40 and run to t = 40 on a grid dx, dt = dx / 2. */
static double pair_gap(double v, double dx)
{
    double t0 = -40.0, worst = 0.0;
    Field f;
    if (field_init(&f, (int)lround(120.0 / dx) + 1, -60.0, dx, 0.5 * dx, 0)) return NAN;
    field_set_pair(&f, v, t0);
    int per = (int)lround(10.0 / f.dt);
    for (int k = 1; k <= 8; k++) {
        for (int s = 0; s < per; s++) field_step(&f);
        double g = gap_to_exact(&f, t0 + 10.0 * k, v);
        if (g > worst) worst = g;
    }
    field_free(&f);
    return worst;
}

static void test_pair_follows_perring_skyrme(void)
{
    double v = 0.5, t0 = -40.0;
    Field f;
    CHECK(field_init(&f, 1201, -60.0, 0.1, 0.05, 0) == 0, "allocation failed");
    field_set_pair(&f, v, t0);
    double xs[2];
    CHECK(field_fronts(&f, xs, 2) == 2 && xs[0] < -15.0 && xs[1] > 15.0, "two separate fronts at the start");
    CHECK(fabs(f.cur[f.n / 2] + 2.0 * PI) < 1e-6, "between them phi = -2 pi before the collision: %.6f", f.cur[f.n / 2]);
    double worst = 0.0;
    for (int k = 1; k <= 8; k++) {
        for (int s = 0; s < 200; s++) field_step(&f);   /* 10 t */
        double g = gap_to_exact(&f, t0 + 10.0 * k, v);
        if (g > worst) worst = g;
    }
    /* The gap is the grid's: the kink runs 0.05 % slow on dx = 0.1 and sits where phi is steepest (2 gamma rad per unit). */
    CHECK(worst < 0.15, "largest |phi - phi_exact| over t = -40..40: %.4g rad", worst);
    double fine = pair_gap(v, 0.05);
    CHECK(worst / fine > 3.5 && worst / fine < 4.5, "gap %.4g on dx 0.1, %.4g on dx 0.05 (second order: 4)", worst, fine);
    CHECK(fabs(f.cur[f.n / 2] - 2.0 * PI) < 0.01, "between them phi = +2 pi after the collision: %.6f", f.cur[f.n / 2]);
    printf("[pair] v = 0.5 from the exact solution at t = -40: largest gap to Perring-Skyrme up to t = 40 is %.3g rad "
           "(%.3g on dx 0.05)\n", worst, fine);
    field_free(&f);
}

static void test_pair_passes_through_with_the_phase_shift(void)
{
    double speeds[5] = {0.1, 0.3, 0.5, 0.7, 0.9};
    for (int k = 0; k < 5; k++) {
        double v = speeds[k], in, out, shift = pair_measure_shift(v, 0.1, &in, &out), theory = sg_pair_shift(v);
        CHECK(isfinite(shift), "v = %.1f: no two fronts after the collision (annihilated?)", v);
        CHECK(fabs(shift / theory - 1.0) < 0.05, "v = %.1f: shift %.5f vs 2 sqrt(1-v^2) ln(1/v) = %.5f", v, shift, theory);
        CHECK(fabs(out / in - 1.0) < 1e-3, "v = %.1f: speed out %.6f vs in %.6f", v, out, in);
        CHECK(fabs(in / v - 1.0) < 0.005, "v = %.1f: fitted speed %.6f", v, in);
        printf("[pair] v = %.1f: shift %.5f, theory %.5f (%.2e); speed in %.5f, out %.5f\n", v, shift, theory,
               fabs(shift / theory - 1.0), in, out);
    }
    /* Half the grid spacing: the error falls by about four (second-order scheme). */
    double coarse = fabs(pair_measure_shift(0.7, 0.1, NULL, NULL) / sg_pair_shift(0.7) - 1.0);
    double fine = fabs(pair_measure_shift(0.7, 0.05, NULL, NULL) / sg_pair_shift(0.7) - 1.0);
    CHECK(coarse / fine > 3.5 && coarse / fine < 4.5, "v = 0.7: shift error %.3g on dx 0.1, %.3g on dx 0.05", coarse, fine);
}

static void test_pair_conserves_energy_momentum_and_speed(void)
{
    double v = 0.5;
    Field f;
    CHECK(field_init(&f, 2401, -120.0, 0.1, 0.05, 0) == 0, "allocation failed");
    field_set_superposed_pair(&f, 20.0, v);
    double e0 = field_energy(&f), swing = 0.0, worst_p = 0.0;
    CHECK(fabs(e0 / sg_pair_energy(v) - 1.0) < 0.002, "pair energy %.5f vs 16 gamma = %.5f", e0, sg_pair_energy(v));
    for (int s = 0; s < 2000; s++) {   /* through the collision at t = 40, to t = 100 */
        field_step(&f);
        double e = fabs(field_energy(&f) / e0 - 1.0), p = fabs(field_momentum(&f));
        if (e > swing) swing = e;
        if (p > worst_p) worst_p = p;
    }
    double change = fabs(field_energy(&f) / e0 - 1.0);
    /* Apart again, the energy is what it was; while they overlap the reading moves by O(dt^2) (see the breather). */
    CHECK(change < 1e-4, "energy before vs after the collision changed by %.3g", change);
    CHECK(swing < 5e-3, "energy reading during the collision moved by %.3g", swing);
    CHECK(worst_p < 1e-9, "momentum of the symmetric pair %.3g", worst_p);
    printf("[pair] v = 0.5 through the collision: energy after vs before %.2e, reading swing while overlapping %.2e, |P| at most %.2e\n",
           change, swing, worst_p);
    field_free(&f);

    /* One kink: P = 8 gamma v. */
    CHECK(field_init(&f, 2000, -100.0, 0.1, 0.05, 0) == 0, "allocation failed");
    field_set_kink(&f, 0.0, 0.6, 1);
    for (int s = 0; s < 200; s++) field_step(&f);
    CHECK(fabs(field_momentum(&f) / 6.0 - 1.0) < 0.005, "kink momentum %.5f vs 8 gamma v = 6", field_momentum(&f));
    field_free(&f);
}

static void test_breather_keeps_frequency_and_shape(void)
{
    double omegas[4] = {0.3, 0.5, 0.8, 0.95};
    for (int k = 0; k < 4; k++) {
        double w = omegas[k], drift, swing, fine_swing;
        double measured = breather_measure_frequency(w, 0.1, 20, &drift, &swing, NULL);
        breather_measure_frequency(w, 0.05, 20, NULL, &fine_swing, NULL);
        CHECK(fabs(measured / w - 1.0) < 0.01, "omega %.2f measured %.6f", w, measured);
        CHECK(drift < 1e-4, "omega %.2f: energy drift between breaths %.3g", w, drift);
        CHECK(fine_swing < 1e-3, "omega %.2f: energy reading swings %.3g within a breath on dx 0.05", w, fine_swing);
        CHECK(swing / fine_swing > 3.5 && swing / fine_swing < 4.5, "omega %.2f: swing %.3g vs %.3g (second order: 4)", w, swing, fine_swing);
        printf("[breather] omega %.2f: measured %.6f (%.2e); energy drift %.1e; reading swing %.2e (dx 0.1), %.2e (dx 0.05)\n",
               w, measured, fabs(measured / w - 1.0), drift, swing, fine_swing);
    }
    /* Shape over one period at omega = 0.5. */
    Field f;
    double w = 0.5, worst = 0.0;
    CHECK(field_init(&f, 1201, -60.0, 0.1, 0.05, 0) == 0, "allocation failed");
    field_set_breather(&f, w, 0.0, 0.0);
    int steps = (int)lround(2.0 * PI / w / f.dt);
    for (int s = 1; s <= steps; s++) {
        field_step(&f);
        for (int j = 0; j < f.n; j += 4) {
            double g = fabs(f.cur[j] - sg_breather_exact(field_x(&f, j), s * f.dt, w));
            if (g > worst) worst = g;
        }
    }
    CHECK(worst < 0.05, "omega 0.5: largest gap to the exact breather over one period %.4g rad", worst);
    printf("[breather] omega 0.5: largest gap to the exact breather over one period %.3g rad\n", worst);
    field_free(&f);
}

static void test_breather_energy(void)
{
    for (int k = 1; k <= 9; k++) {
        double w = 0.1 * k;
        Field f;
        CHECK(field_init(&f, 1201, -60.0, 0.1, 0.05, 0) == 0, "allocation failed");
        field_set_breather(&f, w, 0.25 * 2.0 * PI / w, 0.0);   /* at a peak: the field stands still */
        CHECK(fabs(field_energy(&f) / sg_breather_energy(w) - 1.0) < 1e-3, "omega %.1f: E %.5f vs 16 sqrt(1 - omega^2) = %.5f",
              w, field_energy(&f), sg_breather_energy(w));
        CHECK(sg_breather_energy(w) < 16.0 && sg_pair_energy(w) > 16.0, "bound below 16, free pair above 16");
        field_free(&f);
    }
}

static void test_breather_is_a_bound_pair(void)
{
    double omegas[2] = {0.3, 0.5};
    for (int k = 0; k < 2; k++) {
        double w = omegas[k], wide;
        breather_measure_frequency(w, 0.1, 6, NULL, NULL, &wide);
        CHECK(fabs(wide / sg_breather_separation(w) - 1.0) < 0.01, "omega %.1f: widest separation %.4f vs 2 arccosh(nu/omega)/nu = %.4f",
              w, wide, sg_breather_separation(w));
        printf("[breather] omega %.1f: widest front separation %.4f, theory %.4f\n", w, wide, sg_breather_separation(w));
    }
    double wide;
    breather_measure_frequency(0.8, 0.1, 6, NULL, NULL, &wide);
    CHECK(wide == 0.0 && sg_breather_separation(0.8) == 0.0, "omega 0.8: |phi| never reaches pi, separation %.3f", wide);
}

static void test_browser_adapter(void)
{
    CHECK(pw_reset(2, 0.5) != 0 && pw_reset(0, 0.99) != 0 && pw_reset(1, 0.05) != 0 && pw_reset(0, NAN) != 0, "bad input refused");
    CHECK(pw_reset(0, 0.5) == 0, "pair reset");
    CHECK(fabs(pw_value(7) + 20.0) < 1e-3 && fabs(pw_value(8) - 20.0) < 1e-3, "fronts start at -20, 20: %.4f %.4f", pw_value(7), pw_value(8));
    CHECK(pw_step(4001) != 0 && pw_step(-1) != 0, "bad step count refused");
    /* The adapter steps exactly the solver of field.c from the same start. */
    Field f;
    CHECK(field_init(&f, 1201, -60.0, 0.1, 0.05, 0) == 0, "allocation failed");
    field_set_pair(&f, 0.5, pw_value(1));
    for (int s = 0; s < 300; s++) field_step(&f);
    CHECK(pw_step(300) == 0, "step");
    CHECK(fabs(pw_value(3) - field_energy(&f)) < 1e-12, "adapter energy %.12f vs solver %.12f", pw_value(3), field_energy(&f));
    const double *s = pw_samples();
    CHECK(fabs(s[3 * 150] - field_value_at(&f, 0.0)) < 1e-12, "sample at x = 0");
    field_free(&f);
    while (!pw_value(15)) pw_step(4000);
    double shift = pw_value(9);
    CHECK(fabs(shift / pw_value(10) - 1.0) < 0.05, "panel shift %.5f vs %.5f", shift, pw_value(10));
    CHECK(pw_value(13) < 0.15, "panel gap to exact %.4g", pw_value(13));
    CHECK((int)pw_value(14) == pw_rows(), "space-time map filled: %d of %d rows", (int)pw_value(14), pw_rows());
    CHECK(fabs(pw_value(5)) < 1e-9, "panel momentum %.3g", pw_value(5));
    printf("[panel] pair v = 0.5: shift %.5f (theory %.5f), gap to exact %.3g rad, %d x %d map\n", shift, pw_value(10),
           pw_value(13), pw_rows(), pw_columns());
    CHECK(pw_reset(1, 0.6) == 0, "breather reset");
    while (!pw_value(15)) pw_step(4000);
    CHECK(fabs(pw_value(11) / 0.6 - 1.0) < 0.01, "panel omega %.6f", pw_value(11));
    CHECK(fabs(pw_value(16) - 1.0) < 1e-12 && pw_count() == 301, "mode and sample count");
    const double *h = pw_history();
    CHECK(fabs(h[(pw_rows() - 1) * pw_columns() + pw_columns() / 2] - sg_breather_exact(0.0, pw_value(2), 0.6)) < 0.15,
          "last row of the map at x = 0 near the exact breather");
    printf("[panel] breather omega 0.6: measured %.6f, gap to exact %.3g rad\n", pw_value(11), pw_value(13));
}

int main(void)
{
    test_pair_follows_perring_skyrme();
    test_pair_passes_through_with_the_phase_shift();
    test_pair_conserves_energy_momentum_and_speed();
    test_breather_keeps_frequency_and_shape();
    test_breather_energy();
    test_breather_is_a_bound_pair();
    test_browser_adapter();
    if (failures) {
        printf("%d of %d pair check(s) failed\n", failures, checks);
        return 1;
    }
    printf("pair tests passed (%d checks)\n", checks);
    return 0;
}
