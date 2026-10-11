#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cmath>

constexpr int kNumOsc    = 5;
constexpr int kMaxUnison = 7;
constexpr int kNumVoices = 16;

inline juce::String oscId (int i, const char* name)
{
    return "osc" + juce::String (i + 1) + "_" + name;
}

inline juce::StringArray waveNames()       { return juce::StringArray { "Sine", "Saw", "Square", "Triangle", "Noise" }; }
inline juce::StringArray filterTypeNames() { return juce::StringArray { "LP 12", "LP 24", "HP 12", "BP", "Notch" }; }
inline juce::StringArray lfoShapeNames()   { return juce::StringArray { "Sine", "Triangle", "Square", "Random" }; }

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    using Attr = AudioParameterFloatAttributes;
    using NR   = NormalisableRange<float>;

    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pid     = [] (const String& s) { return ParameterID { s, 1 }; };
    auto secText = [] (float v, int) { return String (v, 3) + " s"; };
    auto hzText  = [] (float v, int) { return String ((int) std::round (v)) + " Hz"; };

    auto addFloat = [&] (const String& id, const String& name, NR range, float def, Attr a = Attr())
    {
        layout.add (std::make_unique<AudioParameterFloat> (pid (id), name, range, def, a));
    };

    // ---------------- Oscillators ----------------
    const int   defOn[kNumOsc]   = { 1, 1, 0, 0, 0 };
    const int   defWave[kNumOsc] = { 1, 1, 0, 2, 4 };
    const int   defOct[kNumOsc]  = { 0, 0, -1, 0, 0 };
    const float defFine[kNumOsc] = { 0.f, 8.f, 0.f, 0.f, 0.f };
    const float defLvl[kNumOsc]  = { 0.8f, 0.6f, 0.5f, 0.4f, 0.15f };

    for (int i = 0; i < kNumOsc; ++i)
    {
        const String n = "Osc " + String (i + 1) + " ";
        layout.add (std::make_unique<AudioParameterBool>   (pid (oscId (i, "on")),   n + "On",     defOn[i] != 0));
        layout.add (std::make_unique<AudioParameterChoice> (pid (oscId (i, "wave")), n + "Wave",   waveNames(), defWave[i]));
        layout.add (std::make_unique<AudioParameterInt>    (pid (oscId (i, "oct")),  n + "Octave", -3, 3, defOct[i]));
        layout.add (std::make_unique<AudioParameterInt>    (pid (oscId (i, "semi")), n + "Semi",   -12, 12, 0));
        addFloat (oscId (i, "fine"),  n + "Fine",  NR (-100.f, 100.f, 0.01f), defFine[i], Attr().withLabel ("ct"));
        addFloat (oscId (i, "level"), n + "Level", NR (0.f, 1.f, 0.001f),     defLvl[i]);
        addFloat (oscId (i, "pan"),   n + "Pan",   NR (-1.f, 1.f, 0.01f),     0.f);
        addFloat (oscId (i, "pw"),    n + "Pulse Width", NR (0.05f, 0.95f, 0.01f), 0.5f);
    }

    // ---------------- Unison ----------------
    layout.add (std::make_unique<AudioParameterInt> (pid ("uni_voices"), "Unison Voices", 1, kMaxUnison, 1));
    addFloat ("uni_detune", "Unison Detune", NR (0.f, 100.f, 0.1f), 15.f, Attr().withLabel ("ct"));
    addFloat ("uni_spread", "Unison Stereo Spread", NR (0.f, 1.f, 0.01f), 0.6f);

    // ---------------- Filter ----------------
    layout.add (std::make_unique<AudioParameterChoice> (pid ("f_type"), "Filter Type", filterTypeNames(), 1));
    addFloat ("f_cutoff", "Filter Cutoff",    NR (20.f, 20000.f, 0.f, 0.3f), 3000.f,
              Attr().withStringFromValueFunction (hzText));
    addFloat ("f_res",    "Filter Resonance", NR (0.f, 1.f, 0.001f), 0.2f);
    addFloat ("f_env",    "Filter Env Amount", NR (-1.f, 1.f, 0.001f), 0.4f);
    addFloat ("f_key",    "Filter Key Track", NR (0.f, 1.f, 0.001f), 0.3f);

    // ---------------- Envelopes ----------------
    auto timeRange = [] (float maxV) { return NR (0.001f, maxV, 0.f, 0.35f); };
    addFloat ("amp_a", "Amp Attack",  timeRange (5.f), 0.005f, Attr().withStringFromValueFunction (secText));
    addFloat ("amp_d", "Amp Decay",   timeRange (5.f), 0.3f,   Attr().withStringFromValueFunction (secText));
    addFloat ("amp_s", "Amp Sustain", NR (0.f, 1.f, 0.001f),   0.7f);
    addFloat ("amp_r", "Amp Release", timeRange (8.f), 0.25f,  Attr().withStringFromValueFunction (secText));

    addFloat ("flt_a", "Filter Attack",  timeRange (5.f), 0.005f, Attr().withStringFromValueFunction (secText));
    addFloat ("flt_d", "Filter Decay",   timeRange (5.f), 0.4f,   Attr().withStringFromValueFunction (secText));
    addFloat ("flt_s", "Filter Sustain", NR (0.f, 1.f, 0.001f),   0.3f);
    addFloat ("flt_r", "Filter Release", timeRange (8.f), 0.3f,   Attr().withStringFromValueFunction (secText));

    // ---------------- LFO ----------------
    layout.add (std::make_unique<AudioParameterChoice> (pid ("lfo_shape"), "LFO Shape", lfoShapeNames(), 0));
    addFloat ("lfo_rate",   "LFO Rate",         NR (0.05f, 20.f, 0.f, 0.4f), 4.f, Attr().withLabel ("Hz"));
    addFloat ("lfo_pitch",  "LFO > Pitch",      NR (0.f, 12.f, 0.01f), 0.f, Attr().withLabel ("st"));
    addFloat ("lfo_filter", "LFO > Filter",     NR (0.f, 1.f, 0.001f), 0.f);
    addFloat ("lfo_amp",    "LFO > Amp",        NR (0.f, 1.f, 0.001f), 0.f);

    // ---------------- Effects / Output ----------------
    addFloat ("fx_drive", "Drive",        NR (0.f, 1.f, 0.001f), 0.f);
    addFloat ("dly_time", "Delay Time",   NR (10.f, 1000.f, 0.f, 0.5f), 350.f, Attr().withLabel ("ms"));
    addFloat ("dly_fb",   "Delay Feedback", NR (0.f, 0.95f, 0.001f), 0.35f);
    addFloat ("dly_mix",  "Delay Mix",    NR (0.f, 1.f, 0.001f), 0.f);
    addFloat ("rvb_size", "Reverb Size",  NR (0.f, 1.f, 0.001f), 0.6f);
    addFloat ("rvb_damp", "Reverb Damping", NR (0.f, 1.f, 0.001f), 0.5f);
    addFloat ("rvb_mix",  "Reverb Mix",   NR (0.f, 1.f, 0.001f), 0.f);
    addFloat ("master",   "Master",       NR (-48.f, 6.f, 0.1f), -6.f, Attr().withLabel ("dB"));

    return layout;
}

