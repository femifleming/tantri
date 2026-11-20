// TantriTest: offline checks + renders.
//   --pitch            measure every swara of every raga octave against its just ratio (cents error)
//   --stress           60 s of random MIDI: no NaN/inf, bounded peak
//   --cpu              real-time factor with 12 sitar voices + tabla + tanpura
//   --wav dir          demo renders (sitar alap in Yaman, tabla teentaal, tanpura, full mix)
//   --spec in.wav out.png   log-frequency spectrogram
//   --shot out.png     UI screenshot (after a short performance so the mandalas are lit)
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_dsp/juce_dsp.h>

struct Ev { double t; juce::MidiMessage m; };
static float gPrePeak = 0, gMaxGR = 0;
static std::vector<Ev> yamanAlap (double t0);
static std::vector<Ev> teentaal (double t0, double bpm, int cycles);
static constexpr double kFs = 48000.0;

static juce::AudioBuffer<float> render (TantriProcessor& p, std::vector<Ev> evs, double seconds, int block = 256,
                                        std::function<void (juce::AudioBuffer<float>&, int)> input = {})
{
    std::sort (evs.begin(), evs.end(), [] (auto& a, auto& b) { return a.t < b.t; });
    const int total = int (seconds * kFs);
    const int nch = std::max (p.getTotalNumOutputChannels(), p.getTotalNumInputChannels());
    juce::AudioBuffer<float> out (p.getTotalNumOutputChannels(), total), buf (nch, block);
    size_t e = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        int n = std::min (block, total - pos);
        buf.setSize (nch, n, false, false, true);
        buf.clear();
        if (input) input (buf, pos);
        juce::MidiBuffer mb;
        while (e < evs.size() && int (evs[e].t * kFs) < pos + n)
        {
            mb.addEvent (evs[e].m, std::max (0, int (evs[e].t * kFs) - pos));
            ++e;
        }
        p.processBlock (buf, mb);
        gPrePeak = std::max (gPrePeak, p.outPeak.load());
        gMaxGR = std::min (gMaxGR, p.limitDb.load());
        for (int c = 0; c < out.getNumChannels(); ++c) out.copyFrom (c, pos, buf, c, 0, n);
    }
    return out;
}

static std::unique_ptr<TantriProcessor> make()
{
    auto p = std::make_unique<TantriProcessor>();
    p->setPlayConfigDetails (TantriProcessor::kFx ? 2 : 0, 2, kFs, 256);
    p->prepareToPlay (kFs, 256);
    return p;
}
static void set (TantriProcessor& p, const char* id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

static void writeWav (const juce::AudioBuffer<float>& b, const juce::File& f)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    auto opts = juce::AudioFormatWriterOptions().withSampleRate (kFs).withNumChannels (2).withBitsPerSample (24);
    if (auto w = wav.createWriterFor (os, opts)) { w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples()); }
    std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
}

// f0 by normalised autocorrelation + parabolic interpolation, around an expected frequency
static double measureF0 (const float* x, int n, double expect)
{
    int lo = int (kFs / (expect * 1.12)), hi = int (kFs / (expect / 1.12)) + 2;
    int W = n - hi - 2;
    std::vector<double> r ((size_t) hi + 2, 0.0);
    double e0 = 0; for (int i = 0; i < W; ++i) e0 += x[i] * x[i];
    for (int L = lo - 1; L <= hi + 1; ++L)
    {
        double s = 0, e1 = 0;
        for (int i = 0; i < W; ++i) { s += x[i] * x[i + L]; e1 += x[i + L] * x[i + L]; }
        r[(size_t) L] = s / std::sqrt (e0 * e1 + 1e-20);
    }
    int best = lo; for (int L = lo; L <= hi; ++L) if (r[(size_t) L] > r[(size_t) best]) best = L;
    double a = r[(size_t) best - 1], b = r[(size_t) best], c = r[(size_t) best + 1];
    double d = 0.5 * (a - c) / (a - 2 * b + c);
    return kFs / (best + d);
}

static int scaleIndexOf (const char* name)
{
    const auto& lib = tantri::scaleLibrary();
    for (int i = 0; i < (int) lib.size(); ++i) if (lib[(size_t) i].name == name) return i;
    std::printf ("no scale %s\n", name); return 0;
}

