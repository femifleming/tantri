#pragma once
// Shared DSP for Tantri: the jawari contact string (digital waveguide with a distributed one-sided
// bridge barrier), modal body resonators, DC blocker and an 8-line FDN reverb.
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace tantri
{
constexpr double kPi = 3.14159265358979323846;

inline int nextPow2 (int n) { int p = 1; while (p < n) p <<= 1; return p; }

struct Rng
{
    uint32_t s = 0x9e3779b9u;
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return float (s) * (2.0f / 4294967296.0f) - 1.0f; }
    float uni()  { return 0.5f * (next() + 1.0f); }
};

struct DcBlock
{
    float x1 = 0, y1 = 0, r = 0.9995f;
    void prepare (double fs) { r = float (1.0 - 2.0 * kPi * 12.0 / fs); }
    float tick (float x) { float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
    void reset() { x1 = y1 = 0; }
};

// RBJ band-pass (0 dB peak), transposed direct form II
struct Biquad
{
    float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void bandpass (double fs, double f, double q)
    {
        double w = 2 * kPi * std::min (f, fs * 0.45) / fs, al = std::sin (w) / (2 * q), a0 = 1 + al;
        b0 = float (al / a0); b1 = 0; b2 = float (-al / a0);
        a1 = float (-2 * std::cos (w) / a0); a2 = float ((1 - al) / a0);
    }
    void lowpass (double fs, double f, double q)
    {
        double w = 2 * kPi * std::min (f, fs * 0.45) / fs, al = std::sin (w) / (2 * q), c = std::cos (w), a0 = 1 + al;
        b0 = float ((1 - c) / 2 / a0); b1 = float ((1 - c) / a0); b2 = b0;
        a1 = float (-2 * c / a0); a2 = float ((1 - al) / a0);
    }
    float tick (float x) { float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
};

// Body: a bank of resonant modes (gourd / tabli / neck). Input = bridge force, output = radiated sound.
struct ModalBody
{
    struct Mode { float f, q, g; };
    std::vector<Biquad> bank;
    std::vector<float> gains;
    void prepare (double fs, const std::vector<Mode>& modes, float qScale = 1.0f)
    {
        bank.assign (modes.size(), {});
        gains.clear();
        for (size_t i = 0; i < modes.size(); ++i)
        {
            bank[i].bandpass (fs, modes[i].f, modes[i].q * qScale);
            gains.push_back (modes[i].g);
        }
    }
    float tick (float x)
    {
        float y = 0;
        for (size_t i = 0; i < bank.size(); ++i) y += gains[i] * bank[i].tick (x);
        return y;
    }
    void reset() { for (auto& b : bank) b.reset(); }
};

// 4-point Lagrange read from a circular buffer: value delayed by d samples (d >= 1)
inline float lagrangeRead (const float* buf, int mask, int w, double d)
{
    int i = int (d);
    float f = float (d - i);
    float x0 = buf[(w - i + 1) & mask], x1 = buf[(w - i) & mask], x2 = buf[(w - i - 1) & mask], x3 = buf[(w - i - 2) & mask];
    float t = f + 1.0f;   // position relative to x0 (delay i-1)
    float h0 = -(t - 1) * (t - 2) * (t - 3) / 6.0f;
    float h1 = t * (t - 2) * (t - 3) / 2.0f;
    float h2 = -t * (t - 1) * (t - 3) / 2.0f;
    float h3 = t * (t - 1) * (t - 2) / 6.0f;
    return h0 * x0 + h1 * x1 + h2 * x2 + h3 * x3;
}

// ----------------------------------------------------------------------------------------------
// ContactString: two travelling-wave rails between nut and bridge. The bridge end has a frequency
// dependent loss filter; the nut end has a variable fractional delay (fretting / meend / gamak).
// Near the bridge, K points of the string can strike a curved one-sided barrier h_j = c * j^2:
// this is the jawari. When the string displacement there falls below -h_j it is pushed back to
// the barrier. Bigger amplitude -> longer contact -> energy thrown into high partials that then
// sweep down as the note decays: the sitar / tanpura buzz.
// ----------------------------------------------------------------------------------------------
class ContactString
{
public:
    // runs internally at 2x the host rate: the fractional-delay interpolation then costs nothing in
    // the audible band and the jawari's new harmonics do not alias
    static constexpr int OS = 2;
    void prepare (double sampleRate, double lowestHz)
    {
        fs = sampleRate * OS;
        int maxLoop = int (fs / lowestHz) + 16;
        size = nextPow2 (maxLoop / 2 + 64); mask = size - 1;
        R.assign ((size_t) size, 0.0f); L.assign ((size_t) size, 0.0f);
        nsize = nextPow2 (maxLoop + 64); nmask = nsize - 1;
        nut.assign ((size_t) nsize, 0.0f);
        hpR = float (1.0 - 2.0 * kPi * 0.5 / fs);   // 0.5 Hz: removes DC without detuning the fundamental against its harmonics
        jawSm = float (1.0 - std::exp (-2.0 * kPi * 2500.0 / fs));
        jawAvgK = float (1.0 - std::exp (-1.0 / (0.03 * fs)));
        reset();
    }
    void reset()
    {
        std::fill (R.begin(), R.end(), 0.0f); std::fill (L.begin(), L.end(), 0.0f); std::fill (nut.begin(), nut.end(), 0.0f);
        for (auto& x : apS) x = 0; jawY = 0; hpX = hpY = 0; jawS = 0; jawAvg = 0;
        lp = 0; w = 0; nw = 0; exPos = exLen = 0; energy = 0;
    }

