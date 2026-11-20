#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace tantri;
using APF = juce::AudioParameterFloat;
using APC = juce::AudioParameterChoice;
using APB = juce::AudioParameterBool;
using API = juce::AudioParameterInt;
using NR = juce::NormalisableRange<float>;

juce::StringArray TantriProcessor::ragaNames()
{
    juce::StringArray s;
    for (auto& r : kRagas) s.add (r.name);
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
    p.push_back (std::make_unique<APC> (juce::ParameterID { "raga", 1 }, "Raga", ragaNames(), 0));
    p.push_back (std::make_unique<APC> (juce::ParameterID { "lock", 1 }, "Raga lock", juce::StringArray { "Off", "Snap", "Mute" }, 0));
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
    return { p.begin(), p.end() };
}

TantriProcessor::TantriProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TANTRI", createLayout())
{
    apvts.addParameterListener ("raga", this);
}
TantriProcessor::~TantriProcessor() { apvts.removeParameterListener ("raga", this); }

void TantriProcessor::parameterChanged (const juce::String& id, float v)
{
    if (id == "raga")
    {
        int i = juce::jlimit (0, (int) kRagas.size() - 1, juce::roundToInt (v));
        setTuning (parseRaga (kRagas[(size_t) i].spec));
    }
}

bool TantriProcessor::tuningIsCustom() const
{
    int i = (int) apvts.getRawParameterValue ("raga")->load();
    return parseRaga (kRagas[(size_t) juce::jlimit (0, (int) kRagas.size() - 1, i)].spec).v != tuningBits.load();
}

bool TantriProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void TantriProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    sitar.prepare (sr); tabla.prepare (sr); tanpura.prepare (sr); reverb.prepare (sr);
    master.reset (sr, 0.05);
    limiter.prepare (sr);
    setLatencySamples (limiter.latency());
    readParams();
}

void TantriProcessor::readParams()
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    TuningBits t { tuningBits.load() };
    double sa = v ("saHz");
    sitar.setTuning (t, sa, tuningVersion.load());
    SitarParams sp;
    sp.mizrab = v ("s_mizrab"); sp.jawari = v ("s_jawari"); sp.meendCents = v ("s_meend");
    sp.taraf = v ("s_taraf"); sp.chikari = v ("s_chikari"); sp.gourd = v ("s_gourd"); sp.gamak = v ("s_gamak");
    sp.decay = v ("s_decay"); sp.pluckPos = v ("s_pluck"); sp.release = v ("s_release"); sp.level = v ("s_level");
    sitar.setParams (sp);
    TablaParams tp;
    static const float dayanR[5] = { 2.0f, 3.0f, 8.0f / 3.0f, 0, 4.0f };
    int dc = (int) v ("t_dayan");
    tp.dayanRatio = dc == 3 ? float (t.ratio (11)) * 2.0f : dayanR[dc];
    if (dc == 2) tp.dayanRatio = float (t.ratio (t.in (6) && ! t.in (5) ? 6 : 5)) * 2.0f;
    tp.bayanHz = v ("t_bayan"); tp.syahi = v ("t_syahi"); tp.glide = v ("t_glide"); tp.press = v ("t_press");
    tp.ring = v ("t_ring"); tp.snap = v ("t_snap"); tp.level = v ("t_level");
    tabla.setParams (tp, sa);
    TanpuraParams np;
    np.on = v ("n_on") > 0.5f; np.cycleBpm = v ("n_cycle"); np.jawari = v ("n_jawari"); np.first = (int) v ("n_first");
    np.human = v ("n_human"); np.octave = (int) v ("n_octave") - 1; np.width = v ("n_width"); np.level = v ("n_level");
    tanpura.setParams (np, t, sa);
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
    const int lock = (int) v ("lock");

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
        sitar.noteOn (n, m.getFloatVelocity(), key, lock);
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

    // meters
    for (int w = 0; w < 12; ++w) { swaraAct[w].store (sitar.activity[w]); sitar.activity[w] *= 0.6f; }
    chikAct.store (std::max (chikAct.load() * 0.0f, sitar.chikFlash)); sitar.chikFlash *= 0.7f;
    dayanAct.store (tabla.dayanFlash); tabla.dayanFlash *= 0.75f;
    bayanAct.store (tabla.bayanFlash); tabla.bayanFlash *= 0.75f;
    lastBol.store (tabla.lastBol);
    strokeCount.store (tabla.strokeCount);
    for (int s = 0; s < 4; ++s) tanpuraAct[s].store (std::max (tanpura.stringLevel (s), tanpura.pluckFlash[s]));
    tanpuraLast.store (tanpura.lastPlucked);
    voicesUsed.store (sitar.voiceCount);
    outPeak.store (std::max (blockPeak, outPeak.load() * 0.9f));
    float gr = limiter.takeMinGain();
    limitDb.store (std::min (limitDb.load() * 0.9f, juce::Decibels::gainToDecibels (gr)));
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