static int testPitch()
{
    double worst = 0; int count = 0;
    std::printf ("%d scales in the library\n", tantri::numScales());
    for (float jaw : { 0.0f, 0.7f })
    {
        std::printf ("jawari %.1f\n", jaw);
        for (const char* nm : { "Yaman", "Bhairavi", "Darbari Kanada", "15 Mayamalavagowla", "22 Shrutis (all)", "Partch 43", "53-EDO", "Maqam Rast" })
        {
            auto p = make();
            int si = scaleIndexOf (nm);
            set (*p, "scale", (float) si); set (*p, "map", 1.0f);   // steps: every degree on its own key
            set (*p, "n_on", 0); set (*p, "s_jawari", jaw); set (*p, "s_taraf", 0); set (*p, "s_chikari", 0);
            set (*p, "s_gourd", 0); set (*p, "s_room", 0); set (*p, "t_room", 0); set (*p, "n_room", 0); set (*p, "s_gamak", 0);
            auto sc = p->activeScale();
            double w1 = 0; int c1 = 0;
            // all degrees of the middle octave (dense scales: every 4th) + a few in the octaves around it
            int stride = sc.n > 24 ? 4 : 1;
            for (int oct = -1; oct <= 1; ++oct)
                for (int d = 0; d < sc.n; d += (oct == 0 ? stride : std::max (stride, sc.n / 3)))
                {
                    int note = 60 + oct * sc.n + d;
                    if (note < 48 && oct < 0) note = 60 + oct * sc.n + d;   // steps can go below the split for dense scales
                    if (note < 48 || note > 127) continue;
                    auto b = render (*p, { { 0.01, juce::MidiMessage::noteOn (1, note, 0.8f) }, { 1.6, juce::MidiMessage::noteOff (1, note) } }, 2.2);
                    double expect = 138.59 * std::pow (2.0, (oct * 1200.0 + sc.cents[d]) / 1200.0);
                    double f = measureF0 (b.getReadPointer (0) + int (0.5 * kFs), int (0.9 * kFs), expect);
                    double cents = 1200 * std::log2 (f / expect);
                    w1 = std::max (w1, std::abs (cents)); ++c1;
                }
            std::printf ("  %-20s %3d degrees  %3d notes  worst %.2f c\n", nm, sc.n, c1, w1);
            worst = std::max (worst, w1); count += c1;
        }
    }
    std::printf ("pitch: %d notes, worst error %.3f cents\n", count, worst);
    // quantize mapping checks (Yaman: S R G M' P D N)
    {
        auto p = make(); set (*p, "scale", (float) scaleIndexOf ("Yaman"));
        auto sc = p->activeScale(); int bad = 0;
        auto chk = [&] (int note, const char* lab, int oct)
        {
            int d, o; tantri::mapNote (sc, note, 60, tantri::mapQuantize, d, o);
            if (std::string (sc.label[d]) != lab || o != oct) { ++bad; std::printf ("  quantize %d -> %s/%d (want %s/%d)\n", note, sc.label[d], o, lab, oct); }
        };
        chk (60, "Sa", 0); chk (61, "Sa", 0); chk (62, "Re", 0); chk (63, "Ga", 0); chk (64, "Ga", 0); chk (65, "Ma'", 0);
        chk (66, "Ma'", 0); chk (67, "Pa", 0); chk (69, "Dha", 0); chk (71, "Ni", 0); chk (72, "Sa", 1); chk (59, "Ni", -1);
        std::printf ("quantize mapping: %s\n", bad ? "FAIL" : "ok");
        worst += bad * 100;
    }
    return worst < 3.0 ? 0 : 1;
}

static int testDebug()
{
    auto p = make();
    set (*p, "n_on", 0); set (*p, "s_room", 0);
    double t = 0.01; std::vector<Ev> ev;
    int notes[] = { 48, 50, 52, 54, 55, 57, 59, 60, 62, 64 };
    for (int n : notes) { ev.push_back ({ t, juce::MidiMessage::noteOn (1, n, 0.8f) }); ev.push_back ({ t + 1.6, juce::MidiMessage::noteOff (1, n) }); t += 2.2; }
    auto b = render (*p, ev, t);
    for (int k = 0; k < 10; ++k)
    {
        std::printf ("note %d:", notes[k]);
        for (int w = 0; w < 11; ++w)
        {
            int s0 = int ((0.01 + k * 2.2 + w * 0.2) * kFs);
            double e = 0; for (int i = 0; i < 4800; ++i) { float v = b.getSample (0, s0 + i); e += v * v; }
            std::printf (" %6.1f", 10 * std::log10 (e / 4800 + 1e-20));
        }
        std::printf ("\n");
    }
    return 0;
}

