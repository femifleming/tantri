#pragma once
// Vortex spectrogram: a tilted spinning disc. Radius = log frequency (each partial is a ring),
// angle = time (each new spectrum is laid down at the write head as the disc turns), and a radial
// twist bends the history into spiral arms. Older spectra fade, which carves the dark arms.
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "Art.h"

class Vortex : public juce::Component
{
public:
    static constexpr int kBins = 300, kSlices = 110, kArms = 3, kOrder = 13, kN = 1 << kOrder;

    Vortex() : fft (kOrder)
    {
        history.assign ((size_t) kBins * kSlices, 0.0f);
        ring.assign ((size_t) kN, 0.0f);
        window.resize ((size_t) kN);
        for (int i = 0; i < kN; ++i) window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / kN);
        // palette: black -> violet -> copper -> gold -> cream (from the reference image)
        const juce::Colour stops[] = { juce::Colour (0xff000000), juce::Colour (0xff2b1f38), juce::Colour (0xff5a3d5c),
                                       juce::Colour (0xff9a5a36), juce::Colour (0xffc9a238), juce::Colour (0xfff3e6c0) };
        const float pos[] = { 0.0f, 0.14f, 0.27f, 0.45f, 0.7f, 1.0f };
        for (int i = 0; i < 256; ++i)
        {
            float t = i / 255.0f; int k = 0;
            while (k < 4 && t > pos[k + 1]) ++k;
            float u = (t - pos[k]) / (pos[k + 1] - pos[k]);
            lut[i] = stops[k].interpolatedWith (stops[k + 1], u).getARGB();
        }
    }

    // called by the editor timer with new mono samples from the audio thread
    void push (const float* x, int n)
    {
        for (int i = 0; i < n; ++i) { ring[(size_t) rw] = x[i]; rw = (rw + 1) & (kN - 1); }
        fresh += n;
    }
    void setSampleRate (double s) { fs = s; }

    void analyse()
    {
        if (fresh <= 0) { addSlice (true); return; }
        fresh = 0;
        std::vector<float> d ((size_t) kN * 2, 0.0f);
        for (int i = 0; i < kN; ++i) d[(size_t) i] = ring[(size_t) ((rw + i) & (kN - 1))] * window[(size_t) i];
        fft.performFrequencyOnlyForwardTransform (d.data());
        float col[kBins], dbs[kBins], frameMax = -200.0f;
        for (int b = 0; b < kBins; ++b)
        {
            // log-frequency radius: 45 Hz at the eye, 12 kHz at the rim; peak-pick within each band
            double f0 = 45.0 * std::pow (12000.0 / 45.0, double (b) / kBins);
            double f1 = 45.0 * std::pow (12000.0 / 45.0, double (b + 1) / kBins);
            int i0 = juce::jlimit (1, kN / 2 - 1, int (f0 / fs * kN)), i1 = juce::jlimit (i0, kN / 2 - 1, int (f1 / fs * kN));
            float m = 0; for (int i = i0; i <= i1; ++i) m = std::max (m, d[(size_t) i]);
            dbs[b] = juce::Decibels::gainToDecibels (m / (kN * 0.25f) + 1e-9f);
            frameMax = std::max (frameMax, dbs[b]);
        }
        // auto-range: follow the loudest recent partial (fast up, slow down) so quiet playing still glows
        ref = frameMax > ref ? frameMax : std::max (frameMax, ref - 0.15f);
        float top = std::max (ref, -60.0f);
        for (int b = 0; b < kBins; ++b) col[b] = juce::jlimit (0.0f, 1.0f, (dbs[b] - (top - 46.0f)) / 40.0f);
        std::copy (col, col + kBins, history.begin() + (size_t) head * kBins);
        addSlice (false);
    }

    void paint (juce::Graphics& g) override
    {
        const int W = getWidth(), H = getHeight();
        if (W <= 0 || H <= 0) return;
        const float scale = 1.5f;   // render above 1x so the partial rings stay crisp
        const int iw = int (W * scale), ih = int (H * scale);
        if (img.getWidth() != iw || img.getHeight() != ih) { img = juce::Image (juce::Image::ARGB, iw, ih, true); buildMap (iw, ih); }
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        const float rot = spin;
        for (int y = 0; y < ih; ++y)
        {
            auto* row = reinterpret_cast<juce::uint32*> (bd.getLinePointer (y));
            const Px* m = map.data() + (size_t) y * iw;
            for (int x = 0; x < iw; ++x)
            {
                const Px& p = m[x];
                if (p.bin < 0) { row[x] = 0xff000000; continue; }
                // angle -> age: 0 at the write head, growing behind it
                float a = (p.angle - rot) * kArms;   // three arms, each holding the last few seconds
                a -= std::floor (a);
                int age = int (a * kSlices);
                int slice = (head - 1 - age + 2 * kSlices) % kSlices;
                float v = history[(size_t) slice * kBins + (size_t) p.bin];
                float fade = 1.0f - 0.7f * a * a;               // older spectra darken toward the arms
                v *= fade * p.groove * p.edge;
                int li = juce::jlimit (0, 255, int (v * 255.0f));
                row[x] = lut[li];
            }
        }
        g.drawImage (img, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
        g.setColour (art::col::edge); g.drawRect (getLocalBounds(), 1);
        g.setColour (art::col::grey); g.setFont (art::font (10));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("VORTEX \xc2\xb7 RINGS = PARTIALS (45 HZ AT THE EYE, 12 KHZ AT THE RIM) \xc2\xb7 ARMS = TIME")), 12, 8, 600, 14, juce::Justification::left);
    }

private:
    struct Px { int bin; float angle, groove, edge; };

    void addSlice (bool silent)
    {
        if (silent) std::fill (history.begin() + (size_t) head * kBins, history.begin() + (size_t) (head + 1) * kBins, 0.0f);
        head = (head + 1) % kSlices;
        spin += 1.0f / (kSlices * kArms);   // the head advances one slice per frame: the disc turns
        spin -= std::floor (spin);
    }

    void buildMap (int iw, int ih)
    {
        map.assign ((size_t) iw * ih, {});
        // tilted disc, larger than the frame and cropped like the reference
        const float cx = iw * 0.5f, cy = ih * 0.56f, R = iw * 0.62f, tilt = 0.42f;
        const float twist = 1.35f;   // turns of spiral from eye to rim
        for (int y = 0; y < ih; ++y)
            for (int x = 0; x < iw; ++x)
            {
                float dx = (x - cx) / R, dy = (y - cy) / (R * tilt);
                float r = std::sqrt (dx * dx + dy * dy);
                Px& p = map[(size_t) y * iw + x];
                if (r >= 1.0f || r < 0.004f) { p.bin = -1; continue; }
                p.bin = juce::jlimit (0, kBins - 1, int (r * kBins));
                float th = std::atan2 (dy, dx) / juce::MathConstants<float>::twoPi;
                p.angle = th + twist * std::sqrt (r);
                p.groove = 0.8f + 0.2f * std::cos (r * 170.0f * juce::MathConstants<float>::twoPi);   // fine lathe lines
                p.edge = juce::jlimit (0.0f, 1.0f, (1.0f - r) * 14.0f) * (0.55f + 0.45f * juce::jlimit (0.0f, 1.0f, (dy + 1.0f)));   // near side brighter
            }
    }

    juce::dsp::FFT fft;
    std::vector<float> history, ring, window;
    std::vector<Px> map;
    juce::Image img;
    juce::uint32 lut[256] {};
    int rw = 0, fresh = 0, head = 0;
    float spin = 0, ref = -40.0f;
    double fs = 48000;
};
