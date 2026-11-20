// quick experiments on ContactString alone
#include "../../Source/Dsp.h"
#include <cstdio>
#include <cstdlib>
using namespace tantri;
int main (int argc, char** argv)
{
    double fs = 48000, f0 = 138.59;
    double jd = argc > 1 ? atof (argv[1]) : 0;
    ContactString s; s.prepare (fs, 25); s.configure (f0, 1220, 0.1); s.setLoss (6, 0.8, f0); s.setContact (0.001, 0); s.setJawari (jd);
    s.setFreq (f0); s.snapFreq();
    Rng r; s.pluck (0.3f, 12, 0.2f, 0.0f, r);
    int n = int (fs * 3);
    std::vector<float> y (n);
    for (int i = 0; i < n; ++i) y[i] = s.tick();
    // energy and simple high-band (first difference) share per 0.25 s
    printf ("jd %.0f:", jd);
    for (int k = 0; k < 12; k += 3)
    {
        double e = 0, d = 0; for (int i = k * 12000; i < (k + 1) * 12000; ++i) { e += y[i] * y[i]; if (i) d += (y[i] - y[i-1]) * (y[i] - y[i-1]); }
        double m = 0; int zc = 0; for (int i = k * 12000; i < (k + 1) * 12000; ++i) { m += y[i]; if (i && (y[i] > 0) != (y[i-1] > 0)) ++zc; }
        printf (" %.1f/%.0f%%/m%.3f/zc%d", 10 * log10 (e / 12000 + 1e-20), 100 * d / (e + 1e-20), m / 12000, zc * 2);
    }
    printf ("\n");
}
