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
    float lw = juce::GlyphArrangement::getStringWidth (font (13), label);
    g.setColour (hover ? col::gold2 : col::grey);
    g.drawText (label, juce::Rectangle<float> (0, 0, lw + 2, (float) getHeight()), juce::Justification::centredLeft);
    g.setColour (valueColour ? valueColour() : col::white);
    g.drawText (text ? text() : juce::String(), juce::Rectangle<float> (lw + 6, 0, getWidth() - lw - 6, (float) getHeight()), juce::Justification::centredLeft);
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
void TanpuraTile::paint (juce::Graphics& g)
{
    g.fillAll (col::tile);
    g.setColour (col::edge); g.drawRect (getLocalBounds(), 1);
    const juce::Point<float> c (184, 184);
    const bool on = getParam (proc, "n_on") > 0.5f;
    g.setColour (col::gold); g.drawEllipse (juce::Rectangle<float> (320, 320).withCentre (c), 10);
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
        fill (g, kite (c, a, 30, 128, 0.42f), col::white.interpolatedWith (col::orange, lv), col::gold, 3);
        fill (g, kite (c, a, 48, 108, 0.22f), col::pink.interpolatedWith (col::coral, lv));
        fill (g, kite (c, a + MathConstants<float>::pi / 4, 30, 96, 0.25f), col::white, col::gold, 2.5f);
        auto q = P (c, 84, a);
        g.setColour (lv > 0.5f ? col::white : col::ink);
        g.setFont (font (14));
        g.drawText (names[i], juce::Rectangle<float> (40, 18).withCentre (q), juce::Justification::centred);
        if (i == 0 && (int) getParam (proc, "n_first") == 0)
        {
            g.setFont (font (8)); g.setColour (col::dim);
            g.drawText ("AUTO", juce::Rectangle<float> (40, 10).withCentre (q.translated (0, 13)), juce::Justification::centred);
        }
    }
    fill (g, star (c, 12, 30, 18), on ? col::red : col::off, col::gold, 2);
    g.setColour (on ? col::white : col::grey); g.fillEllipse (juce::Rectangle<float> (18, 18).withCentre (c));
    g.setColour (col::grey); g.setFont (font (10));
    g.drawText (on ? "CENTRE: STOP" : "CENTRE: PLAY", 12, 8, 200, 14, juce::Justification::left);
    g.drawText ("TOP PETAL: FIRST STRING", 12, 22, 220, 14, juce::Justification::left);
}
void TanpuraTile::mouseDown (const juce::MouseEvent& e)
{
    auto d = e.position - juce::Point<float> (184, 184);
    if (d.getDistanceFromOrigin() < 34) { setParam (proc, "n_on", getParam (proc, "n_on") > 0.5f ? 0.0f : 1.0f); repaint(); return; }
    if (std::abs (d.x) < 34 && d.y < -34 && d.y > -130)
    {
        int f = ((int) getParam (proc, "n_first") + 1) % 5;
        setParam (proc, "n_first", (float) f);
        repaint();
    }
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
    for (size_t i = 0; i < s.size(); ++i) s[i].x = 36 + step * i;
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
        { "n_human", "HUMAN", 2 }, { "n_width", "WIDTH", 2 }, { "n_room", "ROOM", 2 }, { "n_level", "LEVEL", 2 } };
    int idx[3] = { 0, 0, 0 };
    for (auto& k : ks)
    {
        auto* prm = proc.apvts.getParameter (k.id);
        auto knob = std::make_unique<StarKnob> (prm, k.label);
        int i = idx[k.group]++;
        knob->setBounds (28 + k.group * 388 + (i % 6) * 61, 494 + (i / 6) * 84, 61, 82);
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
    setSize (1200, 780);
}
Canvas::~Canvas() = default;

void Canvas::resized()
{
    sitarTile.setBounds (tileBounds (0)); tablaTile.setBounds (tileBounds (1)); tanpuraTile.setBounds (tileBounds (2));
    strip.setBounds (28, 696, 1144, 62);
    int x = 150;
    for (auto* pl : { &saPill, &ragaPill, &lockPill, &meendPill, &roomPill })
    {
        int w = std::max (60, pl->preferredWidth() + 16);
        pl->setBounds (x, 26, w, 28); x += w + 14;
    }
    int vw = voicesPill.preferredWidth() + 4;
    voicesPill.setBounds (1172 - vw, 26, vw, 28);
    int mw = midiPill.preferredWidth() + 8;
    midiPill.setBounds (voicesPill.getX() - 24 - mw, 26, mw, 28);
    outPill.setBounds (1172 - 176, 452, 176, 28);
}

void Canvas::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    g.setColour (col::white); g.setFont (font (24));
    g.drawText ("TANTRI", 28, 22, 120, 36, juce::Justification::centredLeft);
    struct N { const char* a; const char* b; };
    static const N names[3] = { { "SITAR 22", "WAVEGUIDE \xc2\xb7 JAWARI \xc2\xb7 13 TARAF" }, { "TABLA 16", "MODAL \xc2\xb7 TEENTAAL" }, { "TANPURA 4", "" } };
    for (int i = 0; i < 3; ++i)
    {
        float x = 28.0f + i * 388;
        g.setColour (col::white); g.setFont (font (21));
        juce::String title = juce::CharPointer_UTF8 (names[i].a);
        if (i == 0) title = "SITAR " + juce::String (proc.activeScale().n);   // degrees of the active scale
        g.drawText (title, juce::Rectangle<float> (x, 452, 200, 28), juce::Justification::centredLeft);
        float w = juce::GlyphArrangement::getStringWidth (font (21), title);
        juce::String sub = juce::CharPointer_UTF8 (names[i].b);
        if (i == 2)
        {
            static const char* fn[5] = { "PA", "PA", "MA", "NI", "ni" };
            sub = juce::String (fn[juce::jlimit (0, 4, proc.tanpuraFirst.load())]) + " SA SA sa";
        }
        g.setColour (col::grey); g.setFont (font (13));
        g.drawText (sub, juce::Rectangle<float> (x + w + 8, 457, 300, 22), juce::Justification::centredLeft);
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
    static int lastFirst = -1;
    if (proc.tanpuraFirst.load() != lastFirst) { lastFirst = proc.tanpuraFirst.load(); repaint (800, 450, 400, 34); }
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
