#include "../src/observer.h"
#include "../src/field.h"
#include <math.h>
#include <stdio.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL observer:%d: %s\n", __LINE__, #c); failures++; } } while (0)

int main(void)
{
    Field reference;
    CHECK(fr_reset(0.6) == 0);
    CHECK(field_init(&reference, 2000, -100.0, 0.1, 0.05, 0) == 0);
    field_set_kink(&reference, 0.0, 0.6, 1);
    for (int n = 0; n < 400; n++) field_step(&reference);
    for (int n = 0; n < 4; n++) CHECK(fr_step(0, 100) == 0);
    CHECK(fabs(fr_position() - field_kink_position(&reference)) < 1e-10);
    CHECK(fabs(fr_energy() - field_energy(&reference)) < 1e-10);
    CHECK(fabs(fr_time() - 20.0) < 1e-10);
    CHECK(fabs(fr_width() - 0.8) < 0.02);
    const double *p = fr_samples();
    CHECK(fr_count() == OBSERVER_SAMPLES);
    CHECK(fabs(p[OBSERVER_SAMPLES - 1] - 3.141592653589793) < 0.002);
    for (int i = 0; i < fr_count(); i++) {
        CHECK(isfinite(p[2 * i]));
        CHECK(isfinite(p[2 * i + 1]) && p[2 * i + 1] >= 0);
    }
    field_free(&reference);
    CHECK(fr_reset(NAN) == 1);
    CHECK(fr_reset(1.0) == 1);
    CHECK(fr_step(0, 121) == 1);
    CHECK(fr_step(2, 10) == 1);
    CHECK(fr_reset(0) == 0);
    for (int n = 0; n < 10; n++) CHECK(fr_step(1, 100) == 0);
    CHECK(fr_velocity() > 0.5 && fr_position() > 10);
    CHECK(fr_reset(0) == 0);
    for (int n = 0; n < 10; n++) CHECK(fr_step(-1, 100) == 0);
    CHECK(fr_velocity() < -0.5 && fr_position() < -10);
    for (int n = 0; n < 100; n++) CHECK(fr_step(0, 100) == 0);
    CHECK(fr_stopped());
    double t = fr_time(), x = fr_position();
    CHECK(fr_step(1, 100) == 0 && fr_time() == t && fr_position() == x);
    CHECK(fr_reset(0) == 0 && !fr_stopped() && fr_time() == 0);
    puts(failures ? "observer tests failed" : "observer tests passed (C adapter equals native field)");
    return failures != 0;
}
