#pragma once
// Sitar: polyphonic jawari strings + 13 taraf (sympathetic) strings tuned to the raga +
// 2 chikari drone strings + gourd/tabli body. Pitches come only from Sa (Hz) x shruti ratios.
#include "Dsp.h"
#include "Scales.h"

namespace tantri
{
struct SitarParams
{
    float mizrab = 0.62f;   // plectrum hardness 0..1
    float jawari = 0.7f;    // 0 closed .. 1 open (more buzz)
    float meendCents = 500; // pitch-bend range (cents)
    float taraf = 0.55f;    // sympathetic level
    float chikari = 0.4f;
    float gourd = 0.65f;
    float gamak = 0.3f;     // mod-wheel gamak depth / andolan depth
    float decay = 6.0f;     // T60 seconds
    float pluckPos = 0.2f;
    float release = 1.5f;   // T60 after note-off
    float level = 0.8f;
};

class Sitar
{
public:
    static constexpr int kVoices = 12;
    static constexpr int kTaraf = 13;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        for (auto& v : voices) { v.a.prepare (fs, 25.0); v.b.prepare (fs, 25.0); v.active = false; }
        for (auto& t : taraf) t.prepare (fs, 60.0);
        for (auto& c : chik) c.prepare (fs, 60.0);
        body.prepare (fs, { { 98, 9, 0.7f }, { 160, 12, 0.9f }, { 215, 13, 1.0f }, { 292, 15, 0.8f }, { 378, 16, 0.85f },
                            { 512, 18, 0.7f }, { 690, 20, 0.6f }, { 880, 22, 0.55f }, { 1120, 26, 0.5f }, { 1460, 28, 0.42f },
                            { 1900, 30, 0.36f }, { 2480, 32, 0.3f }, { 3250, 34, 0.24f }, { 4300, 38, 0.18f } });
        dc.prepare (fs);
        bodyLP.lowpass (fs, 7500, 0.7);
        tuningVersion = ~0u;
    }

    void setScale (const ActiveScale& sc, double saHz, unsigned version)
    {
        if (version == tuningVersion && saHz == sa) return;
        scale = sc; sa = saHz; tuningVersion = version;
        // taraf: the scale's degrees ascending from the one below upper Sa, 13 strings
        double tarafSa = sa * 2.0;
        int idx = 0;
        tarafHz[idx++] = tarafSa * std::pow (2.0, (scale.cents[scale.n - 1] - 1200.0) / 1200.0);
        for (int oct = 0; idx < kTaraf; ++oct)
            for (int d = 0; d < scale.n && idx < kTaraf; ++d)
                tarafHz[idx++] = tarafSa * std::pow (2.0, (oct * 1200.0 + scale.cents[d]) / 1200.0);
        for (int i = 0; i < kTaraf; ++i) taraf[i].tune (tarafHz[i], 3.5, 0.55);
        chik[0].configure (sa * 2.0, 5, 0.06); chik[1].configure (sa * 4.0, 5, 0.06);
        chik[0].setJawari (60); chik[1].setJawari (50);
        for (auto& c : chik) { c.setLoss (4.0, 0.7, sa * 2.0); c.snapFreq(); }
        // retune held voices to the new scale
        for (auto& v : voices) if (v.active) v.retune = true;
    }

    void setParams (const SitarParams& p)
    {
        bool lossChanged = p.jawari != prm.jawari || p.decay != prm.decay || p.mizrab != prm.mizrab || p.release != prm.release;
        prm = p;
        if (lossChanged) for (auto& v : voices) if (v.active) applyLoss (v);
    }
    void setBend (float b) { bend = b; }              // -1..1
    void setModWheel (float m) { modWheel = m; }
    void setChannelPressure (float p) { chanPress = p; }

