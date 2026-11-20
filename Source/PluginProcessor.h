#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Sitar.h"
#include "Tabla.h"
#include "Tanpura.h"
#include "Space.h"

#ifndef TANTRI_SPATIAL
 #define TANTRI_SPATIAL 0
#endif
#ifndef TANTRI_FX
 #define TANTRI_FX 0
#endif

class TantriProcessor : public juce::AudioProcessor, private juce::AudioProcessorValueTreeState::Listener
{
public:
    TantriProcessor();
    ~TantriProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboard;   // UI auditioning (bottom strip)

    // tuning (not a host parameter: edited on the shruti star; raga choice param loads presets)
    tantri::TuningBits getTuning() const { return { tuningBits.load() }; }
    void setTuning (tantri::TuningBits t) { tuningBits.store (t.v); tuningVersion.fetch_add (1); }
    bool tuningIsCustom() const;
    double saHz() const { return apvts.getRawParameterValue ("saHz")->load(); }
    int saKey() const { return (int) apvts.getRawParameterValue ("saKey")->load(); }

    // meters for the UI (written by the audio thread, read by the editor timer)
    std::atomic<float> swaraAct[tantri::ActiveScale::kMax] {};   // activity per scale degree
    std::atomic<float> chikAct { 0 }, dayanAct { 0 }, bayanAct { 0 };
    std::atomic<int> lastBol { -1 };
    std::atomic<unsigned> strokeCount { 0 };
    std::atomic<float> tanpuraAct[4] {};
    std::atomic<int> tanpuraLast { -1 }, tanpuraFirst { 1 };
    std::atomic<double> beatPos { -1.0 };     // host beat position, -1 if not playing
    std::atomic<int> voicesUsed { 0 };
    std::atomic<float> outPeak { 0 }, limitDb { 0 };   // pre-limiter peak, limiter gain reduction (dB, <= 0)

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // ---- Tantri Akasha (spatial builds): the sources placed in the space
    static constexpr bool kSpatial = TANTRI_SPATIAL != 0, kFx = TANTRI_FX != 0;
    static constexpr int kSources = TANTRI_FX ? 4 : 3;          // sitar, tabla, tanpura (+ input)
    static constexpr const char* kSrcId[4] = { "sitar", "tabla", "tanpura", "input" };
    std::atomic<float> srcX[4] {}, srcY[4] {}, srcD[4] {}, srcAct[4] {};   // where each source is now (play-area units)
    std::atomic<int> orderNow { 0 };                            // 0 = stereo preview
    bool droning (int i) const { return tanpuraDrone[i].load(); }
    std::atomic<bool> tanpuraDrone[4] {};
    static juce::StringArray scaleNames();
    int scaleIndex() const { return (int) apvts.getRawParameterValue ("scale")->load(); }
    tantri::ActiveScale activeScale() const { return tantri::makeScale (scaleIndex(), getTuning()); }

private:
    static BusesProperties buses();
    void processSpatial (juce::AudioBuffer<float>&, juce::MidiBuffer&);
    void updateMeters();
    static constexpr int kChunk = 64;
    void parameterChanged (const juce::String& id, float v) override;
    void handleMidi (const juce::MidiMessage& m);
    void readParams();

    tantri::Sitar sitar;
    tantri::Tabla tabla;
    tantri::Tanpura tanpura;
    tantri::FdnReverb reverb;
    tantri::Limiter limiter;
    std::atomic<uint64_t> tuningBits { tantri::parseRaga (tantri::kRagas[0].spec).v };
    std::atomic<unsigned> tuningVersion { 1 };
    int lastScaleIdx = -1; unsigned lastTuningVer = 0, scaleVersion = 0;
    tantri::ActiveScale scale;
    int tanpuraHeld = 0;
    float tablaPressure = 0;
    double sr = 48000;
    juce::LinearSmoothedValue<float> master;

    // spatial state
    tantri::LimiterN limitN;
    tantri::field::Encoder enc[4], encRev[2];
    tantri::field::Flock flock;
    double tau[4] {};
    int lastMotion[4] { -1, -1, -1, -1 };
    int orderCap = ambi::kMaxOrder;
    juce::AudioBuffer<float> srcBuf, inBuf, fold;
    std::vector<float> scrL, scrR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TantriProcessor)
};
