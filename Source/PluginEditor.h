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
