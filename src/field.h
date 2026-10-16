#ifndef FIELD_H
#define FIELD_H

/* Sine-Gordon field in 1+1 dimensions: phi_tt = phi_xx - sin(phi) + eps  (c = 1).
 * The soliton ("kink") phi = 4 atan(exp(gamma (x - v t))) has rest energy 8 and
 * is Lorentz contracted like a relativistic particle. A uniform bias eps pushes it
 * with a force of about 2 pi eps. Leapfrog integration. */
typedef struct {
    int n, periodic;
    double x0, dx, dt;
    double eps;                     /* uniform bias that acts as a force on a kink */
    double *prev, *cur, *next;
} Field;

int field_init(Field *f, int n, double x0, double dx, double dt, int periodic);
void field_free(Field *f);
double field_x(const Field *f, int j);
/* Exact moving kink (sign +1) or antikink (sign -1) with centre x0 and velocity v. */
void field_set_kink(Field *f, double centre, double v, int sign);
/* Small-amplitude standing wave amp * cos(k x), starting at rest. */
void field_set_wave(Field *f, double amp, double k);
void field_step(Field *f);
/* Energy: integral of phi_t^2/2 + phi_x^2/2 + (1 - cos phi). */
double field_energy(const Field *f);
/* Position where phi crosses pi (linear interpolation), or NAN if it does not. */
double field_kink_position(const Field *f);
double field_gradient(const Field *f, int j);
double field_max_gradient(const Field *f);

#endif
