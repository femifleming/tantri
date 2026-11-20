#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace tantri;
using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;
using APB = juce::AudioParameterBool;
using API = juce::AudioParameterInt;
using NR = juce::NormalisableRange<float>;

juce::StringArray TantriProcessor::scaleNames()
{
    juce::StringArray s;
    for (auto& e : scaleLibrary()) s.add (e.name);
    return s;
}

static juce::AudioParameterFloatAttributes fmt (std::function<juce::String (float)> f)
{
    return juce::AudioParameterFloatAttributes().withStringFromValueFunction ([f] (float v, int) { return f (v); });
}
static juce::String pct (float v) { return juce::String (juce::roundToInt (v * 100)) + "%"; }
static juce::String db (float v)  { return v <= 0.0005f ? "-inf" : juce::String (juce::Decibels::gainToDecibels (v), 1) + " dB"; }
static juce::String secs (float v){ return juce::String (v, v < 10 ? 1 : 0) + " s"; }

juce::AudioProcessorValueTreeState::ParameterLayout TantriProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto F = [&] (const char* id, const char* name, NR r, float def, std::function<juce::String (float)> f)
    { p.push_back (std::make_unique<APF> (juce::ParameterID { id, 1 }, name, r, def, fmt (f))); };
    NR unit (0, 1);
    NR lev (0, 1.5f); lev.setSkewForCentre (0.6f);

    // global
    { NR r (55, 440); r.setSkewForCentre (140); F ("saHz", "Sa (Hz)", r, 138.59f, [] (float v) { return juce::String (v, 2) + " Hz"; }); }
    p.push_back (std::make_unique<API> (juce::ParameterID { "saKey", 1 }, "Sa key", 36, 84, 60));
    p.push_back (std::make_unique<APC> (juce::ParameterID { "scale", 1 }, "Scale", scaleNames(), 0));
    p.push_back (std::make_unique<APC> (juce::ParameterID { "map", 1 }, "Key map", juce::StringArray { "Quantize", "Steps" }, 0));
    F ("master", "Master", lev, 0.8f, db);
    F ("roomSize", "Room size", unit, 0.45f, pct);
    juce::StringArray chans { "Any" };
    for (int i = 1; i <= 16; ++i) chans.add (juce::String (i));
    p.push_back (std::make_unique<APC> (juce::ParameterID { "sitarCh", 1 }, "Sitar channel", chans, 0));
    p.push_back (std::make_unique<APC> (juce::ParameterID { "tablaCh", 1 }, "Tabla channel", chans, 10));
    p.push_back (std::make_unique<APC> (juce::ParameterID { "tanpuraCh", 1 }, "Tanpura channel", chans, 16));
    p.push_back (std::make_unique<APB> (juce::ParameterID { "split", 1 }, "Tabla split", true));

    // sitar
    F ("s_mizrab", "Sitar mizrab", unit, 0.62f, pct);
    F ("s_jawari", "Sitar jawari", unit, 0.7f, pct);
    F ("s_meend", "Sitar meend", NR (0, 1200, 1), 500, [] (float v) { return juce::String (juce::CharPointer_UTF8 ("\xc2\xb1")) + juce::String (juce::roundToInt (v)) + " c"; });
    F ("s_taraf", "Sitar taraf", unit, 0.55f, pct);
    F ("s_chikari", "Sitar chikari", unit, 0.4f, pct);
    F ("s_gourd", "Sitar gourd", unit, 0.65f, pct);
    F ("s_gamak", "Sitar gamak", unit, 0.3f, pct);
    { NR r (1, 20); r.setSkewForCentre (6); F ("s_decay", "Sitar decay", r, 6.0f, secs); }
    F ("s_pluck", "Sitar pluck position", NR (0.05f, 0.5f), 0.2f, [] (float v) { return "1/" + juce::String (juce::roundToInt (1.0f / v)); });
    { NR r (0.1f, 20); r.setSkewForCentre (2); F ("s_release", "Sitar damp", r, 1.5f, secs); }
    F ("s_room", "Sitar room", unit, 0.25f, pct);
    F ("s_level", "Sitar level", lev, 0.8f, db);

    // tabla
    p.push_back (std::make_unique<APC> (juce::ParameterID { "t_dayan", 1 }, "Dayan tuned to",
                                        juce::StringArray { "Sa", "Pa", "Ma", "Ni", "Sa'" }, 0));
    { NR r (60, 160); F ("t_bayan", "Bayan pitch", r, 98, [] (float v) { return juce::String (juce::roundToInt (v)) + " Hz"; }); }
    F ("t_syahi", "Tabla syahi", unit, 1.0f, pct);
    F ("t_glide", "Tabla ghe glide", unit, 0.55f, pct);
    F ("t_press", "Bayan pressure", unit, 0.15f, pct);
    { NR r (0.3f, 2.5f); r.setSkewForCentre (1); F ("t_ring", "Tabla ring", r, 1.0f, [] (float v) { return juce::String (v, 2) + "x"; }); }
    F ("t_snap", "Tabla snap", unit, 0.35f, pct);
    F ("t_room", "Tabla room", unit, 0.2f, pct);
    F ("t_level", "Tabla level", lev, 0.75f, db);

    // tanpura
    p.push_back (std::make_unique<APB> (juce::ParameterID { "n_on", 1 }, "Tanpura on", true));
    { NR r (20, 120); F ("n_cycle", "Tanpura cycle", r, 56, [] (float v) { return juce::String (juce::roundToInt (v)) + " bpm"; }); }
    F ("n_jawari", "Tanpura jawari", unit, 0.8f, pct);
    p.push_back (std::make_unique<APC> (juce::ParameterID { "n_first", 1 }, "Tanpura first string",
                                        juce::StringArray { "Auto", "Pa", "Ma", "Ni", "ni" }, 0));
    F ("n_human", "Tanpura human", unit, 0.5f, pct);
    p.push_back (std::make_unique<APC> (juce::ParameterID { "n_octave", 1 }, "Tanpura register",
                                        juce::StringArray { "Low", "Middle", "High" }, 1));
    F ("n_width", "Tanpura width", unit, 0.6f, pct);
    F ("n_room", "Tanpura room", unit, 0.3f, pct);
    F ("n_level", "Tanpura level", lev, 0.5f, db);
    p.push_back (std::make_unique<APC> (juce::ParameterID { "n_mode", 1 }, "Tanpura mode", juce::StringArray { "Cycle", "Drone" }, 0));
    p.push_back (std::make_unique<APC> (juce::ParameterID { "n_voicing", 1 }, "Drone voicing", juce::StringArray { "Single", "Chord" }, 1));
    p.push_back (std::make_unique<APC> (juce::ParameterID { "n_note", 1 }, "Drone note",
                                        juce::StringArray { "1st string", "Sa", "Sa (2nd)", "Kharaj sa" }, 1));
    for (int i = 0; i < 4; ++i)
    {
        static const char* nm[4] = { "Drone chord 1st string", "Drone chord Sa", "Drone chord Sa (2nd)", "Drone chord kharaj sa" };
        p.push_back (std::make_unique<APB> (juce::ParameterID { "n_dr" + juce::String (i + 1), 1 }, nm[i], true));
    }

   #if TANTRI_SPATIAL
    // ---- the space (Tantri Akasha): one place and motion per source, openfield's distance model
    {
        using namespace tantri::field;
        juce::StringArray motions; for (auto* m : kMotionName) motions.add (m);
        auto deg = [] (float v) { return juce::String (juce::roundToInt (v)) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")); };
        struct D { const char* id; const char* name; float az, el, d; int motion; };
        const D src[4] = { { "sitar", "Sitar", 0, 0, 0.3f, motion::None }, { "tabla", "Tabla", -40, -5, 0.35f, motion::None },
                           { "tanpura", "Tanpura", 40, 5, 0.4f, motion::Drift }, { "input", "Input", 0, 10, 0.3f, motion::None } };
        for (int k = 0; k < kSources; ++k)
        {
            juce::String id (src[k].id), nm (src[k].name);
            p.push_back (std::make_unique<APF> (juce::ParameterID { id + "_az", 1 }, nm + " direction", NR (-180, 180), src[k].az, fmt (deg)));
            p.push_back (std::make_unique<APF> (juce::ParameterID { id + "_el", 1 }, nm + " height", NR (kElLow, kElHigh), src[k].el, fmt (deg)));
            p.push_back (std::make_unique<APF> (juce::ParameterID { id + "_dist", 1 }, nm + " distance", unit, src[k].d, fmt (pct)));
            p.push_back (std::make_unique<APC> (juce::ParameterID { id + "_motion", 1 }, nm + " motion", motions, src[k].motion));
            NR r (0.25f, 4.0f); r.setSkewForCentre (1.0f);
            p.push_back (std::make_unique<APF> (juce::ParameterID { id + "_rate", 1 }, nm + " motion rate", r, 1.0f,
                                                fmt ([] (float v) { return juce::String (v, 2) + "x"; })));
        }
        p.push_back (std::make_unique<APC> (juce::ParameterID { "order", 1 }, "Ambisonic order",
                                            juce::StringArray { "1st", "2nd", "3rd", "4th", "5th", "6th", "7th" }, 2));
        F ("rolloff", "Distance rolloff", unit, 0.5f, pct);
        F ("air", "Air", unit, 0.5f, pct);
        F ("spread", "Near spread", unit, 0.5f, pct);
        F ("blur", "Far blur", unit, 0.0f, pct);
        { NR r (0.02f, 0.5f); F ("near", "Near radius", r, 0.12f, pct); }
        { NR r (0.1f, 4.0f); r.setSkewForCentre (1.0f); F ("mspeed", "Motion speed", r, 1.0f, [] (float v) { return juce::String (v, 2) + "x"; }); }
        F ("yaw", "Scene turn", NR (-180, 180), 0.0f, deg);
        F ("pitch", "Scene tilt", NR (-90, 90), 0.0f, deg);
       #if TANTRI_FX
        F ("i_level", "Input level", lev, 0.8f, db);
        F ("i_symp", "Input sympathy", unit, 0.35f, pct);
       #endif
    }
   #endif
    return { p.begin(), p.end() };
}

