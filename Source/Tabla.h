#pragma once
// Tabla: two modal drums. Each drum is one membrane (a bank of complex resonators) shared by all
// strokes, so a damped stroke (Te, Ke) really mutes what is ringing, and an open stroke (Tun, Ge)
// adds to it. The dayan's syahi loading pulls its first modes onto a harmonic series
// (C.V. Raman, 1934); the SYAHI control blends from a plain membrane to that loaded one.
#include "Dsp.h"

namespace tantri
{
struct TablaParams
{
    float dayanRatio = 2.0f;  // dayan pitch as a multiple of Sa
    float bayanHz = 98.0f;
    float syahi = 1.0f;       // 0 plain membrane .. 1 fully loaded (harmonic)
    float glide = 0.55f;      // ghe glide amount
    float press = 0.2f;       // resting bayan wrist pressure
    float ring = 1.0f;        // decay scale
    float snap = 0.35f;       // stroke noise
    float level = 0.75f;
};

enum Bol { Ge, Ke, Ghe, Na, Tin, Tun, Te, Re, Dha, Dhin, Tit, Kat, kNumBols };
inline constexpr const char* kBolName[kNumBols] = { "GE", "KE", "GHE", "NA", "TIN", "TUN", "TE", "RE", "DHA", "DHIN", "TIT", "KAT" };

class Drum
{
public:
    struct ModeDef { float ideal, loaded; int group; float t60; float m; float z; };
    void prepare (double sampleRate, const std::vector<ModeDef>& defs, float detunePairs)
    {
        fs = sampleRate;
        modes.clear();
        for (auto& d : defs)
        {
            modes.push_back ({ d, 0, 0, 0, 0, 0, 0, 1.0f });
            if (d.m > 0)   // degenerate cos/sin pair split slightly: the beating ring of a real head
            {
                ModeDef e = d; e.ideal *= 1 + detunePairs; e.loaded *= 1 + detunePairs;
                modes.push_back ({ e, 0, 0, 0, 0, 0, 0, 0.6f });
            }
        }
        noiseBP.bandpass (fs, 3000, 1.2);
        reset();
    }
    void reset() { for (auto& m : modes) { m.re = m.im = 0; } pm = pmTarget = 1; env = 0; for (auto& d : extraDamp) d = 0; contact = 0; noiseEnv = 0; }

    void setTuning (float f0Hz, float syahiAmt, float ringScale) { f0 = f0Hz; syahi = syahiAmt; ring = ringScale; }
    void setPitchMul (float p) { pmTarget = p; }

    // strike: force pulse of `contactMs`, per-group excitation, damping left on the head afterwards
    void strike (float amp, float contactMs, const float excite[6], const float dampT60[6], float noise, float noiseHz, float radius)
    {
        contactLen = std::max (2, int (contactMs * 0.001 * fs));
        contact = contactLen;
        impulseNorm = 48.0f / float (contactLen);
        strikeAmp = amp;
        for (size_t i = 0; i < modes.size(); ++i)
        {
            auto& m = modes[i];
            float shape = radialShape (m.def.m, m.def.z, radius);
            m.drive = excite[m.def.group] * shape * m.pairGain;
        }
        for (int g = 0; g < 6; ++g) extraDamp[g] = dampT60[g];
        noiseBP.bandpass (fs, noiseHz, 1.4);
        noiseEnv = noise * amp;
        noiseDecay = float (std::exp (-1.0 / (0.012 * fs)));
    }
    void lift() { for (auto& d : extraDamp) d = 0; }   // fingers leave the head

