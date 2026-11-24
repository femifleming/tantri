#include "PluginEditor.h"

using namespace art;
using juce::MathConstants;
static constexpr float kTwoPi = MathConstants<float>::twoPi;
static constexpr float kHalfPi = MathConstants<float>::halfPi;

static void setParam (TantriProcessor& p, const char* id, float plainValue)
{
    if (auto* prm = p.apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (plainValue));
        prm->endChangeGesture();
    }
}
static float getParam (TantriProcessor& p, const char* id) { return p.apvts.getRawParameterValue (id)->load(); }

// ---------------------------------------------------------------- StarKnob
StarKnob::StarKnob (juce::RangedAudioParameter* p, juce::String label) : param (p), name (std::move (label))
{
    setSliderStyle (juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setMouseDragSensitivity (180);
    setVelocityBasedMode (false);
    setPopupDisplayEnabled (false, false, nullptr);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void StarKnob::paint (juce::Graphics& g)
{
    const float v = (float) valueToProportionOfLength (getValue());
    juce::Point<float> c (getWidth() * 0.5f, 25.0f);
    const int n = 16;
    for (int i = 0; i < n; ++i)
    {
        float t = i / float (n - 1), a = MathConstants<float>::pi * 0.75f + t * MathConstants<float>::pi * 1.5f;
        bool on = t <= v + 1e-4f;
        fill (g, kite (c, a, 13.5f, on ? 24.0f : 19.5f, 0.13f), on ? (hover ? col::white : col::gold2) : col::off);
    }
    fill (g, ngon (c, 8, 12.0f), col::white, col::gold, 1.5f);
    auto tip = P (c, 9.0f, MathConstants<float>::pi * 0.75f + v * MathConstants<float>::pi * 1.5f);
    g.setColour (col::red);
    g.drawLine ({ c, tip }, 2.6f);
    g.setColour (juce::Colour (0xffc7d3d6));
    g.setFont (font (10.0f));
    g.drawText (name, juce::Rectangle<float> (0, 52, (float) getWidth(), 13), juce::Justification::centred);
    g.setColour (isMouseButtonDown() || hover ? col::white : col::dim);
    g.setFont (font (10.0f, false));
    g.drawText (param->getCurrentValueAsText(), juce::Rectangle<float> (0, 65, (float) getWidth(), 13), juce::Justification::centred);
}

// ---------------------------------------------------------------- Pill
int Pill::preferredWidth() const
{
    auto t = text ? text() : juce::String();
    return juce::roundToInt (juce::GlyphArrangement::getStringWidth (font (13), label) + 6 + juce::GlyphArrangement::getStringWidth (font (13), t)) + 4;
}
void Pill::paint (juce::Graphics& g)
{
    g.setFont (font (13));
    const auto t = text ? text() : juce::String();
    float lw = juce::GlyphArrangement::getStringWidth (font (13), label);
    float x0 = 0;
    if (centred) x0 = std::round ((getWidth() - (lw + 6 + juce::GlyphArrangement::getStringWidth (font (13), t))) * 0.5f);
    g.setColour (hover ? col::gold2 : col::grey);
    g.drawText (label, juce::Rectangle<float> (x0, 0, lw + 2, (float) getHeight()), juce::Justification::centredLeft);
    g.setColour (valueColour ? valueColour() : col::white);
    g.drawText (t, juce::Rectangle<float> (x0 + lw + 6, 0, getWidth() - lw - 6 - x0, (float) getHeight()), juce::Justification::centredLeft);
}
void Pill::mouseDown (const juce::MouseEvent& e) { dragged = false; lastY = (float) e.position.y; if (onDragStart && onDrag) onDragStart(); }
void Pill::mouseDrag (const juce::MouseEvent& e)
{
    if (! onDrag) return;
    float dy = lastY - e.position.y;
    if (std::abs (e.getDistanceFromDragStartY()) > 2) dragged = true;
    lastY = e.position.y;
    onDrag (dy, e.mods.isShiftDown());
    repaint();
}
void Pill::mouseUp (const juce::MouseEvent& e)
{
    if (onDragEnd && onDrag) onDragEnd();
    if (! dragged && onClick && e.getNumberOfClicks() == 1) onClick();
}

// ---------------------------------------------------------------- Sitar tile: the 22-shruti star
static float centsAngle (double c) { return float (c / 1200.0) * kTwoPi - kHalfPi; }

// the blades drawn on the star: all 22 shrutis for swara scales (the chosen ones long),
// or every degree of a fixed microtonal scale
std::vector<SitarTile::Blade> SitarTile::blades (const tantri::ActiveScale& sc, tantri::TuningBits t) const
{
    std::vector<Blade> b;
    if (sc.editable)
        for (int s = 0; s < 22; ++s)
        {
            int w = tantri::shrutiToSwara (s), deg = -1;
            if (t.in (w) && t.shruti (w) == s)
                for (int d = 0; d < sc.n; ++d) if (sc.swaraClass[d] == w) deg = d;
            b.push_back ({ tantri::shrutiCents (s), deg, s });
        }
    else
        for (int d = 0; d < sc.n; ++d) b.push_back ({ sc.cents[d], d, -1 });
    return b;
}

void SitarTile::paint (juce::Graphics& g)
{
    g.fillAll (col::tile);
    g.setColour (col::edge); g.drawRect (getLocalBounds(), 1);
    const juce::Point<float> c (184, 184);
    const auto sc = proc.activeScale();
    const auto t = proc.getTuning();
    const auto bl = blades (sc, t);
    const int dense = (int) bl.size();
    const float wLong = std::min (0.075f, kTwoPi / std::max (dense, 1) * 0.32f);
    const float wShort = std::min (0.055f, kTwoPi / std::max (dense, 1) * 0.25f);
    for (int pass = 0; pass < 2; ++pass)
        for (int i = 0; i < dense; ++i)
        {
            const auto& b = bl[(size_t) i];
            bool chosen = b.degree >= 0;
            if ((pass == 1) != chosen) continue;
            float a = centsAngle (b.cents);
            float glow = chosen ? act[b.degree] : 0.0f;
            bool hv = hovered == i;
            auto k = kite (c, a, 58, chosen ? 168.0f : 118.0f, chosen ? wLong : wShort);
            int w = b.shruti >= 0 ? tantri::shrutiToSwara (b.shruti) : -1;
            juce::Colour f = chosen ? col::white.interpolatedWith (col::gold2, glow * 0.7f)
                                    : col::white.withAlpha (w >= 0 && t.in (w) ? 0.42f : 0.62f);
            fill (g, k, f, hv ? col::white : col::gold, hv ? 3.4f : (chosen ? (dense > 30 ? 1.4f : 3.0f) : 2.2f));
            if (chosen && dense <= 30) gem (g, P (c, 140, a), (dense > 16 ? 7.0f : 11.0f) + glow * 3.0f, a, glow);
            if (chosen && sc.andolan[b.degree]) gem (g, P (c, 104, a), 6.0f, a + MathConstants<float>::pi, 0);
        }
    fill (g, star (c, 22, 74, 56), col::orange, col::gold, 3);
    fill (g, star (c, 11, 54, 30), col::coral);
    fill (g, ngon (c, 22, 30), col::red);
    for (int i = 0; i < 22; ++i) fill (g, kite (c, i / 22.0f * kTwoPi, 8, 28, 0.12f), col::pink);
    float anyAct = 0; for (int d = 0; d < sc.n; ++d) anyAct = std::max (anyAct, act[d]);
    g.setColour (col::white);
    g.fillEllipse (juce::Rectangle<float> (10 + anyAct * 6, 10 + anyAct * 6).withCentre (c));

    if (hovered >= 0 && hovered < dense)
    {
        const auto& b = bl[(size_t) hovered];
        juce::String name, ratio;
        if (b.shruti >= 0)
        {
            auto& sh = tantri::kShruti[(size_t) b.shruti];
            name = b.degree >= 0 ? juce::String (sc.label[b.degree]) : juce::String (tantri::kSwaraName[tantri::shrutiToSwara (b.shruti)]);
            ratio = juce::String (sh.num) + "/" + juce::String (sh.den);
        }
        else { name = sc.label[b.degree]; ratio = sc.ratio[b.degree]; }
        juce::String txt = name.toUpperCase() + "  " + ratio + "  " + juce::String (b.cents, 1) + " C";
        g.setFont (font (11)); g.setColour (col::white);
        g.drawText (txt, 12, 8, 340, 16, juce::Justification::left);
        g.setColour (col::grey); g.setFont (font (10, false));
        juce::String hint = ! sc.editable ? "FIXED SCALE" : b.shruti == 0 ? "SA IS FIXED"
                          : b.degree >= 0 ? "CLICK TO REMOVE FROM THE SCALE" : "CLICK TO USE THIS SHRUTI";
        g.drawText (hint, 12, 24, 340, 14, juce::Justification::left);
    }
}

int SitarTile::bladeAt (juce::Point<float> p) const
{
    auto d = p - juce::Point<float> (184, 184);
    float r = d.getDistanceFromOrigin();
    if (r < 60 || r > 178) return -1;
    float a = std::atan2 (d.y, d.x);
    auto bl = blades (proc.activeScale(), proc.getTuning());
    int best = -1; float bd = std::min (0.09f, kTwoPi / std::max<int> (1, (int) bl.size()) * 0.5f);
    for (int i = 0; i < (int) bl.size(); ++i)
    {
        float diff = std::remainder (a - centsAngle (bl[(size_t) i].cents), kTwoPi);
        if (std::abs (diff) < bd) { bd = std::abs (diff); best = i; }
    }
    return best;
}
void SitarTile::mouseMove (const juce::MouseEvent& e)
{
    int s = bladeAt (e.position);
    if (s != hovered) { hovered = s; repaint(); }
    setMouseCursor (s >= 0 && proc.activeScale().editable ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}
void SitarTile::mouseDown (const juce::MouseEvent& e)
{
    auto sc = proc.activeScale();
    if (! sc.editable) return;
    int i = bladeAt (e.position);
    if (i < 0) return;
    int s = blades (sc, proc.getTuning())[(size_t) i].shruti;
    if (s <= 0) return;
    auto t = proc.getTuning();
    int w = tantri::shrutiToSwara (s);
    bool chosen = t.in (w) && t.shruti (w) == s;
    if (chosen) t.setIn (w, false);
    else
    {
        t.setIn (w, true);
        t.setPick (w, tantri::kSwaraShruti[w][1] == s && tantri::kSwaraShruti[w][0] != s ? 1 : 0);
    }
    proc.setTuning (t);
    repaint();
    if (auto* p = getParentComponent()) p->repaint();
}

// ---------------------------------------------------------------- Tabla tile: teentaal 16-gon
void TablaTile::paint (juce::Graphics& g)
{
    g.fillAll (col::tile);
    g.setColour (col::edge); g.drawRect (getLocalBounds(), 1);
    const juce::Point<float> c (184, 184);
    const float R = 150;
    fill (g, ngon (c, 16, R + 6 + bayan * 4), col::gold.interpolatedWith (col::gold2, bayan));
    fill (g, ngon (c, 16, R), col::white);
    for (int i = 0; i < 16; ++i)
    {
        float a0 = -kHalfPi + i * MathConstants<float>::pi / 8, a1 = a0 + MathConstants<float>::pi / 8;
        bool vib = i < 4 || (i >= 8 && i < 12);
        fill (g, poly ({ P (c, R, a0), P (c, R, a1), P (c, R * 0.55f, (a0 + a1) / 2) }), vib ? col::red : col::red2);
    }
    fill (g, ngon (c, 3, R * 0.86f), col::white);
    fill (g, star (c, 8, 62, 44), col::orange.interpolatedWith (col::gold2, dayan), dayan > 0.2f ? col::white : col::gold, 2.5f);
    g.setColour (col::tile); g.fillEllipse (juce::Rectangle<float> (72, 72).withCentre (c));
    g.setColour (col::gold); g.drawEllipse (juce::Rectangle<float> (72, 72).withCentre (c), 3);
    g.setColour (juce::Colour (0xff141414)); g.fillEllipse (juce::Rectangle<float> (56, 56).withCentre (c));   // syahi
    if (bol >= 0 && bolA > 0.02f)
    {
        g.setColour (col::white.withAlpha (juce::jlimit (0.0f, 1.0f, bolA * 1.4f)));
        g.setFont (font (bol == tantri::Dhin ? 12.0f : 14.0f));
        g.drawText (tantri::kBolName[bol], juce::Rectangle<float> (56, 20).withCentre (c), juce::Justification::centred);
    }
    for (int i = 0; i < 16; ++i)
    {
        float a = -kHalfPi + i * MathConstants<float>::pi / 8;
        auto q = P (c, R - 12, a);
        bool cur = i == matra;
        float r = (i % 4 ? 7.0f : 9.0f) + (cur ? 3.0f : 0.0f);
        g.setColour (cur ? col::gold2 : col::white); g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (q));
        g.setColour (i % 4 ? col::gold : col::mag); g.drawEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (q), 1.4f);
        juce::String lbl = i == 0 ? "X" : i == 8 ? "0" : juce::String (i + 1);
        g.setColour (col::ink); g.setFont (font (i % 4 ? 8.0f : 9.0f));
        g.drawText (lbl, juce::Rectangle<float> (20, 12).withCentre (q), juce::Justification::centred);
        if (i % 4 == 0) gem (g, P (c, R + 16, a), 9, a, cur ? 1.0f : 0.0f);
    }
}

// ---------------------------------------------------------------- Tanpura tile: four-fold flower
// cycle: the strings are plucked in turn. drone: the chosen strings sound on and on (petals choose:
// one note, or toggle the strings of the chord)
static bool droneMode (TantriProcessor& p) { return (int) getParam (p, "n_mode") == 1; }
static bool droneWants (TantriProcessor& p, int i)
{
    if ((int) getParam (p, "n_voicing") == 0) return (int) getParam (p, "n_note") == i;
    return getParam (p, ("n_dr" + juce::String (i + 1)).toRawUTF8()) > 0.5f;
}
void TanpuraTile::paint (juce::Graphics& g)
{
    g.fillAll (col::tile);
    g.setColour (col::edge); g.drawRect (getLocalBounds(), 1);
    const juce::Point<float> c (184, 184);
    const bool on = getParam (proc, "n_on") > 0.5f;
    const bool drone = droneMode (proc);
    g.setColour (col::gold); g.drawEllipse (juce::Rectangle<float> (320, 320).withCentre (c), drone ? 14.0f : 10.0f);
    float sw = 0; for (float a : act) sw = std::max (sw, a);
    for (int i = 0; i < 32; ++i)
    {
        float a = i / 32.0f * kTwoPi;
        float lift = on ? sw * 4.0f * (0.5f + 0.5f * std::sin (i * 1.7f + sw * 5)) : 0.0f;
        fill (g, kite (c, a, 120, 156 + lift, 0.07f), (i & 1) ? col::brown : col::gold);
    }
    static const char* firstNames[5] = { "PA", "PA", "MA", "NI", "ni" };
    int fc = proc.tanpuraFirst.load();
    const char* names[4] = { firstNames[juce::jlimit (0, 4, fc)], "SA", "SA", "sa" };
    for (int i = 0; i < 4; ++i)
    {
        float a = -kHalfPi + i * kHalfPi;
        float lv = juce::jlimit (0.0f, 1.0f, act[i]);
        const bool chosen = ! drone || droneWants (proc, i);
        fill (g, kite (c, a, 30, 128, 0.42f), chosen ? col::white.interpolatedWith (col::orange, lv) : col::off, col::gold, chosen && drone ? 4.0f : 3.0f);
        fill (g, kite (c, a, 48, 108, 0.22f), chosen ? col::pink.interpolatedWith (col::coral, lv) : col::edge);
        fill (g, kite (c, a + MathConstants<float>::pi / 4, 30, 96, 0.25f), col::white, col::gold, 2.5f);
        auto q = P (c, 84, a);
        g.setColour (! chosen ? col::dim : lv > 0.5f ? col::white : col::ink);
        g.setFont (font (14));
        g.drawText (names[i], juce::Rectangle<float> (40, 18).withCentre (q), juce::Justification::centred);
        if (i == 0 && (int) getParam (proc, "n_first") == 0)
        {
            g.setFont (font (8)); g.setColour (col::dim);
            g.drawText ("AUTO", juce::Rectangle<float> (40, 10).withCentre (q.translated (0, 13)), juce::Justification::centred);
        }
        if (drone && chosen) gem (g, P (c, 140, a), 7.0f, a, lv);
    }
    fill (g, star (c, 12, 30, 18), on ? col::red : col::off, col::gold, 2);
    g.setColour (on ? col::white : col::grey); g.fillEllipse (juce::Rectangle<float> (18, 18).withCentre (c));
    g.setColour (col::grey); g.setFont (font (10));
    if (drone)
    {
        const bool single = (int) getParam (proc, "n_voicing") == 0;
        g.drawText (on ? "CENTRE: STOP" : "CENTRE: DRONE", 12, 8, 150, 14, juce::Justification::left);
        g.drawText (single ? "PETAL: THE NOTE" : "PETALS: THE CHORD", 12, 22, 150, 14, juce::Justification::left);
        g.setColour (col::gold2);
        g.drawText (juce::String (juce::CharPointer_UTF8 (single ? "DRONE \xc2\xb7 SINGLE" : "DRONE \xc2\xb7 CHORD")), 236, 8, 120, 14, juce::Justification::right);
    }
    else
    {
        g.drawText (on ? "CENTRE: STOP" : "CENTRE: PLAY", 12, 8, 200, 14, juce::Justification::left);
        g.drawText ("TOP PETAL: FIRST STRING", 12, 22, 220, 14, juce::Justification::left);
    }
}
void TanpuraTile::mouseDown (const juce::MouseEvent& e)
{
    auto d = e.position - juce::Point<float> (184, 184);
    if (d.getDistanceFromOrigin() < 34) { setParam (proc, "n_on", getParam (proc, "n_on") > 0.5f ? 0.0f : 1.0f); repaint(); return; }
    if (droneMode (proc))
    {
        if (d.getDistanceFromOrigin() > 140) return;
        // petal under the click: 0 top, 1 right, 2 bottom, 3 left
        float a = std::atan2 (d.y, d.x) + kHalfPi;
        int i = (int) std::floor (a / kHalfPi + 0.5f);
        i = ((i % 4) + 4) % 4;
        if ((int) getParam (proc, "n_voicing") == 0) setParam (proc, "n_note", (float) i);
        else
        {
            juce::String id = "n_dr" + juce::String (i + 1);
            setParam (proc, id.toRawUTF8(), getParam (proc, id.toRawUTF8()) > 0.5f ? 0.0f : 1.0f);
        }
        repaint();
        return;
    }
    if (std::abs (d.x) < 34 && d.y < -34 && d.y > -130)
    {
        int f = ((int) getParam (proc, "n_first") + 1) % 5;
        setParam (proc, "n_first", (float) f);
        repaint();
    }
}

// ---------------------------------------------------------------- Space (Tantri Akasha)
namespace
{
    const char* const kSrcName[4] = { "SITAR", "TABLA", "TANPURA", "INPUT" };
    juce::Colour srcColour (int k)
    {
        switch (k) { case 0: return col::orange; case 1: return col::red2; case 2: return col::gold2; default: return col::pink; }
    }
    juce::String upper (const char* s) { return juce::String (s).toUpperCase(); }
    constexpr float kFloorZ = -0.74f;
    constexpr float kViewPerMetre = 1.0f / 10.5f;
    float srcParam (TantriProcessor& p, int k, const char* suffix)
    {
        return p.apvts.getRawParameterValue (juce::String (TantriProcessor::kSrcId[k]) + suffix)->load();
    }
    // a short name for a modulation target
    juce::String targetShort (int t)
    {
        auto names = TantriProcessor::modTargetNames();
        juce::String s = names[juce::jlimit (0, names.size() - 1, t)];
        s = s.replace ("direction", "dir").replace ("distance", "dist").replace ("Tanpura bow rate", "bow rate")
             .replace ("Scene ", "").replace ("Motion speed", "speed").replace ("Near spread", "spread")
             .replace ("Far blur", "blur").replace ("Reflections", "reflect").replace ("Tanpura", "tanp");
        return s.toUpperCase();
    }
}

Space3D::Space3D (TantriProcessor& p) : proc (p) {}

float Space3D::pv (int k, const char* suffix) const { return srcParam (proc, k, suffix); }

juce::Rectangle<float> Space3D::tabRect (int i) const
{
    const float w = 62, total = w * (NumModes + 1);
    return { (getWidth() - total) * 0.5f + i * w, 3, w, 24 };
}

void Space3D::setMode (int m)
{
    mode = m;
    switch (m)
    {
        case Free:  tYaw = freeYaw; tPitch = freePitch; break;
        case Top:   tYaw = juce::MathConstants<float>::pi; tPitch = 1.5f; break;    // from above, front up
        case Front: tYaw = juce::MathConstants<float>::pi; tPitch = 0.12f; break;   // from behind you, looking ahead
        case Side:  tYaw = -kHalfPi; tPitch = 0.12f; break;                         // from your right
        default: break;
    }
    repaint();
}

Space3D::V3 Space3D::worldOf (float x, float y, float d) const
{
    using namespace tantri::field;
    float az = azimuth (x), el = elevation (y);
    SpaceParams sp; sp.yaw = proc.rotYaw.load(); sp.pitch = proc.rotPitch.load();
    rotate (az, el, sp);
    const float r = 0.1f + 0.9f * d;
    return { r * std::cos (el) * std::cos (az), r * std::cos (el) * std::sin (az), r * std::sin (el) };
}

Space3D::Proj Space3D::project (V3 p) const
{
    const auto area = viewArea();
    const float cp = std::cos (camPitch), sp = std::sin (camPitch), cy = std::cos (camYaw), sy = std::sin (camYaw);
    const V3 C { camDist * cp * cy, camDist * cp * sy, camDist * sp };
    const V3 f { -cp * cy, -cp * sy, -sp };
    const V3 r { -sy, cy, 0 };
    const V3 u { -sp * cy, -sp * sy, cp };
    const V3 q { p.x - C.x, p.y - C.y, p.z - C.z };
    const float zs = q.x * f.x + q.y * f.y + q.z * f.z;
    Proj o;
    if (zs < 0.15f) return o;
    const float xs = q.x * r.x + q.y * r.y, ys = q.x * u.x + q.y * u.y + q.z * u.z;
    const float focal = area.getHeight() * 1.3f;
    o.p = { area.getCentreX() + xs / zs * focal, area.getCentreY() + 14.0f - ys / zs * focal };
    o.depth = zs; o.scale = focal / zs; o.ok = true;
    return o;
}

void Space3D::line3D (juce::Graphics& g, V3 a, V3 b, juce::Colour c, float w, bool depthFade) const
{
    auto pa = project (a), pb = project (b);
    if (! pa.ok || ! pb.ok) return;
    float alpha = 1.0f;
    if (depthFade) alpha = juce::jlimit (0.22f, 1.0f, 0.95f - (0.5f * (pa.depth + pb.depth) - camDist) * 0.75f);
    g.setColour (c.withMultipliedAlpha (alpha));
    g.drawLine ({ pa.p, pb.p }, w);
}

juce::Point<float> Space3D::mapPoint (float x, float y) const
{
    auto a = viewArea();
    return { a.getX() + x * a.getWidth(), a.getY() + (1.0f - y) * a.getHeight() };
}

void Space3D::paint (juce::Graphics& g)
{
    g.setColour (col::tile); g.fillRect (getLocalBounds());
    g.setColour (col::edge); g.drawRect (getLocalBounds(), 1);
    auto area = viewArea();
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (area.toNearestInt());
        if (mode == Map) paintMap (g, area); else paint3D (g, area);
    }
    // the camera tabs, centred; status either side (mirrored)
    static const char* tabs[NumModes + 1] = { "FREE", "TOP", "FRONT", "SIDE", "MAP", "ORBIT" };
    for (int i = 0; i <= NumModes; ++i)
    {
        const bool on = i == NumModes ? orbit : i == mode;
        auto r = tabRect (i);
        g.setColour (on ? col::gold2 : col::grey); g.setFont (font (11));
        g.drawText (tabs[i], r, juce::Justification::centred);
        if (on) { g.setColour (col::gold); g.fillRect (r.getCentreX() - 12, r.getBottom() - 3, 24.0f, 2.0f); }
    }
    g.setColour (col::edge); g.drawHorizontalLine (29, 1.0f, getWidth() - 1.0f);
    const int on = proc.orderNow.load();
    static const char* ord[8] = { "", "1ST", "2ND", "3RD", "4TH", "5TH", "6TH", "7TH" };
    juce::String sub = on == 0 ? juce::String ("STEREO PREVIEW")
                     : juce::String (ord[juce::jlimit (1, 7, on)]) + juce::String (juce::CharPointer_UTF8 (" ORDER \xc2\xb7 ")) + juce::String ((on + 1) * (on + 1)) + " CH";
    g.setColour (col::dim); g.setFont (font (10));
    g.drawText (sub, 10, 3, 140, 24, juce::Justification::centredLeft);
    juce::String right;
    if (hoverSrc >= 0)
    {
        const int k = hoverSrc;
        right = upper (kSrcName[k]) + "  " + juce::String (juce::roundToInt (tantri::field::azDegForX (proc.srcX[k].load()))) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0 / "))
              + juce::String (juce::roundToInt (tantri::field::elDegForY (proc.srcY[k].load()))) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0  ")) + juce::String (tantri::field::metres (proc.srcD[k].load()), 1) + " M";
    }
    else right = mode == Map ? juce::String ("THE SPHERE UNROLLED") : "ZOOM " + juce::String (3.3f / camDist, 2) + "X";
    g.drawText (right, getWidth() - 190, 3, 180, 24, juce::Justification::centredRight);
}