    // base frequency and the largest upward bend (cents) that must be reachable without re-layout
    void configure (double f0, double maxUpCents, double contactLenFrac)
    {
        double N = fs / f0;
        double Nmin = N / std::pow (2.0, std::max (0.0, maxUpCents) / 1200.0);
        M = std::max (6, int ((Nmin - 8.0 - 6.0) * 0.5) + 1);
        M = std::min (M, size - 40);
        K = std::clamp (int (contactLenFrac * M), 0, std::min (48, M / 3));
        V = std::max (2.0, N - 2.0 * M - lossDelay (f0));
        Vtarget = V;
    }
    void setLoss (double t60, double brightness, double f0)
    {
        // brightness 0..1 -> pole a (darker = larger a)
        a = float (std::clamp (0.32 - 0.31 * brightness, 0.004, 0.6));
        double periods = t60 * f0;
        g = float (std::pow (10.0, -3.0 / std::max (1.0, periods)));
        // the one-pole loses gain at f0 too; compensate so T60 holds for the fundamental
        double w0 = 2 * kPi * f0 / fs;
        double mag = (1 - a) / std::sqrt (1 - 2 * a * std::cos (w0) + a * a);
        g = float (std::min (0.99999, g / mag));
        curF0 = f0;
    }
    void setContact (double curvature, double depth = 1.0) { cCurv = float (curvature); cDepth = float (depth); }
    // jawari length modulation: samples of shortening per unit of bridge-side displacement
    void setJawari (double perUnit) { jawDepth = float (perUnit); }
    void setFreq (double f)   // target pitch; applied by ramping the nut delay
    {
        Vtarget = std::clamp (fs / f - 2.0 * M - lossDelay (f), 1.5, double (nsize - 8));
    }
    void snapFreq() { V = Vtarget; }

    // start an excitation: a raised-cosine displacement pulse of `widthSamples`, injected at
    // `pos` (0 = bridge, 1 = nut) as a fraction of the rail length
    void pluck (float amp, float widthSamples, float pos, float noiseAmt, Rng& rng)
    {
        exLen = std::clamp (int (widthSamples * OS), 2, 1024);
        exPos = 0; exAmp = amp; exNoise = noiseAmt; rngRef = &rng;
        exAt = std::clamp (int (pos * (M - 2)), 2, M - 3);
    }
    void damp (float extra) { dampG = extra; }   // 1 = none, <1 extra loss per period (release)