static void report (const char* name, const juce::AudioBuffer<float>& b)
{
    double pk = 0, e = 0;
    for (int c = 0; c < 2; ++c) for (int i = 0; i < b.getNumSamples(); ++i) { float v = b.getSample (c, i); pk = std::max (pk, (double) std::fabs (v)); e += v * v; }
    std::printf ("  %-34s pre-limiter peak %6.1f dBFS   out peak %6.1f dBFS   rms %6.1f dBFS   limiter %5.1f dB\n", name,
                 juce::Decibels::gainToDecibels (gPrePeak), juce::Decibels::gainToDecibels ((float) pk),
                 10 * std::log10 (e / (2.0 * b.getNumSamples()) + 1e-20), gMaxGR);
}
static int testLevels()
{
    std::printf ("levels at default settings:\n");
    auto run = [] (const char* name, std::function<void (TantriProcessor&)> setup, std::vector<Ev> ev, double secs)
    { gPrePeak = 0; gMaxGR = 0; auto p = make(); setup (*p); auto b = render (*p, ev, secs); report (name, b); };
    auto sitarOnly = [] (TantriProcessor& p) { set (p, "n_on", 0); };
    run ("sitar alap (vel 0.6-0.85)", sitarOnly, yamanAlap (0.1), 22);
    std::vector<Ev> hard; for (int i = 0; i < 6; ++i) hard.push_back ({ 0.05 + i * 0.01, juce::MidiMessage::noteOn (1, 55 + i * 3, 1.0f) });
    run ("sitar 6-note chord vel 127", sitarOnly, hard, 4);
    std::vector<Ev> h12; for (int i = 0; i < 12; ++i) h12.push_back ({ 0.05, juce::MidiMessage::noteOn (1, 48 + i * 2, 1.0f) });
    run ("sitar 12 notes at once vel 127", sitarOnly, h12, 4);
    run ("tabla teentaal", [] (TantriProcessor& p) { set (p, "n_on", 0); }, teentaal (0.1, 96, 2), 12);
    std::vector<Ev> th; for (int i = 0; i < 12; ++i) th.push_back ({ 0.05 + i * 0.25, juce::MidiMessage::noteOn (10, 36 + i, 1.0f) });
    run ("tabla every bol vel 127", [] (TantriProcessor& p) { set (p, "n_on", 0); }, th, 4);
    run ("tanpura drone", [] (TantriProcessor&) {}, {}, 20);
    auto all = yamanAlap (0.1); auto tb = teentaal (0.1, 96, 3); all.insert (all.end(), tb.begin(), tb.end());
    run ("everything together", [] (TantriProcessor&) {}, all, 22);
    run ("everything, all levels + master max", [] (TantriProcessor& p) { for (auto id : { "s_level", "t_level", "n_level", "master" }) set (p, id, 1.5f); }, all, 22);
    return 0;
}

// tanpura drone: the level must hold (no decay) and sit near the plucked cycle's level
static int testDrone()
{
    auto windows = [] (const juce::AudioBuffer<float>& b, double from, std::vector<float>& w)
    {
        const int W = int (kFs);
        for (int s = int (from * kFs); s + W <= b.getNumSamples(); s += W)
        {
            double e = 0;
            for (int c = 0; c < 2; ++c) { auto* x = b.getReadPointer (c); for (int i = 0; i < W; ++i) e += x[s + i] * x[s + i]; }
            w.push_back ((float) std::sqrt (e / (2.0 * W)));
        }
    };
    auto stats = [] (const char* name, const std::vector<float>& w)
    {
        float lo = 1e9f, hi = 0, avg = 0;
        for (float x : w) { lo = std::min (lo, x); hi = std::max (hi, x); avg += x; }
        avg /= std::max<size_t> (1, w.size());
        std::printf ("%-34s rms avg %6.1f dB  min %6.1f  max %6.1f  (first->last %6.1f -> %6.1f)\n", name,
                     juce::Decibels::gainToDecibels (avg), juce::Decibels::gainToDecibels (lo), juce::Decibels::gainToDecibels (hi),
                     juce::Decibels::gainToDecibels (w.front()), juce::Decibels::gainToDecibels (w.back()));
        return avg;
    };
    int rc = 0;
    std::vector<float> wc; { auto p = make(); set (*p, "n_room", 0); windows (render (*p, {}, 30), 4, wc); }
    float cyc = stats ("cycle (plucked)", wc);
    struct Case { const char* name; int voicing, note, mask; };
    for (auto c : { Case { "drone chord (all four)", 1, 0, 15 }, Case { "drone chord Pa + kharaj", 1, 0, 9 },
                    Case { "drone single Sa", 0, 1, 0 }, Case { "drone single kharaj sa", 0, 3, 0 }, Case { "drone single 1st string", 0, 0, 0 } })
    {
        auto p = make();
        set (*p, "n_room", 0); set (*p, "n_mode", 1); set (*p, "n_voicing", (float) c.voicing); set (*p, "n_note", (float) c.note);
        for (int i = 0; i < 4; ++i) set (*p, ("n_dr" + juce::String (i + 1)).toRawUTF8(), (c.mask >> i) & 1 ? 1.0f : 0.0f);
        gPrePeak = 0;
        auto b = render (*p, {}, 60);
        std::vector<float> w; windows (b, 4, w);
        float avg = stats (c.name, w);
        // no decay: the last ten seconds within 3 dB of the average; no blow-up
        float tail = 0; for (size_t i = w.size() - 10; i < w.size(); ++i) tail += w[i] / 10.0f;
        bool ok = std::abs (juce::Decibels::gainToDecibels (tail / avg)) < 3.0f && gPrePeak < 2.0f && std::isfinite (avg);
        std::printf ("   %s  (pre-limiter peak %.1f dB, vs cycle %+.1f dB)\n", ok ? "ok" : "FAIL", juce::Decibels::gainToDecibels (gPrePeak),
                     juce::Decibels::gainToDecibels (avg / cyc));
        if (! ok) rc = 1;
        if (auto* d = std::getenv ("DRONE_WAV")) writeWav (b, juce::File (d).getChildFile (juce::String (c.name).replaceCharacters (" +()", "____") + ".wav"));
    }
    return rc;
}


