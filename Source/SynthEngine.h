#pragma once
#include "Parameters.h"

struct SynthSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

class SynthVoice : public juce::SynthesiserVoice
{
public:
    explicit SynthVoice (const ParamPtrs& params) : P (params) {}

    bool canPlaySound (juce::SynthesiserSound* s) override
    {
        return dynamic_cast<SynthSound*> (s) != nullptr;
    }

    void setCurrentPlaybackSampleRate (double newRate) override
    {
        juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
        ampEnv.setSampleRate (newRate);
        fltEnv.setSampleRate (newRate);
    }

    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int pitchWheel) override
    {
        note = midiNoteNumber;
        vel  = velocity;
        bend = (float) (pitchWheel - 8192) / 8192.f * 2.f;

        for (auto& row : phase)
            for (auto& ph : row)
                ph = rng.nextFloat();

        lfoPhase = 0.f;
        shValue  = rng.nextFloat() * 2.f - 1.f;

        for (auto& st : ic1) for (auto& v : st) v = 0.f;
        for (auto& st : ic2) for (auto& v : st) v = 0.f;

        updateEnvelopes();
        ampEnv.noteOn();
        fltEnv.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            ampEnv.noteOff();
            fltEnv.noteOff();
        }
        else
        {
            ampEnv.reset();
            fltEnv.reset();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved (int newValue) override
    {
        bend = (float) (newValue - 8192) / 8192.f * 2.f;   // +/- 2 semitones
    }

    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& out, int startSample, int numSamples) override
    {
        if (! isVoiceActive())
            return;

        const float sr = (float) getSampleRate();
        updateEnvelopes();

        // ---- block-constant values ----
        const int   nUni      = juce::jlimit (1, kMaxUnison, (int) P.uniVoices->load());
        const float uniDetune = P.uniDetune->load();
        const float uniSpread = P.uniSpread->load();
        const float uniNorm   = 1.f / std::sqrt ((float) nUni);

        float uniRatio[kMaxUnison], uniPos[kMaxUnison];
        for (int u = 0; u < nUni; ++u)
        {
            uniPos[u]   = nUni > 1 ? ((float) u / (float) (nUni - 1)) * 2.f - 1.f : 0.f;
            uniRatio[u] = std::exp2 (uniPos[u] * uniDetune / 1200.f);
        }

        bool  on[kNumOsc];
        int   wave[kNumOsc];
        float pw[kNumOsc], oscRatio[kNumOsc];
        float gainL[kNumOsc][kMaxUnison], gainR[kNumOsc][kMaxUnison];

        for (int i = 0; i < kNumOsc; ++i)
        {
            on[i]   = P.on[i]->load() > 0.5f;
            wave[i] = (int) P.wave[i]->load();
            pw[i]   = P.pw[i]->load();

            const float semis = P.oct[i]->load() * 12.f + P.semi[i]->load() + P.fine[i]->load() / 100.f;
            oscRatio[i] = std::exp2 (semis / 12.f);

            const float level = P.level[i]->load() * uniNorm * 0.35f;
            const float pan0  = P.pan[i]->load();
            for (int u = 0; u < nUni; ++u)
            {
                const float pan = juce::jlimit (-1.f, 1.f, pan0 + uniPos[u] * uniSpread);
                const float ang = (pan + 1.f) * juce::MathConstants<float>::pi * 0.25f;
                gainL[i][u] = level * std::cos (ang) * 1.41421356f;
                gainR[i][u] = level * std::sin (ang) * 1.41421356f;
            }
        }

        const int   fType     = (int) P.fType->load();
        const float cutoff    = P.fCutoff->load();
        const float res       = P.fRes->load();
        const float envAmt    = P.fEnv->load();
        const float keyTrack  = P.fKey->load();
        const float k         = 2.f - 1.9f * res;   // damping (lower = more resonance)

        const int   lfoShape  = (int) P.lfoShape->load();
        const float lfoInc    = P.lfoRate->load() / sr;
        const float lfoPitch  = P.lfoPitch->load();
        const float lfoFilter = P.lfoFilter->load();
        const float lfoAmp    = P.lfoAmp->load();

        const int stages = (fType == 1) ? 2 : 1;
        const int mode   = (fType == 2) ? 1 : (fType == 3) ? 2 : (fType == 4) ? 3 : 0;

        float g = 0.f, a1 = 0.f, a2 = 0.f, a3 = 0.f;

        auto* outL = out.getWritePointer (0, startSample);
        auto* outR = out.getNumChannels() > 1 ? out.getWritePointer (1, startSample) : nullptr;

        for (int n = 0; n < numSamples; ++n)
        {
            const float aE = ampEnv.getNextSample();
            const float fE = fltEnv.getNextSample();

            if (! ampEnv.isActive())
            {
                clearCurrentNote();
                break;
            }

            // LFO
            float lfo = 0.f;
            switch (lfoShape)
            {
                case 0:  lfo = std::sin (juce::MathConstants<float>::twoPi * lfoPhase); break;
                case 1:  lfo = 4.f * std::abs (lfoPhase - 0.5f) - 1.f; break;
                case 2:  lfo = lfoPhase < 0.5f ? 1.f : -1.f; break;
                default: lfo = shValue; break;
            }
            lfoPhase += lfoInc;
            if (lfoPhase >= 1.f)
            {
                lfoPhase -= 1.f;
                shValue = rng.nextFloat() * 2.f - 1.f;
            }

            // Pitch
            const float semitone = (float) note + bend + lfo * lfoPitch;
            const float baseFreq = 440.f * std::exp2 ((semitone - 69.f) / 12.f);

            // Oscillators
            float sumL = 0.f, sumR = 0.f;
            for (int i = 0; i < kNumOsc; ++i)
            {
                if (! on[i]) continue;

                for (int u = 0; u < nUni; ++u)
                {
                    const float f  = baseFreq * oscRatio[i] * uniRatio[u];
                    const float dt = juce::jmin (f / sr, 0.45f);

                    float& ph = phase[i][u];
                    const float s = oscSample (wave[i], ph, dt, pw[i]);

                    ph += dt;
                    if (ph >= 1.f) ph -= 1.f;

                    sumL += s * gainL[i][u];
                    sumR += s * gainR[i][u];
                }
            }

            // Filter coefficients (control rate)
            if ((n & 15) == 0)
            {
                const float octaves = envAmt * 6.f * fE
                                    + lfo * lfoFilter * 4.f
                                    + keyTrack * ((float) note - 60.f) / 12.f;
                const float fc = juce::jlimit (20.f, juce::jmin (20000.f, sr * 0.45f),
                                               cutoff * std::exp2 (octaves));
                g  = std::tan (juce::MathConstants<float>::pi * fc / sr);
                a1 = 1.f / (1.f + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;
            }

            for (int s = 0; s < stages; ++s)
            {
                sumL = svf (sumL, ic1[s][0], ic2[s][0], a1, a2, a3, k, mode);
                sumR = svf (sumR, ic1[s][1], ic2[s][1], a1, a2, a3, k, mode);
            }

            const float tremolo = 1.f - lfoAmp * (0.5f - 0.5f * lfo);
            const float gain    = aE * vel * tremolo;

            outL[n] += sumL * gain;
            if (outR != nullptr) outR[n] += sumR * gain;
        }
    }

private:
    static float polyBlep (float t, float dt)
    {
        if (t < dt)       { t /= dt;           return t + t - t * t - 1.f; }
        if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
        return 0.f;
    }

    float oscSample (int wave, float ph, float dt, float pw)
    {
        switch (wave)
        {
            case 0: return std::sin (juce::MathConstants<float>::twoPi * ph);
            case 1: return 2.f * ph - 1.f - polyBlep (ph, dt);
            case 2:
            {
                float v = ph < pw ? 1.f : -1.f;
                v += polyBlep (ph, dt);
                float t2 = ph - pw;
                if (t2 < 0.f) t2 += 1.f;
                v -= polyBlep (t2, dt);
                return v;
            }
            case 3: return 4.f * std::abs (ph - 0.5f) - 1.f;
            default: return rng.nextFloat() * 2.f - 1.f;
        }
    }

    // TPT state-variable filter (Zavalishin)
    static float svf (float x, float& s1, float& s2, float a1, float a2, float a3, float k, int mode)
    {
        const float v3 = x - s2;
        const float v1 = a1 * s1 + a2 * v3;
        const float v2 = s2 + a2 * s1 + a3 * v3;
        s1 = 2.f * v1 - s1;
        s2 = 2.f * v2 - s2;

        switch (mode)
        {
            case 1:  return x - k * v1 - v2;   // high-pass
            case 2:  return k * v1;            // band-pass (peak ~ unity)
            case 3:  return x - k * v1;        // notch
            default: return v2;                // low-pass
        }
    }

    void updateEnvelopes()
    {
        ampEnv.setParameters ({ P.ampA->load(), P.ampD->load(), P.ampS->load(), P.ampR->load() });
        fltEnv.setParameters ({ P.fltA->load(), P.fltD->load(), P.fltS->load(), P.fltR->load() });
    }

    const ParamPtrs& P;
    juce::ADSR ampEnv, fltEnv;
    juce::Random rng;

    int   note = 60;
    float vel = 1.f, bend = 0.f;
    float phase[kNumOsc][kMaxUnison] {};
    float lfoPhase = 0.f, shValue = 0.f;
    float ic1[2][2] {}, ic2[2][2] {};   // [stage][channel]
};