    // one host sample. `couple` is injected next to the bridge (sympathetic drive); returns bridge force
    inline float tick (float couple = 0.0f)
    {
        float a0 = step (couple), a1 = step (0.0f);
        float o = 0.25f * (z1 + 2.0f * a0 + a1);   // [1 2 1]/4 across the pair: simple anti-alias decimation
        z1 = a1;
        return o;
    }
    float level() const { return energy; }
    int railLen() const { return M; }

private:
    float z1 = 0;
    inline float step (float couple)
    {
        // the wrapped string is shorter; lengthen the nut side by the average wrap so the settled
        // pitch stays on the shruti (the attack keeps its brief, real sharpness)
        jawAvg += (jawS - jawAvg) * jawAvgK;
        float kAvg = kRest - (kRest - kMin) * jawAvg;
        double comp = kSections * ((1 + kRest) / (1 - kRest) - (1 + kAvg) / (1 - kAvg));
        V += (Vtarget + comp - V) * 0.002;   // smooth nut motion (meend glides)
        const float rEnd = R[(w - (M - 1)) & mask];
        const float lEnd = L[(w - (M - 1)) & mask];
        // bridge: passive curved-bridge termination (Pierce & Van Duyne). A chain of normalised
        // lattice allpasses whose pole follows how hard the string presses into the bridge: the
        // effective length shortens while it wraps onto the curve. Each step is a rotation, so the
        // termination can never add energy, however fast it is modulated.
        float press = std::max (0.0f, -jawY);
        float want = std::min (1.0f, jawDepth * press);
        jawS += (want - jawS) * jawSm;
        float k = kRest - (kRest - kMin) * jawS;
        float c = std::sqrt (1.0f - k * k);
        float bOut = rEnd;
        for (int i = 0; i < kSections; ++i)
        {
            float y = -k * bOut + c * apS[i];
            apS[i] = c * bOut + k * apS[i];
            bOut = y;
        }
        lp = (1 - a) * bOut + a * lp;
        float hp = lp - hpX + hpR * hpY; hpX = lp; hpY = hp;   // a fixed-end string carries no DC
        float lIn = -g * dampG * hp;
        // nut: variable fractional delay + inversion
        nut[nw & nmask] = lEnd;
        float nutOut = lagrangeRead (nut.data(), nmask, nw, V);
        ++nw;
        float rIn = -nutOut;
        ++w;
        R[w & mask] = rIn;
        L[w & mask] = lIn;

        // excitation pulse
        if (exPos < exLen)
        {
            float ph = float (exPos) / float (exLen);
            float e = exAmp * 0.5f * (1.0f - std::cos (2.0f * float (kPi) * ph));
            if (exNoise > 0 && rngRef) e += exAmp * exNoise * rngRef->next() * (1.0f - ph);
            R[(w - (M - 1 - exAt)) & mask] += 0.5f * e;
            L[(w - exAt) & mask] += 0.5f * e;
            ++exPos;
        }
        if (couple != 0.0f)
        {
            R[(w - (M - 3)) & mask] += 0.5f * couple;
            L[(w - 2) & mask] += 0.5f * couple;
        }
        // jawari: distributed one-sided barrier near the bridge
        if (K > 0 && cDepth > 0)
        {
            for (int j = 1; j <= K; ++j)
            {
                float& rr = R[(w - (M - 1 - j)) & mask];
                float& ll = L[(w - j) & mask];
                float y = rr + ll;
                float h = cCurv * float (j * j);
                if (y < -h)
                {
                    float d = (-h - y) * 0.5f * cDepth;
                    rr += d; ll += d;
                }
            }
        }
        jawY = R[(w - (M - 4)) & mask] + L[(w - 3) & mask];
        energy = energy * 0.99975f + 0.00025f * rEnd * rEnd;
        return rEnd;
    }

