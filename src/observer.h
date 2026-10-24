#ifndef OBSERVER_H
#define OBSERVER_H

/* Presentation adapter. Units: c=1, kink rest energy=8, phi in radians. */
#define OBSERVER_SAMPLES 601
int fr_reset(double speed);
int fr_step(int direction, int steps);
double fr_time(void);
double fr_position(void);
double fr_velocity(void);
double fr_energy(void);
double fr_width(void);
int fr_stopped(void);
/* Interleaved phi, energy density over [position-12, position+12]. */
const double *fr_samples(void);
int fr_count(void);

#endif
