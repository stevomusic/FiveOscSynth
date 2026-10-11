#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    struct Preset
    {
        const char* name;
        std::vector<std::pair<juce::String, float>> values;
    };

    const std::vector<Preset>& getPresets()
    {
        static const std::vector<Preset> presets =
        {
            { "Init", {} },

            { "Fat Saw Lead", {
                { "osc3_on", 1 }, { "osc3_wave", 2 }, { "osc3_oct", 0 }, { "osc3_level", 0.35f },
                { "osc2_fine", 7 },
                { "uni_voices", 3 }, { "uni_detune", 18 },
                { "f_type", 1 }, { "f_cutoff", 3500 }, { "f_res", 0.3f }, { "f_env", 0.4f },
                { "amp_a", 0.005f }, { "amp_d", 0.3f }, { "amp_s", 0.7f }, { "amp_r", 0.25f },
                { "flt_d", 0.4f }, { "flt_s", 0.3f },
                { "dly_mix", 0.2f }, { "rvb_mix", 0.15f } } },

            { "Sub Bass", {
                { "osc1_wave", 0 }, { "osc1_oct", -1 }, { "osc1_level", 0.9f },
                { "osc2_wave", 1 }, { "osc2_oct", -1 }, { "osc2_fine", 0 }, { "osc2_level", 0.5f },
                { "f_type", 1 }, { "f_cutoff", 400 }, { "f_res", 0.25f }, { "f_env", 0.5f },
                { "amp_a", 0.003f }, { "amp_d", 0.2f }, { "amp_s", 0.8f }, { "amp_r", 0.1f },
                { "flt_d", 0.25f }, { "flt_s", 0.f } } },

            { "Soft Pad", {
                { "osc2_fine", 12 },
                { "osc3_on", 1 }, { "osc3_wave", 3 }, { "osc3_oct", 1 }, { "osc3_level", 0.3f },
                { "uni_voices", 5 }, { "uni_detune", 25 }, { "uni_spread", 0.8f },
                { "f_cutoff", 1800 }, { "f_res", 0.15f }, { "f_env", 0.3f },
                { "amp_a", 1.2f }, { "amp_d", 1.f }, { "amp_s", 0.8f }, { "amp_r", 2.f },
                { "flt_a", 1.5f }, { "flt_d", 2.f }, { "flt_s", 0.5f },
                { "lfo_rate", 0.3f }, { "lfo_filter", 0.15f },
                { "dly_time", 450 }, { "dly_mix", 0.25f },
                { "rvb_size", 0.8f }, { "rvb_mix", 0.4f } } },

            { "Pluck", {
                { "osc1_wave", 2 }, { "osc1_pw", 0.4f },
                { "osc2_oct", 1 }, { "osc2_fine", 5 }, { "osc2_level", 0.4f },
                { "f_cutoff", 900 }, { "f_res", 0.3f }, { "f_env", 0.8f },
                { "flt_d", 0.18f }, { "flt_s", 0.f },
                { "amp_a", 0.001f }, { "amp_d", 0.35f }, { "amp_s", 0.f }, { "amp_r", 0.2f },
                { "dly_fb", 0.35f }, { "dly_mix", 0.2f }, { "rvb_mix", 0.2f } } },
        };
        return presets;
    }
}

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    params.bind (apvts);

    for (int i = 0; i < kNumVoices; ++i)
        synth.addVoice (new SynthVoice (params));
    synth.addSound (new SynthSound());
    synth.setCurrentPlaybackSampleRate (44100.0);
}

void PluginProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);

    const size_t size = (size_t) (sampleRate * 1.2) + 4;
    for (auto& b : delayBuf)
        b.assign (size, 0.f);
    delayWrite = 0;
    delaySmoothed = (float) (0.35 * sampleRate);

    reverb.setSampleRate (sampleRate);
    reverb.reset();
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    synth.renderNextBlock (buffer, midi, 0, numSamples);

    if (buffer.getNumChannels() < 2)
        return;

    auto* L = buffer.getWritePointer (0);
    auto* R = buffer.getWritePointer (1);

    // ---- Drive ----
    const float drive = params.drive->load();
    if (drive > 0.001f)
    {
        const float pre  = 1.f + drive * 19.f;
        const float comp = 1.f / std::sqrt (pre);
        for (int i = 0; i < numSamples; ++i)
        {
            L[i] = std::tanh (L[i] * pre) * comp;
            R[i] = std::tanh (R[i] * pre) * comp;
        }
    }

    // ---- Ping-pong delay ----
    {
        const float mix = params.dlyMix->load();
        const float fb  = params.dlyFb->load();
        const float target = params.dlyTime->load() * 0.001f * (float) getSampleRate();
        const int   size = (int) delayBuf[0].size();

        for (int i = 0; i < numSamples; ++i)
        {
            delaySmoothed += 0.0005f * (target - delaySmoothed);

            float rp = (float) delayWrite - delaySmoothed;
            while (rp < 0.f) rp += (float) size;
            const int   i0 = (int) rp % size;
            const int   i1 = (i0 + 1) % size;
            const float fr = rp - std::floor (rp);

            const float dL = delayBuf[0][(size_t) i0] + fr * (delayBuf[0][(size_t) i1] - delayBuf[0][(size_t) i0]);
            const float dR = delayBuf[1][(size_t) i0] + fr * (delayBuf[1][(size_t) i1] - delayBuf[1][(size_t) i0]);

            delayBuf[0][(size_t) delayWrite] = L[i] + fb * dR;
            delayBuf[1][(size_t) delayWrite] = R[i] + fb * dL;

            L[i] += mix * dL;
            R[i] += mix * dR;

            if (++delayWrite >= size) delayWrite = 0;
        }
    }

    // ---- Reverb ----
    {
        juce::Reverb::Parameters rp;
        rp.roomSize   = params.rvbSize->load();
        rp.damping    = params.rvbDamp->load();
        rp.wetLevel   = params.rvbMix->load() * 0.4f;
        rp.dryLevel   = 1.0f;
        rp.width      = 1.0f;
        rp.freezeMode = 0.f;
        reverb.setParameters (rp);
        reverb.processStereo (L, R, numSamples);
    }

    // ---- Master + safety clip ----
    const float gain = juce::Decibels::decibelsToGain (params.master->load());
    for (int i = 0; i < numSamples; ++i)
    {
        L[i] = juce::jlimit (-1.f, 1.f, L[i] * gain);
        R[i] = juce::jlimit (-1.f, 1.f, R[i] * gain);
    }
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

int PluginProcessor::getNumPrograms() { return (int) getPresets().size(); }

const juce::String PluginProcessor::getProgramName (int index)
{
    if (index >= 0 && index < getNumPrograms())
        return getPresets()[(size_t) index].name;
    return {};
}

void PluginProcessor::setCurrentProgram (int index)
{
    if (index < 0 || index >= getNumPrograms())
        return;

    currentProgram = index;

    for (auto* p : getParameters())
        p->setValueNotifyingHost (p->getDefaultValue());

    for (auto& kv : getPresets()[(size_t) index].values)
        if (auto* p = apvts.getParameter (kv.first))
            p->setValueNotifyingHost (p->convertTo0to1 (kv.second));
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