// Raw pointers to all parameters (read from the audio thread)
struct ParamPtrs
{
    std::atomic<float>* on[kNumOsc]    {};
    std::atomic<float>* wave[kNumOsc]  {};
    std::atomic<float>* oct[kNumOsc]   {};
    std::atomic<float>* semi[kNumOsc]  {};
    std::atomic<float>* fine[kNumOsc]  {};
    std::atomic<float>* level[kNumOsc] {};
    std::atomic<float>* pan[kNumOsc]   {};
    std::atomic<float>* pw[kNumOsc]    {};

    std::atomic<float> *uniVoices {}, *uniDetune {}, *uniSpread {};
    std::atomic<float> *fType {}, *fCutoff {}, *fRes {}, *fEnv {}, *fKey {};
    std::atomic<float> *ampA {}, *ampD {}, *ampS {}, *ampR {};
    std::atomic<float> *fltA {}, *fltD {}, *fltS {}, *fltR {};
    std::atomic<float> *lfoShape {}, *lfoRate {}, *lfoPitch {}, *lfoFilter {}, *lfoAmp {};
    std::atomic<float> *drive {}, *dlyTime {}, *dlyFb {}, *dlyMix {};
    std::atomic<float> *rvbSize {}, *rvbDamp {}, *rvbMix {}, *master {};

    void bind (juce::AudioProcessorValueTreeState& s)
    {
        auto get = [&] (const juce::String& id) { auto* p = s.getRawParameterValue (id); jassert (p != nullptr); return p; };

        for (int i = 0; i < kNumOsc; ++i)
        {
            on[i]    = get (oscId (i, "on"));
            wave[i]  = get (oscId (i, "wave"));
            oct[i]   = get (oscId (i, "oct"));
            semi[i]  = get (oscId (i, "semi"));
            fine[i]  = get (oscId (i, "fine"));
            level[i] = get (oscId (i, "level"));
            pan[i]   = get (oscId (i, "pan"));
            pw[i]    = get (oscId (i, "pw"));
        }

        uniVoices = get ("uni_voices"); uniDetune = get ("uni_detune"); uniSpread = get ("uni_spread");
        fType = get ("f_type"); fCutoff = get ("f_cutoff"); fRes = get ("f_res"); fEnv = get ("f_env"); fKey = get ("f_key");
        ampA = get ("amp_a"); ampD = get ("amp_d"); ampS = get ("amp_s"); ampR = get ("amp_r");
        fltA = get ("flt_a"); fltD = get ("flt_d"); fltS = get ("flt_s"); fltR = get ("flt_r");
        lfoShape = get ("lfo_shape"); lfoRate = get ("lfo_rate"); lfoPitch = get ("lfo_pitch");
        lfoFilter = get ("lfo_filter"); lfoAmp = get ("lfo_amp");
        drive = get ("fx_drive"); dlyTime = get ("dly_time"); dlyFb = get ("dly_fb"); dlyMix = get ("dly_mix");
        rvbSize = get ("rvb_size"); rvbDamp = get ("rvb_damp"); rvbMix = get ("rvb_mix"); master = get ("master");
    }
};