// A VST2 host takes a plug-in's outputs as they are: where tracks are stereo (Ableton Live and most
// others) the spatial build is stereo there, or W and Y would be played as left and right (as openfield)
static bool stereoOnlyVst2Host()
{
    static const bool wide = [] {
        const juce::PluginHostType h;
        return h.isReaper() || h.isArdour() || h.isJUCEPluginHost() || h.isPluginval();
    }();
    return ! wide;
}

// Tantri: stereo out. Tantri Akasha: Ambisonics out - 3rd order (16 channels) as an Audio Unit, up
// to 7th (64) elsewhere; the desktop app plays a stereo preview. The FX build takes stereo in.
juce::AudioProcessor::BusesProperties TantriProcessor::buses()
{
    BusesProperties b;
   #if TANTRI_FX
    b = b.withInput ("Input", juce::AudioChannelSet::stereo(), true);
   #endif
   #if TANTRI_SPATIAL
    const auto t = juce::PluginHostType::getPluginLoadedAs();
    if (t == wrapperType_Standalone || t == wrapperType_Undefined || (t == wrapperType_VST && stereoOnlyVst2Host()))
        return b.withOutput ("Output", juce::AudioChannelSet::stereo(), true);
    return b.withOutput ("Output", juce::AudioChannelSet::ambisonic (t == wrapperType_AudioUnit ? 3 : 7), true);
   #else
    return b.withOutput ("Output", juce::AudioChannelSet::stereo(), true);
   #endif
}