void Space3D::paint3D (juce::Graphics& g, juce::Rectangle<float> area)
{
    using namespace tantri::field;
    const int ns = TantriProcessor::kSources;
    // the floor: rings a quarter of the field apart, spokes every 30 degrees
    for (int ring = 1; ring <= 4; ++ring)
    {
        const float rr = ring * 0.25f;
        V3 prev { rr, 0, kFloorZ };
        for (int i = 1; i <= 72; ++i)
        {
            const float a = i / 72.0f * kTwoPi;
            V3 q { rr * std::cos (a), rr * std::sin (a), kFloorZ };
            line3D (g, prev, q, ring == 4 ? col::edge.brighter (0.6f) : col::edge.brighter (0.25f), ring == 4 ? 1.4f : 1.0f);
            prev = q;
        }
    }
    for (int i = 0; i < 12; ++i)
    {
        const float a = i / 12.0f * kTwoPi;
        line3D (g, { 0.08f * std::cos (a), 0.08f * std::sin (a), kFloorZ }, { std::cos (a), std::sin (a), kFloorZ }, col::edge.brighter (0.2f), 0.8f);
    }
    // the sphere of directions (your frame): height rings, meridians every 45 degrees, gold horizon
    for (int e : { -45, -30, 0, 30, 60 })
    {
        const float el = e * juce::MathConstants<float>::pi / 180.0f, z = std::sin (el), rr = std::cos (el);
        V3 prev { rr, 0, z };
        for (int i = 1; i <= 96; ++i)
        {
            const float a = i / 96.0f * kTwoPi;
            V3 q { rr * std::cos (a), rr * std::sin (a), z };
            line3D (g, prev, q, e == 0 ? col::gold : col::grey.withAlpha (e == -45 ? 0.18f : 0.3f), e == 0 ? 2.0f : 1.0f);
            prev = q;
        }
    }
    for (int i = 0; i < 8; ++i)
    {
        const float a = i / 8.0f * kTwoPi;
        V3 prev { std::cos (a) * std::cos (-0.785f), std::sin (a) * std::cos (-0.785f), std::sin (-0.785f) };
        for (int j = 1; j <= 30; ++j)
        {
            const float el = -0.785f + j / 30.0f * (kHalfPi + 0.785f);
            V3 q { std::cos (a) * std::cos (el), std::sin (a) * std::cos (el), std::sin (el) };
            line3D (g, prev, q, col::grey.withAlpha (i % 2 ? 0.14f : 0.24f), 1.0f);
            prev = q;
        }
    }
    // the directions, on the horizon
    struct L { const char* t; float a; };
    for (auto l : { L { "FRONT", 0 }, L { "LEFT", kHalfPi }, L { "BEHIND", juce::MathConstants<float>::pi }, L { "RIGHT", -kHalfPi } })
    {
        auto pr = project ({ 1.16f * std::cos (l.a), 1.16f * std::sin (l.a), 0 });
        if (! pr.ok) continue;
        fill (g, star (pr.p.translated (0, 0), 4, 6, 2.5f), col::gold2);
        g.setColour (col::grey.withMultipliedAlpha (juce::jlimit (0.35f, 1.0f, 1.2f - (pr.depth - camDist) * 0.8f)));
        g.setFont (font (10));
        g.drawText (l.t, juce::Rectangle<float> (70, 12).withCentre (pr.p.translated (0, -12)), juce::Justification::centred);
    }
    if (auto top = project ({ 0, 0, 1.08f }); top.ok)
    {
        g.setColour (col::dim); g.setFont (font (9));
        g.drawText ("ABOVE", juce::Rectangle<float> (60, 12).withCentre (top.p.translated (0, -8)), juce::Justification::centred);
    }

    // the room the reflections come from (and each source's six images in it)
    const float reflect = proc.fieldNow[0].load();
    if (reflect > 0.001f)
    {
        const auto rm = roomFor (proc.fieldNow[2].load());
        const float X = rm.len * kViewPerMetre, Y = rm.wid * kViewPerMetre, Z0 = -rm.floor * kViewPerMetre, Z1 = rm.ceil * kViewPerMetre;
        const juce::Colour rc = col::gold.withAlpha (0.18f + 0.4f * reflect);
        V3 c[8];
        for (int i = 0; i < 8; ++i) c[i] = { (i & 1) ? X : -X, (i & 2) ? Y : -Y, (i & 4) ? Z1 : Z0 };
        const int edges[12][2] = { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } };
        for (auto& e : edges) line3D (g, c[e[0]], c[e[1]], rc, 1.0f);
        for (int k = 0; k < ns; ++k)
        {
            float az = azimuth (proc.srcX[k].load()), el = elevation (proc.srcY[k].load());
            SpaceParams sp; sp.yaw = proc.rotYaw.load(); sp.pitch = proc.rotPitch.load();
            rotate (az, el, sp);
            Image im[6];
            images (az, el, proc.srcD[k].load(), rm, im);
            for (auto& m : im)
            {
                auto pr = project ({ m.x * kViewPerMetre, m.y * kViewPerMetre, m.z * kViewPerMetre });
                if (! pr.ok) continue;
                const float a = juce::jlimit (0.0f, 0.8f, reflect * m.gain * 2.2f) * (0.4f + 0.6f * juce::jlimit (0.0f, 1.0f, act[k] * 4.0f));
                fill (g, ngon (pr.p, 6, std::max (2.5f, 0.025f * pr.scale)), srcColour (k).withAlpha (a));
            }
        }
    }

    // you: a head with a nose to the front and two ears
    {
        auto h = project ({ 0, 0, 0 });
        if (h.ok)
        {
            const float r = std::max (6.0f, 0.065f * h.scale);
            if (auto sh = project ({ 0, 0, kFloorZ }); sh.ok)
            {
                g.setColour (juce::Colours::black.withAlpha (0.35f));
                g.fillEllipse (juce::Rectangle<float> (r * 2.2f, r * 0.9f).withCentre (sh.p));
                g.setColour (col::edge.brighter (0.5f)); g.drawLine ({ h.p, sh.p }, 1.0f);
            }
            auto nose = project ({ 0.12f, 0, 0 }), el = project ({ 0, 0.075f, 0 }), er = project ({ 0, -0.075f, 0 });
            if (nose.ok) fill (g, poly ({ nose.p, project ({ 0.03f, 0.035f, 0 }).p, project ({ 0.03f, -0.035f, 0 }).p }), col::gold2);
            for (auto* e : { &el, &er }) if (e->ok && e->depth > h.depth) { g.setColour (col::pink); g.fillEllipse (juce::Rectangle<float> (r * 0.5f, r * 0.5f).withCentre (e->p)); }
            g.setColour (col::white); g.fillEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (h.p));
            g.setColour (col::gold); g.drawEllipse (juce::Rectangle<float> (r * 2, r * 2).withCentre (h.p), 1.5f);
            for (auto* e : { &el, &er }) if (e->ok && e->depth <= h.depth) { g.setColour (col::pink); g.fillEllipse (juce::Rectangle<float> (r * 0.5f, r * 0.5f).withCentre (e->p)); }
        }
    }

    // the sources, far ones first
    struct S { int k; V3 now, home; Proj pn; };
    std::vector<S> srcs;
    for (int k = 0; k < ns; ++k)
    {
        S s { k, worldOf (proc.srcX[k].load(), proc.srcY[k].load(), proc.srcD[k].load()),
              worldOf (xForAzDeg (pv (k, "_az")), yForElDeg (pv (k, "_el")), pv (k, "_dist")), {} };
        s.pn = project (s.now);
        srcs.push_back (s);
    }
    std::sort (srcs.begin(), srcs.end(), [] (const S& a, const S& b) { return a.pn.depth > b.pn.depth; });
    for (auto& s : srcs)
    {
        const int k = s.k;
        const auto colr = srcColour (k);
        // trail
        auto& tr = trails[(size_t) k];
        for (int j = 1; j < tr.n; ++j)
        {
            const int i0 = (tr.w - j + 96) % 96, i1 = (tr.w - j - 1 + 96) % 96;
            const float a = 1.0f - j / (float) tr.n;
            line3D (g, tr.p[(size_t) i0], tr.p[(size_t) i1], colr.withAlpha (0.75f * a), 1.0f + 2.0f * a, false);
        }
        // ray from you, drop line and shadow on the floor
        line3D (g, { 0, 0, 0 }, s.now, colr.withAlpha (0.22f), 1.0f);
        const V3 foot { s.now.x, s.now.y, kFloorZ };
        line3D (g, s.now, foot, colr.withAlpha (0.4f), 1.0f, false);
        if (auto pf = project (foot); pf.ok)
        {
            const float r = std::max (3.0f, 0.05f * pf.scale);
            g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillEllipse (juce::Rectangle<float> (r * 2.4f, r).withCentre (pf.p));
            g.setColour (colr.withAlpha (0.5f)); g.drawEllipse (juce::Rectangle<float> (r * 2.4f, r).withCentre (pf.p), 1.0f);
        }
        // home, and the line to where the motion has taken it
        if (auto ph = project (s.home); ph.ok)
        {
            fill (g, ngon (ph.p, 8, 7), col::tile, col::gold, 1.4f);
            if (ph.p.getDistanceFrom (s.pn.p) > 4) line3D (g, s.home, s.now, colr.withAlpha (0.45f), 1.0f, false);
            screenHome[(size_t) k] = ph.p;
        }
        if (! s.pn.ok) continue;
        screenNow[(size_t) k] = s.pn.p;
        const float r = juce::jlimit (7.0f, 30.0f, 0.085f * s.pn.scale);
        // size: a translucent halo as wide as the source is
        const float size = proc.srcSize[k].load();
        if (size > 0.01f)
        {
            const float hr = r * (1.0f + 3.5f * size);
            g.setColour (colr.withAlpha (0.12f)); g.fillEllipse (juce::Rectangle<float> (hr * 2, hr * 2).withCentre (s.pn.p));
            g.setColour (colr.withAlpha (0.4f)); g.drawEllipse (juce::Rectangle<float> (hr * 2, hr * 2).withCentre (s.pn.p), 1.0f);
        }
        const float glow = juce::jlimit (0.0f, 1.0f, act[k] * 4.0f);
        if (glow > 0.02f)
        {
            g.setColour (col::gold2.withAlpha (0.3f * glow));
            g.fillEllipse (juce::Rectangle<float> (r * 3.4f, r * 3.4f).withCentre (s.pn.p));
        }
        fill (g, star (s.pn.p, 8, r, r * 0.55f, -kHalfPi + frame * 0.02f * (k + 1)), colr, dragging == k || hoverSrc == k ? col::white : col::gold, 2.0f);
        g.setColour (col::white); g.fillEllipse (juce::Rectangle<float> (r * 0.5f, r * 0.5f).withCentre (s.pn.p));
        g.setColour (col::white); g.setFont (font (10));
        g.drawText (kSrcName[k], juce::Rectangle<float> (90, 12).withCentre (s.pn.p.translated (0, r + 9)), juce::Justification::centred);
    }
    g.setColour (col::dim); g.setFont (font (9, false));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("DRAG SPACE: TURN \xc2\xb7 DRAG SOURCE: PLACE \xc2\xb7 SHIFT: DISTANCE \xc2\xb7 WHEEL: ZOOM / DISTANCE \xc2\xb7 DOUBLE-CLICK: MOTION / RESET")),
                area.withTrimmedTop (area.getHeight() - 16), juce::Justification::centred);
}