    inline float tick (Rng& rng)
    {
        if ((ctr++ & 15) == 0) update();
        float force = 0;
        if (contact > 0)
        {
            float ph = 1.0f - float (contact) / float (contactLen);
            force = strikeAmp * (1.0f - std::cos (2.0f * float (kPi) * ph)) * impulseNorm;   // unit-area pulse: same momentum at any contact time
            --contact;
        }
        float y = 0;
        for (auto& m : modes)
        {
            float nre = m.cr * m.re - m.ci * m.im + force * m.drive;
            float nim = m.ci * m.re + m.cr * m.im;
            m.re = nre; m.im = nim;
            y += nim;
        }
        float n = 0;
        if (noiseEnv > 1e-5f) { n = noiseBP.tick (rng.next()) * noiseEnv; noiseEnv *= noiseDecay; }
        float out = y * 0.06f + n;
        env = env * 0.999f + 0.001f * std::fabs (out);
        return out;
    }
    float level() const { return env; }

private:
    struct Mode { ModeDef def; float re, im, cr, ci, drive, unused; float pairGain; };

    static float besselJ (int m, float x)
    {
        // J_m(x) = 1/pi * integral_0^pi cos(m t - x sin t) dt
        const int N = 48; double s = 0;
        for (int i = 0; i <= N; ++i)
        {
            double t = kPi * i / N, wgt = (i == 0 || i == N) ? 0.5 : 1.0;
            s += wgt * std::cos (m * t - x * std::sin (t));
        }
        return float (s / N);
    }
    static float radialShape (float m, float z, float r)
    {
        float v = besselJ (int (m), z * r);
        // normalise by the peak of the mode's radial profile
        float pk = 1e-3f;
        for (int i = 1; i <= 24; ++i) pk = std::max (pk, std::fabs (besselJ (int (m), z * i / 24.0f)));
        return v / pk;
    }

    void update()
    {
        pm += (pmTarget - pm) * 0.04f;
        float tension = 1.0f + 0.9f * std::min (0.05f, env);   // louder = slightly sharper (tension modulation)
        for (auto& m : modes)
        {
            float ratio = m.def.ideal + (m.def.loaded - m.def.ideal) * syahi;
            double f = f0 * ratio * pm * tension;
            if (f > fs * 0.45) { m.cr = m.ci = 0; continue; }
            double t60 = m.def.t60 * ring;
            float ed = extraDamp[m.def.group];
            if (ed > 0) t60 = 1.0 / (1.0 / t60 + 1.0 / ed);
            double r = std::exp (-6.9078 / (t60 * fs));
            double w = 2 * kPi * f / fs;
            m.cr = float (r * std::cos (w)); m.ci = float (r * std::sin (w));
        }
    }