TantriProcessor::TantriProcessor()
    : AudioProcessor (buses()),
      apvts (*this, nullptr, "TANTRI", createLayout())
{
    apvts.addParameterListener ("scale", this);
}
TantriProcessor::~TantriProcessor() { apvts.removeParameterListener ("scale", this); }

void TantriProcessor::parameterChanged (const juce::String& id, float v)
{
    if (id == "scale")
    {
        const auto& e = scaleLibrary()[(size_t) juce::jlimit (0, numScales() - 1, juce::roundToInt (v))];
        if (e.swara) setTuning (e.bits);
        else tuningVersion.fetch_add (1);
    }
}

bool TantriProcessor::tuningIsCustom() const
{
    const auto& e = scaleLibrary()[(size_t) juce::jlimit (0, numScales() - 1, scaleIndex())];
    return e.swara && e.bits.v != tuningBits.load();
}

bool TantriProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
   #if TANTRI_SPATIAL
    // REAPER asks a VST3 for the track's width on both sides as plain channels and falls back to
    // stereo if refused: take any width from 4 up and send the highest order that fits
    const auto in0 = l.getMainInputChannelSet();
    if (wrapperType == wrapperType_VST3 && out.size() >= 4 && out.size() <= ambi::kCh && (in0.isDisabled() || in0 == out)) return true;
    bool ok = out == juce::AudioChannelSet::stereo();
    if (! (wrapperType == wrapperType_VST && stereoOnlyVst2Host()))
        for (int o = 1; o <= ambi::kMaxOrder; ++o)
            ok = ok || out == juce::AudioChannelSet::ambisonic (o)
                    || (out == juce::AudioChannelSet::discreteChannels (ambi::chFor (o)) && wrapperType != wrapperType_AudioUnit);
    if (! ok) return false;
   #else
    if (out != juce::AudioChannelSet::stereo()) return false;
   #endif
   #if TANTRI_FX
    const auto in = l.getMainInputChannelSet();
    if (wrapperType != wrapperType_VST3 && in == juce::AudioChannelSet::mono()) return false;   // never a mono input outside VST3
    return in.isDisabled() || in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo() || in == out;
   #else
    return true;
   #endif
}

void TantriProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    sitar.prepare (sr); tabla.prepare (sr); tanpura.prepare (sr); reverb.prepare (sr);
    master.reset (sr, 0.05);
    limiter.prepare (sr);
    setLatencySamples (limiter.latency());
   #if TANTRI_SPATIAL
    limitN.prepare (sr, ambi::kCh);
    for (auto& e : enc) e.reset();
    for (auto& e : encRev) e.reset();
    srcBuf.setSize (10, kChunk);
    inBuf.setSize (2, 4096);
    fold.setSize (4, 4096);
    scrL.assign (kChunk, 0.0f); scrR.assign (kChunk, 0.0f);
    // a VST3 bus of plain speaker channels keeps only its first 19 in order: up to 3rd order
    orderCap = ambi::kMaxOrder;
    if (wrapperType == wrapperType_VST3)
        if (auto* bus = getBus (false, 0))
        {
            const auto& set = bus->getCurrentLayout();
            if (set.size() >= 4 && set.getAmbisonicOrder() < 0 && ! set.isDiscreteLayout()) orderCap = 3;
        }
   #endif
    readParams();
}

void TantriProcessor::readParams()
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    TuningBits t { tuningBits.load() };
    double sa = v ("saHz");
    // rebuild the active scale when the choice or the shruti edits change (no allocation)
    int si = scaleIndex();
    unsigned tv = tuningVersion.load();
    if (si != lastScaleIdx || tv != lastTuningVer)
    {
        scale = makeScale (si, t);
        lastScaleIdx = si; lastTuningVer = tv; ++scaleVersion;
    }
    sitar.setScale (scale, sa, scaleVersion);
    SitarParams sp;
    sp.mizrab = v ("s_mizrab"); sp.jawari = v ("s_jawari"); sp.meendCents = v ("s_meend");
    sp.taraf = v ("s_taraf"); sp.chikari = v ("s_chikari"); sp.gourd = v ("s_gourd"); sp.gamak = v ("s_gamak");
    sp.decay = v ("s_decay"); sp.pluckPos = v ("s_pluck"); sp.release = v ("s_release"); sp.level = v ("s_level");
    sitar.setParams (sp);
    TablaParams tp;
    // dayan: Sa, or the scale's own Pa / Ma / Ni, or upper Sa (one octave above the sitar's Sa)
    int dc = (int) v ("t_dayan");
    double dcents = 0;
    if (dc == 1) dcents = nearestCents (scale, 702, 25, 701.955);
    else if (dc == 2) dcents = nearestCents (scale, 498, 40, 498.045);
    else if (dc == 3) dcents = nearestCents (scale, 1088, 40, 1088.269);
    else if (dc == 4) dcents = 1200;
    tp.dayanRatio = float (2.0 * std::pow (2.0, dcents / 1200.0));
    tp.bayanHz = v ("t_bayan"); tp.syahi = v ("t_syahi"); tp.glide = v ("t_glide"); tp.press = v ("t_press");
    tp.ring = v ("t_ring"); tp.snap = v ("t_snap"); tp.level = v ("t_level");
    tabla.setParams (tp, sa);
    TanpuraParams np;
    np.on = v ("n_on") > 0.5f; np.cycleBpm = v ("n_cycle"); np.jawari = v ("n_jawari"); np.first = (int) v ("n_first");
    np.human = v ("n_human"); np.octave = (int) v ("n_octave") - 1; np.width = v ("n_width"); np.level = v ("n_level");
    np.mode = (int) v ("n_mode"); np.voicing = (int) v ("n_voicing"); np.note = (int) v ("n_note");
    np.chordMask = (v ("n_dr1") > 0.5f ? 1 : 0) | (v ("n_dr2") > 0.5f ? 2 : 0) | (v ("n_dr3") > 0.5f ? 4 : 0) | (v ("n_dr4") > 0.5f ? 8 : 0);
    tanpura.setParams (np, scale, scaleVersion, sa);
    tanpuraFirst.store (tanpura.firstStringChoice());
    reverb.setSize (v ("roomSize"));
}