void Space3D::paintMap (juce::Graphics& g, juce::Rectangle<float> area)
{
    using namespace tantri::field;
    const int ns = TantriProcessor::kSources;
    const float W = area.getWidth();
    for (int e = -45; e < 90; e += 15)
    {
        const float y0 = mapPoint (0, yForElDeg ((float) e + 15)).y, y1 = mapPoint (0, yForElDeg ((float) e)).y;
        g.setColour (col::bg.interpolatedWith (col::tile, 0.55f + 0.45f * (e + 45) / 135.0f));
        g.fillRect (area.getX(), y0, W, y1 - y0 - 1.0f);
    }
    const float hy = mapPoint (0, yForElDeg (0)).y;
    g.setColour (col::gold); g.fillRect (area.getX(), hy - 1.5f, W, 3.0f);
    static const char* dirs[9] = { "BEHIND", "", "RIGHT", "", "FRONT", "", "LEFT", "", "BEHIND" };
    for (int i = 0; i <= 8; ++i)
    {
        const float x = area.getX() + i / 8.0f * W;
        g.setColour (col::off); g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        fill (g, star ({ x, hy }, 4, i % 2 ? 6.0f : 9.0f, 3.0f), i % 2 ? col::gold : col::white, col::gold, 1.0f);
        if (dirs[i][0] != 0)
        {
            g.setColour (col::grey); g.setFont (font (10));
            g.drawText (dirs[i], juce::Rectangle<float> (70, 14).withCentre ({ juce::jlimit (area.getX() + 36, area.getRight() - 36, x), area.getY() + 12 }), juce::Justification::centred);
        }
    }
    g.setColour (col::dim); g.setFont (font (9, false));
    g.drawText ("OVERHEAD", (int) area.getX() + 8, (int) area.getY() + 24, 80, 12, juce::Justification::left);
    g.drawText (juce::String (juce::CharPointer_UTF8 ("45\xc2\xb0 BELOW")), (int) area.getX() + 8, (int) area.getBottom() - 15, 80, 12, juce::Justification::left);
    for (int k = 0; k < ns; ++k)
    {
        const auto colr = srcColour (k);
        auto& tr = trails[(size_t) k];
        for (int j = 0; j < tr.n; ++j)
        {
            const int idx = (tr.w - 1 - j + 96) % 96;
            const float a = 1.0f - j / 96.0f;
            if (j > 0 && std::abs (mapPts[(size_t) k][(size_t) idx].x - mapPts[(size_t) k][(size_t) ((idx + 1) % 96)].x) > 0.5f) break;
            g.setColour (colr.withAlpha (0.55f * a));
            g.fillEllipse (juce::Rectangle<float> (3.0f + 2.0f * a, 3.0f + 2.0f * a).withCentre (mapPoint (mapPts[(size_t) k][(size_t) idx].x, mapPts[(size_t) k][(size_t) idx].y)));
        }
        const auto home = mapPoint (xForAzDeg (pv (k, "_az")), yForElDeg (pv (k, "_el")));
        const auto now = mapPoint (proc.srcX[k].load(), proc.srcY[k].load());
        screenNow[(size_t) k] = now; screenHome[(size_t) k] = home;
        const float d = proc.srcD[k].load();
        fill (g, ngon (home, 8, 9), col::tile, col::gold, 1.5f);
        if (home.getDistanceFrom (now) > 3 && home.getDistanceFrom (now) < W * 0.5f) { g.setColour (colr.withAlpha (0.5f)); g.drawLine ({ home, now }, 1.2f); }
        const float r = 10.0f + 16.0f * (1.0f - d);
        const float glow = juce::jlimit (0.0f, 1.0f, act[k] * 4.0f);
        if (glow > 0.02f) { g.setColour (col::gold2.withAlpha (0.3f * glow)); g.fillEllipse (juce::Rectangle<float> (r * 3.2f, r * 3.2f).withCentre (now)); }
        fill (g, star (now, 8, r, r * 0.55f), colr, dragging == k ? col::white : col::gold, 2.0f);
        g.setColour (col::white); g.fillEllipse (juce::Rectangle<float> (r * 0.5f, r * 0.5f).withCentre (now));
        g.setFont (font (10));
        g.drawText (kSrcName[k], juce::Rectangle<float> (90, 12).withCentre (now.translated (0, r + 9)), juce::Justification::centred);
    }
}