#if TANTRI_SPATIAL
// Tantri Akasha: Ambisonic encoding, motions, the flock and the ceiling
static std::unique_ptr<TantriProcessor> makeAmbi (int order)
{
    auto p = std::make_unique<TantriProcessor>();
    juce::AudioProcessor::BusesLayout l;
    if (TantriProcessor::kFx) l.inputBuses.add (juce::AudioChannelSet::stereo());
    l.outputBuses.add (juce::AudioChannelSet::ambisonic (order));
    if (! p->setBusesLayout (l)) { std::printf ("layout refused\n"); return nullptr; }
    p->setRateAndBufferSizeDetails (kFs, 256);
    p->prepareToPlay (kFs, 256);
    set (*p, "order", (float) (order - 1));
    return p;
}
static double energy (const juce::AudioBuffer<float>& b, int ch, int from = 0)
{
    double e = 0; auto* x = b.getReadPointer (ch);
    for (int i = from; i < b.getNumSamples(); ++i) e += x[i] * x[i];
    return e;
}
static double corr (const juce::AudioBuffer<float>& b, int c1, int c2, int from, int to)
{
    double s = 0; auto* x = b.getReadPointer (c1); auto* y = b.getReadPointer (c2);
    for (int i = from; i < to; ++i) s += x[i] * y[i];
    return s;
}
static int testSpace()
{
    int rc = 0;
    auto check = [&] (bool ok, const char* what) { std::printf ("  %s  %s\n", ok ? "ok  " : "FAIL", what); if (! ok) rc = 1; };
    std::printf ("Tantri Akasha (%s):\n", TantriProcessor::kFx ? "FX" : "instrument");
    // 1. the tanpura alone at 90 degrees left, level: Y follows W, X (front) silent
    {
        auto p = makeAmbi (3);
        for (const char* id : { "tanpura_az" }) set (*p, id, 90);
        set (*p, "tanpura_motion", 0); set (*p, "tanpura_el", 0); set (*p, "n_room", 0); set (*p, "spread", 0); set (*p, "tanpura_dist", 0.6f);
        auto b = render (*p, {}, 6);
        const double w = energy (b, 0, int (kFs)), y = energy (b, 1, int (kFs)), x = energy (b, 3, int (kFs)), z = energy (b, 2, int (kFs));
        std::printf ("    W %.3g  Y %.3g  Z %.3g  X %.3g\n", w, y, z, x);
        check (b.getNumChannels() == 16, "3rd order = 16 channels");
        check (y > 0.7 * w && x < 0.05 * w && z < 0.05 * w, "source at 90 degrees left: Y ~ W, X and Z ~ 0");
    }
    // 2. orbit: the tanpura goes round you - Y changes sign over the circle
    {
        auto p = makeAmbi (1);
        set (*p, "tanpura_motion", (float) tantri::field::motion::Orbit); set (*p, "tanpura_rate", 4.0f); set (*p, "n_room", 0);
        set (*p, "n_mode", 1);   // drone: steady sound to follow
        auto b = render (*p, {}, 9);
        int pos = 0, neg = 0;
        for (int s = int (kFs); s + 4800 < b.getNumSamples(); s += 4800)
        {
            double c = corr (b, 0, 1, s, s + 4800);
            if (c > 0) ++pos; else ++neg;
        }
        std::printf ("    windows with the source left %d, right %d\n", pos, neg);
        check (pos > 5 && neg > 5, "orbit: the tanpura passes both sides");
    }
    // 3. every source on boids / lorenz with the drone + sitar + tabla: 30 s, finite, under the ceiling at 7th order
    for (int mo : { (int) tantri::field::motion::Boids, (int) tantri::field::motion::Lorenz })
    {
        auto p = makeAmbi (7);
        for (int k = 0; k < TantriProcessor::kSources; ++k) set (*p, (juce::String (TantriProcessor::kSrcId[k]) + "_motion").toRawUTF8(), (float) mo);
        for (auto id : { "s_level", "t_level", "n_level", "master" }) set (*p, id, 1.5f);
        set (*p, "n_mode", 1); set (*p, "mspeed", 4.0f);
        auto ev = yamanAlap (0.1); auto tb = teentaal (0.1, 96, 3); ev.insert (ev.end(), tb.begin(), tb.end());
        float x0[4], y0[4];
        render (*p, {}, 0.1);
        for (int k = 0; k < TantriProcessor::kSources; ++k) { x0[k] = p->srcX[k].load(); y0[k] = p->srcY[k].load(); }
        auto b = render (*p, ev, 30, 256, [] (juce::AudioBuffer<float>& io, int pos)
        {
            for (int i = 0; i < io.getNumSamples(); ++i)
                for (int c = 0; c < std::min (2, io.getNumChannels()); ++c)
                    io.setSample (c, i, 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f * float (pos + i) / float (kFs)));
        });
        float pk = 0; bool finite = true;
        for (int c = 0; c < b.getNumChannels(); ++c) { pk = std::max (pk, b.getMagnitude (c, 0, b.getNumSamples())); auto* x = b.getReadPointer (c); for (int i = 0; i < b.getNumSamples(); ++i) finite &= std::isfinite (x[i]); }
        float moved = 0;
        for (int k = 0; k < TantriProcessor::kSources; ++k) moved = std::max (moved, std::abs (p->srcX[k].load() - x0[k]) + std::abs (p->srcY[k].load() - y0[k]));
        std::printf ("    %s: 64 ch, peak %.2f dBFS, moved %.3f\n", mo == tantri::field::motion::Boids ? "boids" : "lorenz", juce::Decibels::gainToDecibels (pk), moved);
        check (finite && pk <= 0.8913f + 1e-4f, "finite and never above -1 dBFS");
        check (moved > 0.01f, "the sources move");
    }
    // 4. the FX: its input is heard (W carries the 220 Hz tone with every instrument silent)
    if (TantriProcessor::kFx)
    {
        auto p = makeAmbi (3);
        set (*p, "n_on", 0); set (*p, "i_symp", 0);
        auto b = render (*p, {}, 2, 256, [] (juce::AudioBuffer<float>& io, int pos)
        {
            for (int i = 0; i < io.getNumSamples(); ++i)
                for (int c = 0; c < 2; ++c) io.setSample (c, i, 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f * float (pos + i) / float (kFs)));
        });
        const double w = energy (b, 0, int (kFs)) / kFs;
        std::printf ("    input in W: rms %.3f\n", std::sqrt (w));
        check (w > 1e-3, "the input is placed in the space");
        // and the input sounds the taraf
        auto p2 = makeAmbi (3);
        set (*p2, "n_on", 0); set (*p2, "i_level", 0); set (*p2, "i_symp", 1.0f);
        auto b2 = render (*p2, {}, 2, 256, [] (juce::AudioBuffer<float>& io, int pos)
        {
            for (int i = 0; i < io.getNumSamples(); ++i)
                for (int c = 0; c < 2; ++c) io.setSample (c, i, 0.3f * std::sin (2.0f * juce::MathConstants<float>::pi * 277.18f * float (pos + i) / float (kFs)));
        });
        const double w2 = energy (b2, 0, int (kFs)) / kFs;
        std::printf ("    taraf sounded by the input: rms %.4f\n", std::sqrt (w2));
        check (w2 > 1e-7, "the input sets the sympathetic strings ringing");
    }
    // 5. stereo preview (the app): something left/right, not silent
    {
        auto p = make();
        set (*p, "n_mode", 1); set (*p, "tanpura_az", 90); set (*p, "tanpura_motion", 0);
        auto b = render (*p, {}, 4);
        const double l = energy (b, 0, int (kFs)), r = energy (b, 1, int (kFs));
        std::printf ("    stereo preview L %.3g  R %.3g\n", l, r);
        check (l > 2 * r && r > 0, "stereo preview: a source on the left is louder left");
    }
    return rc;
}
#endif