    double lossDelay (double f) const
    {
        // phase delay of the loop filters at f: one-pole lowpass + in-loop DC blocker (negative delay)
        double wv = 2 * kPi * f / fs;
        std::complex<double> z1 = std::polar (1.0, -wv);
        std::complex<double> H = ((1.0 - double (a)) / (1.0 - double (a) * z1));
        std::complex<double> ap = (-double (kRest) + z1) / (1.0 - double (kRest) * z1);   // resting bridge
        for (int i = 0; i < kSections; ++i) H *= ap;
        return -std::arg (H) / wv;
    }
    double fs = 48000;
    std::vector<float> R, L, nut;
    int size = 0, mask = 0, nsize = 0, nmask = 0, w = 0, nw = 0;
    int M = 64, K = 0;
    double V = 4, Vtarget = 4, curF0 = 100;
    float a = 0.2f, g = 0.999f, lp = 0, dampG = 1.0f, hpX = 0, hpY = 0, hpR = 0.9996f;
    float cCurv = 0.001f, cDepth = 1.0f;
    // curved-bridge length modulation (Valimaki / Siddiq style): bridge delay shortens while the
    // string presses into the bridge
    static constexpr int kSections = 3;
    static constexpr float kRest = 0.3f, kMin = -0.7f;
    float apS[kSections] {};
    float jawDepth = 0.0f, jawY = 0.0f, jawS = 0, jawSm = 0.2f, jawAvg = 0, jawAvgK = 0.0003f;
    int exPos = 0, exLen = 0, exAt = 4;
    float exAmp = 0, exNoise = 0;
    Rng* rngRef = nullptr;
    float energy = 0;
};

// Plain sympathetic string (single delay loop): used for the taraf
class SympString
{
public:
    void prepare (double sampleRate, double lowestHz)
    {
        fs = sampleRate;
        size = nextPow2 (int (fs / lowestHz) + 64); mask = size - 1;
        buf.assign ((size_t) size, 0.0f); w = 0; lp = 0;
    }
    void tune (double f, double t60, double bright)
    {
        a = float (std::clamp (0.6 - 0.55 * bright, 0.02, 0.7));
        double wv = 2 * kPi * f / fs;
        double ld = std::atan2 (a * std::sin (wv), 1 - a * std::cos (wv)) / wv;
        D = std::clamp (fs / f - ld, 2.0, double (size - 8));
        double mag = (1 - a) / std::sqrt (1 - 2 * a * std::cos (wv) + a * a);
        g = float (std::min (0.99995, std::pow (10.0, -3.0 / (t60 * f)) / mag));
    }
    inline float tick (float in)
    {
        float d = lagrangeRead (buf.data(), mask, w, D - 1.0);
        lp = (1 - a) * d + a * lp;
        float y = in + g * lp;
        buf[(++w) & mask] = y;
        return y;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); lp = 0; }
private:
    double fs = 48000, D = 100;
    std::vector<float> buf;
    int size = 0, mask = 0, w = 0;
    float a = 0.3f, g = 0.999f, lp = 0;
};

// 8-line feedback delay network with Householder mixing and per-line damping
class FdnReverb
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        static const int base[8] = { 1103, 1307, 1499, 1637, 1871, 2063, 2309, 2539 };
        for (int i = 0; i < 8; ++i)
        {
            int n = int (base[i] * fs / 48000.0 * 1.6) + 8;
            lines[(size_t) i].assign ((size_t) nextPow2 (n + 8), 0.0f);
        }
        pre.assign ((size_t) nextPow2 (int (fs * 0.05) + 8), 0.0f);
        setSize (0.5f);
        reset();
    }
    void setSize (float s)
    {
        static const int base[8] = { 1103, 1307, 1499, 1637, 1871, 2063, 2309, 2539 };
        float scale = 0.55f + 1.0f * s;
        double t60 = 0.8 + 3.2 * s;
        for (int i = 0; i < 8; ++i)
        {
            len[i] = int (base[i] * fs / 48000.0 * scale);
            gain[i] = float (std::pow (10.0, -3.0 * len[i] / (fs * t60)));
        }
        damp = 0.35f + 0.25f * (1 - s);
    }
    void process (float inL, float inR, float& outL, float& outR)
    {
        int pm = int (pre.size()) - 1;
        pre[(size_t) (pw & pm)] = 0.5f * (inL + inR);
        float x = pre[(size_t) ((pw - int (fs * 0.018)) & pm)];
        ++pw;
        float o[8], s = 0;
        for (int i = 0; i < 8; ++i)
        {
            auto& b = lines[(size_t) i];
            int m = int (b.size()) - 1;
            float v = b[(size_t) ((wp[i] - len[i]) & m)];
            lpz[i] = v + damp * (lpz[i] - v);
            o[i] = lpz[i] * gain[i];
            s += o[i];
        }
        s *= 0.25f;   // Householder: y = x - 2/N * sum
        for (int i = 0; i < 8; ++i)
        {
            auto& b = lines[(size_t) i];
            int m = int (b.size()) - 1;
            float inj = (i & 1) ? x : -x;
            b[(size_t) ((++wp[i]) & m)] = o[i] - s + inj * 0.35f;
        }
        outL = 0.6f * (o[0] - o[2] + o[4] - o[6]);
        outR = 0.6f * (o[1] - o[3] + o[5] - o[7]);
    }
    void reset()
    {
        for (auto& l : lines) std::fill (l.begin(), l.end(), 0.0f);
        std::fill (pre.begin(), pre.end(), 0.0f);
        for (int i = 0; i < 8; ++i) { lpz[i] = 0; wp[i] = 0; }
        pw = 0;
    }