int Space3D::sourceAt (juce::Point<float> p) const
{
    int best = -1; float bd = 22;
    for (int k = 0; k < TantriProcessor::kSources; ++k)
        for (auto q : { screenNow[(size_t) k], screenHome[(size_t) k] })
            if (float d = q.getDistanceFrom (p); d < bd) { bd = d; best = k; }
    return best;
}

void Space3D::mouseMove (const juce::MouseEvent& e)
{
    const int k = viewArea().contains (e.position) ? sourceAt (e.position) : -1;
    if (k != hoverSrc) { hoverSrc = k; repaint(); }
    setMouseCursor (k >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void Space3D::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i <= NumModes; ++i)
        if (tabRect (i).contains (e.position))
        {
            if (i == NumModes) orbit = ! orbit;
            else setMode (i);
            repaint(); return;
        }
    lastMouse = e.position;
    dragging = viewArea().contains (e.position) ? sourceAt (e.position) : -1;
    distDrag = e.mods.isShiftDown();
    if (dragging >= 0)
    {
        for (auto* sfx : { "_az", "_el", "_dist" }) proc.apvts.getParameter (juce::String (TantriProcessor::kSrcId[dragging]) + sfx)->beginChangeGesture();
        if (mode == Map && ! distDrag) mouseDrag (e);
    }
    else orbiting = mode != Map;
}

