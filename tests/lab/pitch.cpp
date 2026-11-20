#include "../../Source/Dsp.h"
#include <cstdio>
using namespace tantri;
static double f0of (const std::vector<float>& x, int s0, int n, double expect, double fs)
{
    int lo = int (fs / (expect * 1.1)), hi = int (fs / (expect / 1.1)) + 2; int W = n;
    double best = -1; int bl = lo; std::vector<double> r (hi + 2);
    for (int L = lo - 1; L <= hi + 1; ++L) { double s = 0, e0 = 0, e1 = 0; for (int i = s0; i < s0 + W; ++i) { s += x[i] * x[i + L]; e0 += x[i]*x[i]; e1 += x[i+L]*x[i+L]; } r[L] = s / sqrt (e0 * e1); }
    for (int L = lo; L <= hi; ++L) if (r[L] > best) { best = r[L]; bl = L; }
    double a = r[bl-1], b = r[bl], c = r[bl+1]; return fs / (bl + 0.5 * (a - c) / (a - 2*b + c));
}
int main()
{
    double fs = 48000;
    for (double f0 : { 69.3, 138.59, 259.86, 554.0 })
    for (double head : { 0.0, 1220.0 })
    {
        ContactString s; s.prepare (fs, 25); s.configure (f0, head, 0.1); s.setLoss (6, 0.8, f0); s.setContact (0.001, 0); s.setJawari (0);
        s.setFreq (f0); s.snapFreq(); Rng r; s.pluck (0.3f, 12, 0.2f, 0, r);
        std::vector<float> y (int (fs * 2)); for (auto& v : y) v = s.tick();
        double f = f0of (y, int (fs * 0.5), int (fs * 0.5), f0, fs);
        printf ("f0 %7.2f head %4.0f M %4d  -> %+.2f cents\n", f0, head, s.railLen(), 1200 * log2 (f / f0));
    }
}