    double fs = 48000;
    std::vector<Mode> modes;
    float f0 = 280, syahi = 1, ring = 1, pm = 1, pmTarget = 1, env = 0;
    float extraDamp[6] {};
    int contact = 0, contactLen = 1;
    float strikeAmp = 0, noiseEnv = 0, noiseDecay = 0.99f, impulseNorm = 1.0f;
    Biquad noiseBP;
    unsigned ctr = 0;
};

class Tabla
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        // dayan: (m,n) Bessel zero z; ideal ratio = z/z01; loaded ratio from Raman's harmonic groups
        std::vector<Drum::ModeDef> dayanModes {
            { 1.000f, 1.000f, 0, 1.6f, 0, 2.4048f },
            { 1.593f, 2.000f, 1, 1.3f, 1, 3.8317f },
            { 2.136f, 3.000f, 2, 1.0f, 2, 5.1356f }, { 2.295f, 3.010f, 2, 0.9f, 0, 5.5201f },
            { 2.653f, 4.000f, 3, 0.75f, 3, 6.3802f }, { 2.917f, 4.020f, 3, 0.7f, 1, 7.0156f },
            { 3.155f, 5.000f, 4, 0.5f, 4, 7.5883f }, { 3.500f, 5.020f, 4, 0.45f, 2, 8.4172f }, { 3.598f, 5.050f, 4, 0.45f, 0, 8.6537f },
            { 3.647f, 5.830f, 5, 0.16f, 5, 8.7715f }, { 4.059f, 6.400f, 5, 0.14f, 3, 9.7610f }, { 4.132f, 7.300f, 5, 0.12f, 6, 9.9361f },
            { 4.230f, 6.900f, 5, 0.12f, 1, 10.1735f }, { 4.601f, 7.800f, 5, 0.1f, 4, 11.0647f }, { 4.832f, 8.600f, 5, 0.09f, 2, 11.6198f } };
        // bayan: lighter, off-centre loading: mildly stretched, not harmonic
        std::vector<Drum::ModeDef> bayanModes {
            { 1.000f, 1.000f, 0, 1.1f, 0, 2.4048f },
            { 1.593f, 1.520f, 1, 0.8f, 1, 3.8317f },
            { 2.136f, 2.000f, 2, 0.55f, 2, 5.1356f }, { 2.295f, 2.180f, 2, 0.45f, 0, 5.5201f },
            { 2.653f, 2.650f, 3, 0.35f, 3, 6.3802f }, { 2.917f, 2.900f, 3, 0.3f, 1, 7.0156f },
            { 3.155f, 3.200f, 4, 0.25f, 4, 7.5883f }, { 3.500f, 3.600f, 4, 0.2f, 2, 8.4172f },
            { 4.059f, 4.100f, 5, 0.12f, 3, 9.7610f }, { 4.601f, 4.700f, 5, 0.09f, 4, 11.0647f } };
        dayan.prepare (fs, dayanModes, 0.0035f);
        bayan.prepare (fs, bayanModes, 0.006f);
        shell.prepare (fs, { { 420, 8, 0.4f }, { 980, 10, 0.3f }, { 2100, 12, 0.2f } });
        dcL.prepare (fs); dcR.prepare (fs);
    }
    void setParams (const TablaParams& p, double saHz) { prm = p; sa = saHz; }
    void setPressure (float p) { atPress = p; }

    void play (int bol, float vel)
    {
        float v = 0.25f + 0.75f * vel;
        float hard = 1.25f - 0.55f * vel;   // harder strike = shorter contact
        auto D = [&] (const float ex[6], const float dm[6], float ms, float noise, float nhz, float r, float gain = 1.0f)
        { dayan.strike (v * gain, ms * hard, ex, dm, noise * prm.snap * 2.0f, nhz, r); dayanFlash = 1; };
        auto B = [&] (const float ex[6], const float dm[6], float ms, float noise, float nhz, float r, float gain = 1.0f)
        { bayan.strike (v * 1.3f * gain, ms * hard, ex, dm, noise * prm.snap * 2.0f, nhz, r); bayanFlash = 1; };
        static const float none[6] = { 0, 0, 0, 0, 0, 0 };
        // dayan strokes
        static const float naEx[6]  = { 0.12f, 1.0f, 0.85f, 0.6f, 0.5f, 0.7f };
        static const float naDm[6]  = { 0.04f, 0, 0, 0, 0, 0 };          // ring finger kills the fundamental
        static const float tinEx[6] = { 0.55f, 1.0f, 0.55f, 0.4f, 0.3f, 0.35f };
        static const float tunEx[6] = { 1.0f, 0.25f, 0.3f, 0.12f, 0.1f, 0.05f };
        static const float teEx[6]  = { 0.8f, 0.35f, 0.4f, 0.25f, 0.2f, 0.4f };
        static const float teDm[6]  = { 0.045f, 0.04f, 0.035f, 0.03f, 0.03f, 0.02f };
        static const float reEx[6]  = { 0.6f, 0.5f, 0.45f, 0.3f, 0.3f, 0.45f };
        static const float reDm[6]  = { 0.06f, 0.05f, 0.045f, 0.04f, 0.035f, 0.025f };
        // bayan strokes
        static const float geEx[6]  = { 1.0f, 0.55f, 0.35f, 0.25f, 0.18f, 0.15f };
        static const float keEx[6]  = { 0.6f, 0.45f, 0.35f, 0.3f, 0.25f, 0.3f };
        static const float keDm[6]  = { 0.035f, 0.03f, 0.025f, 0.025f, 0.02f, 0.02f };
        switch (bol)
        {
            case Na:   D (naEx, naDm, 0.45f, 0.25f, 3800, 0.93f, 1.8f); break;
            case Tin:  dayan.lift(); D (tinEx, none, 0.7f, 0.12f, 3200, 0.62f); break;
            case Tun:  dayan.lift(); D (tunEx, none, 1.4f, 0.05f, 2000, 0.08f); break;
            case Te:   D (teEx, teDm, 1.8f, 0.45f, 1800, 0.15f, 2.6f); break;
            case Re:   D (reEx, reDm, 1.1f, 0.4f, 2400, 0.3f, 1.8f); break;
            case Tit:  D (teEx, teDm, 1.6f, 0.45f, 1800, 0.15f, 2.6f); pendingRe = int (0.045 * fs); pendingVel = vel * 0.8f; break;
            case Ge:   bayan.lift(); B (geEx, none, 3.0f, 0.06f, 900, 0.72f); gheEnv = 0; break;
            case Ghe:  bayan.lift(); B (geEx, none, 3.0f, 0.06f, 900, 0.72f); gheEnv = 0.0001f; gheDir = 1; break;
            case Ke:   B (keEx, keDm, 4.0f, 0.6f, 700, 0.45f, 1.7f); break;
            case Kat:  B (keEx, keDm, 2.5f, 0.8f, 1100, 0.6f, 1.7f); break;
            case Dha:  D (naEx, naDm, 0.45f, 0.25f, 3800, 0.93f, 1.8f); bayan.lift(); B (geEx, none, 3.0f, 0.06f, 900, 0.72f); break;
            case Dhin: dayan.lift(); D (tinEx, none, 0.7f, 0.12f, 3200, 0.62f); bayan.lift(); B (geEx, none, 3.0f, 0.06f, 900, 0.72f); break;
            default: break;
        }
        lastBol = bol; bolFlash = 1.0f; ++strokeCount;
    }

    inline void tick (float& outL, float& outR)
    {
        if ((ctl++ & 31) == 0) control();
        if (pendingRe > 0 && --pendingRe == 0) play (Re, pendingVel);
        float d = dayan.tick (rng), b = bayan.tick (rng);
        float sh = shell.tick (d * 0.3f + b * 0.2f);
        // player's view: dayan on the right, bayan on the left
        float l = 0.62f * b + 0.38f * d + sh * 0.2f, r = 0.62f * d + 0.38f * b + sh * 0.2f;
        outL = dcL.tick (l) * prm.level * 0.15f;
        outR = dcR.tick (r) * prm.level * 0.15f;
    }

    int lastBol = -1;
    float bolFlash = 0, dayanFlash = 0, bayanFlash = 0;
    unsigned strokeCount = 0;

private:
    void control()
    {
        const float dt = float (32.0 / fs);
        dayan.setTuning (float (sa * prm.dayanRatio), prm.syahi, prm.ring);
        bayan.setTuning (prm.bayanHz, 1.0f, prm.ring);
        // ghe: wrist slides forward after the stroke -> pitch rises then settles
        if (gheEnv > 0)
        {
            if (gheDir > 0) { gheEnv += dt / 0.12f; if (gheEnv >= 1) { gheEnv = 1; gheDir = -1; } }
            else { gheEnv -= dt / 0.9f; if (gheEnv <= 0) gheEnv = 0; }
        }
        float p = std::clamp (prm.press + atPress + gheEnv * prm.glide, 0.0f, 1.5f);
        bayan.setPitchMul (1.0f + 0.55f * p);
        dayan.setPitchMul (1.0f);
    }

    double fs = 48000, sa = 138.59;
    Drum dayan, bayan;
    ModalBody shell;
    DcBlock dcL, dcR;
    TablaParams prm;
    Rng rng;
    float atPress = 0, gheEnv = 0, pendingVel = 0;
    int gheDir = 0, pendingRe = 0;
    unsigned ctl = 0;
};
} // namespace tantri