void Space3D::dragSourceTo (juce::Point<float> from, juce::Point<float> to)
{
    using namespace tantri::field;
    const int k = dragging;
    const float azH = pv (k, "_az"), elH = pv (k, "_el");
    const float ox = proc.srcX[k].load() - xForAzDeg (azH), oy = proc.srcY[k].load() - yForElDeg (elH), d = proc.srcD[k].load();
    auto f = [&] (float a, float el) { return project (worldOf (xForAzDeg (a) + ox, yForElDeg (el) + oy, d)).p; };
    const auto p0 = f (azH, elH), pa = f (azH + 1, elH), pe = f (azH, elH + 1);
    const float j11 = pa.x - p0.x, j21 = pa.y - p0.y, j12 = pe.x - p0.x, j22 = pe.y - p0.y;
    const auto dm = to - from;
    const float t = j11 * j11 + j21 * j21 + j12 * j12 + j22 * j22, lam = 0.04f * t + 0.01f;
    const float a11 = j11 * j11 + j21 * j21 + lam, a12 = j11 * j12 + j21 * j22, a22 = j12 * j12 + j22 * j22 + lam;
    const float b1 = j11 * dm.x + j21 * dm.y, b2 = j12 * dm.x + j22 * dm.y;
    const float det = a11 * a22 - a12 * a12;
    if (std::abs (det) < 1e-9f) return;
    const float daz = juce::jlimit (-25.0f, 25.0f, (a22 * b1 - a12 * b2) / det), del = juce::jlimit (-25.0f, 25.0f, (a11 * b2 - a12 * b1) / det);
    float az = azH + daz; az -= 360.0f * std::round (az / 360.0f);
    const float el = juce::jlimit (kElLow, kElHigh, elH + del);
    juce::String pre (TantriProcessor::kSrcId[k]);
    auto* pA = proc.apvts.getParameter (pre + "_az");
    auto* pE = proc.apvts.getParameter (pre + "_el");
    pA->setValueNotifyingHost (pA->convertTo0to1 (az));
    pE->setValueNotifyingHost (pE->convertTo0to1 (el));
}

void Space3D::mouseDrag (const juce::MouseEvent& e)
{
    using namespace tantri::field;
    const auto dm = e.position - lastMouse;
    if (dragging >= 0)
    {
        juce::String pre (TantriProcessor::kSrcId[dragging]);
        if (distDrag)
        {
            auto* p = proc.apvts.getParameter (pre + "_dist");
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, pv (dragging, "_dist") + dm.y * 0.004f));
        }
        else if (mode == Map)
        {
            auto a = viewArea();
            const float x = juce::jlimit (0.0f, 1.0f, (e.position.x - a.getX()) / a.getWidth()), y = juce::jlimit (0.0f, 1.0f, 1.0f - (e.position.y - a.getY()) / a.getHeight());
            auto* az = proc.apvts.getParameter (pre + "_az");
            auto* el = proc.apvts.getParameter (pre + "_el");
            az->setValueNotifyingHost (az->convertTo0to1 (azDegForX (x)));
            el->setValueNotifyingHost (el->convertTo0to1 (elDegForY (y)));
        }
        else dragSourceTo (lastMouse, e.position);
    }
    else if (orbiting)
    {
        camYaw = tYaw = camYaw - dm.x * 0.008f;
        camPitch = tPitch = juce::jlimit (-0.25f, 1.55f, camPitch + dm.y * 0.006f);
        freeYaw = camYaw; freePitch = camPitch;
        mode = Free;
    }
    lastMouse = e.position;
    repaint();
}

void Space3D::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
        for (auto* sfx : { "_az", "_el", "_dist" }) proc.apvts.getParameter (juce::String (TantriProcessor::kSrcId[dragging]) + sfx)->endChangeGesture();
    dragging = -1; orbiting = false;
    repaint();
}

void Space3D::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (int k = sourceAt (e.position); k >= 0 && viewArea().contains (e.position))
    {
        if (onMotionMenu) onMotionMenu (k, e.getPosition());
        return;
    }
    if (mode != Map) { freeYaw = 3.6f; freePitch = 0.42f; tDist = 3.3f; setMode (Free); }
}

void Space3D::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    if (int k = sourceAt (e.position); k >= 0)
    {
        auto* prm = proc.apvts.getParameter (juce::String (TantriProcessor::kSrcId[k]) + "_dist");
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, pv (k, "_dist") - w.deltaY * 0.15f));
        prm->endChangeGesture();
        return;
    }
    if (mode != Map) tDist = juce::jlimit (1.7f, 7.0f, tDist * (1.0f - w.deltaY * 0.5f));
}

void Space3D::tick()
{
    frame += 1.0f;
    if (orbit && mode != Map) { tYaw += 0.005f; if (mode == Free) freeYaw = tYaw; }
    camYaw += std::remainder (tYaw - camYaw, kTwoPi) * 0.2f;
    camPitch += (tPitch - camPitch) * 0.2f;
    camDist += (tDist - camDist) * 0.2f;
    for (int k = 0; k < TantriProcessor::kSources; ++k)
    {
        act[k] = std::max (proc.srcAct[k].load(), act[k] * 0.85f);
        auto& tr = trails[(size_t) k];
        tr.p[(size_t) tr.w] = worldOf (proc.srcX[k].load(), proc.srcY[k].load(), proc.srcD[k].load());
        mapPts[(size_t) k][(size_t) tr.w] = { proc.srcX[k].load(), proc.srcY[k].load() };
        tr.w = (tr.w + 1) % 96; tr.n = std::min (96, tr.n + 1);
    }
    repaint();
}

// ---------------------------------------------------------------- a modulator
ModRow::ModRow (TantriProcessor& p, int i) : proc (p), index (i), id ("m" + juce::String (i + 1))
{
    using namespace tantri::field;
    shapePill.label = juce::String (i + 1);
    shapePill.text = [this] { return upper (kLfoShape[juce::jlimit (0, lfo::Count - 1, (int) getParam (proc, (id + "_shape").toRawUTF8()))]); };
    shapePill.valueColour = [] { return col::gold2; };
    shapePill.onClick = [this]
    {
        juce::PopupMenu m;
        const int cur = (int) getParam (proc, (id + "_shape").toRawUTF8());
        for (int s = 0; s < lfo::Count; ++s) m.addItem (s + 1, kLfoShape[s], true, s == cur);
        juce::Component::SafePointer<ModRow> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&shapePill), [safe] (int r)
        { if (safe != nullptr && r > 0) { setParam (safe->proc, (safe->id + "_shape").toRawUTF8(), (float) (r - 1)); safe->repaint(); } });
    };
    targetPill.label = juce::String (juce::CharPointer_UTF8 ("\xe2\x86\x92"));
    targetPill.text = [this] { return targetShort ((int) getParam (proc, (id + "_target").toRawUTF8())); };
    targetPill.valueColour = [this] { return (int) getParam (proc, (id + "_target").toRawUTF8()) == 0 ? col::dim : col::white; };
    targetPill.onClick = [this]
    {
        juce::PopupMenu m;
        const int cur = (int) getParam (proc, (id + "_target").toRawUTF8());
        static const char* nm[4] = { "Sitar", "Tabla", "Tanpura", "Input" };
        m.addItem (1, "None", true, cur == 0);
        for (int k = 0; k < TantriProcessor::kSources; ++k)
        {
            juce::PopupMenu sub;
            for (int j = 0; j < mt::PerSource; ++j)
            {
                const int t = 1 + k * mt::PerSource + j;
                sub.addItem (t + 1, juce::String (kModSrcPart[j]).substring (0, 1).toUpperCase() + juce::String (kModSrcPart[j]).substring (1), true, t == cur);
            }
            m.addSubMenu (nm[k], sub, true, nullptr, cur >= 1 + k * mt::PerSource && cur < 1 + (k + 1) * mt::PerSource);
        }
        juce::PopupMenu field;
        const int gb = 1 + TantriProcessor::kSources * mt::PerSource;
        for (int j = 0; j < mt::Global; ++j) field.addItem (gb + j + 1, kModGlobal[j], true, gb + j == cur);
        m.addSubMenu ("Field", field, true, nullptr, cur >= gb);
        juce::Component::SafePointer<ModRow> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&targetPill), [safe] (int r)
        { if (safe != nullptr && r > 0) { setParam (safe->proc, (safe->id + "_target").toRawUTF8(), (float) (r - 1)); safe->repaint(); } });
    };
    addAndMakeVisible (shapePill); addAndMakeVisible (targetPill);
    rate = std::make_unique<StarKnob> (proc.apvts.getParameter (id + "_rate"), "RATE");
    depth = std::make_unique<StarKnob> (proc.apvts.getParameter (id + "_depth"), "DEPTH");
    for (auto* k : { rate.get(), depth.get() }) addAndMakeVisible (*k);
    attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id + "_rate", *rate));
    attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id + "_depth", *depth));
}

void ModRow::resized()
{
    shapePill.setBounds (0, 0, 96, 18);
    targetPill.setBounds (100, 0, getWidth() - 100, 18);
    scope = { 2, 24, 116, 58 };
    rate->setBounds (122, 18, 61, 82);
    depth->setBounds (183, 18, 61, 82);
}

