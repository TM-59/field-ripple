#include "../src/observer.h"
#include <math.h>
#include <stdio.h>

int main(void)
{
    int failures = 0;
    const double speeds[] = {0, 0.6, 0.9};
    puts("v0,time,x,v,energy,width,relative_energy_drift");
    for (int k = 0; k < 3; k++) {
        double v = speeds[k], gamma = 1 / sqrt(1 - v * v);
        if (fr_reset(v)) return 1;
        double energy = fr_energy();
        for (int i = 0; i < 10; i++) if (fr_step(0, 80)) return 1;
        double drift = fabs(fr_energy() / energy - 1);
        printf("%.1f,%.1f,%.6f,%.6f,%.6f,%.6f,%.8f\n", v, fr_time(), fr_position(), fr_velocity(), fr_energy(), fr_width(), drift);
        failures += fabs(fr_position() - v * 40) > 0.1 || drift > 0.001;
        failures += fabs(fr_width() * gamma - 1) > 0.03;
    }
    if (fr_reset(0)) return 1;
    for (int i = 0; i < 10; i++) if (fr_step(1, 60)) return 1;
    double push = fr_velocity();
    for (int i = 0; i < 10; i++) if (fr_step(-1, 60)) return 1;
    printf("push/brake: v %.6f -> %.6f; x %.6f\n", push, fr_velocity(), fr_position());
    failures += push < 0.6 || fabs(fr_velocity()) > 0.05;
    return failures != 0;
}