static bool chanMatches (int setting, int ch) { return setting == 0 || setting == ch; }

void TantriProcessor::handleMidi (const juce::MidiMessage& m)
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const int ch = m.getChannel();
    const int sCh = (int) v ("sitarCh"), tCh = (int) v ("tablaCh"), nCh = (int) v ("tanpuraCh");
    const int key = saKey();
    const bool split = v ("split") > 0.5f;
    const int mapMode = (int) v ("map");

    if (m.isController() && m.getControllerNumber() == 123) { sitar.allOff(); return; }

    // tabla channel (default 10): every key maps to a bol, twelve per octave
    if (tCh != 0 && ch == tCh)
    {
        if (m.isNoteOn()) tabla.play ((m.getNoteNumber() % 12 + 12) % 12, m.getFloatVelocity());
        else if (m.isChannelPressure()) tabla.setPressure (m.getChannelPressureValue() / 127.0f);
        else if (m.isAftertouch()) tabla.setPressure (m.getAfterTouchValue() / 127.0f);
        else if (m.isController() && m.getControllerNumber() == 1) tabla.setPressure (m.getControllerValue() / 127.0f);
        return;
    }
    if (nCh != 0 && ch == nCh)
    {
        if (m.isNoteOn()) { ++tanpuraHeld; tanpura.setGate (true); }
        else if (m.isNoteOff()) { tanpuraHeld = std::max (0, tanpuraHeld - 1); tanpura.setGate (tanpuraHeld > 0); }
        return;
    }
    if (! chanMatches (sCh, ch)) return;

    if (m.isNoteOn())
    {
        int n = m.getNoteNumber();
        if (split && n < key - 12)
        {
            if (n >= key - 24) tabla.play (n - (key - 24), m.getFloatVelocity());   // tabla zone, one octave
            else sitar.chikariStrike (m.getFloatVelocity());                         // below it: chikari
            return;
        }
        sitar.noteOn (n, m.getFloatVelocity(), key, mapMode);
    }
    else if (m.isNoteOff())
    {
        int n = m.getNoteNumber();
        if (split && n < key - 12) return;
        sitar.noteOff (n);
    }
    else if (m.isPitchWheel()) sitar.setBend ((m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isAftertouch()) sitar.polyPressure (m.getNoteNumber(), m.getAfterTouchValue() / 127.0f);
    else if (m.isChannelPressure()) { sitar.setChannelPressure (m.getChannelPressureValue() / 127.0f); }
    else if (m.isController() && m.getControllerNumber() == 1) sitar.setModWheel (m.getControllerValue() / 127.0f);
    else if (m.isAllNotesOff() || m.isAllSoundOff()) sitar.allOff();
}

void TantriProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    keyboard.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);
    readParams();
    if (auto* ph = getPlayHead())
    {
        auto pos = ph->getPosition();
        if (pos && pos->getIsPlaying() && pos->getPpqPosition()) beatPos.store (*pos->getPpqPosition());
        else beatPos.store (-1.0);
    }
   #if TANTRI_SPATIAL
    processSpatial (buffer, midi);
    updateMeters();
    return;
   #endif
    const int n = buffer.getNumSamples();
    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const float sRoom = v ("s_room"), tRoom = v ("t_room"), nRoom = v ("n_room");
    master.setTargetValue (v ("master"));

    float blockPeak = 0;
    auto it = midi.begin();
    for (int i = 0; i < n; ++i)
    {
        while (it != midi.end() && (*it).samplePosition <= i) { handleMidi ((*it).getMessage()); ++it; }
        float sl, sr2, tl, tr, nl, nr;
        sitar.tick (sl, sr2);
        tabla.tick (tl, tr);
        tanpura.tick (nl, nr);
        float rl, rr;
        reverb.process (sl * sRoom + tl * tRoom + nl * nRoom, sr2 * sRoom + tr * tRoom + nr * nRoom, rl, rr);
        float g = master.getNextValue();
        float outL = (sl + tl + nl + rl * 0.7f) * g, outR = (sr2 + tr + nr + rr * 0.7f) * g;
        float pre = std::max (std::fabs (outL), std::fabs (outR));
        if (pre > blockPeak) blockPeak = pre;
        limiter.process (outL, outR);   // transparent look-ahead ceiling at -1 dBFS: never clips
        L[i] = outL; R[i] = outR;
    }
    while (it != midi.end()) { handleMidi ((*it).getMessage()); ++it; }

    outPeak.store (std::max (blockPeak, outPeak.load() * 0.9f));
    float gr = limiter.takeMinGain();
    limitDb.store (std::min (limitDb.load() * 0.9f, juce::Decibels::gainToDecibels (gr)));
    updateMeters();
}

