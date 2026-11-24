#pragma once
// Tantri Akasha: the space. The same model as openfield (poppsphere-bundle): a sound has a place
// (direction round you, height, distance), and can move by itself with one of twelve motions -
// orbit, swing, rise, drift, spiral, figure 8, arc, approach, hop, flutter, boids (one flock of
// every sound that has it) and Lorenz (its own stretch of the attractor). Distance sets level,
// air (far sounds lose their top), width (near sounds are wide) and sharpness (near / blurred
// sounds lose their higher orders). Encoded as Ambisonics, ACN / SN3D up to 7th order.
#include "Ambi.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace tantri::field
{
constexpr float kPi = 3.14159265358979f;

namespace motion { enum { None = 0, Orbit, Swing, Rise, Drift, Spiral, Figure8, Arc, Approach, Hop, Flutter, Boids, Lorenz, Count }; }
inline const char* const kMotionName[motion::Count] = { "None", "Orbit", "Swing", "Rise", "Drift", "Spiral", "Figure 8",
                                                        "Arc", "Approach", "Hop", "Flutter", "Boids", "Lorenz" };

// The play area is the sphere unrolled: left to right goes once round you (the middle is straight
// ahead, both edges straight behind), bottom to top from 45 degrees below to overhead.
constexpr float kElLow = -45.0f, kElHigh = 90.0f;
inline float clamp01 (float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
inline float xForAzDeg (float deg) { float x = 0.5f - deg / 360.0f; return x - std::floor (x); }
inline float azDegForX (float x) { float a = (0.5f - x) * 360.0f; a -= 360.0f * std::round (a / 360.0f); return a; }
inline float yForElDeg (float deg) { return (deg - kElLow) / (kElHigh - kElLow); }
inline float elDegForY (float y) { return kElLow + y * (kElHigh - kElLow); }
inline float azimuth (float x) { return (0.5f - x) * 2.0f * kPi; }                      // radians, + = left
inline float elevation (float y) { return (kElLow + y * (kElHigh - kElLow)) * kPi / 180.0f; }

struct SpaceParams
{
    float rolloff = 0.5f;   // how fast the level falls with distance
    float air = 0.5f;       // how much top far sounds lose
    float spread = 0.5f;    // how wide near sounds are
    float blur = 0.0f;      // how much far sounds lose their sharpness of direction
    float nearR = 0.12f;    // inside this distance a sound spreads all round you
    float yaw = 0.0f, pitch = 0.0f;   // scene rotation, radians
    float mspeed = 1.0f;    // all motions faster or slower
    float prox = 0.0f;      // near-field bass: sounds right by your head gain low end
    float hcue = 0.0f;      // height cue: raised sounds brighten, lowered sounds dull (pinna-like)
};

// level by distance. rolloff 0.5: 0 dB at your head, -6 dB at 0.35, about -30 dB at the far edge
inline float distanceGain (float d, const SpaceParams& sp)
{
    const float a = 0.3f + 1.04f * sp.rolloff;
    float g = 1.0f - a * d;
    g = g < 0.03f ? 0.03f : g;
    return g * g * 1.15f / (1.0f + 0.15f * g);
}
// how far a stereo sound is spread: wide when near, a point when far (radians, the whole width)
inline float width (float d, const SpaceParams& sp)
{
    const float nearW = 20.0f + 120.0f * sp.spread;
    return (nearW - (nearW - 10.0f) * d) * kPi / 180.0f;
}

// the Lorenz attractor (sigma 10, rho 28, beta 8/3), integrated once: 30 minutes at 25 points a
// second, a quarter of Lorenz time per second (one turn of a wing takes about three seconds)
inline const std::vector<float>& lorenzTable()
{
    static const std::vector<float> t = []
    {
        const int n = 45000;
        std::vector<float> v ((size_t) n * 3);
        double x = 0.1, y = 0.0, z = 20.0;
        auto f = [] (double a, double b, double c, double& da, double& db, double& dc) {
            da = 10.0 * (b - a); db = a * (28.0 - c) - b; dc = a * b - 8.0 / 3.0 * c; };
        for (int i = 0; i < 2000; ++i) { double a, b, c; f (x, y, z, a, b, c); x += a * 0.005; y += b * 0.005; z += c * 0.005; }
        for (int i = 0; i < n; ++i)
        {
            for (int s = 0; s < 2; ++s)
            {
                double k1[3], k2[3], k3[3], k4[3]; const double h = 0.005;
                f (x, y, z, k1[0], k1[1], k1[2]);
                f (x + h / 2 * k1[0], y + h / 2 * k1[1], z + h / 2 * k1[2], k2[0], k2[1], k2[2]);
                f (x + h / 2 * k2[0], y + h / 2 * k2[1], z + h / 2 * k2[2], k3[0], k3[1], k3[2]);
                f (x + h * k3[0], y + h * k3[1], z + h * k3[2], k4[0], k4[1], k4[2]);
                x += h / 6 * (k1[0] + 2 * k2[0] + 2 * k3[0] + k4[0]);
                y += h / 6 * (k1[1] + 2 * k2[1] + 2 * k3[1] + k4[1]);
                z += h / 6 * (k1[2] + 2 * k2[2] + 2 * k3[2] + k4[2]);
            }
            v[(size_t) i * 3] = (float) x; v[(size_t) i * 3 + 1] = (float) y; v[(size_t) i * 3 + 2] = (float) z;
        }
        return v;
    }();
    return t;
}
inline void lorenzAt (double sec, float out[3])
{
    const auto& t = lorenzTable();
    const int n = (int) (t.size() / 3);
    double p = std::fmod (sec * 25.0, (double) (n - 1));
    if (p < 0) p += n - 1;
    const int i = (int) p; const float f = (float) (p - i);
    for (int k = 0; k < 3; ++k) out[k] = t[(size_t) i * 3 + k] + (t[(size_t) (i + 1) * 3 + k] - t[(size_t) i * 3 + k]) * f;
}

inline float hash01 (int n, float seed)
{
    const double v = std::sin (n * 127.1 + seed * 311.7) * 43758.5453;
    return (float) (v - std::floor (v));
}

// A placed sound: home place in play-area units (x round you, y height), distance d, its motion.
struct Place
{
    float x = 0.5f, y = 0.33f, d = 0.3f;
    int motion = motion::None;
    float mrate = 1.0f;      // its own speed, 0.25 .. 4
    float mseed = 0.5f;      // its own variation: direction, small differences of rate and path
};

// where a sound is, tau seconds after its motion was put on. Every motion starts exactly where the
// sound was put (no jump), and each sound has its own rate, direction and small differences.
inline void moved (const Place& i, double tau, float& x, float& y, float& d)
{
    x = i.x; y = i.y; d = i.d;
    if (i.motion == motion::None || i.motion == motion::Boids) return;
    const double tw = 2.0 * kPi;
    const float sd = i.mseed;
    const double k = i.mrate * (0.85 + 0.3 * sd);
    const double t = tau * k;
    const float dir = sd < 0.5f ? 1.0f : -1.0f;
    switch (i.motion)
    {
        case motion::Orbit: x -= dir * (float) std::fmod (t / 24.0, 1.0); break;              // once round you, 24 s
        case motion::Swing: x += dir * 0.11f * (float) std::sin (tw * t / 8.0); break;        // +-40 degrees, 8 s
        case motion::Rise:  y += (1.0f - y) * 0.5f * (1.0f - (float) std::cos (tw * t / 16.0)); break;
        case motion::Drift:
            x += dir * 0.07f * (float) (std::sin ((0.31 + 0.1 * sd) * t) + 0.6 * std::sin ((0.17 + 0.05 * sd) * t));
            y += 0.09f * (float) (std::sin ((0.23 + 0.07 * sd) * t) + 0.5 * std::sin ((0.11 + 0.04 * sd) * t));
            break;
        case motion::Spiral:
            x -= dir * (float) std::fmod (t / 20.0, 1.0);
            y += (1.0f - y) * 0.5f * (1.0f - (float) std::cos (tw * t / 40.0));
            break;
        case motion::Figure8:
            x += dir * 0.12f * (float) std::sin (tw * t / 10.0);
            y += 0.08f * (float) std::sin (2.0 * tw * t / 10.0);
            break;
        case motion::Arc:
        {
            const float p = 0.5f * (1.0f - (float) std::cos (tw * t / 14.0));
            const float up = p < 0.5f ? 2.0f * p : 2.0f - 2.0f * p;
            y += (1.0f - y) * up;
            if (p > 0.5f) x += 0.5f;
            break;
        }
        case motion::Approach:
            d = d * (0.1f + 0.9f * 0.5f * (1.0f + (float) std::cos (tw * t / 12.0)));
            break;
        case motion::Hop:
        {
            auto at = [&] (int n, float& ox, float& oy) {
                if (n <= 0) { ox = 0; oy = 0; return; }
                ox = 0.3f * (hash01 (n, sd) - 0.5f);
                oy = 0.3f * (hash01 (n + 977, sd) - 0.5f);
            };
            const double steps = t / 3.0;
            const int n = (int) std::floor (steps);
            const float f = (float) ((steps - n) * 3.0 / 0.15);
            const float g = f >= 1.0f ? 1.0f : f * f * (3.0f - 2.0f * f);
            float ax, ay, bx, by;
            at (n - 1, ax, ay); at (n, bx, by);
            x += ax + (bx - ax) * g;
            y += ay + (by - ay) * g;
            break;
        }
        case motion::Lorenz:
        {
            const double t0 = sd * 1700.0;
            float a[3], b[3];
            lorenzAt (t0, a); lorenzAt (t0 + t, b);
            x += dir * (b[0] - a[0]) / 40.0f * 0.35f;
            y += (b[2] - a[2]) / 50.0f * 0.5f;
            d += (b[1] - a[1]) / 54.0f * 0.4f;
            break;
        }
        case motion::Flutter:
            x += 0.012f * (float) (std::sin (tw * (5.3 + sd) * t) + 0.7 * std::sin (tw * (7.9 - sd) * t));
            y += 0.016f * (float) (std::sin (tw * (4.1 + 0.5 * sd) * t) + 0.6 * std::sin (tw * 9.7 * t));
            d = clamp01 (d + 0.03f * (float) std::sin (tw * 3.3 * t));
            break;
        default: break;
    }
    x -= std::floor (x);
    y = clamp01 (y);
    d = clamp01 (d);
}

// the boids: every sound with the boids motion is one bird of a single flock, simulated block by
// block with cohesion, separation, alignment, a pull home and a little wandering (as openfield)
struct Flock
{
    static constexpr int kMax = 8;
    struct Bird { int id = -1; float hx = 0, hy = 0, hd = 0; float x = 0, y = 0, d = 0, vx = 0, vy = 0, vd = 0; bool seen = false; float k = 1; };
    std::array<Bird, kMax> birds {};
    uint32_t rnd = 12345;

    float noise() { rnd = rnd * 1664525u + 1013904223u; return (float) ((rnd >> 8) & 0xffff) / 32768.0f - 1.0f; }
    void begin() { for (auto& b : birds) b.seen = false; }
    int add (int id, float hx, float hy, float hd, float k)
    {
        int freeSlot = -1;
        for (int i = 0; i < kMax; ++i)
        {
            if (birds[(size_t) i].id == id) { auto& b = birds[(size_t) i]; b.hx = hx; b.hy = hy; b.hd = hd; b.k = k; b.seen = true; return i; }
            if (freeSlot < 0 && birds[(size_t) i].id < 0) freeSlot = i;
        }
        if (freeSlot < 0) return -1;
        auto& b = birds[(size_t) freeSlot];
        b = Bird();
        b.id = id; b.hx = b.x = hx; b.hy = b.y = hy; b.hd = b.d = hd; b.k = k; b.seen = true;
        b.vx = 0.03f * noise(); b.vy = 0.02f * noise();
        return freeSlot;
    }
    void step (float dt)
    {
        for (auto& b : birds) if (b.id >= 0 && ! b.seen) b.id = -1;
        if (dt <= 0.0f) return;
        for (auto& b : birds)
        {
            if (b.id < 0) continue;
            float cx = 0, cy = 0, cd = 0, ax = 0, ay = 0, ad = 0, sx = 0, sy = 0, sd = 0; int n = 0;
            for (auto& o : birds)
            {
                if (o.id < 0 || &o == &b) continue;
                float dx = o.x - b.x; dx -= std::round (dx);
                const float dy = o.y - b.y, dd = o.d - b.d;
                const float r2 = dx * dx + dy * dy + 0.5f * dd * dd;
                if (r2 > 0.09f) continue;
                cx += dx; cy += dy; cd += dd; ax += o.vx; ay += o.vy; ad += o.vd; ++n;
                if (r2 < 0.004f) { const float k = 1.0f / (r2 + 0.0004f); sx -= dx * k; sy -= dy * k; sd -= dd * k; }
            }
            float fx = 0, fy = 0, fd = 0;
            if (n > 0)
            {
                fx += 0.9f * cx / n + 0.8f * (ax / n - b.vx) + 0.0009f * sx;
                fy += 0.9f * cy / n + 0.8f * (ay / n - b.vy) + 0.0009f * sy;
                fd += 0.6f * cd / n + 0.8f * (ad / n - b.vd) + 0.0009f * sd;
            }
            float hx = b.hx - b.x; hx -= std::round (hx);
            fx += 0.25f * hx + 0.06f * noise();
            fy += 0.25f * (b.hy - b.y) + 0.05f * noise();
            fd += 0.25f * (b.hd - b.d) + 0.04f * noise();
            const float h = dt * b.k;
            b.vx += fx * h; b.vy += fy * h; b.vd += fd * h;
            const float v = std::sqrt (b.vx * b.vx + b.vy * b.vy);
            if (v > 0.09f) { b.vx *= 0.09f / v; b.vy *= 0.09f / v; }
            b.vd = b.vd > 0.08f ? 0.08f : (b.vd < -0.08f ? -0.08f : b.vd);
        }
        for (auto& b : birds)
        {
            if (b.id < 0) continue;
            const float h = dt * b.k;
            b.x += b.vx * h; b.x -= std::floor (b.x);
            b.y += b.vy * h; if (b.y < 0) { b.y = 0; b.vy = -b.vy; } if (b.y > 1) { b.y = 1; b.vy = -b.vy; }
            b.d += b.vd * h; if (b.d < 0) { b.d = 0; b.vd = -b.vd; } if (b.d > 1) { b.d = 1; b.vd = -b.vd; }
        }
    }
    const Bird* find (int id) const { for (auto& b : birds) if (b.id == id) return &b; return nullptr; }
};

// a direction turned by the scene rotation (yaw about the vertical, then pitch)
inline void rotate (float& az, float& el, const SpaceParams& sp)
{
    if (sp.yaw == 0.0f && sp.pitch == 0.0f) return;
    float vx = std::cos (el) * std::cos (az), vy = std::cos (el) * std::sin (az), vz = std::sin (el);
    const float cy = std::cos (sp.yaw), sy = std::sin (sp.yaw);
    const float x1 = vx * cy - vy * sy, y1 = vx * sy + vy * cy;
    const float cp = std::cos (sp.pitch), spi = std::sin (sp.pitch);
    const float x2 = x1 * cp - vz * spi, z2 = x1 * spi + vz * cp;
    az = std::atan2 (y1, x2);
    el = std::asin (z2 < -1.0f ? -1.0f : (z2 > 1.0f ? 1.0f : z2));
}

// One stereo source encoded into Ambisonics: air filter, distance gain, width, near / blur order
// weighting, gains ramped across each sub-block so moving sounds never click.
struct Encoder
{
    std::array<float, ambi::kCh> cL {}, cR {};
    float lpL = 0, lpR = 0, pxL = 0, pxR = 0, hcL = 0, hcR = 0;
    void reset() { cL.fill (0); cR.fill (0); lpL = lpR = pxL = pxR = hcL = hcR = 0; }

    // az/el in radians (after rotation), d distance; extraOrderW < 1 makes the source more diffuse
    void process (const float* inL, const float* inR, float* const* out, int nch, int order, int n,
                  float az, float el, float d, float gain, const SpaceParams& sp, double sr, float extraOrderW = 1.0f,
                  float* scratchL = nullptr, float* scratchR = nullptr, float size = 0.0f)
    {
        // air: far sounds lose their top
        const float cutoff = 18000.0f * std::pow (3500.0f / 18000.0f, d * d * sp.air * 2.0f);
        const float kAir = 1.0f - std::exp (-2.0f * kPi * std::fmin (cutoff, (float) sr * 0.45f) / (float) sr);
        // near-field bass (proximity): up to about +8 dB below 160 Hz inside a third of the field
        const float nearAmt = clamp01 (1.0f - d / 0.35f);
        const float kProx = sp.prox * nearAmt * nearAmt * 1.5f;
        const float kPx = 1.0f - std::exp (-2.0f * kPi * 160.0f / (float) sr);
        // height cue: the band above 6 kHz lifted for sounds above you, dulled for sounds below
        const float elN = el > 0.0f ? 0.55f * el / (0.5f * kPi) : 0.75f * el / (0.25f * kPi);
        const float kHc = sp.hcue * (elN < -0.75f ? -0.75f : elN);
        const float kHp = 1.0f - std::exp (-2.0f * kPi * std::fmin (6000.0f, (float) sr * 0.4f) / (float) sr);
        for (int i = 0; i < n; ++i)
        {
            float l = inL[i], r = inR[i];
            pxL += (l - pxL) * kPx; pxR += (r - pxR) * kPx;
            l += kProx * pxL; r += kProx * pxR;
            hcL += (l - hcL) * kHp; hcR += (r - hcR) * kHp;
            l += kHc * (l - hcL); r += kHc * (r - hcR);
            lpL += (l - lpL) * kAir; lpR += (r - lpR) * kAir;
            scratchL[i] = lpL; scratchR[i] = lpR;
        }
        // size: a large source is wide and loses its sharpness of direction
        float half = width (d, sp) * 0.5f;
        half += (0.5f * kPi - half) * clamp01 (size) * 0.9f;
        extraOrderW *= 1.0f - 0.8f * clamp01 (size);
        const auto yL = ambi::sh (az + half, el, order);
        const auto yR = ambi::sh (az - half, el, order);
        float nearK = clamp01 (d / std::fmax (0.001f, sp.nearR)) * (1.0f - 0.85f * sp.blur * d);
        nearK *= extraOrderW;
        const float g = gain * 0.7071f;
        float wl = 1.0f;
        int lNow = 0;
        for (int c = 0; c < nch; ++c)
        {
            const int l = ambi::orderOf (c);
            if (l != lNow) { wl *= nearK; lNow = l; }
            const float tL = g * wl * yL[(size_t) c], tR = g * wl * yR[(size_t) c];
            const float c0L = cL[(size_t) c], c0R = cR[(size_t) c];
            const float dL = (tL - c0L) / (float) n, dR = (tR - c0R) / (float) n;
            float* o = out[c];
            float gl = c0L, gr = c0R;
            for (int i = 0; i < n; ++i)
            {
                gl += dL; gr += dR;
                o[i] += scratchL[i] * gl + scratchR[i] * gr;
            }
            cL[(size_t) c] = tL; cR[(size_t) c] = tR;
        }
        for (int c = nch; c < ambi::kCh; ++c) { cL[(size_t) c] = 0; cR[(size_t) c] = 0; }
    }
};

// ---- the delay each source travels (Doppler) and its first reflections
inline float metres (float d) { return 0.5f + 9.5f * d; }        // the field's distance in metres
constexpr float kSound = 343.0f;

struct DelayLine
{
    std::vector<float> l, r;
    int mask = 0, w = 0;
    void prepare (double sr)
    {
        int n = 1; while (n < (int) (sr * 0.22) + 8) n <<= 1;
        l.assign ((size_t) n, 0.0f); r.assign ((size_t) n, 0.0f); mask = n - 1; w = 0;
    }
    void write (float a, float b) { l[(size_t) w] = a; r[(size_t) w] = b; w = (w + 1) & mask; }
    // dl samples back from the newest sample (>= 2), 4-point Hermite
    static float herm (const std::vector<float>& x, int mask, double pos)
    {
        const int i = (int) std::floor (pos); const float f = (float) (pos - i);
        const float y0 = x[(size_t) ((i - 1) & mask)], y1 = x[(size_t) (i & mask)], y2 = x[(size_t) ((i + 1) & mask)], y3 = x[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }
    void read (double dl, float& a, float& b) const
    {
        const double pos = (double) (w - 1) - dl;
        a = herm (l, mask, pos); b = herm (r, mask, pos);
    }
    float readMono (double dl) const
    {
        const double pos = (double) (w - 1) - dl;
        const int i = (int) std::floor (pos); const float f = (float) (pos - i);
        const float a = l[(size_t) (i & mask)] + r[(size_t) (i & mask)], b = l[(size_t) ((i + 1) & mask)] + r[(size_t) ((i + 1) & mask)];
        return 0.5f * (a + (b - a) * f);
    }
};

// the room the reflections come from: half its length (front/back), half its width, ceiling and
// floor heights from your ears, in metres; ROOM makes it bigger
struct Room { float len, wid, ceil, floor; };
inline Room roomFor (float size) { return { 3.5f + 11.0f * size, 2.5f + 9.0f * size, 1.3f + 4.5f * size, 1.6f }; }

// the six first-order image sources of a sound at (az, el, d): their directions, how much later
// they arrive than the sound itself, and their level (spreading + the walls' absorption)
struct Image { float az, el, extra, gain, x, y, z; };
inline void images (float az, float el, float d, const Room& rm, Image out[6])
{
    const float r = metres (d);
    float p[3] = { r * std::cos (el) * std::cos (az), r * std::cos (el) * std::sin (az), r * std::sin (el) };
    const float lim[3][2] = { { -rm.len, rm.len }, { -rm.wid, rm.wid }, { -rm.floor, rm.ceil } };
    for (int a = 0; a < 3; ++a) p[a] = std::fmax (lim[a][0] * 0.95f, std::fmin (lim[a][1] * 0.95f, p[a]));
    const float pr = std::sqrt (p[0] * p[0] + p[1] * p[1] + p[2] * p[2]) + 1e-3f;
    static constexpr float absorb[6] = { 0.72f, 0.72f, 0.75f, 0.75f, 0.6f, 0.8f };   // front, back, left, right, ceiling, floor
    for (int k = 0; k < 6; ++k)
    {
        float q[3] = { p[0], p[1], p[2] };
        const int a = k / 2;
        q[a] = 2.0f * lim[a][k & 1 ? 0 : 1] - p[a];
        if (k == 4) q[2] = 2.0f * rm.ceil - p[2];
        if (k == 5) q[2] = -2.0f * rm.floor - p[2];
        const float qr = std::sqrt (q[0] * q[0] + q[1] * q[1] + q[2] * q[2]) + 1e-3f;
        out[k] = { std::atan2 (q[1], q[0]), std::asin (std::fmax (-1.0f, std::fmin (1.0f, q[2] / qr))),
                   (qr - pr) / kSound, absorb[k] * pr / qr, q[0], q[1], q[2] };
    }
}

// one mono sound encoded with ramped gains (the reflections), its top softened by the walls
struct MonoEncoder
{
    std::array<float, ambi::kCh> c {};
    float lp = 0;
    void reset() { c.fill (0); lp = 0; }
    void process (const float* in, float* const* out, int nch, int order, int n, float az, float el, float gain, float orderW, float kLp)
    {
        const auto y = ambi::sh (az, el, order);
        float wl = 1.0f; int lNow = 0;
        for (int ch = 0; ch < nch; ++ch)
        {
            const int l = ambi::orderOf (ch);
            if (l != lNow) { wl *= orderW; lNow = l; }
            const float t = gain * wl * y[(size_t) ch], c0 = c[(size_t) ch], dc = (t - c0) / (float) n;
            float g = c0, z = lp;
            float* o = out[ch];
            for (int i = 0; i < n; ++i) { z += (in[i] - z) * kLp; g += dc; o[i] += z * g; }
            c[(size_t) ch] = t;
        }
        for (int i = 0; i < n; ++i) lp += (in[i] - lp) * kLp;
        for (int ch = nch; ch < ambi::kCh; ++ch) c[(size_t) ch] = 0;
    }
};

// ---- the modulators: four LFOs, each a pure function of its phase (in cycles) so the UI can
// draw exactly what the audio hears
namespace lfo { enum { Sine = 0, Triangle, RampUp, RampDown, Square, SampleHold, Smooth, Lorenz, Count }; }
inline const char* const kLfoShape[lfo::Count] = { "Sine", "Triangle", "Ramp up", "Ramp down", "Square", "S&H", "Smooth", "Lorenz" };
inline float lfoValue (int shape, double phase, float seed)
{
    const double n = std::floor (phase);
    const float f = (float) (phase - n);
    switch (shape)
    {
        case lfo::Sine:     return std::sin (2.0f * kPi * f);
        case lfo::Triangle: return f < 0.25f ? 4.0f * f : (f < 0.75f ? 2.0f - 4.0f * f : 4.0f * f - 4.0f);
        case lfo::RampUp:   return 2.0f * f - 1.0f;
        case lfo::RampDown: return 1.0f - 2.0f * f;
        case lfo::Square:   return f < 0.5f ? 1.0f : -1.0f;
        case lfo::SampleHold: return 2.0f * hash01 ((int) std::fmod (n, 1.0e6), seed) - 1.0f;
        case lfo::Smooth:
        {
            const int k = (int) std::fmod (n, 1.0e6);
            const float a = hash01 (k, seed), b = hash01 (k + 1, seed);
            const float s = 0.5f - 0.5f * std::cos (kPi * f);
            return 2.0f * (a + (b - a) * s) - 1.0f;
        }
        case lfo::Lorenz:
        {
            float v[3]; lorenzAt (seed * 900.0 + phase * 2.0, v);
            const float x = v[0] / 18.0f;
            return x < -1.0f ? -1.0f : (x > 1.0f ? 1.0f : x);
        }
        default: return 0.0f;
    }
}
// what an LFO can move. Per source: direction, height, distance, size; then the whole field.
namespace mt { enum { Dir = 0, Height, Dist, Size, PerSource }; enum { Turn = 0, Tilt, Air, Spread, Blur, Rolloff, Doppler, Reflect, Speed, Bow, Global }; }
inline const char* const kModSrcPart[mt::PerSource] = { "direction", "height", "distance", "size" };
inline const char* const kModGlobal[mt::Global] = { "Scene turn", "Scene tilt", "Air", "Near spread", "Far blur", "Rolloff",
                                                     "Doppler", "Reflections", "Motion speed", "Tanpura bow rate" };
inline int modTargetCount (int sources) { return 1 + sources * mt::PerSource + mt::Global; }
} // namespace tantri::field