static int testBols()
{
    for (float snap : { 0.0f, 0.35f })
    {
        std::printf ("snap %.2f:", snap);
        for (int b = 0; b < 12; ++b)
        {
            gPrePeak = 0; auto p = make(); set (*p, "n_on", 0); set (*p, "t_snap", snap); set (*p, "t_room", 0);
            auto buf = render (*p, { { 0.01, juce::MidiMessage::noteOn (10, 36 + b, 1.0f) } }, 1.0);
            float pk = gPrePeak; (void) buf;
            std::printf (" %s %.1f", tantri::kBolName[b], juce::Decibels::gainToDecibels (pk));
        }
        std::printf ("\n");
    }
    return 0;
}

static void jawTest (const juce::File& dir)
{
    dir.createDirectory();
    for (float j : { 0.0f, 0.5f, 1.0f })
    {
        auto p = make(); set (*p, "n_on", 0); set (*p, "s_taraf", 0); set (*p, "s_room", 0); set (*p, "s_gourd", 0); set (*p, "s_jawari", j);
        auto b = render (*p, { { 0.01, juce::MidiMessage::noteOn (1, 60, 0.9f) } }, 6.0);
        // high-band share over time
        std::printf ("jawari %.1f  >2kHz share:", j);
        juce::dsp::IIR::Filter<float> hp; hp.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (kFs, 2000.0);
        std::vector<float> h ((size_t) b.getNumSamples());
        for (int i = 0; i < b.getNumSamples(); ++i) h[(size_t) i] = hp.processSample (b.getSample (0, i));
        for (double t : { 0.1, 0.3, 0.6, 1.0, 1.5, 2.0, 3.0, 4.0 })
        {
            double eh = 0, ea = 0; int s0 = int (t * kFs);
            for (int i = s0; i < s0 + 4800; ++i) { eh += h[(size_t) i] * h[(size_t) i]; ea += b.getSample (0, i) * b.getSample (0, i); }
            std::printf (" %.0fs:%5.1f%%", t, 100 * eh / (ea + 1e-20));
        }
        std::printf ("\n");
        writeWav (b, dir.getChildFile ("jaw" + juce::String (int (j * 10)) + ".wav"));
    }
}