#if TANTRI_SPATIAL
// Tantri Akasha: every source rendered into its own stereo pair, then placed in the space (its home
// plus its motion, or its bird of the flock), turned by the scene rotation and encoded; the room is
// encoded wide and diffuse. One look-ahead ceiling over all channels.
void TantriProcessor::processSpatial (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    using namespace tantri::field;
    const int n = buffer.getNumSamples();
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto vs = [this] (int k, const char* suffix) { return apvts.getRawParameterValue (juce::String (kSrcId[k]) + suffix)->load(); };

    // the input (FX build) is read before the outputs are cleared: the buffer is shared
    const int inCh = getTotalNumInputChannels();
    if (inBuf.getNumSamples() < n) inBuf.setSize (2, n, false, false, true);
    for (int c = 0; c < 2; ++c)
    {
        if (inCh > 0) inBuf.copyFrom (c, 0, buffer, std::min (c, inCh - 1), 0, n);
        else inBuf.clear (c, 0, n);
    }
    const int numOut = getTotalNumOutputChannels();
    buffer.clear();

    SpaceParams sp;
    sp.rolloff = v ("rolloff"); sp.air = v ("air"); sp.spread = v ("spread"); sp.blur = v ("blur"); sp.nearR = v ("near");
    sp.yaw = v ("yaw") * tantri::field::kPi / 180.0f; sp.pitch = v ("pitch") * tantri::field::kPi / 180.0f; sp.mspeed = v ("mspeed");

    // where the output goes: Ambisonics (as many orders as fit), or a 1st-order stereo preview (W +- Y)
    const int wanted = 1 + (int) v ("order");
    float* dst[ambi::kCh] {};
    int nch, order;
    const bool preview = numOut < 4;
    if (! preview)
    {
        nch = std::min (numOut, ambi::kCh);
        order = std::min ({ wanted, ambi::orderFor (nch), orderCap });
        nch = ambi::chFor (order);
        for (int c = 0; c < nch; ++c) dst[c] = buffer.getWritePointer (c);
    }
    else
    {
        if (fold.getNumSamples() < n) fold.setSize (4, n, false, false, true);
        fold.clear();
        nch = 4; order = 1;
        for (int c = 0; c < 4; ++c) dst[c] = fold.getWritePointer (c);
    }
    orderNow.store (preview ? 0 : order);

    const float sRoom = v ("s_room"), tRoom = v ("t_room"), nRoom = v ("n_room");
   #if TANTRI_FX
    const float inLevel = v ("i_level"), symp = v ("i_symp");
   #else
    const float inLevel = 0.0f, symp = 0.0f;
   #endif
    master.setTargetValue (v ("master"));

    // the places (home + motion); motion clocks restart when a source's motion changes
    Place pl[4];
    static constexpr float seeds[4] = { 0.21f, 0.68f, 0.43f, 0.91f };
    for (int k = 0; k < kSources; ++k)
    {
        pl[k].x = xForAzDeg (vs (k, "_az")); pl[k].y = yForElDeg (vs (k, "_el")); pl[k].d = vs (k, "_dist");
        pl[k].motion = (int) vs (k, "_motion"); pl[k].mrate = vs (k, "_rate"); pl[k].mseed = seeds[k];
        if (pl[k].motion != lastMotion[k]) { lastMotion[k] = pl[k].motion; tau[k] = 0.0; }
    }

    float blockPeak = 0;
    float act[4] {};
    auto it = midi.begin();
    for (int pos = 0; pos < n; pos += kChunk)
    {
        const int m = std::min (kChunk, n - pos);
        float* sL[5]; float* sR[5];
        for (int k = 0; k < 5; ++k) { sL[k] = srcBuf.getWritePointer (2 * k); sR[k] = srcBuf.getWritePointer (2 * k + 1); }
        const float* inL = inBuf.getReadPointer (0) + pos;
        const float* inR = inBuf.getReadPointer (1) + pos;
        float g = 1.0f;
        for (int i = 0; i < m; ++i)
        {
            while (it != midi.end() && (*it).samplePosition <= pos + i) { handleMidi ((*it).getMessage()); ++it; }
            sitar.extDrive = symp * 0.0008f * (inL[i] + inR[i]);
            sitar.tick (sL[0][i], sR[0][i]);
            tabla.tick (sL[1][i], sR[1][i]);
            tanpura.tick (sL[2][i], sR[2][i]);
            sL[3][i] = inL[i] * inLevel; sR[3][i] = inR[i] * inLevel;
            reverb.process (sL[0][i] * sRoom + sL[1][i] * tRoom + sL[2][i] * nRoom + sL[3][i] * 0.2f,
                            sR[0][i] * sRoom + sR[1][i] * tRoom + sR[2][i] * nRoom + sR[3][i] * 0.2f, sL[4][i], sR[4][i]);
            g = master.getNextValue();
        }
        for (int k = 0; k < kSources; ++k)
            for (int i = 0; i < m; ++i) act[k] = std::max (act[k], std::max (std::fabs (sL[k][i]), std::fabs (sR[k][i])));

        // move: clocks, the flock, then encode each source
        const float dt = (float) m / (float) sr;
        flock.begin();
        for (int k = 0; k < kSources; ++k)
        {
            tau[k] += dt * sp.mspeed;
            if (pl[k].motion == motion::Boids) flock.add (k, pl[k].x, pl[k].y, pl[k].d, pl[k].mrate * sp.mspeed);
        }
        flock.step (dt);
        float* out[ambi::kCh];
        for (int c = 0; c < nch; ++c) out[c] = dst[c] + pos;
        for (int k = 0; k < kSources; ++k)
        {
            float x, y, d;
            if (pl[k].motion == motion::Boids)
            {
                if (auto* b = flock.find (k)) { x = b->x; y = b->y; d = b->d; }
                else { x = pl[k].x; y = pl[k].y; d = pl[k].d; }
            }
            else moved (pl[k], tau[k], x, y, d);
            srcX[k].store (x); srcY[k].store (y); srcD[k].store (d);
            float az = azimuth (x), el = elevation (y);
            rotate (az, el, sp);
            enc[k].process (sL[k], sR[k], out, nch, order, m, az, el, d, distanceGain (d, sp) * g * 1.7f, sp, sr, 1.0f, scrL.data(), scrR.data());
        }
        // the room: wide, high, diffuse (its higher orders mostly gone)
        for (int s = 0; s < 2; ++s)
        {
            float az = (s == 0 ? 70.0f : -70.0f) * tantri::field::kPi / 180.0f, el = 20.0f * tantri::field::kPi / 180.0f;
            rotate (az, el, sp);
            encRev[s].process (s == 0 ? sL[4] : sR[4], s == 0 ? sL[4] : sR[4], out, nch, order, m, az, el, 0.5f,
                               0.7f * g * 1.2f, sp, sr, 0.35f, scrL.data(), scrR.data());
        }
    }
    while (it != midi.end()) { handleMidi ((*it).getMessage()); ++it; }

    // the stereo preview: fold the 1st order to left / right
    if (preview)
    {
        const float* w = fold.getReadPointer (0);
        const float* y = fold.getReadPointer (1);
        for (int ch = 0; ch < std::min (numOut, 2); ++ch)
        {
            float* d = buffer.getWritePointer (ch);
            const float sgn = ch == 0 ? 1.0f : -1.0f;
            for (int i = 0; i < n; ++i) d[i] = (w[i] + sgn * y[i]) * 0.7071f;
        }
        nch = std::min (numOut, 2);
        for (int c = 0; c < nch; ++c) dst[c] = buffer.getWritePointer (c);
    }
    for (int i = 0; i < n; ++i)
    {
        for (int c = 0; c < nch; ++c) blockPeak = std::max (blockPeak, std::fabs (dst[c][i]));
        limitN.process (dst, nch, i);   // look-ahead ceiling at -1 dBFS over every channel: never clips
    }
    outPeak.store (std::max (blockPeak, outPeak.load() * 0.9f));
    float gr = limitN.takeMinGain();
    limitDb.store (std::min (limitDb.load() * 0.9f, juce::Decibels::gainToDecibels (gr)));
    for (int k = 0; k < kSources; ++k) srcAct[k].store (std::max (act[k], srcAct[k].load() * 0.8f));
}
#endif

