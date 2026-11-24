#pragma once
#include "PluginProcessor.h"
#include "Art.h"

// Star knob: gold spikes light up to the value; label and value text underneath
class StarKnob : public juce::Slider
{
public:
    StarKnob (juce::RangedAudioParameter* p, juce::String label);
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent& e) override { hover = true; Slider::mouseEnter (e); repaint(); }
    void mouseExit (const juce::MouseEvent& e) override { hover = false; Slider::mouseExit (e); repaint(); }
private:
    juce::RangedAudioParameter* param;
    juce::String name;
    bool hover = false;
};

// Header value: LABEL value. Drag to change, double-click to type (floats), click for menus (choices)
class Pill : public juce::Component
{
public:
    Pill (juce::String label) : label (std::move (label)) {}
    std::function<juce::String()> text;
    std::function<void (float dy, bool fine)> onDrag;
    std::function<void()> onClick, onDoubleClick, onDragStart, onDragEnd;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override { if (onDoubleClick) onDoubleClick(); }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    int preferredWidth() const;
    juce::String label;
    std::function<juce::Colour()> valueColour;
    bool centred = false;   // draw label + value centred in the bounds (the VIEW pill under the title)
private:
    bool hover = false, dragged = false;
    float lastY = 0;
};

class TantriEditor;

class SitarTile : public juce::Component
{
public:
    SitarTile (TantriProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hovered = -1; repaint(); }
    void mouseDown (const juce::MouseEvent&) override;
    float act[tantri::ActiveScale::kMax] {};
    struct Blade { double cents; int degree; int shruti; };
private:
    std::vector<Blade> blades (const tantri::ActiveScale&, tantri::TuningBits) const;
    int bladeAt (juce::Point<float>) const;
    TantriProcessor& proc;
    int hovered = -1;
};

class TablaTile : public juce::Component
{
public:
    TablaTile (TantriProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    float dayan = 0, bayan = 0, bolA = 0;
    int bol = -1, matra = 0;
private:
    TantriProcessor& proc;
};

class TanpuraTile : public juce::Component
{
public:
    TanpuraTile (TantriProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    float act[4] {};
private:
    TantriProcessor& proc;
};

// Tantri Akasha: the space in 3D. An orbiting camera round you (drag the empty space to turn it,
// wheel to zoom), the sphere of directions, the floor, the room the reflections come from, every
// source with its trail, shadow and home; or the sphere unrolled flat (MAP).
class Space3D : public juce::Component
{
public:
    Space3D (TantriProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void tick();
    std::function<void (int source, juce::Point<int> where)> onMotionMenu;
    enum Mode { Free = 0, Top, Front, Side, Map, NumModes };
private:
    struct V3 { float x = 0, y = 0, z = 0; };
    struct Proj { juce::Point<float> p; float depth = 0, scale = 0; bool ok = false; };
    Proj project (V3) const;
    V3 worldOf (float x, float y, float d) const;        // play-area place -> view space (scene rotation applied)
    juce::Rectangle<float> viewArea() const { return getLocalBounds().toFloat().withTrimmedTop (30).reduced (1); }
    juce::Rectangle<float> tabRect (int i) const;
    void setMode (int m);
    void paintMap (juce::Graphics&, juce::Rectangle<float>);
    void paint3D (juce::Graphics&, juce::Rectangle<float>);
    void line3D (juce::Graphics&, V3 a, V3 b, juce::Colour, float w, bool depthFade = true) const;
    juce::Point<float> mapPoint (float x, float y) const;
    int sourceAt (juce::Point<float>) const;
    void dragSourceTo (juce::Point<float> from, juce::Point<float> to);
    float pv (int k, const char* suffix) const;

    TantriProcessor& proc;
    int mode = Free;
    bool orbit = false;
    float camYaw = 3.6f, camPitch = 0.42f, camDist = 3.3f;        // where the camera is (radians, view units)
    float tYaw = 3.6f, tPitch = 0.42f, tDist = 3.3f;              // where it is going
    float freeYaw = 3.6f, freePitch = 0.42f;
    struct Trail { std::array<V3, 96> p {}; int n = 0, w = 0; };
    std::array<Trail, 4> trails;
    std::array<std::array<juce::Point<float>, 96>, 4> mapPts {};
    std::array<juce::Point<float>, 4> screenNow {}, screenHome {};
    int dragging = -1, hoverSrc = -1;
    bool orbiting = false, distDrag = false;
    juce::Point<float> lastMouse;
    float act[4] {};
    float frame = 0;
};

// one modulator: shape and target (menus), a live scope of the wave, rate and depth
class ModRow : public juce::Component
{
public:
    ModRow (TantriProcessor&, int index);
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick() { repaint(); }
private:
    TantriProcessor& proc;
    int index;
    juce::String id;
    Pill shapePill { "" }, targetPill { "" };
    std::unique_ptr<StarKnob> rate, depth;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    juce::Rectangle<float> scope;
};

// the space page: field + effects (left), the 3D view (middle), the modulators (right), and each
// source's place and motion along the bottom - mirror symmetrical about the middle
class SpacePanel : public juce::Component
{
public:
    SpacePanel (TantriProcessor&);
    ~SpacePanel() override;
    void paint (juce::Graphics&) override;
    void tick();
    static constexpr int kW = 1144, kH = 604, kColW = 244, kTopH = 432;
private:
    void motionMenu (int k, juce::Component* target, juce::Point<int> at = {});
    float pv (int k, const char* suffix) const;
    TantriProcessor& proc;
    Space3D view;
    std::vector<std::unique_ptr<ModRow>> mods;
    std::vector<std::unique_ptr<StarKnob>> knobs;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    std::vector<std::unique_ptr<Pill>> motionPills;
};

class RagaStrip : public juce::Component
{
public:
    RagaStrip (TantriProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    float act[tantri::ActiveScale::kMax] {};
private:
    struct Slot { int swara, oct; float x; };
    std::vector<Slot> slots() const;
    TantriProcessor& proc;
    int heldNote = -1;
};

class Canvas : public juce::Component
{
public:
    Canvas (TantriProcessor&);
    ~Canvas() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void tick();
    void setSpaceView (bool);
private:
    void editValue (Pill&, const char* paramId);
    void showMidiMenu();
    void showRagaMenu();
    juce::Rectangle<int> tileBounds (int i) const { return { 28 + i * 388, 76, 368, 368 }; }

    TantriProcessor& proc;
    SitarTile sitarTile;
    TablaTile tablaTile;
    TanpuraTile tanpuraTile;
    RagaStrip strip;
    std::unique_ptr<SpacePanel> space;
    bool spaceView = false;
    Pill viewPill { "VIEW" };
    Pill saPill { "SA" }, ragaPill { "SCALE" }, lockPill { "MAP" }, meendPill { "MEEND" }, roomPill { "ROOM" }, midiPill { "MIDI" }, voicesPill { "VOICES" }, outPill { "OUT" };
    std::vector<std::unique_ptr<StarKnob>> knobs;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;
    std::unique_ptr<juce::TextEditor> typer;
    juce::Rectangle<int> knobArea[3];
    int lastScale = -1, lastN = -1;
};

class TantriEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TantriEditor (TantriProcessor&);
    void paint (juce::Graphics& g) override { g.fillAll (art::col::bg); }
    void resized() override;
    Canvas& getCanvas() { return canvas; }
private:
    void timerCallback() override { canvas.tick(); }
    TantriProcessor& proc;
    Canvas canvas;
};
