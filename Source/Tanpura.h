#pragma once
// Tanpura: four strings plucked in an endless cycle (Pa/Ma/Ni - Sa - Sa - kharaj sa). Each string has
// two polarisations, both grazing a wide flat bridge whose contact is opened by the jawari thread;
// that is where the slow "swirl" of harmonics comes from.
#include "Dsp.h"
#include "Scales.h"

namespace tantri
{
struct TanpuraParams
{
    bool on = true;
    float cycleBpm = 56;
    float jawari = 0.8f;
    int first = 0;          // 0 auto, 1 Pa, 2 Ma, 3 Ni, 4 komal ni
    float human = 0.5f;
    int octave = 0;         // -1, 0, +1
    float width = 0.6f;
    float level = 0.6f;
};

class Tanpura
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& s : str) { s.a.prepare (fs, 20.0); s.b.prepare (fs, 20.0); }
        body.prepare (fs, { { 82, 9, 0.8f }, { 128, 11, 1.0f }, { 186, 13, 0.9f }, { 255, 14, 0.8f }, { 336, 16, 0.75f },
                            { 448, 18, 0.65f }, { 598, 20, 0.55f }, { 795, 22, 0.5f }, { 1046, 25, 0.42f },
                            { 1395, 28, 0.36f }, { 1840, 30, 0.3f }, { 2400, 32, 0.24f }, { 3120, 35, 0.18f } });
        dcL.prepare (fs); dcR.prepare (fs);
        lp.lowpass (fs, 9000, 0.7);
        timer = 0.3; step = 0;
        configured = false;
    }

    void setParams (const TanpuraParams& p, const ActiveScale& sc, unsigned version, double saHz)
    {
        bool retune = ! configured || p.first != prm.first || p.octave != prm.octave || saHz != sa || version != scaleVersion;
        prm = p; sa = saHz;
        if (version != scaleVersion) { scale = sc; scaleVersion = version; }
        if (retune) configure();
        double c = 0.0035 * std::pow (0.3, prm.jawari);
        double jd = 30.0 * std::pow (prm.jawari, 1.3);
        for (auto& s : str) { s.a.setContact (c, 1.0); s.b.setContact (c * 1.6, 0.8); s.a.setJawari (jd); s.b.setJawari (jd * 0.7); }
    }

    int firstStringChoice() const
    {
        if (prm.first != 0) return prm.first;
        if (nearestCents (scale, 702, 25, -1) >= 0) return 1;                                     // Pa
        if (nearestCents (scale, 498, 40, -1) >= 0 || nearestCents (scale, 590, 30, -1) >= 0) return 2;  // Ma
        return 3;                                                                                 // Ni
    }

    inline void tick (float& outL, float& outR)
    {
        if ((ctl++ & 31) == 0) control();
        float force = 0, l = 0, r = 0;
        for (int i = 0; i < 4; ++i)
        {
            auto& s = str[i];
            float y = s.a.tick (coupling) + 0.8f * s.b.tick (coupling);
            force += y;
            float pan = float (i - 1.5) / 1.5f * prm.width * 0.8f;
            l += y * (1 - pan); r += y * (1 + pan);
            s.level = s.level * 0.9995f + 0.0005f * y * y;
        }
        coupling = force * 0.0015f;
        float b = lp.tick (body.tick (force)) * 0.9f;
        float L = 0.45f * l * 0.5f + b, R = 0.45f * r * 0.5f + b;
        outL = dcL.tick (L) * prm.level * 7.7f;
        outR = dcR.tick (R) * prm.level * 7.7f;
    }

    void setGate (bool g) { gate = g; }
    float stringLevel (int i) const { return std::min (1.0f, std::sqrt (str[i].level) * 25.0f); }
    float pluckFlash[4] {};
    double stringHz[4] {};
    int lastPlucked = -1;