void ModRow::paint (juce::Graphics& g)
{
    using namespace tantri::field;
    g.setColour (col::bg.interpolatedWith (col::tile, 0.5f)); g.fillRect (scope);
    g.setColour (col::edge); g.drawRect (scope, 1.0f);
    g.drawHorizontalLine ((int) scope.getCentreY(), scope.getX() + 1, scope.getRight() - 1);
    const int shape = (int) getParam (proc, (id + "_shape").toRawUTF8());
    const float dep = getParam (proc, (id + "_depth").toRawUTF8());
    const double ph = proc.modPhase[index].load();
    const float seed = 0.37f + 0.17f * (float) index;
    const bool live = (int) getParam (proc, (id + "_target").toRawUTF8()) > 0;
    juce::Path full, scaled;
    const int N = 116;
    const float cy = scope.getCentreY(), hh = scope.getHeight() * 0.42f;
    for (int i = 0; i <= N; ++i)
    {
        const double p = ph - 2.0 + 2.0 * i / N;
        const float v = lfoValue (shape, p, seed);
        const float x = scope.getX() + i * scope.getWidth() / N;
        if (i == 0) { full.startNewSubPath (x, cy - v * hh); scaled.startNewSubPath (x, cy - v * dep * hh); }
        else { full.lineTo (x, cy - v * hh); scaled.lineTo (x, cy - v * dep * hh); }
    }
    g.setColour (col::grey.withAlpha (0.3f)); g.strokePath (full, juce::PathStrokeType (1.0f));
    g.setColour (live ? col::gold2 : col::dim); g.strokePath (scaled, juce::PathStrokeType (2.0f));
    const float v = proc.modOut[index].load() * dep;
    fill (g, star ({ scope.getRight() - 3, cy - v * hh }, 4, 6, 2.5f), live ? col::white : col::dim);
}

// ---------------------------------------------------------------- the space page
SpacePanel::SpacePanel (TantriProcessor& p) : proc (p), view (p)
{
    using namespace tantri::field;
    addAndMakeVisible (view);
    view.setBounds (kColW + 12, 0, kW - 2 * (kColW + 12), kTopH);
    view.onMotionMenu = [this] (int k, juce::Point<int> at) { motionMenu (k, &view, at); };

    auto addKnob = [this] (const juce::String& id, const char* label, int x, int y, int w = 61)
    {
        auto* prm = proc.apvts.getParameter (id);
        if (prm == nullptr) return;
        auto knob = std::make_unique<StarKnob> (prm, label);
        knob->setBounds (x, y, w, 82);
        addAndMakeVisible (*knob);
        attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, *knob));
        knobs.push_back (std::move (knob));
    };
    // the field and its effects, four to a row, rows centred
    struct K { juce::String id; const char* label; };
    std::vector<K> field { { "order", "ORDER" }, { "rolloff", "ROLLOFF" }, { "air", "AIR" }, { "spread", "SPREAD" },
                           { "blur", "BLUR" }, { "near", "NEAR" }, { "mspeed", "SPEED" }, { "spin", "SPIN" },
                           { "yaw", "TURN" }, { "pitch", "TILT" }, { "doppler", "DOPPLER" }, { "reflect", "REFLECT" },
                           { "prox", "PROXIMITY" }, { "hcue", "HEIGHT" } };
    if (TantriProcessor::kFx) { field.push_back ({ "i_level", "INPUT" }); field.push_back ({ "i_symp", "SYMPATHY" }); }
    const int nf = (int) field.size();
    for (int i = 0; i < nf; ++i)
    {
        const int row = i / 4, inRow = std::min (4, nf - row * 4);
        addKnob (field[(size_t) i].id, field[(size_t) i].label, (kColW - inRow * 61) / 2 + (i % 4) * 61, 30 + row * 100 + 9);
    }
    // the modulators
    for (int m = 0; m < TantriProcessor::kMods; ++m)
    {
        auto row = std::make_unique<ModRow> (proc, m);
        row->setBounds (kW - kColW, 30 + m * 100, kColW, 100);
        addAndMakeVisible (*row);
        mods.push_back (std::move (row));
    }
    // each source: motion, then direction, height, distance, size, motion rate
    const int ns = TantriProcessor::kSources;
    const int colW = kW / ns;
    const int kw = std::min (61, (colW - 8) / 5);
    for (int k = 0; k < ns; ++k)
    {
        juce::String pre (TantriProcessor::kSrcId[k]);
        const int x0 = k * colW + (colW - 5 * kw) / 2;
        int j = 0;
        for (auto [sfx, lbl] : { std::pair<const char*, const char*> { "_az", "DIR" }, { "_el", "HEIGHT" }, { "_dist", "DIST" }, { "_size", "SIZE" }, { "_rate", "RATE" } })
            addKnob (pre + sfx, lbl, x0 + kw * j++, 514, kw);
        auto pill = std::make_unique<Pill> ("MOTION");
        pill->text = [this, k] { return upper (kMotionName[juce::jlimit (0, motion::Count - 1, (int) pv (k, "_motion"))]); };
        pill->valueColour = [k] { return srcColour (k); };
        auto* raw = pill.get();
        pill->onClick = [this, k, raw] { motionMenu (k, raw); };
        addAndMakeVisible (*pill);
        motionPills.push_back (std::move (pill));
    }
    for (auto& kn : knobs) kn->onValueChange = [this] { repaint(); };
    tick();
}
SpacePanel::~SpacePanel() = default;

float SpacePanel::pv (int k, const char* suffix) const { return srcParam (proc, k, suffix); }

void SpacePanel::paint (juce::Graphics& g)
{
    using namespace tantri::field;
    // column titles, centred over their columns
    g.setColour (col::white); g.setFont (font (19));
    g.drawText ("FIELD", 0, 0, kColW, 26, juce::Justification::centred);
    g.drawText ("MODULATORS", kW - kColW, 0, kColW, 26, juce::Justification::centred);
    g.setColour (col::edge);
    g.drawHorizontalLine (27, 30.0f, kColW - 30.0f);
    g.drawHorizontalLine (27, kW - kColW + 30.0f, kW - 30.0f);
    // the sources' columns
    const int ns = TantriProcessor::kSources;
    const int colW = kW / ns;
    for (int k = 0; k < ns; ++k)
    {
        const int x0 = k * colW;
        g.setColour (srcColour (k)); g.setFont (font (19));
        g.drawText (kSrcName[k], x0, 444, colW, 24, juce::Justification::centred);
        g.setColour (col::grey); g.setFont (font (10));
        const float dd = proc.srcD[k].load();
        g.drawText (juce::String (juce::roundToInt (azDegForX (proc.srcX[k].load()))) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0 \xc2\xb7 "))
                    + juce::String (juce::roundToInt (elDegForY (proc.srcY[k].load()))) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0 \xc2\xb7 "))
                    + juce::String (metres (dd), 1) + " M", x0, 467, colW, 14, juce::Justification::centred);
        if (k > 0) { g.setColour (col::edge); g.drawVerticalLine (x0, 452.0f, 596.0f); }
    }
}

void SpacePanel::motionMenu (int k, juce::Component* target, juce::Point<int> at)
{
    using namespace tantri::field;
    juce::PopupMenu m;
    const int cur = (int) pv (k, "_motion");
    static const char* hint[motion::Count] = { "stays where it is", "once round you, 24 s", "sways 40\xc2\xb0 either side, 8 s",
        "up overhead and back, 16 s", "wanders slowly", "round you while it climbs, 40 s", "a figure of eight, 10 s",
        "over your head to the other side, 14 s", "comes close and goes back, 12 s", "jumps nearby every 3 s",
        "trembles, like wings", "one flock with every source on Boids", "its own stretch of the Lorenz attractor" };
    for (int i = 0; i < motion::Count; ++i)
        m.addItem (i + 1, juce::String (kMotionName[i]) + "   " + juce::String (juce::CharPointer_UTF8 (hint[i])), true, i == cur);
    juce::Component::SafePointer<SpacePanel> safe (this);
    auto opts = juce::PopupMenu::Options().withTargetComponent (target);
    if (at != juce::Point<int>()) opts = opts.withTargetScreenArea (target->localAreaToGlobal (juce::Rectangle<int> (at.x, at.y, 1, 1)));
    m.showMenuAsync (opts, [safe, k] (int r)
    {
        if (safe == nullptr || r < 1) return;
        setParam (safe->proc, (juce::String (TantriProcessor::kSrcId[k]) + "_motion").toRawUTF8(), (float) (r - 1));
        safe->repaint();
    });
}

void SpacePanel::tick()
{
    view.tick();
    for (auto& m : mods) m->tick();
    const int ns = TantriProcessor::kSources;
    const int colW = kW / ns;
    for (int k = 0; k < ns; ++k)
    {
        auto& p = *motionPills[(size_t) k];
        const int w = p.preferredWidth() + 6;
        p.setBounds (k * colW + (colW - w) / 2, 486, w, 22);
        p.repaint();
    }
    repaint (0, 440, kW, 44);
}

// ---------------------------------------------------------------- raga strip (3 saptaks)
std::vector<RagaStrip::Slot> RagaStrip::slots() const
{
    const auto sc = proc.activeScale();
    const int maxSlots = std::max (1, (getWidth() - 48) / 34);
    int octs = juce::jlimit (1, 3, maxSlots / std::max (1, sc.n));
    int stride = std::max (1, (sc.n + maxSlots - 1) / maxSlots);   // very dense scales: every k-th degree
    std::vector<Slot> s;
    int o0 = octs == 3 ? -1 : 0;
    for (int o = o0; o < o0 + octs; ++o)
        for (int d = 0; d < sc.n; d += stride) s.push_back ({ d, o, 0 });
    float step = std::min (54.0f, (getWidth() - 72.0f) / std::max<size_t> (1, s.size() - 1));
    const float x0 = (getWidth() - step * (float) (s.size() - 1)) * 0.5f;   // centred
    for (size_t i = 0; i < s.size(); ++i) s[i].x = x0 + step * i;
    return s;
}
void RagaStrip::paint (juce::Graphics& g)
{
    g.fillAll (col::tile);
    g.setColour (col::edge); g.drawRect (getLocalBounds(), 1);
    const auto sc = proc.activeScale();
    auto sl = slots();
    float spacing = sl.size() > 1 ? sl[1].x - sl[0].x : 54;
    for (auto& s : sl)
    {
        bool sa = s.swara == 0;
        float glow = act[s.swara];
        gem (g, { s.x, 24 }, (sa ? 13.0f : 10.0f) * std::min (1.0f, spacing / 40.0f), -kHalfPi, glow);
        juce::String lbl = juce::String (sc.label[s.swara]).toUpperCase();
        if (spacing >= 46) lbl << " " << sc.ratio[s.swara];
        g.setFont (font (spacing >= 30 ? 10.0f : 8.0f)); g.setColour (sa ? col::white : col::grey);
        g.drawText (lbl, juce::Rectangle<float> (std::max (30.0f, spacing + 6), 12).withCentre ({ s.x, 50 }), juce::Justification::centred);
        // Bhatkhande octave dots: below = mandra, above = taar
        g.setColour (col::gold2);
        if (s.oct < 0) g.fillEllipse (s.x - 1.5f, 57, 3, 3);
        if (s.oct > 0) g.fillEllipse (s.x - 1.5f, 2.5f, 3, 3);
    }
}
void RagaStrip::mouseDown (const juce::MouseEvent& e)
{
    const auto sc = proc.activeScale();
    auto sl = slots();
    float half = sl.size() > 1 ? (sl[1].x - sl[0].x) * 0.5f : 18;
    for (auto& s : sl)
        if (std::abs (e.position.x - s.x) < half)
        {
            bool steps = (int) getParam (proc, "map") == tantri::mapSteps;
            heldNote = proc.saKey() + (steps ? s.oct * sc.n + s.swara : juce::roundToInt ((s.oct * 1200.0 + sc.cents[s.swara]) / 100.0));
            heldNote = juce::jlimit (0, 127, heldNote);
            int ch = (int) getParam (proc, "sitarCh"); if (ch == 0) ch = 1;
            proc.keyboard.noteOn (ch, heldNote, 0.8f);
            return;
        }
}
void RagaStrip::mouseUp (const juce::MouseEvent&)
{
    if (heldNote >= 0)
    {
        int ch = (int) getParam (proc, "sitarCh"); if (ch == 0) ch = 1;
        proc.keyboard.noteOff (ch, heldNote, 0.0f);
        heldNote = -1;
    }
}