    void noteOn (int note, float vel, int saKey, int mapMode)
    {
        int oct = 0, w = 0;
        mapNote (scale, note, saKey, mapMode, w, oct);   // quantise / step into the active scale
        Voice* v = nullptr;
        for (auto& x : voices) if (x.active && x.note == note) { v = &x; break; }   // re-pluck same string
        bool fresh = false;
        if (! v) for (auto& x : voices) if (! x.active) { v = &x; fresh = true; break; }
        if (! v)
        {   // steal the oldest released voice, else the oldest
            Voice* best = nullptr;
            for (auto& x : voices) if (! x.held && (! best || x.age > best->age)) best = &x;
            if (! best) for (auto& x : voices) if (! best || x.age > best->age) best = &x;
            v = best; fresh = true;
        }
        v->note = note; v->swara = w; v->oct = oct; v->held = true; v->active = true; v->age = 0;
        v->vel = vel; v->polyAT = 0;
        double f = noteHz (w, oct);
        if (fresh)
        {
            v->a.reset(); v->b.reset();
            v->a.configure (f, prm.meendCents + 720, 0.10);   // headroom for meend + gamak toward next swara
            v->b.configure (f * std::pow (2.0, 0.6 / 1200.0), prm.meendCents + 720, 0.0);
            v->baseHz = f;
            v->a.setFreq (f); v->a.snapFreq(); v->b.setFreq (f); v->b.snapFreq();
            v->lfo = 0;
        }
        v->baseHz = f;
        applyLoss (*v);
        v->a.damp (1.0f); v->b.damp (1.0f);
        // mizrab: wire plectrum. harder + faster = narrower pulse; da / ra strokes alternate polarity
        stroke = -stroke;
        float hard = 0.35f + 0.65f * prm.mizrab;
        float widthMs = (0.9f - 0.65f * hard) * (1.25f - 0.5f * vel);
        float amp = 0.32f * (0.25f + 0.75f * vel) * float (stroke);
        v->a.pluck (amp, float (widthMs * 0.001 * fs), prm.pluckPos, 0.08f * hard, rng);
        v->b.pluck (amp * 0.35f, float (widthMs * 0.0013 * fs), prm.pluckPos * 1.1f, 0.0f, rng);
        activity[w] = 1.0f;
    }

    void noteOff (int note)
    {
        for (auto& v : voices)
            if (v.active && v.note == note && v.held)
            {
                v.held = false;
                applyLoss (v);
            }
    }
    void polyPressure (int note, float p) { for (auto& v : voices) if (v.active && v.note == note) v.polyAT = p; }

    void chikariStrike (float vel)
    {
        float amp = 0.18f * (0.3f + 0.7f * vel);
        chik[0].pluck (amp, float (0.0004 * fs), 0.25f, 0.05f, rng);
        chik[1].pluck (amp * 0.8f, float (0.0003 * fs), 0.25f, 0.05f, rng);
        chikFlash = 1.0f;
    }

    void allOff() { for (auto& v : voices) { v.active = false; v.held = false; } }

    // render one sample, stereo
    inline void tick (float& outL, float& outR)
    {
        if ((ctl++ & 15) == 0) control();
        float bridge = 0, dirL = 0, dirR = 0;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            float s = v.a.tick() + v.b.tick();
            bridge += s;
            float pan = v.pan;
            dirL += s * (1.0f - pan); dirR += s * (1.0f + pan);
        }
        float chs = chik[0].tick (bridge * 0.01f) + chik[1].tick (bridge * 0.008f);
        float chikOut = chs * prm.chikari * 1.6f;
        // taraf driven by the bridge (all strings share it)
        float drive = (bridge + chs * 0.5f) * 0.012f + extDrive;
        float tl = 0, tr = 0, tsum = 0;
        for (int i = 0; i < kTaraf; ++i)
        {
            float y = taraf[i].tick (drive);
            tsum += y;
            if (i & 1) tr += y; else tl += y;
        }
        tarafFeedback = tsum;
        float tg = prm.taraf * prm.taraf * 1.6f;
        float force = bridge + chikOut * 0.5f + tsum * tg * 0.3f;
        float b = bodyLP.tick (body.tick (force)) * 0.9f;
        float mixB = prm.gourd;
        float dry = 1.0f - 0.6f * mixB;
        float l = dry * (dirL * 0.5f + chikOut * 0.45f) + mixB * b + tg * tl * 0.9f;
        float r = dry * (dirR * 0.5f + chikOut * 0.55f) + mixB * b + tg * tr * 0.9f;
        float m = dc.tick (0.5f * (l + r));
        float side = 0.5f * (l - r);
        outL = (m + side) * prm.level * 5.2f;
        outR = (m - side) * prm.level * 5.2f;
    }

    float activity[ActiveScale::kMax] {};   // per scale degree
    float chikFlash = 0;
    float extDrive = 0;     // Tantri Akasha FX: the input sounding the taraf (sympathetic strings)
    double tarafHz[kTaraf] {};