private:
    struct Str { ContactString a, b; float level = 0; };

    void configure()
    {
        double base = sa * std::pow (2.0, prm.octave);
        // the first string takes the scale's own pitch for Pa / Ma / Ni (one octave down)
        double c = 702;
        switch (firstStringChoice())
        {
            case 1: c = nearestCents (scale, 702, 25, 701.955); break;
            case 2: { double m = nearestCents (scale, 498, 40, -1); c = m >= 0 ? m : nearestCents (scale, 590, 30, 498.045); } break;
            case 3: c = nearestCents (scale, 1088, 40, 1088.269); break;
            case 4: c = nearestCents (scale, 1018, 40, 1017.596); break;
            default: break;
        }
        double firstR = std::pow (2.0, (c - 1200.0) / 1200.0);
        stringHz[0] = base * firstR; stringHz[1] = base; stringHz[2] = base; stringHz[3] = base * 0.5;
        for (int i = 0; i < 4; ++i)
        {
            auto& s = str[i];
            double f = stringHz[i];
            double detune = (i == 2) ? 0.9 : (i == 1 ? -0.4 : 0.0);   // the two Sa strings beat gently
            f *= std::pow (2.0, detune / 1200.0);
            s.a.configure (f, 10, 0.12);
            s.b.configure (f * std::pow (2.0, 0.7 / 1200.0), 10, 0.10);
            double t60 = (i == 3 ? 11.0 : 8.5) * std::pow (f / 130.0, -0.25);
            s.a.setLoss (t60, i == 3 ? 0.62 : 0.74, f);
            s.b.setLoss (t60 * 0.85, 0.65, f);
            s.a.setFreq (f); s.a.snapFreq();
            s.b.setFreq (f * std::pow (2.0, 0.7 / 1200.0)); s.b.snapFreq();
            drift[i] = 0;
        }
        configured = true;
    }

    void control()
    {
        const double dt = 32.0 / fs;
        for (auto& f : pluckFlash) f = std::max (0.0f, f - float (dt) * 1.5f);
        // slow pitch drift: the instrument is alive, never a frozen oscillator
        for (int i = 0; i < 4; ++i)
        {
            drift[i] += (rng.next() * 0.02 - drift[i] * 0.002);
            double cents = std::clamp (drift[i], -1.2, 1.2);
            double f = stringHz[i] * std::pow (2.0, cents / 1200.0) * std::pow (2.0, ((i == 2) ? 0.9 : (i == 1 ? -0.4 : 0.0)) / 1200.0);
            str[i].a.setFreq (f);
            str[i].b.setFreq (f * std::pow (2.0, 0.7 / 1200.0));
        }
        if (! (prm.on || gate)) { timer = 0.05; step = 0; return; }
        timer -= dt;
        if (timer <= 0)
        {
            // cycle: 1 . 2 . 3 . 4 . . . ; the gap after the kharaj lets the swirl bloom
            int i = step;
            float vel = 0.75f + 0.25f * rng.next() * prm.human;
            if (i == 3) vel *= 1.1f;
            float amp = 0.2f * vel;
            str[i].a.pluck (amp, float (0.0026 * fs * (1 + 0.2 * rng.next() * prm.human)), 0.32f, 0.0f, rng);
            str[i].b.pluck (amp * 0.6f, float (0.003 * fs), 0.3f, 0.0f, rng);
            pluckFlash[i] = 1.0f; lastPlucked = i;
            double beat = 60.0 / std::max (10.0f, prm.cycleBpm);
            double next = (step == 3) ? beat * 2.0 : beat;
            next *= 1.0 + 0.08 * prm.human * rng.next();
            timer += next;
            step = (step + 1) & 3;
        }
    }

    double fs = 48000, sa = 138.59, timer = 0.3;
    std::array<Str, 4> str;
    double drift[4] {};
    ModalBody body;
    Biquad lp;
    DcBlock dcL, dcR;
    TanpuraParams prm;
    ActiveScale scale = makeScale (0, parseRaga (kRagas[0].spec));
    unsigned scaleVersion = ~0u;
    Rng rng;
    float coupling = 0;
    int step = 0;
    bool gate = false, configured = false;
    unsigned ctl = 0;
};
} // namespace tantri