static int testStress()
{
    auto p = make();
    juce::Random rnd (7);
    std::vector<Ev> ev;
    for (double t = 0; t < 60; t += 0.02 + rnd.nextDouble() * 0.15)
    {
        int ch = rnd.nextInt (10) < 7 ? 1 : 10;
        int n = 30 + rnd.nextInt (60);
        ev.push_back ({ t, juce::MidiMessage::noteOn (ch, n, (float) rnd.nextDouble()) });
        ev.push_back ({ t + rnd.nextDouble() * 3, juce::MidiMessage::noteOff (ch, n) });
        if (rnd.nextInt (5) == 0) ev.push_back ({ t, juce::MidiMessage::pitchWheel (1, rnd.nextInt (16383)) });
        if (rnd.nextInt (6) == 0) ev.push_back ({ t, juce::MidiMessage::aftertouchChange (1, n, rnd.nextInt (127)) });
        if (rnd.nextInt (8) == 0) ev.push_back ({ t, juce::MidiMessage::controllerEvent (1, 1, rnd.nextInt (127)) });
    }
    for (auto id : { "s_jawari", "n_jawari", "s_taraf", "t_syahi" }) set (*p, id, 1.0f);
    auto b = render (*p, ev, 62);
    double peak = 0; bool finite = true;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            float v = b.getSample (c, i);
            if (! std::isfinite (v)) finite = false;
            peak = std::max (peak, (double) std::abs (v));
        }
    std::printf ("stress: finite=%d peak=%.3f\n", finite, peak);
    return finite && peak < 1.01 ? 0 : 1;
}

static int testCpu()
{
   #if TANTRI_SPATIAL
    auto p = std::getenv ("CPU_ORDER") ? makeAmbi (std::atoi (std::getenv ("CPU_ORDER"))) : make();
    for (int k = 0; k < TantriProcessor::kSources; ++k) set (*p, (juce::String (TantriProcessor::kSrcId[k]) + "_motion").toRawUTF8(), (float) (k == 1 ? 11 : 12));
   #else
    auto p = make();
   #endif
    std::vector<Ev> ev;
    for (int i = 0; i < 12; ++i) ev.push_back ({ 0.01 + i * 0.05, juce::MidiMessage::noteOn (1, 48 + i * 3, 0.9f) });
    for (double t = 0; t < 10; t += 0.25) ev.push_back ({ t, juce::MidiMessage::noteOn (10, 36 + int (t * 4) % 12, 0.8f) });
    auto t0 = juce::Time::getMillisecondCounterHiRes();
    render (*p, ev, 10);
    double ms = juce::Time::getMillisecondCounterHiRes() - t0;
    std::printf ("cpu: 10 s rendered in %.0f ms -> %.1f%% of one core at 48 kHz\n", ms, ms / 100.0);
    return 0;
}

