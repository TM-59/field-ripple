#ifndef PAIR_H
#define PAIR_H

#include "field.h"

/* Two-soliton solutions of the sine-Gordon equation phi_tt = phi_xx - sin(phi) (c = 1, eps = 0) and
 * the measurements that compare the leapfrog solver of field.c against them. Nothing here changes
 * field.c.
 *
 * Kink-antikink pair (Perring-Skyrme): phi = 4 atan( sinh(gamma v t) / (v cosh(gamma x)) ), the two
 * meet at x = 0 when t = 0 and pass through each other; each comes out ahead of its free path by 2
 * sqrt(1 - v^2) ln(1/v). Total energy 16 gamma. Breather: phi = 4 atan( nu sin(omega t) / (omega
 * cosh(nu x)) ), nu = sqrt(1 - omega^2); a bound pair that oscillates at the angular frequency
 * omega with energy 16 nu < 16. */

double sg_pair_exact(double x, double t, double v);
double sg_breather_exact(double x, double t, double omega);
/* Forward shift of each soliton after the collision, 2 sqrt(1 - v^2) ln(1/v). */
double sg_pair_shift(double v);
/* Energy of the pair, 16 / sqrt(1 - v^2), and of the breather, 16 sqrt(1 - omega^2). */
double sg_pair_energy(double v);
double sg_breather_energy(double omega);

/* Initial levels (t0 and t0 - dt) from the exact solutions. The pair meets at x = 0 at t = 0. */
void field_set_pair(Field *f, double v, double t0);
/* A breather of angular frequency omega at phase time t0, centred at `centre`. */
typedef struct {
    double omega, t0, centre;
} Breather;
void field_set_breather(Field *f, Breather b);
/* Sum of a free kink at -a moving at +v and a free antikink at +a moving at -v (no interaction
 * built in). */
void field_set_superposed_pair(Field *f, double a, double v);

/* Field momentum P = -integral of phi_t phi_x dx (a kink alone carries 8 gamma v). */
double field_momentum(const Field *f);
/* Positions, left to right, where |phi| crosses pi (the soliton centres); returns how many were
 * found. */
int field_fronts(const Field *f, double *xs, int max);
/* phi at x by linear interpolation between grid points (clamped to the grid). */
double field_value_at(const Field *f, double x);

/* Collide the superposed pair (half distance a = 20) on a grid dx, dt = dx / 2 and return the
 * measured forward shift of the right-moving soliton: straight lines are fitted to its centre well
 * before (separation >= 16) and well after the collision, and their gap at the meeting time is the
 * shift. The fitted speeds are returned too (speeds may be NULL). */
/* One collision run: the speed of each soliton and the grid step. */
typedef struct {
    double v, dx;
} PairTrial;
/* The fitted soliton speeds before and after the collision. */
typedef struct {
    double in, out;
} Speeds;
double pair_measure_shift(PairTrial trial, Speeds *speeds);
/* The fit itself: a line through (tb, xb) (one soliton before) and one through (ta, xa) (the same
 * soliton after); returns the gap of the second over the first at t = meet, or NAN with fewer than
 * 3 points on either side. */
double pair_shift_from_tracks(const double *tb, const double *xb, int nb, const double *ta,
                              const double *xa, int na, double meet, Speeds *speeds);
/* Run an exact breather for `periods` periods on a grid dx, dt = dx / 2 and return the angular
 * frequency measured from the zero crossings of phi(0, t). energy_drift: largest relative change of
 * E between the peaks of |phi(0, t)|, where the field stands still (the same phase every half
 * period), against the first peak; swing: largest |E / E0 - 1| over all steps; separation: largest
 * distance between the two fronts. stats may be NULL. */
/* One breather run: frequency, grid step and how many periods to follow. */
typedef struct {
    double omega, dx;
    int periods;
} BreatherTrial;
/* What the run saw besides the frequency (see above). */
typedef struct {
    double energy_drift, swing, separation;
} BreatherStats;
double breather_measure_frequency(BreatherTrial trial, BreatherStats *stats);
/* Widest front separation of the exact breather: 2 arccosh(nu / omega) / nu (0 when nu <= omega:
 * |phi| never reaches pi). */
double sg_breather_separation(double omega);

#endif