// ---------------------------------------------------------------- Canvas (1200 x 780)
Canvas::Canvas (TantriProcessor& p) : proc (p), sitarTile (p), tablaTile (p), tanpuraTile (p), strip (p)
{
    addAndMakeVisible (sitarTile); addAndMakeVisible (tablaTile); addAndMakeVisible (tanpuraTile); addAndMakeVisible (strip);

    struct K { const char* id; const char* label; int group; };
    static const K ks[] = {
        { "s_mizrab", "MIZRAB", 0 }, { "s_jawari", "JAWARI", 0 }, { "s_meend", "MEEND", 0 }, { "s_taraf", "TARAF", 0 },
        { "s_chikari", "CHIKARI", 0 }, { "s_gourd", "GOURD", 0 }, { "s_gamak", "GAMAK", 0 }, { "s_decay", "DECAY", 0 },
        { "s_pluck", "PLUCK", 0 }, { "s_release", "DAMP", 0 }, { "s_room", "ROOM", 0 }, { "s_level", "LEVEL", 0 },
        { "t_dayan", "DAYAN", 1 }, { "t_bayan", "BAYAN", 1 }, { "t_syahi", "SYAHI", 1 }, { "t_glide", "GLIDE", 1 },
        { "t_press", "PRESS", 1 }, { "t_ring", "RING", 1 }, { "t_snap", "SNAP", 1 }, { "t_room", "ROOM", 1 }, { "t_level", "LEVEL", 1 },
        { "n_cycle", "CYCLE", 2 }, { "n_jawari", "JAWARI", 2 }, { "n_first", "1ST STR", 2 }, { "n_octave", "REGISTER", 2 },
        { "n_human", "HUMAN", 2 }, { "n_width", "WIDTH", 2 }, { "n_room", "ROOM", 2 }, { "n_level", "LEVEL", 2 },
        { "n_mode", "MODE", 2 }, { "n_voicing", "VOICING", 2 }, { "n_bow", "BOW RATE", 2 }, { "n_bowpw", "BOW WIDTH", 2 } };
    int count[3] = { 0, 0, 0 };
    for (auto& k : ks) ++count[k.group];
    int idx[3] = { 0, 0, 0 };
    for (auto& k : ks)
    {
        auto* prm = proc.apvts.getParameter (k.id);
        auto knob = std::make_unique<StarKnob> (prm, k.label);
        int i = idx[k.group]++;
        // six to a row, every row centred under its tile (the page is mirror symmetrical)
        const int row = i / 6, inRow = std::min (6, count[k.group] - row * 6);
        knob->setBounds (28 + k.group * 388 + (368 - inRow * 61) / 2 + (i % 6) * 61, 494 + row * 84, 61, 82);
        addAndMakeVisible (*knob);
        attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, k.id, *knob));
        knob->onValueChange = [this] { sitarTile.repaint(); tanpuraTile.repaint(); };
        knobs.push_back (std::move (knob));
    }

    saPill.text = [this] { return juce::String (proc.saHz(), 2) + " HZ"; };
    saPill.onDragStart = [this] { proc.apvts.getParameter ("saHz")->beginChangeGesture(); };
    saPill.onDragEnd = [this] { proc.apvts.getParameter ("saHz")->endChangeGesture(); };
    saPill.onDrag = [this] (float dy, bool fine)
    {
        auto* prm = proc.apvts.getParameter ("saHz");
        float hz = juce::jlimit (55.0f, 440.0f, (float) proc.saHz() + dy * (fine ? 0.01f : 0.25f));
        prm->setValueNotifyingHost (prm->convertTo0to1 (hz));
    };
    saPill.onDoubleClick = [this] { editValue (saPill, "saHz"); };

    ragaPill.text = [this]
    {
        const auto& e = tantri::scaleLibrary()[(size_t) juce::jlimit (0, tantri::numScales() - 1, proc.scaleIndex())];
        return juce::String (e.name).toUpperCase() + (proc.tuningIsCustom() ? " *" : "");
    };
    ragaPill.onClick = [this] { showRagaMenu(); };

    lockPill.text = [this] { return juce::String ((int) getParam (proc, "map") == tantri::mapSteps ? "STEPS" : "QUANTIZE"); };
    lockPill.onClick = [this] { setParam (proc, "map", (int) getParam (proc, "map") == 0 ? 1.0f : 0.0f); lockPill.repaint(); strip.repaint(); };

    meendPill.text = [this] { return juce::String (juce::CharPointer_UTF8 ("\xc2\xb1")) + juce::String (juce::roundToInt (getParam (proc, "s_meend"))) + " C"; };
    meendPill.onDragStart = [this] { proc.apvts.getParameter ("s_meend")->beginChangeGesture(); };
    meendPill.onDragEnd = [this] { proc.apvts.getParameter ("s_meend")->endChangeGesture(); };
    meendPill.onDrag = [this] (float dy, bool fine)
    {
        auto* prm = proc.apvts.getParameter ("s_meend");
        float c = juce::jlimit (0.0f, 1200.0f, getParam (proc, "s_meend") + dy * (fine ? 1.0f : 5.0f));
        prm->setValueNotifyingHost (prm->convertTo0to1 (c));
    };
    meendPill.onDoubleClick = [this] { editValue (meendPill, "s_meend"); };

    roomPill.text = [this] { return juce::String (juce::roundToInt (getParam (proc, "roomSize") * 100)) + "%"; };
    roomPill.onDragStart = [this] { proc.apvts.getParameter ("roomSize")->beginChangeGesture(); };
    roomPill.onDragEnd = [this] { proc.apvts.getParameter ("roomSize")->endChangeGesture(); };
    roomPill.onDrag = [this] (float dy, bool)
    {
        auto* prm = proc.apvts.getParameter ("roomSize");
        prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, getParam (proc, "roomSize") + dy * 0.005f));
    };

    midiPill.text = [this]
    {
        auto ch = [this] (const char* id) { int c = (int) getParam (proc, id); return c == 0 ? juce::String ("ANY") : juce::String (c); };
        return "S " + ch ("sitarCh") + " / T " + ch ("tablaCh") + " / N " + ch ("tanpuraCh");
    };
    midiPill.onClick = [this] { showMidiMenu(); };
    voicesPill.text = [this] { return juce::String (proc.voicesUsed.load()) + "/12"; };
    // output meter: pre-limiter peak; shows the limiter's gain reduction whenever it acts (never clips)
    outPill.text = [this]
    {
        float pk = proc.outPeak.load(), gr = proc.limitDb.load();
        juce::String s = pk < 1e-5f ? juce::String ("-INF") : juce::String (juce::Decibels::gainToDecibels (pk), 1);
        if (gr < -0.1f) s << "  LIMIT " << juce::String (-gr, 1);
        return s + " DB";
    };
    outPill.valueColour = [this] { return proc.limitDb.load() < -0.1f ? col::coral : col::white; };

    for (auto* pl : { &saPill, &ragaPill, &lockPill, &meendPill, &roomPill, &midiPill, &voicesPill, &outPill }) addAndMakeVisible (*pl);
    if (TantriProcessor::kSpatial)
    {
        space = std::make_unique<SpacePanel> (proc);
        addChildComponent (*space);
        viewPill.text = [this] { return juce::String (spaceView ? "SPACE" : "INSTRUMENT"); };
        viewPill.valueColour = [this] { return spaceView ? col::gold2 : col::white; };
        viewPill.onClick = [this] { setSpaceView (! spaceView); };
        viewPill.centred = true;
        addAndMakeVisible (viewPill);
    }
    setSize (1200, 780);
}
Canvas::~Canvas() = default;

void Canvas::setSpaceView (bool on)
{
    spaceView = on && space != nullptr;
    for (auto* c : { (juce::Component*) &sitarTile, (juce::Component*) &tablaTile, (juce::Component*) &tanpuraTile }) c->setVisible (! spaceView);
    for (auto& k : knobs) k->setVisible (! spaceView);
    if (space) space->setVisible (spaceView);
    resized(); repaint();
}

void Canvas::resized()
{
    sitarTile.setBounds (tileBounds (0)); tablaTile.setBounds (tileBounds (1)); tanpuraTile.setBounds (tileBounds (2));
    strip.setBounds (28, 696, 1144, 62);
    if (space) space->setBounds (28, 76, 1144, 604);
    // the header mirrors about the title: SA SCALE MAP | OUT MIDI VOICES on the top row,
    // MEEND | ROOM under them, VIEW under the title
    auto widthOf = [] (Pill& p, int cap = 1000) { return std::min (cap, std::max (60, p.preferredWidth() + 12)); };
    int x = 28;
    for (auto* pl : { &saPill, &ragaPill, &lockPill })
    {
        const int w = widthOf (*pl, pl == &ragaPill ? 250 : 1000);
        pl->setBounds (x, 12, w, 28); x += w + 14;
    }
    int r = 1172;
    for (auto* pl : { &voicesPill, &midiPill, &outPill })
    {
        const int w = pl == &outPill ? 176 : widthOf (*pl);
        r -= w; pl->setBounds (r, 12, w, 28); r -= 14;
    }
    meendPill.setBounds (28, 42, widthOf (meendPill), 26);
    const int rw = widthOf (roomPill);
    roomPill.setBounds (1172 - rw, 42, rw, 26);
    if (space) viewPill.setBounds (500, 46, 200, 26);   // drawn centred on the title's axis (x = 600)
}