// ---- demos
static std::vector<Ev> yamanAlap (double t0)
{
    // madhya Sa = 60. Yaman: Ni Re Ga, Ma'(66) Dha(69) Ni(71). meend via pitch bend
    std::vector<Ev> e;
    auto note = [&] (double t, int n, double dur, float v = 0.75f)
    { e.push_back ({ t0 + t, juce::MidiMessage::noteOn (1, n, v) }); e.push_back ({ t0 + t + dur, juce::MidiMessage::noteOff (1, n) }); };
    auto bend = [&] (double t, double dur, double from, double to)
    {
        for (int i = 0; i <= 40; ++i)
        {
            double x = from + (to - from) * (0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * i / 40));
            e.push_back ({ t0 + t + dur * i / 40, juce::MidiMessage::pitchWheel (1, juce::jlimit (0, 16383, int (8192 + x * 8191))) });
        }
    };
    note (0.0, 59, 1.4, 0.6);        // Ni (mandra)
    note (1.2, 62, 1.0);             // Re
    note (2.0, 64, 2.4, 0.8);        // Ga, then meend Ga -> Ma' -> Ga
    bend (2.8, 0.6, 0, 0.42); bend (3.6, 0.7, 0.42, 0);
    note (4.6, 62, 0.8); note (5.3, 60, 1.8, 0.7);
    e.push_back ({ t0 + 5.3, juce::MidiMessage::noteOn (1, 30, 0.6f) });   // chikari
    note (7.4, 66, 0.7); note (8.0, 67, 0.7); note (8.6, 69, 1.4, 0.8);
    bend (9.2, 0.5, 0, 0.3); bend (9.8, 0.6, 0.3, 0);
    note (10.4, 71, 0.6); note (10.9, 72, 2.6, 0.85);   // taar Sa
    e.push_back ({ t0 + 11.6, juce::MidiMessage::noteOn (1, 30, 0.5f) });
    note (13.6, 71, 0.5); note (14.0, 69, 0.5); note (14.4, 67, 0.6); note (14.9, 66, 0.6);
    note (15.4, 64, 2.2, 0.8); bend (16.0, 1.2, 0, 0.0);
    // jhala-like chikari pulses with the final Sa
    note (17.8, 60, 4.0, 0.8);
    for (int i = 0; i < 12; ++i) e.push_back ({ t0 + 18.1 + i * 0.22, juce::MidiMessage::noteOn (1, 30, 0.45f) });
    return e;
}
static std::vector<Ev> teentaal (double t0, double bpm, int cycles)
{
    using namespace tantri;
    static const int theka[16] = { Dha, Dhin, Dhin, Dha, Dha, Dhin, Dhin, Dha, Dha, Tin, Tin, Na, Na, Dhin, Dhin, Dha };
    std::vector<Ev> e;
    double beat = 60.0 / bpm;
    for (int c = 0; c < cycles; ++c)
        for (int i = 0; i < 16; ++i)
        {
            double t = t0 + (c * 16 + i) * beat;
            float v = (i == 0) ? 0.95f : (i >= 8 && i < 12) ? 0.6f : 0.75f;
            e.push_back ({ t, juce::MidiMessage::noteOn (10, 36 + theka[i], v) });
            e.push_back ({ t + 0.1, juce::MidiMessage::noteOff (10, 36 + theka[i]) });
            if (c == cycles - 1 && i == 15)   // tihai-ish flourish
                for (int k = 1; k <= 3; ++k)
                    e.push_back ({ t + beat * 0.25 * k, juce::MidiMessage::noteOn (10, 36 + (k == 3 ? Dha : Tit), 0.8f) });
        }
    // a little ghe at the start of khali
    return e;
}

static void demos (const juce::File& dir)
{
    dir.createDirectory();
    { auto p = make(); set (*p, "n_on", 0); set (*p, "t_level", 0); writeWav (render (*p, yamanAlap (0.2), 24), dir.getChildFile ("sitar-yaman-alap.wav")); }
    { auto p = make(); set (*p, "n_on", 0); set (*p, "s_level", 0); writeWav (render (*p, teentaal (0.2, 96, 4), 42), dir.getChildFile ("tabla-teentaal.wav")); }
    { auto p = make(); writeWav (render (*p, {}, 30), dir.getChildFile ("tanpura-pa-sa-sa-sa.wav")); }
    {
        auto p = make();
        auto ev = yamanAlap (4.0);
        auto tb = teentaal (14.0, 88, 3);
        ev.insert (ev.end(), tb.begin(), tb.end());
        writeWav (render (*p, ev, 50), dir.getChildFile ("tantri-mix.wav"));
    }
}