private:
    double fs = 48000;
    std::array<std::vector<float>, 8> lines;
    std::vector<float> pre;
    int len[8] {}, wp[8] {}, pw = 0;
    float gain[8] {}, lpz[8] {}, damp = 0.4f;
};
} // namespace tantri

namespace tantri
{
// Stereo-linked look-ahead peak limiter. The gain needed for the loudest sample in the next
// `look` samples is reached before that sample leaves the delay line, so the output never
// exceeds the ceiling and nothing is ever clipped or saturated.
class Limiter
{
public:
    void prepare (double fs, double lookMs = 1.5, double releaseMs = 150.0, float ceilingDb = -1.0f)
    {
        look = std::max (8, int (fs * lookMs * 0.001));
        size = nextPow2 (look + 4); mask = size - 1;
        dl[0].assign ((size_t) size, 0.0f); dl[1].assign ((size_t) size, 0.0f);
        pk.assign ((size_t) size, 0.0f);
        dqIdx.assign ((size_t) size, 0); dqHead = dqTail = 0;
        ceiling = std::pow (10.0f, ceilingDb / 20.0f);
        atk = 1.0f - std::exp (-5.0f / float (look));
        rel = 1.0f - float (std::exp (-1.0 / (fs * releaseMs * 0.001)));
        g = 1.0f; w = 0; minGain = 1.0f;
    }
    int latency() const { return look; }
    inline void process (float& l, float& r)
    {
        float a = std::max (std::fabs (l), std::fabs (r));
        dl[0][(size_t) (w & mask)] = l; dl[1][(size_t) (w & mask)] = r; pk[(size_t) (w & mask)] = a;
        // sliding max over the last look+1 samples (monotonic deque of indices)
        while (dqTail != dqHead && pk[(size_t) (dqIdx[(size_t) ((dqTail - 1) & mask)] & mask)] <= a) --dqTail;
        dqIdx[(size_t) (dqTail & mask)] = w; ++dqTail;
        while (dqIdx[(size_t) (dqHead & mask)] <= w - look - 1) ++dqHead;
        float peak = pk[(size_t) (dqIdx[(size_t) (dqHead & mask)] & mask)];
        float target = peak > ceiling ? ceiling / peak : 1.0f;
        g += (target - g) * (target < g ? atk : rel);
        int rd = (w - look) & mask;
        ++w;
        l = dl[0][(size_t) rd] * g; r = dl[1][(size_t) rd] * g;
        // the ramp above lands within 0.7 % of the target in time; this catches that remainder
        float o = std::max (std::fabs (l), std::fabs (r));
        if (o > ceiling) { float s = ceiling / o; l *= s; r *= s; }
        minGain = std::min (minGain, g);
    }
    float takeMinGain() { float m = minGain; minGain = 1.0f; return m; }
private:
    std::vector<float> dl[2], pk;
    std::vector<int64_t> dqIdx;
    int64_t w = 0, dqHead = 0, dqTail = 0;
    int look = 72, size = 128, mask = 127;
    float ceiling = 0.89f, atk = 0.1f, rel = 0.001f, g = 1.0f, minGain = 1.0f;
};

// The same look-ahead ceiling for many channels (Ambisonics): one gain for all of them, taken from
// the loudest, so the spatial image never tilts while it limits
class LimiterN
{
public:
    void prepare (double fs, int channels, double lookMs = 1.5, double releaseMs = 150.0, float ceilingDb = -1.0f)
    {
        nch = std::max (1, channels);
        look = std::max (8, int (fs * lookMs * 0.001));
        size = nextPow2 (look + 4); mask = size - 1;
        dl.assign ((size_t) (size * nch), 0.0f);
        pk.assign ((size_t) size, 0.0f);
        dqIdx.assign ((size_t) size, 0); dqHead = dqTail = 0;
        ceiling = std::pow (10.0f, ceilingDb / 20.0f);
        atk = 1.0f - std::exp (-5.0f / float (look));
        rel = 1.0f - float (std::exp (-1.0 / (fs * releaseMs * 0.001)));
        g = 1.0f; w = 0; minGain = 1.0f;
    }
    int latency() const { return look; }
    int channels() const { return nch; }
    // x[c] for c < n (n <= channels()), in place
    inline void process (float* const* x, int n, int i)
    {
        float a = 0;
        const int base = (int) ((w & mask) * nch);
        for (int c = 0; c < nch; ++c)
        {
            const float v = c < n ? x[c][i] : 0.0f;
            dl[(size_t) (base + c)] = v;
            a = std::max (a, std::fabs (v));
        }
        pk[(size_t) (w & mask)] = a;
        while (dqTail != dqHead && pk[(size_t) (dqIdx[(size_t) ((dqTail - 1) & mask)] & mask)] <= a) --dqTail;
        dqIdx[(size_t) (dqTail & mask)] = w; ++dqTail;
        while (dqIdx[(size_t) (dqHead & mask)] <= w - look - 1) ++dqHead;
        float peak = pk[(size_t) (dqIdx[(size_t) (dqHead & mask)] & mask)];
        float target = peak > ceiling ? ceiling / peak : 1.0f;
        g += (target - g) * (target < g ? atk : rel);
        const int rd = (int) (((w - look) & mask) * nch);
        ++w;
        float o = 0;
        for (int c = 0; c < n; ++c) { x[c][i] = dl[(size_t) (rd + c)] * g; o = std::max (o, std::fabs (x[c][i])); }
        if (o > ceiling) { float s = ceiling / o; for (int c = 0; c < n; ++c) x[c][i] *= s; }
        minGain = std::min (minGain, g);
    }
    float takeMinGain() { float m = minGain; minGain = 1.0f; return m; }
private:
    std::vector<float> dl, pk;
    std::vector<int64_t> dqIdx;
    int64_t w = 0, dqHead = 0, dqTail = 0;
    int look = 72, size = 128, mask = 127, nch = 2;
    float ceiling = 0.89f, atk = 0.1f, rel = 0.001f, g = 1.0f, minGain = 1.0f;
};
} // namespace tantri
