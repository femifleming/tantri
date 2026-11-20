#pragma once
// Flat geometric drawing kit for the Tantri UI: gold-rimmed stars, polygons, kites and faceted gems.
#include <juce_gui_basics/juce_gui_basics.h>

namespace art
{
namespace col
{
    inline const juce::Colour bg     { 0xff0a0c0d };
    inline const juce::Colour tile   { 0xff0f2128 };
    inline const juce::Colour edge   { 0xff1b3640 };
    inline const juce::Colour gold   { 0xffc9a238 };
    inline const juce::Colour gold2  { 0xffe2bf55 };
    inline const juce::Colour white  { 0xfff3f1ec };
    inline const juce::Colour orange { 0xffef8a2e };
    inline const juce::Colour coral  { 0xffec5f55 };
    inline const juce::Colour red    { 0xffc3261f };
    inline const juce::Colour red2   { 0xffe43a2c };
    inline const juce::Colour pink   { 0xfff2a9c2 };
    inline const juce::Colour mag    { 0xffe2316b };
    inline const juce::Colour brown  { 0xff94672a };
    inline const juce::Colour grey   { 0xff9fb3ba };
    inline const juce::Colour dim    { 0xff7f959c };
    inline const juce::Colour off    { 0xff22424d };
    inline const juce::Colour ink    { 0xff111111 };
}

inline juce::Font font (float size, bool bold = true)
{
    return juce::Font (juce::FontOptions ("Helvetica Neue", size, bold ? juce::Font::bold : juce::Font::plain));
}

inline juce::Point<float> P (juce::Point<float> c, float r, float a) { return { c.x + r * std::cos (a), c.y + r * std::sin (a) }; }

inline juce::Path poly (std::initializer_list<juce::Point<float>> pts)
{
    juce::Path p; bool first = true;
    for (auto q : pts) { if (first) p.startNewSubPath (q); else p.lineTo (q); first = false; }
    p.closeSubPath(); return p;
}
inline void fill (juce::Graphics& g, const juce::Path& p, juce::Colour f, juce::Colour stroke = {}, float sw = 0)
{
    g.setColour (f); g.fillPath (p);
    if (sw > 0) { g.setColour (stroke); g.strokePath (p, juce::PathStrokeType (sw, juce::PathStrokeType::curved)); }
}
inline juce::Path star (juce::Point<float> c, int n, float ro, float ri, float rot = -juce::MathConstants<float>::halfPi)
{
    juce::Path p;
    for (int i = 0; i < n * 2; ++i)
    {
        auto q = P (c, (i & 1) ? ri : ro, rot + i * juce::MathConstants<float>::pi / n);
        if (i == 0) p.startNewSubPath (q); else p.lineTo (q);
    }
    p.closeSubPath(); return p;
}
inline juce::Path ngon (juce::Point<float> c, int n, float r, float rot = -juce::MathConstants<float>::halfPi)
{
    juce::Path p;
    for (int i = 0; i < n; ++i)
    {
        auto q = P (c, r, rot + i * juce::MathConstants<float>::twoPi / n);
        if (i == 0) p.startNewSubPath (q); else p.lineTo (q);
    }
    p.closeSubPath(); return p;
}
// kite petal pointing outward at angle a, from radius r0 to r1, half-width w (radians at the waist)
inline juce::Path kite (juce::Point<float> c, float a, float r0, float r1, float w)
{
    float m = (r0 + r1) * 0.5f * 0.9f;
    return poly ({ P (c, r0, a), P (c, m, a - w), P (c, r1, a), P (c, m, a + w) });
}
// faceted jewel (white triangle split into magenta / pink / coral facets)
inline void gem (juce::Graphics& g, juce::Point<float> c, float s, float a, float glow = 0)
{
    auto t0 = P (c, s, a), t1 = P (c, s, a + 2.1f), t2 = P (c, s, a - 2.1f);
    if (glow > 0.01f)
    {
        g.setColour (col::gold2.withAlpha (0.35f * glow));
        g.fillEllipse (juce::Rectangle<float> (s * 4.4f, s * 4.4f).withCentre (c));
    }
    fill (g, poly ({ t0, t1, t2 }), col::white, col::gold, 1.2f);
    fill (g, poly ({ t0, c, t1 }), col::mag.interpolatedWith (col::white, glow * 0.5f));
    fill (g, poly ({ t0, c, t2 }), col::pink);
    fill (g, poly ({ t1, c, t2 }), col::coral.interpolatedWith (col::gold2, glow * 0.6f));
}
} // namespace art