static void spectrogram (const juce::File& in, const juce::File& out)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (in));
    if (! r) return;
    juce::AudioBuffer<float> b (1, (int) r->lengthInSamples);
    r->read (&b, 0, b.getNumSamples(), 0, true, false);
    const int order = 12, N = 1 << order, hop = 256;
    juce::dsp::FFT fft (order);
    const int W = 1400, H = 600;
    juce::Image img (juce::Image::RGB, W, H, true);
    std::vector<float> data ((size_t) N * 2);
    int frames = (b.getNumSamples() - N) / hop;
    float norm = 1e-9f;
    for (int pass = 0; pass < 2; ++pass)
    for (int x = 0; x < W; ++x)
    {
        int fr = int ((double) x / W * frames);
        const float* s = b.getReadPointer (0) + fr * hop;
        for (int i = 0; i < N; ++i) data[(size_t) i] = s[i] * (0.5f - 0.5f * std::cos (2 * juce::MathConstants<float>::pi * i / N));
        std::fill (data.begin() + N, data.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (data.data());
        for (int y = 0; y < H; ++y)
        {
            double f = std::getenv ("LINSPEC") ? 6000.0 * (1.0 - (double) y / H) + 20 : 40.0 * std::pow (12000.0 / 40.0, 1.0 - (double) y / H);
            int bin = juce::jlimit (1, N / 2 - 1, int (f / kFs * N));
            float mag = data[(size_t) bin];
            if (pass == 0) { norm = std::max (norm, mag); continue; }
            float dbv = juce::Decibels::gainToDecibels (mag / norm + 1e-9f);
            float v = std::pow (juce::jlimit (0.0f, 1.0f, (dbv + 75) / 75), 1.3f);
            img.setPixelAt (x, y, juce::Colour::fromHSV (0.66f - 0.6f * v, 0.8f, v, 1.0f));
        }
    }
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
}

static void shot (const juce::File& out)
{
    auto p = make();
    if (auto* nm = std::getenv ("SHOT_SCALE")) set (*p, "scale", (float) scaleIndexOf (nm));
    if (std::getenv ("SHOT_STEPS")) set (*p, "map", 1.0f);
    // play a bit so the UI shows activity
    std::vector<Ev> ev { { 0.01, juce::MidiMessage::noteOn (1, 64, 0.9f) }, { 0.05, juce::MidiMessage::noteOn (1, 71, 0.8f) },
                         { 0.3, juce::MidiMessage::noteOn (10, 36 + tantri::Dha, 0.9f) } };
    render (*p, ev, 4.4);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p->createEditor());
    auto& canvas = dynamic_cast<TantriEditor*> (ed.get())->getCanvas();
    if (std::getenv ("SHOT_SPACE")) canvas.setSpaceView (true);
    if (std::getenv ("SHOT_DRONE")) { set (*p, "n_mode", 1); set (*p, "n_dr3", 0); render (*p, {}, 2.0); }
    for (int i = 0; i < 3; ++i) canvas.tick();
    auto img = canvas.createComponentSnapshot (canvas.getLocalBounds(), true, 2.0f);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("wrote %s\n", out.getFullPathName().toRawUTF8());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::StringArray a; for (int i = 1; i < argc; ++i) a.add (argv[i]);
    int rc = 0;
    if (a.contains ("--pitch")) rc |= testPitch();
    if (a.contains ("--bols")) rc |= testBols();
    if (a.contains ("--drone")) rc |= testDrone();
   #if TANTRI_SPATIAL
    if (a.contains ("--space")) rc |= testSpace();
   #endif
    if (a.isEmpty()) { rc |= testPitch(); rc |= testDrone(); rc |= testStress();
                      #if TANTRI_SPATIAL
                       rc |= testSpace();
                      #endif
                     }
    if (int i = a.indexOf ("--jaw"); i >= 0) jawTest (juce::File (a[i + 1]));
    if (a.contains ("--levels")) rc |= testLevels();
    if (a.contains ("--debug")) rc |= testDebug();
    if (a.contains ("--stress")) rc |= testStress();
    if (a.contains ("--cpu")) rc |= testCpu();
    if (int i = a.indexOf ("--wav"); i >= 0 && i + 1 < a.size()) demos (juce::File (a[i + 1]));
    if (int i = a.indexOf ("--spec"); i >= 0 && i + 2 < a.size()) spectrogram (juce::File (a[i + 1]), juce::File (a[i + 2]));
    if (int i = a.indexOf ("--shot"); i >= 0 && i + 1 < a.size()) shot (juce::File (a[i + 1]));
    return rc;
}