void TantriProcessor::updateMeters()
{
    for (int w = 0; w < ActiveScale::kMax; ++w) { swaraAct[w].store (sitar.activity[w]); sitar.activity[w] *= 0.6f; }
    chikAct.store (std::max (chikAct.load() * 0.0f, sitar.chikFlash)); sitar.chikFlash *= 0.7f;
    dayanAct.store (tabla.dayanFlash); tabla.dayanFlash *= 0.75f;
    bayanAct.store (tabla.bayanFlash); tabla.bayanFlash *= 0.75f;
    lastBol.store (tabla.lastBol);
    strokeCount.store (tabla.strokeCount);
    for (int s = 0; s < 4; ++s) { tanpuraAct[s].store (std::max (tanpura.stringLevel (s), tanpura.pluckFlash[s])); tanpuraDrone[s].store (tanpura.droning (s)); }
    tanpuraLast.store (tanpura.lastPlucked);
    voicesUsed.store (sitar.voiceCount);
}

void TantriProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("tuning", juce::String::toHexString ((juce::int64) tuningBits.load()), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void TantriProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.isValid()) return;
        auto tuningHex = tree.getProperty ("tuning").toString();
        apvts.replaceState (tree);
        if (tuningHex.isNotEmpty()) setTuning ({ (uint64_t) tuningHex.getHexValue64() });
    }
}

juce::AudioProcessorEditor* TantriProcessor::createEditor() { return new TantriEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TantriProcessor(); }