private:
    struct Voice
    {
        ContactString a, b;
        int note = -1, swara = 0, oct = 0, age = 0;
        bool active = false, held = false, retune = false;
        float vel = 0, polyAT = 0, pan = 0;
        double baseHz = 100, lfo = 0;
    };

    double noteHz (int d, int oct) const { return sa * std::pow (2.0, degreeCents (scale, d, oct) / 1200.0); }

    void applyLoss (Voice& v)
    {
        double t60 = v.held ? prm.decay * std::pow (v.baseHz / 140.0, -0.35) : std::min<double> (prm.release, prm.decay);
        double bright = 0.55 + 0.4 * prm.mizrab;
        v.a.setLoss (t60, bright, v.baseHz);
        v.b.setLoss (t60 * 0.8, bright * 0.8, v.baseHz);
        // jawari: open = small curvature (string grazes the bridge over a longer stretch)
        double c = 0.0045 * std::pow (0.3, prm.jawari);
        v.a.setContact (c, 0.0);
        double jd = 45.0 * std::pow (prm.jawari, 1.5);
        v.a.setJawari (jd);
        v.b.setJawari (jd * 0.45);
    }

    // every 16 samples: pitch (meend, poly AT meend, gamak, andolan), panning, voice ageing
    void control()
    {
        const double dt = 16.0 / fs;
        int count = 0;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            ++count;
            v.age++;
            if (v.retune) { v.swara = std::min (v.swara, scale.n - 1); v.baseHz = noteHz (v.swara, v.oct); applyLoss (v); v.retune = false; }
            double nextR = std::pow (2.0, stepUpCents (scale, v.swara) / 1200.0);   // meend target: next scale degree
            double cents = bend * prm.meendCents;
            double atAmt = std::max<double> (v.polyAT, chanPress);
            cents += atAmt * 1200.0 * std::log2 (nextR);              // pressure pulls to the next swara
            double gamakRate = 5.5, depth = 0;
            if (modWheel > 0.001f) depth = modWheel * prm.gamak * 1200.0 * std::log2 (nextR) * 0.5;
            if (scale.andolan[v.swara]) { gamakRate = 0.7; depth = std::max (depth, 18.0 * prm.gamak * 2.0); }
            v.lfo += dt * gamakRate * 2 * kPi;
            if (depth > 0) cents += depth * 0.5 * (1.0 - std::cos (v.lfo));   // gamak oscillates upward like a pulled string
            double f = v.baseHz * std::pow (2.0, cents / 1200.0);
            v.a.setFreq (f);
            v.b.setFreq (f * std::pow (2.0, 0.6 / 1200.0));
            v.pan = float (std::clamp ((v.oct * 1200 + scale.cents[v.swara] - 600) / 4000.0, -0.35, 0.35));
            float lev = v.a.level();
            if (! v.held && lev < 1e-9f && v.age > 200) v.active = false;
            if (v.held && lev < 1e-11f && v.age > 4000) v.active = false;
            activity[v.swara] = std::max (activity[v.swara], std::min (1.0f, std::sqrt (lev) * 30.0f));
        }
        voiceCount = count;
    }

public:
    int voiceCount = 0;

private:
    double fs = 48000, sa = 138.59;
    std::array<Voice, kVoices> voices;
    std::array<SympString, kTaraf> taraf;
    std::array<ContactString, 2> chik;
    ModalBody body;
    Biquad bodyLP;
    DcBlock dc;
    ActiveScale scale = makeScale (0, parseRaga (kRagas[0].spec));
    unsigned tuningVersion = ~0u;
    SitarParams prm;
    Rng rng;
    float bend = 0, modWheel = 0, chanPress = 0, tarafFeedback = 0;
    int stroke = 1;
    unsigned ctl = 0;
};
} // namespace tantri