void Canvas::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    g.setColour (col::white); g.setFont (font (24));
    if (TantriProcessor::kSpatial)
    {
        g.drawText ("TANTRI", 500, 6, 200, 28, juce::Justification::centred);
        g.setColour (col::gold2); g.setFont (font (10));
        g.drawText (TantriProcessor::kFx ? "AKASHA FX" : "AKASHA", 500, 32, 200, 12, juce::Justification::centred);
    }
    else g.drawText ("TANTRI", 500, 20, 200, 36, juce::Justification::centred);
    if (spaceView) return;
    struct N { const char* a; const char* b; };
    static const N names[3] = { { "SITAR 22", "WAVEGUIDE \xc2\xb7 JAWARI \xc2\xb7 13 TARAF" }, { "TABLA 16", "MODAL \xc2\xb7 TEENTAAL" }, { "TANPURA 4", "" } };
    for (int i = 0; i < 3; ++i)
    {
        float x = 28.0f + i * 388;
        juce::String title = juce::CharPointer_UTF8 (names[i].a);
        if (i == 0) title = "SITAR " + juce::String (proc.activeScale().n);   // degrees of the active scale
        const float w = juce::GlyphArrangement::getStringWidth (font (21), title);
        juce::String sub = juce::CharPointer_UTF8 (names[i].b);
        if (i == 2)
        {
            static const char* fn[5] = { "PA", "PA", "MA", "NI", "ni" };
            const char* nm[4] = { fn[juce::jlimit (0, 4, proc.tanpuraFirst.load())], "SA", "SA", "sa" };
            if ((int) getParam (proc, "n_mode") == 1)
            {
                sub = "DRONE";
                for (int k = 0; k < 4; ++k) if (droneWants (proc, k)) sub << " " << nm[k];
            }
            else sub = juce::String (nm[0]) + " SA SA sa";
        }
        // title and subtitle as one line, centred under the tile
        const float sw = sub.isEmpty() ? 0.0f : juce::GlyphArrangement::getStringWidth (font (13), sub) + 8;
        const float x0 = x + (368 - (w + sw)) * 0.5f;
        g.setColour (col::white); g.setFont (font (21));
        g.drawText (title, juce::Rectangle<float> (x0, 452, w + 2, 28), juce::Justification::centredLeft);
        g.setColour (col::grey); g.setFont (font (13));
        g.drawText (sub, juce::Rectangle<float> (x0 + w + 8, 457, sw + 4, 22), juce::Justification::centredLeft);
    }
}

void Canvas::tick()
{
    if (proc.scaleIndex() != lastScale || proc.activeScale().n != lastN) { lastScale = proc.scaleIndex(); lastN = proc.activeScale().n; resized(); repaint(); }
    for (int w = 0; w < tantri::ActiveScale::kMax; ++w)
    {
        float a = proc.swaraAct[w].load();
        sitarTile.act[w] = std::max (a, sitarTile.act[w] * 0.86f);
        strip.act[w] = sitarTile.act[w];
    }
    tablaTile.dayan = std::max (proc.dayanAct.load(), tablaTile.dayan * 0.85f);
    tablaTile.bayan = std::max (proc.bayanAct.load(), tablaTile.bayan * 0.85f);
    static unsigned lastStroke = 0;
    unsigned sc = proc.strokeCount.load();
    if (sc != lastStroke) { tablaTile.bol = proc.lastBol.load(); tablaTile.bolA = 1.0f; lastStroke = sc; }
    else tablaTile.bolA *= 0.93f;
    double beat = proc.beatPos.load();
    tablaTile.matra = beat >= 0 ? int (std::floor (beat)) % 16 : int ((sc + 15) % 16);
    for (int i = 0; i < 4; ++i) tanpuraTile.act[i] = std::max (proc.tanpuraAct[i].load(), tanpuraTile.act[i] * 0.9f);
    sitarTile.repaint(); tablaTile.repaint(); tanpuraTile.repaint(); strip.repaint();
    voicesPill.repaint(); ragaPill.repaint(); saPill.repaint(); outPill.repaint();
    if (spaceView && space) space->tick();
    else repaint (800, 450, 372, 34);   // the tanpura's subtitle (drone notes)
    static int lastFirst = -1;
    if (proc.tanpuraFirst.load() != lastFirst) { lastFirst = proc.tanpuraFirst.load(); repaint (800, 450, 372, 34); }
}

void Canvas::editValue (Pill& pill, const char* paramId)
{
    typer = std::make_unique<juce::TextEditor>();
    typer->setFont (font (13));
    typer->setColour (juce::TextEditor::backgroundColourId, col::tile);
    typer->setColour (juce::TextEditor::textColourId, col::white);
    typer->setColour (juce::TextEditor::outlineColourId, col::gold);
    typer->setColour (juce::TextEditor::focusedOutlineColourId, col::gold2);
    typer->setText (juce::String (getParam (proc, paramId), 2));
    typer->setBounds (pill.getBounds().withWidth (std::max (110, pill.getWidth())));
    addAndMakeVisible (*typer);
    typer->grabKeyboardFocus();
    typer->selectAll();
    juce::String id (paramId);
    typer->onReturnKey = [this, id]
    {
        float v = typer->getText().retainCharacters ("0123456789.").getFloatValue();
        if (v > 0) setParam (proc, id.toRawUTF8(), v);
        juce::MessageManager::callAsync ([this] { typer.reset(); resized(); repaint(); });
    };
    typer->onEscapeKey = [this] { juce::MessageManager::callAsync ([this] { typer.reset(); }); };
    typer->onFocusLost = [this] { juce::MessageManager::callAsync ([this] { typer.reset(); }); };
}

void Canvas::showRagaMenu()
{
    using namespace tantri;
    juce::PopupMenu m;
    const auto& lib = scaleLibrary();
    int cur = proc.scaleIndex();
    static const char* chakras[12] = { "Indu", "Netra", "Agni", "Veda", "Bana", "Rutu", "Rishi", "Vasu", "Brahma", "Disi", "Rudra", "Aditya" };
    for (int f = 0; f < kNumFamilies; ++f)
    {
        juce::PopupMenu sub;
        if (f == famMela)
        {
            juce::PopupMenu ch[12];
            int k = 0;
            for (int i = 0; i < (int) lib.size(); ++i)
                if (lib[(size_t) i].family == famMela) { ch[k / 6].addItem (i + 1, lib[(size_t) i].name, true, i == cur); ++k; }
            for (int c = 0; c < 12; ++c)
                sub.addSubMenu (juce::String (c + 1) + "  " + chakras[c] + "  (" + juce::String (c * 6 + 1) + "-" + juce::String (c * 6 + 6) + ")", ch[c]);
        }
        else
            for (int i = 0; i < (int) lib.size(); ++i)
                if (lib[(size_t) i].family == f) sub.addItem (i + 1, lib[(size_t) i].name, true, i == cur);
        bool here = lib[(size_t) juce::jlimit (0, (int) lib.size() - 1, cur)].family == f;
        m.addSubMenu (kFamilyName[f], sub, true, nullptr, here);
    }
    m.addSeparator();
    m.addItem (10000, "Reload scale (discard shruti edits)", proc.tuningIsCustom());
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&ragaPill), [this] (int r)
    {
        const auto& lib2 = tantri::scaleLibrary();
        if (r >= 1 && r <= (int) lib2.size())
        {
            setParam (proc, "scale", float (r - 1));
            if (lib2[(size_t) r - 1].swara) proc.setTuning (lib2[(size_t) r - 1].bits);
        }
        else if (r == 10000)
        {
            const auto& e = lib2[(size_t) proc.scaleIndex()];
            if (e.swara) proc.setTuning (e.bits);
        }
        resized(); repaint();
    });
}

void Canvas::showMidiMenu()
{
    juce::PopupMenu m;
    auto chanMenu = [this] (const char* id, int base)
    {
        juce::PopupMenu sub;
        int cur = (int) getParam (proc, id);
        sub.addItem (base, "Any", true, cur == 0);
        for (int c = 1; c <= 16; ++c) sub.addItem (base + c, "Channel " + juce::String (c), true, cur == c);
        return sub;
    };
    m.addSubMenu ("Sitar channel", chanMenu ("sitarCh", 1000));
    m.addSubMenu ("Tabla channel (12 bols per octave)", chanMenu ("tablaCh", 2000));
    m.addSubMenu ("Tanpura channel (held note plays the drone)", chanMenu ("tanpuraCh", 3000));
    m.addSeparator();
    m.addItem (4000, "Split: tabla + chikari below the sitar range", true, getParam (proc, "split") > 0.5f);
    juce::PopupMenu keys;
    int sk = proc.saKey();
    for (int n = 36; n <= 84; ++n)
        keys.addItem (5000 + n, juce::String (n) + "  (" + juce::MidiMessage::getMidiNoteName (n, true, true, 4) + ")", true, n == sk);
    m.addSubMenu ("Madhya Sa key", keys);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&midiPill), [this] (int r)
    {
        if (r >= 1000 && r < 1017) setParam (proc, "sitarCh", float (r - 1000));
        else if (r >= 2000 && r < 2017) setParam (proc, "tablaCh", float (r - 2000));
        else if (r >= 3000 && r < 3017) setParam (proc, "tanpuraCh", float (r - 3000));
        else if (r == 4000) setParam (proc, "split", getParam (proc, "split") > 0.5f ? 0.0f : 1.0f);
        else if (r >= 5000) setParam (proc, "saKey", float (r - 5000));
        resized(); repaint();
    });
}

// ---------------------------------------------------------------- editor (scales the canvas)
TantriEditor::TantriEditor (TantriProcessor& p) : AudioProcessorEditor (p), proc (p), canvas (p)
{
    addAndMakeVisible (canvas);
    setResizable (true, true);
    if (auto* c = getConstrainer()) { c->setFixedAspectRatio (1200.0 / 780.0); c->setSizeLimits (840, 546, 2400, 1560); }
    setSize (1200, 780);
    startTimerHz (30);
}
void TantriEditor::resized()
{
    float s = getWidth() / 1200.0f;
    canvas.setTransform (juce::AffineTransform::scale (s));
    canvas.setBounds (0, 0, 1200, 780);
}
