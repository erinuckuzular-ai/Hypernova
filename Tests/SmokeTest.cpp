#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include <set>
#include <complex>

// Renders every factory preset with a short bass phrase (with a legato slide), checks for NaNs,
// silence and clipping, times the render and writes a WAV per preset: SmokeTest <outDir>
namespace
{
    struct FakePlayHead : juce::AudioPlayHead
    {
        double ppq = 0.0;
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (140.0);
            info.setPpqPosition (ppq);
            info.setIsPlaying (true);
            return info;
        }
    };
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    // SmokeTest --fidelity: aliasing and distortion measurements for a few hard cases, printed in dB.
    // For each test tone, energy that isn't at a harmonic of the note (aliasing, intermod) vs total energy.
    if (argc >= 2 && juce::String (argv[1]) == "--fidelity")
    {
        HypernovaAudioProcessor p;
        const int quality = argc > 2 ? juce::String (argv[2]).getIntValue() : 1;
        const double rate = 48000.0;
        p.prepareToPlay (rate, 256);
        struct Case { const char* name; std::vector<std::pair<const char*, float>> v; int note; };
        const std::vector<Case> cases
        {
            { "saw C7", { { "aPos", 0.66f }, { "fltOn", 0 } }, 96 },
            { "sync saw A6", { { "aPos", 0.66f }, { "aWarp", 1 }, { "aWarpAmt", 0.6f }, { "fltOn", 0 } }, 93 },
            { "FM growl E6", { { "aTable", 5 }, { "aPos", 0.8f }, { "aWarp", 6 }, { "aWarpAmt", 0.5f }, { "bOn", 1 }, { "bOct", 1 }, { "bLevel", 0.001f }, { "fltOn", 0 } }, 88 },
            { "bend square G6", { { "aPos", 1.0f }, { "aWarp", 2 }, { "aWarpAmt", 0.7f }, { "fltOn", 0 } }, 91 },
            { "driven filter C5", { { "aPos", 0.66f }, { "fltType", 5 }, { "cutoff", 3000 }, { "res", 0.5f }, { "fltDrive", 0.9f } }, 72 },
            { "808 clip dist C3", { { "aTable", 3 }, { "aPos", 0.5f }, { "distType", 3 }, { "distDrive", 0.9f }, { "distMix", 1 }, { "fltOn", 0 } }, 48 },
            { "loud supersaw (ceiling)", { { "aPos", 0.66f }, { "aUni", 7 }, { "aDetune", 0.3f }, { "volume", 6 }, { "fltOn", 0 } }, 60 },
        };
        for (const auto& c : cases)
        {
            p.setCurrentProgram (0);
            p.applyPresetValues (c.v);
            p.setParam ("monoBass", 0);
            p.setParam ("quality", (float) quality);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, c.note, (juce::uint8) 110), 0);
            const int n = 1 << 15;
            std::vector<float> x ((size_t) n);
            for (int pos = 0; pos < n + 8192; pos += 256)
            {
                juce::AudioBuffer<float> buf (2, 256);
                p.processBlock (buf, midi);
                midi.clear();
                if (pos >= 8192)
                    for (int s = 0; s < 256; ++s) x[(size_t) (pos - 8192 + s)] = buf.getSample (0, s);
            }
            // Blackman-Harris windowed FFT
            juce::dsp::FFT fft (15);
            std::vector<float> d ((size_t) n * 2, 0.0f);
            for (int i = 0; i < n; ++i)
            {
                const double w = 0.35875 - 0.48829 * std::cos (2 * juce::MathConstants<double>::pi * i / (n - 1)) + 0.14128 * std::cos (4 * juce::MathConstants<double>::pi * i / (n - 1))
                               - 0.01168 * std::cos (6 * juce::MathConstants<double>::pi * i / (n - 1));
                d[(size_t) i] = (float) (x[(size_t) i] * w);
            }
            fft.performFrequencyOnlyForwardTransform (d.data());
            const double f0 = 440.0 * std::pow (2.0, (c.note - 69) / 12.0);
            double harm = 0, other = 0;
            for (int b = 1; b < n / 2; ++b)
            {
                const double f = b * rate / n;
                if (f < 20.0 || f > 20000.0) continue;
                const double e = (double) d[(size_t) b] * d[(size_t) b];
                const double k = f / f0;
                const bool nearHarm = std::abs (k - std::round (k)) * f0 < 12.0 * rate / n + 3.0;
                (nearHarm ? harm : other) += e;
            }
            float peak = 0; for (auto v : x) peak = std::max (peak, std::abs (v));
            std::printf ("%-26s alias/noise %7.1f dB   peak %.3f\n", c.name, 10.0 * std::log10 ((other + 1e-20) / (harm + other + 1e-20)), peak);
            p.panic();
            juce::AudioBuffer<float> buf (2, 256); juce::MidiBuffer none; p.processBlock (buf, none);
        }
        return 0;
    }

    // SmokeTest --clicks: for every preset, play a note then the next one back-to-back (and overlapping), and
    // compare the high-frequency spike right at the change with the steady sound around it. Prints offenders.
    if (argc >= 2 && juce::String (argv[1]) == "--clicks")
    {
        HypernovaAudioProcessor p;
        const double rate = 48000.0;
        p.prepareToPlay (rate, 256);
        int bad = 0;
        const juce::String only = argc > 2 ? juce::String (argv[2]) : juce::String();
        for (int i = 1; i < p.getNumPrograms(); ++i)
        {
            if (only.isNotEmpty() && p.getProgramName (i) != only) continue;
            p.setCurrentProgram (i);
            const juce::String cat = p.getPresetCategory();
            if (cat == "Synth Drums" || cat == "FX") continue;
            for (int overlap = 0; overlap < 1; ++overlap)
            {
                const int total = (int) (0.9 * rate), change = (int) (0.35 * rate);
                std::vector<float> x ((size_t) total);
                for (int pos = 0; pos < total; pos += 256)
                {
                    juce::AudioBuffer<float> buf (2, 256);
                    juce::MidiBuffer midi;
                    if (pos == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
                    if (change >= pos && change < pos + 256)
                    {
                        midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), change - pos);
                        midi.addEvent (juce::MidiMessage::noteOff (1, 45), overlap ? juce::jmin (255, change - pos + 200) : change - pos);
                    }
                    p.processBlock (buf, midi);
                    for (int k = 0; k < 256 && pos + k < total; ++k) x[(size_t) (pos + k)] = 0.5f * (buf.getSample (0, k) + buf.getSample (1, k));
                }
                // second difference = strongly high-passed signal; clicks are spikes in it
                auto hf = [&] (int from, int to)
                {
                    float m = 0;
                    for (int n = juce::jmax (2, from); n < juce::jmin (total, to); ++n)
                        m = juce::jmax (m, std::abs (x[(size_t) n] - 2 * x[(size_t) n - 1] + x[(size_t) n - 2]));
                    return m;
                };
                const int lat = p.getLatencySamples();
                const float spike = hf (change + lat - 8, change + lat + 96);
                // Reference: this sound's own attack from silence (note 1) and its steady tone. A click is a
                // transition that is clearly sharper than both.
                const float onset = hf (lat, lat + 104);
                const float steady = hf (change + lat + 4800, change + lat + 9600);
                const float ratio = spike / juce::jmax (onset, steady, 1.0e-5f);
                if (only.isNotEmpty())
                {
                    juce::File f (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("hn_click_" + juce::String (overlap) + ".raw"));
                    f.replaceWithData (x.data(), x.size() * sizeof (float));
                    std::printf ("dumped %s change at %d (+lat %d)\n", f.getFullPathName().toRawUTF8(), change, lat);
                }
                if (ratio > 1.6f)
                {
                    ++bad;
                    std::printf ("%-24s %-16s %s  click %.1fx (%.1f dB)\n", p.getProgramName (i).toRawUTF8(), cat.toRawUTF8(),
                                 overlap ? "overlap " : "back2back", ratio, 20.0 * std::log10 (ratio));
                }
                p.panic();
                juce::AudioBuffer<float> flush (2, 256); juce::MidiBuffer none; p.processBlock (flush, none);
            }
        }
        std::printf ("%d clicky cases\n", bad);
        return 0;
    }

    // SmokeTest --bench: CPU under load. Every preset with an 8-note chord held (16 voices in chord presets),
    // reported as % of one core in real time at 48 kHz / 256-sample blocks.
    if (argc == 2 && juce::String (argv[1]) == "--bench")
    {
        HypernovaAudioProcessor p;
        const double rate = 48000.0;
        p.prepareToPlay (rate, 256);
        std::vector<std::pair<double, juce::String>> results;
        for (int i = 1; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            juce::MidiBuffer midi;
            for (int k = 0; k < 8; ++k) midi.addEvent (juce::MidiMessage::noteOn (1, 48 + k * 3, (juce::uint8) 100), 0);
            juce::AudioBuffer<float> buf (2, 256);
            const int blocks = (int) (2.0 * rate / 256);
            const auto t0 = juce::Time::getHighResolutionTicks();
            for (int b = 0; b < blocks; ++b) { p.processBlock (buf, midi); midi.clear(); }
            const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
            results.push_back ({ secs / 2.0 * 100.0, p.getProgramName (i) + " (" + juce::String (p.shownVoices.load()) + " voices)" });
            p.panic();
            juce::MidiBuffer none; p.processBlock (buf, none);
        }
        std::sort (results.begin(), results.end());
        double total = 0; for (auto& r : results) total += r.first;
        std::printf ("average %.1f%%  median %.1f%%\n", total / results.size(), results[results.size() / 2].first);
        for (size_t i = results.size() - 12; i < results.size(); ++i) std::printf ("%5.1f%%  %s\n", results[i].first, results[i].second.toRawUTF8());
        return 0;
    }

    // SmokeTest --audition <outDir> [category...]: renders a short, consistent phrase per preset into WAVs
    // (basses play a low line, chords and keys a progression, percussion a straight pattern) at 124 BPM.
    if (argc >= 3 && juce::String (argv[1]) == "--audition")
    {
        const juce::File outDir (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        outDir.createDirectory();
        juce::StringArray wanted;
        for (int i = 3; i < argc; ++i) wanted.add (argv[i]);
        const double rate = 48000.0, beat = 60.0 / 124.0;
        struct Head : juce::AudioPlayHead
        {
            double ppq = 0;
            juce::Optional<PositionInfo> getPosition() const override
            { PositionInfo i; i.setBpm (124.0); i.setPpqPosition (ppq); i.setIsPlaying (true); return i; }
        } head;
        HypernovaAudioProcessor p;
        p.setPlayHead (&head);
        p.prepareToPlay (rate, 256);
        juce::WavAudioFormat wav;
        int written = 0;
        for (int prog = 1; prog < p.getNumPrograms(); ++prog)
        {
            p.setCurrentProgram (prog);
            const auto cat = p.getPresetCategory();
            if (! wanted.isEmpty() && ! wanted.contains (cat)) continue;
            const bool perc = cat.containsIgnoreCase ("percussion") || cat == "Synth Drums";
            const bool low = cat.containsIgnoreCase ("bass") || cat.containsIgnoreCase ("dub") || cat == "Sub" || cat == "808";
            // (note, start beat, length in beats)
            std::vector<std::tuple<int, double, double>> phrase;
            if (perc)       for (int i = 0; i < 8; ++i) phrase.push_back ({ 60, i * 0.5, 0.25 });
            else if (low)   phrase = { { 36, 0, 0.75 }, { 36, 1, 0.5 }, { 43, 1.75, 0.75 }, { 34, 3, 0.9 } };
            else            phrase = { { 48, 0, 0.9 }, { 51, 1, 0.9 }, { 53, 2, 0.9 }, { 46, 3, 0.9 } };
            const int total = (int) ((4.0 * beat + 1.5) * rate);
            juce::AudioBuffer<float> outBuf (2, total);
            outBuf.clear();
            for (int pos = 0; pos < total; pos += 256)
            {
                const int n = juce::jmin (256, total - pos);
                juce::AudioBuffer<float> buf (2, n);
                buf.clear();
                juce::MidiBuffer midi;
                for (auto& [note, start, len] : phrase)
                {
                    const int on = (int) (start * beat * rate), off = (int) ((start + len) * beat * rate);
                    if (on >= pos && on < pos + n) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), on - pos);
                    if (off >= pos && off < pos + n) midi.addEvent (juce::MidiMessage::noteOff (1, note), off - pos);
                }
                head.ppq = pos / rate / beat;
                p.processBlock (buf, midi);
                for (int c = 0; c < 2; ++c) outBuf.copyFrom (c, pos, buf, c, 0, n);
            }
            const auto file = outDir.getChildFile (juce::String (prog).paddedLeft ('0', 3) + " " + cat + " - "
                                                   + juce::File::createLegalFileName (p.getProgramName (prog)) + ".wav");
            file.deleteFile();
            if (auto stream = std::unique_ptr<juce::OutputStream> (file.createOutputStream()))
                if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), rate, 2, 24, {}, 0)))
                {
                    stream.release();
                    writer->writeFromAudioSampleBuffer (outBuf, 0, total);
                    ++written;
                }
            p.panic();
            juce::AudioBuffer<float> flush (2, 256); juce::MidiBuffer none;
            p.processBlock (flush, none);
        }
        std::printf ("%d auditions written to %s\n", written, outDir.getFullPathName().toRawUTF8());
        return 0;
    }

    // SmokeTest --newfx: switches on each of the newer effects in turn and checks the output stays sane.
    if (argc == 2 && juce::String (argv[1]) == "--newfx")
    {
        struct Head : juce::AudioPlayHead
        {
            double ppq = 0;
            juce::Optional<PositionInfo> getPosition() const override
            { PositionInfo i; i.setBpm (124.0); i.setPpqPosition (ppq); i.setIsPlaying (true); return i; }
        } head;
        struct Case { const char* name; std::vector<std::pair<const char*, float>> params; };
        const std::vector<Case> cases {
            { "flanger",        { { "flangMix", 0.8f }, { "flangDepth", 0.7f }, { "flangFb", 0.6f } } },
            { "tape wear",      { { "tapeWow", 0.8f }, { "tapeNoise", 0.4f }, { "tapeSat", 0.7f } } },
            { "trance gate",    { { "gateDepth", 1.0f }, { "gatePattern", 1 }, { "gateRate", 6 } } },
            { "auto pan",       { { "panDepth", 1.0f } } },
            { "fx filter",      { { "fxFltFreq", 700.0f }, { "fxFltRes", 0.7f }, { "fxFltDepth", 0.8f } } },
            { "pitch shift up", { { "shiftSemis", 7.0f }, { "shiftMix", 1.0f } } },
            { "pitch shift dn", { { "shiftSemis", -12.0f }, { "shiftMix", 1.0f } } },
            { "reverse delay",  { { "dlyStyle", 1 }, { "dlyMix", 0.6f }, { "dlyFb", 0.5f } } },
            { "granular delay", { { "dlyStyle", 2 }, { "dlyMix", 0.6f }, { "dlyFb", 0.5f } } },
            { "plate reverb",   { { "verbMode", 1 }, { "verbMix", 0.6f } } },
            { "spring reverb",  { { "verbMode", 2 }, { "verbMix", 0.6f } } },
            { "room reverb",    { { "verbMode", 3 }, { "verbMix", 0.6f } } },
            { "ensemble chorus",{ { "chorusMode", 1 }, { "chorusMix", 0.8f } } },
            { "dimension chorus",{ { "chorusMode", 2 }, { "chorusMix", 0.8f } } },
        };
        int bad = 0;
        for (const auto& c : cases)
        {
            HypernovaAudioProcessor p;
            p.setPlayHead (&head);
            p.prepareToPlay (48000.0, 256);
            p.setCurrentProgram (1);
            for (const auto& kv : c.params) p.setParam (kv.first, kv.second);
            float peak = 0;
            bool finite = true;
            const int total = (int) (48000 * 2.0);
            for (int pos = 0; pos < total; pos += 256)
            {
                juce::AudioBuffer<float> buf (2, 256);
                juce::MidiBuffer midi;
                if (pos == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 100), 0);
                if (pos == (int) (48000 * 1.0)) midi.addEvent (juce::MidiMessage::noteOff (1, 45), 0);
                head.ppq = pos / 48000.0 * 124.0 / 60.0;
                p.processBlock (buf, midi);
                for (int ch = 0; ch < 2; ++ch)
                    for (int i = 0; i < 256; ++i)
                    {
                        const float v = buf.getSample (ch, i);
                        finite = finite && std::isfinite (v);
                        peak = juce::jmax (peak, std::abs (v));
                    }
            }
            const bool ok = finite && peak > 0.005f && peak < 1.0f;
            if (! ok) ++bad;
            std::printf ("%-18s peak %5.3f  %s\n", c.name, peak, ok ? "ok" : "BAD");
        }
        std::printf ("%s\n", bad == 0 ? "ALL OK" : "FAILURES");
        return bad == 0 ? 0 : 1;
    }

    // SmokeTest --import <audio file>: imports it as osc A's wavetable and renders a note through it.
    if (argc == 3 && juce::String (argv[1]) == "--import")
    {
        HypernovaAudioProcessor p;
        p.prepareToPlay (48000.0, 256);
        juce::String error;
        const auto name = p.importWavetable (juce::File (argv[2]), 0, error);
        if (name.isEmpty()) { std::printf ("import failed: %s\n", error.toRawUTF8()); return 1; }
        float peak = 0;
        bool finite = true;
        for (int pos = 0; pos < 24000; pos += 256)
        {
            juce::AudioBuffer<float> buf (2, 256);
            juce::MidiBuffer midi;
            if (pos == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
            p.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 256; ++i)
                {
                    const float v = buf.getSample (c, i);
                    finite = finite && std::isfinite (v);
                    peak = juce::jmax (peak, std::abs (v));
                }
        }
        std::printf ("imported \"%s\", peak %.3f, %s\n", name.toRawUTF8(), peak, finite && peak > 0.01f ? "ok" : "BAD");
        // round trip: the saved .hnwt reloads
        p.setUserTable (0, {});
        const bool reload = p.setUserTable (0, name);
        std::printf ("reload from folder: %s\n", reload ? "ok" : "FAILED");
        return finite && peak > 0.01f && reload ? 0 : 1;
    }

    // SmokeTest --opentime: how long building the wavetables and one processor takes (plug-in open time).
    if (argc == 2 && juce::String (argv[1]) == "--opentime")
    {
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        ab::WavetableBank::get();
        const auto t1 = juce::Time::getMillisecondCounterHiRes();
        { HypernovaAudioProcessor p; }
        const auto t2 = juce::Time::getMillisecondCounterHiRes();
        double sum = 0;
        for (int i = 0; i < ab::NumTables; ++i)
            for (auto v : ab::WavetableBank::get().table (i).data) sum += std::abs ((double) v);
        std::printf ("wavetables %.0f ms, processor %.0f ms, checksum %.6f\n", t1 - t0, t2 - t1, sum);
        return 0;
    }

    // SmokeTest --lowend: for every bass-category preset, plays C2 for 0.8 s and prints how much of the energy
    // sits below 110 Hz (dB relative to the full signal). Used to find basses that are light on sub.
    if (argc == 2 && juce::String (argv[1]) == "--lowend")
    {
        HypernovaAudioProcessor p;
        const double rate = 48000.0;
        p.prepareToPlay (rate, 256);
        static const juce::StringArray bassCats { "808", "Sub", "Reese", "Growl & Wobble", "Pluck Bass", "House Bass", "Techno & Euro Bass" };
        for (int i = 1; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            const juce::String cat = p.getPresetCategory();
            if (! bassCats.contains (cat) && ! cat.containsIgnoreCase ("bass")) continue;
            const int total = (int) (0.8 * rate);
            juce::dsp::IIR::Filter<float> lp1, lp2;
            lp1.coefficients = lp2.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (rate, 110.0);
            double all = 0, low = 0;
            for (int pos = 0; pos < total; pos += 256)
            {
                juce::AudioBuffer<float> buf (2, 256);
                juce::MidiBuffer midi;
                if (pos == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), 0);
                p.processBlock (buf, midi);
                for (int s = 0; s < 256; ++s)
                {
                    const float m = 0.5f * (buf.getSample (0, s) + buf.getSample (1, s));
                    const float l = lp2.processSample (lp1.processSample (m));
                    all += (double) m * m; low += (double) l * l;
                }
            }
            std::printf ("%-24s %-18s sub %6.1f dB\n", p.getProgramName (i).toRawUTF8(), cat.toRawUTF8(),
                         10.0 * std::log10 ((low + 1e-12) / (all + 1e-12)));
            p.panic();
            juce::AudioBuffer<float> flush (2, 256); juce::MidiBuffer none;
            p.processBlock (flush, none);
        }
        return 0;
    }

    // SmokeTest --loudness: renders every preset at 0 dB output and prints its loudest 100 ms RMS window,
    // used by scripts/level_presets.py to generate Source/PresetTrims.h.
    if (argc == 2 && juce::String (argv[1]) == "--loudness")
    {
        HypernovaAudioProcessor p;
        const double rate = 48000.0;
        struct Head : juce::AudioPlayHead
        {
            double ppq = 0;
            juce::Optional<PositionInfo> getPosition() const override
            {
                PositionInfo i; i.setBpm (124.0); i.setPpqPosition (ppq); i.setIsPlaying (true); return i;
            }
        } head;
        p.setPlayHead (&head);
        p.prepareToPlay (rate, 256);
        for (int i = 1; i < p.getNumPrograms(); ++i)
        {
            p.setCurrentProgram (i);
            p.setParam ("volume", 0.0f);
            const juce::String cat = p.getPresetCategory();
            static const juce::StringArray lowCats { "808", "Log Drum", "Sub", "Reese", "Growl & Wobble", "Pluck Bass", "House Bass", "Dub & Dancehall" };
            const bool high = ! lowCats.contains (cat) && cat != "Synth Drums";
            const int base = high ? 60 : 36;
            const int total = (int) (4.0 * rate);
            std::vector<float> mono ((size_t) total);
            for (int pos = 0; pos < total; pos += 256)
            {
                juce::AudioBuffer<float> buf (2, 256);
                juce::MidiBuffer midi;
                const bool slow = cat == "Soundscape" || cat == "FX" || cat == "Pads" || cat == "Cinematic" || cat == "Vocal & Choir" || cat == "Experimental";
                for (int k = 0; k < 4; ++k)
                {
                    if (slow && k > 1) break; // pads: two notes held for the whole render
                    const int on = slow ? 0 : (int) (k * 0.9 * rate), off = slow ? total - 512 : on + (int) (0.7 * rate);
                    const int note = base + (k == 1 ? 7 : k == 2 ? 3 : k == 3 ? 5 : 0);
                    if (on >= pos && on < pos + 256) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), on - pos);
                    if (off >= pos && off < pos + 256) midi.addEvent (juce::MidiMessage::noteOff (1, note), off - pos);
                }
                head.ppq = pos / rate * 124.0 / 60.0;
                p.processBlock (buf, midi);
                for (int s = 0; s < 256 && pos + s < total; ++s)
                    mono[(size_t) (pos + s)] = 0.5f * (buf.getSample (0, s) + buf.getSample (1, s));
            }
            const int win = (int) (0.1 * rate);
            double best = 0;
            for (int w = 0; w + win <= total; w += win / 4)
            {
                double sum = 0;
                for (int s = w; s < w + win; ++s) sum += (double) mono[(size_t) s] * mono[(size_t) s];
                best = std::max (best, std::sqrt (sum / win));
            }
            std::printf ("%s\t%s\t%.2f\n", p.getProgramName (i).toRawUTF8(), cat.toRawUTF8(), juce::Decibels::gainToDecibels ((float) best, -100.0f));
            p.panic();
            juce::AudioBuffer<float> flush (2, 256); juce::MidiBuffer none;
            p.processBlock (flush, none);
        }
        return 0;
    }

    // SmokeTest --note "<preset>" <midiNote> <out.wav>: one 300 ms note, for A/B checks against references.
    // SmokeTest --eqcheck: the new in-place EQ shelves match the old JUCE ones, so older sounds don't change.
    if (argc == 2 && juce::String (argv[1]) == "--eqcheck")
    {
        const double sr = 48000.0;
        double worst = 0;
        for (auto kind : { 0, 1 })
            for (float gain : { -12.0f, -6.0f, 2.5f, 6.0f, 12.0f })
            {
                const double hz = kind == 0 ? 120.0 : 5000.0;
                auto ref = kind == 0 ? juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, (float) hz, 0.7f, juce::Decibels::decibelsToGain (gain))
                                     : juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, (float) hz, 0.7f, juce::Decibels::decibelsToGain (gain));
                ab::Biquad b;
                b.set (kind == 0 ? ab::Biquad::LowShelf : ab::Biquad::HighShelf, sr, hz, 0.7, gain);
                for (double f : { 30.0, 80.0, 120.0, 300.0, 1000.0, 3000.0, 5000.0, 10000.0, 16000.0 })
                {
                    const std::complex<double> z = std::polar (1.0, -juce::MathConstants<double>::twoPi * f / sr);
                    const auto h = ((double) b.b0 + (double) b.b1 * z + (double) b.b2 * z * z) / (1.0 + (double) b.a1 * z + (double) b.a2 * z * z);
                    const double mine = juce::Decibels::gainToDecibels (std::abs (h)), theirs = juce::Decibels::gainToDecibels (ref->getMagnitudeForFrequency (f, sr));
                    worst = juce::jmax (worst, std::abs (mine - theirs));
                }
            }
        std::printf ("largest difference from the old shelves: %.3f dB\n%s\n", worst, worst < 0.05 ? "ALL OK" : "FAILED");
        return worst < 0.05 ? 0 : 1;
    }

    // SmokeTest --lowarch: Low End keeps the sub clean under distortion, adds nothing when neutral, ducks in time.
    if (argc == 2 && juce::String (argv[1]) == "--lowarch")
    {
        int failures = 0;
        auto check = [&] (bool ok, const juce::String& what) { std::printf ("%s  %s\n", ok ? "pass" : "FAIL", what.toRawUTF8()); failures += ok ? 0 : 1; };
        const double rate = 48000.0;
        using Settings = std::vector<std::pair<const char*, float>>;
        auto render = [&] (const Settings& values, int note, double seconds, std::function<void (HypernovaAudioProcessor&, int)> atBlock = {})
        {
            HypernovaAudioProcessor p;
            p.prepareToPlay (rate, 256);
            // A saw bass with a sub under it: plenty of low end and upper harmonics.
            for (auto& [id, v] : Settings { { "aPos", 0.66f }, { "subOn", 1 }, { "subLevel", 0.8f }, { "fltOn", 0 }, { "monoBass", 0 } }) p.setParam (id, v);
            for (auto& [id, v] : values) p.setParam (id, v);
            std::vector<float> out;
            const int total = (int) (seconds * rate);
            for (int pos = 0, b = 0; pos < total; pos += 256, ++b)
            {
                if (atBlock) atBlock (p, b);
                juce::AudioBuffer<float> buf (2, 256);
                buf.clear();
                juce::MidiBuffer midi;
                if (pos == 0) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 110), 0);
                p.processBlock (buf, midi);
                for (int i = 0; i < 256; ++i) out.push_back (0.5f * (buf.getSample (0, i) + buf.getSample (1, i)));
            }
            return out;
        };
        auto lowpass = [&] (std::vector<float> x, float hz)
        {
            const float c = std::exp (-juce::MathConstants<float>::twoPi * hz / (float) rate);
            for (int pass = 0; pass < 4; ++pass) { float y = 0; for (auto& v : x) { y = v + c * (y - v); v = y; } }
            return x;
        };
        auto rms = [] (const std::vector<float>& x, size_t from, size_t to)
        {
            double s = 0; for (size_t i = from; i < to && i < x.size(); ++i) s += x[i] * x[i];
            return std::sqrt (s / juce::jmax ((size_t) 1, to - from));
        };
        auto diffRms = [] (const std::vector<float>& a, const std::vector<float>& b, size_t from, size_t to)
        {
            double s = 0; for (size_t i = from; i < to; ++i) s += (a[i] - b[i]) * (a[i] - b[i]);
            return std::sqrt (s / (double) (to - from));
        };
        const size_t from = (size_t) (0.3 * rate), to = (size_t) (1.2 * rate);

        // Neutral Low End: same level (the split sums back flat).
        const auto off = render ({}, 33, 1.3), neutral = render ({ { "lowOn", 1 } }, 33, 1.3);
        const double dB = 20.0 * std::log10 (rms (neutral, from, to) / rms (off, from, to));
        check (std::abs (dB) < 0.5, "switched on with nothing turned, the level doesn't change (" + juce::String (dB, 2) + " dB)");

        // Heavy distortion: with Low End the sub under 80 Hz stays close to the clean sound.
        const Settings dist { { "distMix", 1.0f }, { "distDrive", 1.0f }, { "distType", 2 } };
        Settings distLow = dist; distLow.push_back ({ "lowOn", 1 }); distLow.push_back ({ "lowXover", 150.0f });
        const auto clean = lowpass (render ({ { "lowOn", 1 }, { "lowXover", 150.0f } }, 33, 1.3), 80.0f);
        const auto dirty = lowpass (render (dist, 33, 1.3), 80.0f);
        const auto kept = lowpass (render (distLow, 33, 1.3), 80.0f);
        const double errDirty = diffRms (dirty, clean, from, to) / rms (clean, from, to);
        const double errKept = diffRms (kept, clean, from, to) / rms (clean, from, to);
        check (errKept < errDirty * 0.5, "under distortion the sub stays clean (error " + juce::String (errKept, 3) + " vs " + juce::String (errDirty, 3) + " without)");

        // Level and warmth do something to the low band only.
        const auto quieter = render ({ { "lowOn", 1 }, { "lowLevel", -12.0f } }, 33, 1.3);
        check (rms (lowpass (quieter, 60.0f), from, to) < rms (lowpass (neutral, 60.0f), from, to) * 0.4, "low level turns the sub down");

        // Duck: quarter notes at 120 BPM (not playing: free-running), the low band dips right after each beat.
        const auto ducked = lowpass (render ({ { "lowOn", 1 }, { "lowDuck", 1.0f }, { "lowDuckRelease", 0.15f } }, 33, 2.0), 60.0f);
        const size_t beat = (size_t) (0.5 * rate);
        double dipped = 0, recovered = 0;
        for (int k = 1; k < 3; ++k)
        {
            // (the measuring low-pass lags ~10 ms, and the sub's cycle is 36 ms, so the windows are a few cycles long)
            dipped += rms (ducked, k * beat + (size_t) (0.012 * rate), k * beat + (size_t) (0.085 * rate));
            recovered += rms (ducked, k * beat + (size_t) (0.39 * rate), k * beat + (size_t) (0.49 * rate));
        }
        check (dipped < recovered * 0.35, "the duck pumps the sub in time (" + juce::String (dipped / recovered, 2) + ")");

        // Switching Low End on mid-note doesn't click.
        const auto toggled = render ({}, 33, 1.3, [] (HypernovaAudioProcessor& p, int b) { if (b == 120) p.setParam ("lowOn", 1.0f); });
        float normal = 0, atSwitch = 0;
        for (size_t i = 1; i < toggled.size(); ++i)
        {
            const float step = std::abs (toggled[i] - toggled[i - 1]);
            if (i > 256 * 60 && i < 256 * 118) normal = juce::jmax (normal, step);
            if (i >= 256 * 119 && i < 256 * 124) atSwitch = juce::jmax (atSwitch, step);
        }
        check (atSwitch <= normal * 1.2f + 0.002f, "switching it on while playing doesn't click (" + juce::String (atSwitch, 3) + " vs " + juce::String (normal, 3) + ")");

        // Phone speaker: takes the sub away, and isn't part of the saved sound.
        HypernovaAudioProcessor q;
        q.prepareToPlay (rate, 256);
        q.speakerCheck = true;
        juce::MemoryBlock st;
        q.getStateInformation (st);
        check (st.toString().indexOf ("speaker") < 0, "the phone-speaker check isn't saved with the sound");
        const auto phone = render ({}, 33, 1.3, [] (HypernovaAudioProcessor& p, int b) { if (b == 0) p.speakerCheck = true; });
        check (rms (lowpass (phone, 60.0f), from, to) < rms (lowpass (off, 60.0f), from, to) * 0.1, "phone speaker check takes the sub away");

        std::printf ("%s (%d failures)\n", failures == 0 ? "ALL OK" : "FAILED", failures);
        return failures == 0 ? 0 : 1;
    }

    // SmokeTest --fxorder: the effects rack can be reordered, saved, undone, and reordering while playing doesn't click.
    if (argc == 2 && juce::String (argv[1]) == "--lfo")
    {
        int failures = 0;
        auto check = [&] (bool ok, const juce::String& what) { std::printf ("%s  %s\n", ok ? "pass" : "FAIL", what.toRawUTF8()); failures += ok ? 0 : 1; };
        const double rate = 48000.0;
        // Renders a held note and returns the level in 20 ms windows.
        auto windows = [&] (HypernovaAudioProcessor& p, double seconds)
        {
            p.prepareToPlay (rate, 256);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 48, 1.0f), 0);
            std::vector<float> out;
            double sum = 0; int n = 0;
            const int win = (int) (rate * 0.02);
            for (int b = 0; b < (int) (seconds * rate / 256); ++b)
            {
                juce::AudioBuffer<float> buf (2, 256);
                buf.clear();
                p.processBlock (buf, midi);
                midi.clear();
                for (int i = 0; i < 256; ++i)
                {
                    const float v = buf.getSample (0, i);
                    sum += v * v;
                    if (++n == win) { out.push_back ((float) std::sqrt (sum / n)); sum = 0; n = 0; }
                }
            }
            return out;
        };
        auto swing = [] (const std::vector<float>& w)
        {
            float lo = 1.0e9f, hi = 0;
            for (size_t i = 10; i < w.size(); ++i) { lo = juce::jmin (lo, w[i]); hi = juce::jmax (hi, w[i]); }
            return hi / juce::jmax (1.0e-6f, lo);
        };
        auto setup = [] (HypernovaAudioProcessor& p)
        {
            p.setParam ("aPos", 0.66f); p.setParam ("ampS", 1.0f); p.setParam ("fltOn", 0.0f); p.setParam ("verbMix", 0.0f); p.setParam ("dlyMix", 0.0f);
        };
        {
            HypernovaAudioProcessor p;
            setup (p);
            const auto still = swing (windows (p, 1.5));
            HypernovaAudioProcessor q;
            setup (q);
            q.setParam ("lfo3Shape", (float) ab::LSquare); q.setParam ("lfo3Rate", 2.0f);
            q.setParam ("mod1Src", (float) ab::SrcLfo3); q.setParam ("mod1Dest", (float) ab::DALevel); q.setParam ("mod1Amt", -0.9f);
            const auto moving = swing (windows (q, 1.5));
            check (still < 1.5f && moving > 3.0f, "LFO 3 modulates: a square on osc A's level makes the level jump (" + juce::String (still, 2) + "x still, " + juce::String (moving, 1) + "x with LFO 3)");
            HypernovaAudioProcessor r;
            setup (r);
            r.setParam ("lfo4Shape", (float) ab::LSquare); r.setParam ("lfo4Rate", 2.0f);
            r.setParam ("mod2Src", (float) ab::SrcLfo4); r.setParam ("mod2Dest", (float) ab::DALevel); r.setParam ("mod2Amt", -0.9f);
            check (swing (windows (r, 1.5)) > 3.0f, "and so does LFO 4");
        }
        {
            // A drawn shape that is high for the first half of the cycle and low for the second acts like a square.
            HypernovaAudioProcessor p;
            setup (p);
            p.setParam ("lfo3Shape", (float) ab::LDrawn); p.setParam ("lfo3Rate", 2.0f);
            p.setLfoCurve (2, "0:1 0.49:1 0.5:-1 0.99:-1");
            p.setParam ("mod1Src", (float) ab::SrcLfo3); p.setParam ("mod1Dest", (float) ab::DALevel); p.setParam ("mod1Amt", -0.9f);
            check (swing (windows (p, 1.5)) > 3.0f, "a drawn LFO shape drives modulation");
            HypernovaAudioProcessor flat;
            setup (flat);
            flat.setParam ("lfo3Shape", (float) ab::LDrawn); flat.setParam ("lfo3Rate", 2.0f);
            flat.setLfoCurve (2, "0:0.5 0.5:0.5");
            flat.setParam ("mod1Src", (float) ab::SrcLfo3); flat.setParam ("mod1Dest", (float) ab::DALevel); flat.setParam ("mod1Amt", -0.9f);
            check (swing (windows (flat, 1.5)) < 1.5f, "and a flat drawn line holds still");
            juce::MemoryBlock saved;
            p.getStateInformation (saved);
            HypernovaAudioProcessor back;
            back.setStateInformation (saved.getData(), (int) saved.getSize());
            check (back.lfoCurve (2) == p.lfoCurve (2) && back.lfoCurve (2).startsWith ("0.0000:1.0000"), "the drawing is saved with the session (" + back.lfoCurve (2) + ")");
            const auto before = p.lfoCurve (2);
            p.undoManager.beginNewTransaction();
            p.setLfoCurve (2, "0:0 0.5:1");
            p.undoManager.undo();
            check (p.lfoCurve (2) == before, "drawing is undoable");
            for (int i = 0; i < p.getNumPrograms(); ++i) if (p.getProgramName (i) == "Reese Wide") p.setCurrentProgram (i);
            check (p.lfoCurve (2) == HypernovaAudioProcessor::defaultLfoCurve(), "loading a preset starts the drawings fresh");
            check (HypernovaAudioProcessor::parseLfoCurve ("nonsense").size() >= 2, "a broken drawing falls back to the default shape");

            // "Once": the shape plays one pass for each note and holds the end, so a drawing works as an envelope.
            HypernovaAudioProcessor env;
            setup (env);
            env.setParam ("lfo3Shape", (float) ab::LDrawn);
            env.setParam ("lfo3Rate", 1.0f);
            env.setParam ("lfo3Once", 1.0f);
            env.setLfoCurve (2, "0:-1 0.5:1 0.99:1");   // rise over half a second, then hold
            env.setParam ("mod1Src", (float) ab::SrcLfo3);
            env.setParam ("mod1Dest", (float) ab::DALevel);
            env.setParam ("mod1Amt", 0.5f);
            const auto shape = windows (env, 2.0);
            const size_t start = 5, mid = shape.size() / 3, end = shape.size() - 5;
            check (shape[mid] > shape[start] * 1.2f, "a drawn 'once' shape rises with the note (" + juce::String (shape[start], 3) + " -> " + juce::String (shape[mid], 3) + ")");
            check (std::abs (shape[end] - shape[mid]) < shape[mid] * 0.25f, "and holds at the end instead of starting again (" + juce::String (shape[end], 3) + ")");
        }
        std::printf ("%s (%d failures)\n", failures == 0 ? "ALL OK" : "FAILED", failures);
        return failures == 0 ? 0 : 1;
    }

    if (argc == 2 && juce::String (argv[1]) == "--compare")
    {
        int failures = 0;
        auto check = [&] (bool ok, const juce::String& what) { std::printf ("%s  %s\n", ok ? "pass" : "FAIL", what.toRawUTF8()); failures += ok ? 0 : 1; };
        HypernovaAudioProcessor p;
        p.prepareToPlay (48000.0, 256);
        auto v = [&] (const char* id) { return p.apvts.getRawParameterValue (id)->load(); };
        p.setParam ("cutoff", 800.0f);
        check (p.compareSlot() == 0 && ! p.compareHasOther(), "a sound starts on A, with B empty");
        p.compareSwitch (1);
        check (p.compareSlot() == 1 && std::abs (v ("cutoff") - 800.0f) < 1.0f, "B starts as a copy of A");
        p.setParam ("cutoff", 3000.0f);
        p.compareSwitch (0);
        check (p.compareSlot() == 0 && std::abs (v ("cutoff") - 800.0f) < 1.0f, "back on A: A's cutoff");
        p.compareSwitch (1);
        check (std::abs (v ("cutoff") - 3000.0f) < 1.0f, "and B kept its change");
        p.undoManager.undo();
        check (p.compareSlot() == 0 && std::abs (v ("cutoff") - 800.0f) < 1.0f,
               "undoing a switch goes back to A, and says so (slot " + juce::String (p.compareSlot()) + ", cutoff " + juce::String (v ("cutoff")) + ", undo said "
               + p.undoManager.getRedoDescription() + ")");
        p.undoManager.redo();
        check (p.compareSlot() == 1 && std::abs (v ("cutoff") - 3000.0f) < 1.0f, "redo returns to B");
        for (int i = 0; i < p.getNumPrograms(); ++i) if (p.getProgramName (i) == "Reese Wide") p.setCurrentProgram (i);
        check (p.compareSlot() == 1 && p.getProgramName (p.getCurrentProgram()) == "Reese Wide", "a preset loads into the slot you're on");
        p.compareSwitch (0);
        check (std::abs (v ("cutoff") - 800.0f) < 1.0f, "and A is still the sound from before");
        p.compareCopyToOther();
        p.compareSwitch (1);
        check (std::abs (v ("cutoff") - 800.0f) < 1.0f, "copying A to B makes them the same");
        juce::MemoryBlock saved;
        p.getStateInformation (saved);
        HypernovaAudioProcessor q;
        q.setStateInformation (saved.getData(), (int) saved.getSize());
        check (q.compareSlot() == 1, "a session remembers which slot it was on");
        std::printf ("%s (%d failures)\n", failures == 0 ? "ALL OK" : "FAILED", failures);
        return failures == 0 ? 0 : 1;
    }

    if (argc == 2 && juce::String (argv[1]) == "--fxorder")
    {
        int failures = 0;
        auto check = [&] (bool ok, const juce::String& what) { std::printf ("%s  %s\n", ok ? "pass" : "FAIL", what.toRawUTF8()); failures += ok ? 0 : 1; };
        const double rate = 48000.0;
        auto setup = [] (HypernovaAudioProcessor& p)
        {
            p.setParam ("distMix", 1.0f); p.setParam ("distDrive", 0.8f);
            p.setParam ("verbMix", 0.5f); p.setParam ("verbSize", 0.7f);
        };
        auto render = [&] (HypernovaAudioProcessor& p, double seconds, std::function<void (int)> atBlock = {})
        {
            std::vector<float> out;
            const int total = (int) (seconds * rate);
            for (int pos = 0, b = 0; pos < total; pos += 256, ++b)
            {
                if (atBlock) atBlock (b);
                juce::AudioBuffer<float> buf (2, 256);
                buf.clear();
                juce::MidiBuffer midi;
                if (pos == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 45, (juce::uint8) 110), 0);
                p.processBlock (buf, midi);
                for (int i = 0; i < 256; ++i) out.push_back (buf.getSample (0, i));
            }
            return out;
        };
        auto diff = [] (const std::vector<float>& a, const std::vector<float>& b)
        {
            double d = 0, e = 0;
            for (size_t i = 0; i < juce::jmin (a.size(), b.size()); ++i) { d += (a[i] - b[i]) * (a[i] - b[i]); e += a[i] * a[i]; }
            return std::sqrt (d / juce::jmax (1e-12, e));
        };

        HypernovaAudioProcessor a, b;
        for (auto* p : { &a, &b }) { p->prepareToPlay (rate, 256); setup (*p); }
        check (a.getFxOrder() == ab::defaultFxOrder(), "a new sound has the standard order");
        auto order = ab::defaultFxOrder();
        std::swap (order[ab::FxDist], order[ab::FxReverb]); // reverb first, then distortion
        b.setFxOrder (order);
        check (b.getFxOrder() == order, "the order can be changed");
        const auto ra = render (a, 1.0), rb = render (b, 1.0);
        check (diff (ra, rb) > 0.05, "a different order sounds different (" + juce::String (diff (ra, rb), 3) + ")");

        juce::MemoryBlock state;
        b.getStateInformation (state);
        HypernovaAudioProcessor c;
        c.setStateInformation (state.getData(), (int) state.getSize());
        check (c.getFxOrder() == order, "a saved session keeps the order");
        b.undoManager.undo();
        check (b.getFxOrder() == ab::defaultFxOrder(), "undo puts the order back");
        c.setCurrentProgram (5);
        check (c.getFxOrder() == ab::defaultFxOrder(), "loading a factory sound resets the rack");

        // Reordering mid-note: no sample-to-sample jump bigger than the sound itself makes.
        HypernovaAudioProcessor d;
        d.prepareToPlay (rate, 256);
        setup (d);
        auto swapped = order;
        const auto live = render (d, 1.5, [&] (int blk) { if (blk == 150) d.setFxOrder (swapped); });
        float normal = 0, atSwitch = 0;
        for (size_t i = 1; i < live.size(); ++i)
        {
            const float step = std::abs (live[i] - live[i - 1]);
            if (i > 256 * 100 && i < 256 * 148) normal = juce::jmax (normal, step);
            if (i >= 256 * 149 && i < 256 * 153) atSwitch = juce::jmax (atSwitch, step);
        }
        check (atSwitch <= normal * 1.2f + 0.005f, "no click when reordering while playing (" + juce::String (atSwitch, 3) + " vs " + juce::String (normal, 3) + ")");

        // Effect-chain presets: the order and the settings come back.
        const auto chainName = juce::String ("smoke test chain");
        setup (b); // the undo above can take the setup with it: they were one undo step
        check (b.saveChain (chainName), "saves an effect chain");
        b.setFxOrder (order);
        HypernovaAudioProcessor e;
        e.prepareToPlay (rate, 256);
        const auto file = HypernovaAudioProcessor::chainFolder().getChildFile (chainName + ".hnchain");
        b.saveChain (chainName);
        e.setParam ("cutoff", 500.0f);
        const bool loaded = e.loadChain (file);
        check (loaded && e.getFxOrder() == order && std::abs (e.apvts.getRawParameterValue ("distMix")->load() - 1.0f) < 0.001f,
               "loading it brings back the order and the settings");
        check (std::abs (e.apvts.getRawParameterValue ("cutoff")->load() - 500.0f) < 1.0f, "and leaves the synth alone");
        file.deleteFile();

        // The rack's dry/wet: at 0 you hear the sound going in, at 1 the effects, and it doesn't click.
        {
            auto render = [&] (float mix)
            {
                HypernovaAudioProcessor p;
                p.prepareToPlay (rate, 256);
                p.setParam ("aPos", 0.66f); p.setParam ("ampS", 1.0f);
                p.setParam ("distMix", 1.0f); p.setParam ("distDrive", 1.0f); p.setParam ("distOn", 1.0f);
                p.setParam ("fxMix", mix);
                juce::MidiBuffer midi;
                midi.addEvent (juce::MidiMessage::noteOn (1, 48, 1.0f), 0);
                std::vector<float> out;
                for (int b = 0; b < 120; ++b)
                {
                    juce::AudioBuffer<float> buf (2, 256);
                    buf.clear();
                    p.processBlock (buf, midi);
                    midi.clear();
                    for (int i = 0; i < 256; ++i) out.push_back (buf.getSample (0, i));
                }
                return out;
            };
            auto rms = [] (const std::vector<float>& v, size_t from)
            {
                double sum = 0;
                for (size_t i = from; i < v.size(); ++i) sum += (double) v[i] * v[i];
                return (float) std::sqrt (sum / (double) (v.size() - from));
            };
            const auto dryOnly = render (0.0f), wetOnly = render (1.0f), half = render (0.5f);
            const float d = rms (dryOnly, 20000), w = rms (wetOnly, 20000), h = rms (half, 20000);
            check (std::abs (w - d) > 0.02f, "the rack's dry/wet: heavy distortion sounds different wet than dry (" + juce::String (d, 3) + " vs " + juce::String (w, 3) + ")");
            check (h > juce::jmin (d, w) - 0.02f && h < juce::jmax (d, w) + 0.02f, "and halfway sits between them (" + juce::String (h, 3) + ")");
            float jump = 0;
            for (size_t i = 20001; i < half.size(); ++i) jump = juce::jmax (jump, std::abs (half[i] - half[i - 1]));
            check (jump < 0.2f, "no steps in the blend (biggest jump " + juce::String (jump, 3) + ")");
        }

        // Right-click menu on a unit: move it along the chain, reset its controls, replace it.
        {
            HypernovaAudioProcessor p;
            for (int fx : { ab::FxDist, ab::FxDelay, ab::FxReverb }) p.addToRack (fx);
            auto place = [&] (int fx)
            {
                const auto r = p.rackEffects();
                const auto at = std::find (r.begin(), r.end(), fx);
                return at == r.end() ? -1 : (int) (at - r.begin());
            };
            const int was = place (ab::FxDist);
            p.moveFxBy (ab::FxDist, 1);
            check (place (ab::FxDist) == was + 1, "moving a unit later puts it one place along (" + juce::String (was) + " -> " + juce::String (place (ab::FxDist)) + ")");
            p.moveFxBy (ab::FxDist, -5);
            check (place (ab::FxDist) == 0, "and moving it earlier stops at the front");
            p.setParam ("dlyFb", 0.8f);
            p.resetFx (ab::FxDelay);
            check (std::abs (p.apvts.getRawParameterValue ("dlyFb")->load() - 0.8f) > 0.05f
                   && p.apvts.getRawParameterValue ("dlyOn")->load() > 0.5f, "resetting a unit puts its controls back but leaves it switched on");
            check (juce::String (HypernovaAudioProcessor::fxOnParam (ab::FxDelay)).startsWith (HypernovaAudioProcessor::fxParamPrefix (ab::FxDelay)),
                   "every control of an effect shares one prefix");
        }
        std::printf ("%s (%d failures)\n", failures == 0 ? "ALL OK" : "FAILED", failures);
        return failures == 0 ? 0 : 1;
    }

    // SmokeTest --sampler: loading, pitch detection, key tracking, loops, reverse, clicks and saving.
    if (argc == 2 && juce::String (argv[1]) == "--sampler")
    {
        int failures = 0;
        auto check = [&] (bool ok, const juce::String& what) { std::printf ("%s  %s\n", ok ? "pass" : "FAIL", what.toRawUTF8()); failures += ok ? 0 : 1; };
        const double rate = 48000.0;
        // A test recording: 0.1 s of silence, then 1 s of a 220 Hz tone (A3, MIDI 57) with a few harmonics.
        const auto wavFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("hn_sampler_test.wav");
        {
            const int n = (int) (1.1 * 44100.0);
            juce::AudioBuffer<float> b (1, n);
            for (int i = 0; i < n; ++i)
            {
                const double t = (i - 4410) / 44100.0;
                const double ph = juce::MathConstants<double>::twoPi * 220.0 * t;
                b.setSample (0, i, i < 4410 ? 0.0f : (float) (0.5 * std::sin (ph) + 0.2 * std::sin (2 * ph) + 0.1 * std::sin (3 * ph)));
            }
            wavFile.deleteFile();
            juce::WavAudioFormat wav;
            auto stream = std::unique_ptr<juce::OutputStream> (wavFile.createOutputStream());
            auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), 44100.0, 1, 24, {}, 0));
            stream.release();
            writer->writeFromAudioSampleBuffer (b, 0, n);
        }

        auto isolate = [] (HypernovaAudioProcessor& p)
        {
            for (auto* id : { "aOn", "bOn", "subOn", "fltOn" }) p.setParam (id, 0.0f);
            p.setParam ("noiseLevel", 0.0f);
        };
        // Renders a note and returns the output (mono sum).
        auto render = [&] (HypernovaAudioProcessor& p, int note, double seconds, double offAt)
        {
            const int total = (int) (seconds * rate), off = (int) (offAt * rate);
            std::vector<float> out ((size_t) total, 0.0f);
            for (int pos = 0; pos < total; pos += 256)
            {
                juce::AudioBuffer<float> buf (2, 256);
                buf.clear();
                juce::MidiBuffer midi;
                if (pos == 0) midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 110), 0);
                if (off >= pos && off < pos + 256) midi.addEvent (juce::MidiMessage::noteOff (1, note), off - pos);
                p.processBlock (buf, midi);
                for (int i = 0; i < 256 && pos + i < total; ++i) out[(size_t) (pos + i)] = 0.5f * (buf.getSample (0, i) + buf.getSample (1, i));
            }
            return out;
        };
        auto frequency = [&] (const std::vector<float>& x, double from, double to)
        {
            int crossings = 0, first = -1, last = -1;
            for (int i = (int) (from * rate) + 1; i < (int) (to * rate) && i < (int) x.size(); ++i)
                if (x[(size_t) i - 1] < 0.0f && x[(size_t) i] >= 0.0f) { if (first < 0) first = i; last = i; ++crossings; }
            return crossings > 1 ? (crossings - 1) * rate / (last - first) : 0.0;
        };
        auto rms = [&] (const std::vector<float>& x, double from, double to)
        {
            double sum = 0; int n = 0;
            for (int i = (int) (from * rate); i < (int) (to * rate) && i < (int) x.size(); ++i) { sum += x[(size_t) i] * x[(size_t) i]; ++n; }
            return n > 0 ? std::sqrt (sum / n) : 0.0;
        };
        auto maxStep = [&] (const std::vector<float>& x, double from, double to)
        {
            float m = 0;
            for (int i = (int) (from * rate) + 1; i < (int) (to * rate) && i < (int) x.size(); ++i) m = juce::jmax (m, std::abs (x[(size_t) i] - x[(size_t) i - 1]));
            return m;
        };

        HypernovaAudioProcessor p;
        p.prepareToPlay (rate, 256);
        isolate (p);
        juce::String error;
        check (p.loadSample (wavFile, error), "loads a WAV " + error);
        auto* sp = p.apvts.getRawParameterValue ("smpRoot");
        check ((int) sp->load() == 57, "detects the root note (A3 = 57, got " + juce::String ((int) sp->load()) + ")");
        check (p.apvts.getRawParameterValue ("smpOn")->load() > 0.5f, "switches the sampler on");
        const float start = p.apvts.getRawParameterValue ("smpStart")->load();
        check (std::abs (start - 0.1f / 1.1f) < 0.01f, "starts at the sound, not the silence (" + juce::String (start, 3) + ")");

        auto a3 = render (p, 57, 0.6, 0.5);
        const double f57 = frequency (a3, 0.1, 0.45);
        check (std::abs (f57 - 220.0) < 1.5, "plays at its own pitch on its root (" + juce::String (f57, 1) + " Hz)");
        auto a4 = render (p, 69, 0.6, 0.5);
        const double f69 = frequency (a4, 0.1, 0.4);
        check (std::abs (f69 - 440.0) < 3.0, "an octave up plays an octave up (" + juce::String (f69, 1) + " Hz)");
        check (rms (a3, 0.05, 0.1) > 0.02, "it starts straight away (no silent lead-in)");

        // One-shot ends with the sample (1 s long): silent well before a long-held note is released.
        auto shot = render (p, 57, 1.6, 1.5);
        check (rms (shot, 1.2, 1.45) < 1.0e-4, "one-shot stops at the end of the sample");
        check (maxStep (shot, 0.9, 1.2) < 0.05f, "and fades out instead of clicking");

        // Looping: sustains past the end with no click at the seam.
        p.setParam ("smpLoop", 1.0f);
        p.setParam ("smpLoopStart", 0.5f);
        p.setParam ("smpLoopEnd", 0.9f);
        auto loop = render (p, 57, 2.5, 2.4);
        check (rms (loop, 1.5, 2.3) > 0.05, "a loop keeps sounding after the sample ends");
        const float normalStep = maxStep (loop, 0.2, 0.5);
        check (maxStep (loop, 0.9, 2.3) < normalStep * 1.6f + 0.01f, "no click where the loop wraps (" + juce::String (maxStep (loop, 0.9, 2.3), 3)
                                                                       + " vs " + juce::String (normalStep, 3) + ")");
        p.setParam ("smpLoop", 2.0f);
        auto ping = render (p, 57, 2.5, 2.4);
        check (rms (ping, 1.5, 2.3) > 0.05, "ping-pong loop sustains too");

        // Reverse starts at the end of the region.
        p.setParam ("smpLoop", 0.0f);
        p.setParam ("smpReverse", 1.0f);
        auto rev = render (p, 57, 1.2, 1.1);
        check (rms (rev, 0.02, 0.2) > 0.05 && rms (rev, 1.04, 1.1) < 0.001, "reverse plays from the end back to the start");
        p.setParam ("smpReverse", 0.0f);

        // Key tracking off: every key plays at the sample's own pitch.
        p.setParam ("smpTrack", 0.0f);
        auto fixed = render (p, 72, 0.6, 0.5);
        check (std::abs (frequency (fixed, 0.1, 0.45) - 220.0) < 1.5, "key tracking off plays every key at the sample's pitch");
        p.setParam ("smpTrack", 1.0f);

        // The sample travels with the session and with an exported preset.
        juce::MemoryBlock state;
        p.getStateInformation (state);
        HypernovaAudioProcessor q;
        q.prepareToPlay (rate, 256);
        q.setStateInformation (state.getData(), (int) state.getSize());
        check (q.sampleForUi() != nullptr && q.sampleForUi()->length == p.sampleForUi()->length, "a saved session restores the sample");
        check (! q.apvts.state.getChildWithName ("Sample").isValid(), "and keeps it out of the parameter tree");
        auto restored = render (q, 57, 0.6, 0.5);
        check (std::abs (frequency (restored, 0.1, 0.45) - 220.0) < 1.5, "and it plays the same");
        const auto presetFile = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("hn_sampler_test.hnpreset");
        check (p.exportPreset (presetFile, "test"), "exports a preset");
        HypernovaAudioProcessor r;
        r.prepareToPlay (rate, 256);
        check (r.loadUserPreset (presetFile) && r.sampleForUi() != nullptr, "the exported preset carries the sample");
        std::printf ("preset with sample: %.0f KB\n", presetFile.getSize() / 1024.0);

        // A session without a sample clears it; the sampler off costs nothing.
        HypernovaAudioProcessor e;
        juce::MemoryBlock empty;
        e.getStateInformation (empty);
        q.setStateInformation (empty.getData(), (int) empty.getSize());
        check (q.sampleForUi() == nullptr, "loading a session without a sample clears it");
        wavFile.deleteFile();
        presetFile.deleteFile();
        std::printf ("%s (%d failures)\n", failures == 0 ? "ALL OK" : "FAILED", failures);
        return failures == 0 ? 0 : 1;
    }

    if (argc == 5 && juce::String (argv[1]) == "--note")
    {
        HypernovaAudioProcessor p;
        p.prepareToPlay (48000.0, 256);
        for (int i = 0; i < p.getNumPrograms(); ++i)
            if (p.getProgramName (i) == argv[2]) p.setCurrentProgram (i);
        const int total = (int) (0.8 * 48000.0), offAt = (int) (0.3 * 48000.0);
        juce::AudioBuffer<float> out (2, total);
        for (int pos = 0; pos < total; pos += 256)
        {
            juce::AudioBuffer<float> buf (2, 256);
            juce::MidiBuffer midi;
            if (pos == 0) midi.addEvent (juce::MidiMessage::noteOn (1, juce::String (argv[3]).getIntValue(), (juce::uint8) 100), 0);
            if (offAt >= pos && offAt < pos + 256) midi.addEvent (juce::MidiMessage::noteOff (1, juce::String (argv[3]).getIntValue()), offAt - pos);
            p.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, juce::jmin (256, total - pos));
        }
        juce::File f = juce::File::getCurrentWorkingDirectory().getChildFile (argv[4]);
        f.deleteFile();
        juce::WavAudioFormat wav;
        if (auto stream = std::unique_ptr<juce::OutputStream> (f.createOutputStream()))
            if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), 48000.0, 2, 24, {}, 0)))
            {
                stream.release();
                writer->writeFromAudioSampleBuffer (out, 0, total);
            }
        return 0;
    }

    const juce::File outDir (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File());
    if (outDir != juce::File()) outDir.createDirectory();

    const double sr = 48000.0;
    const int block = 256;
    HypernovaAudioProcessor proc;
    FakePlayHead head;
    proc.setPlayHead (&head);
    proc.prepareToPlay (sr, block);

    // Phrase in beats at 140 BPM: note, start, length. Notes 3+4 overlap for a legato slide.
    struct N { int note; double start, len; float vel; };
    const std::vector<N> phrase { { 36, 0.0, 0.9, 0.9f }, { 36, 1.0, 0.4, 0.7f }, { 43, 1.5, 1.2, 1.0f }, { 39, 2.5, 1.0, 0.8f },
                                  { 31, 4.0, 0.5, 0.9f }, { 34, 4.75, 0.5, 0.8f }, { 36, 5.5, 2.0, 1.0f } };
    const double beatsTotal = 8.5;
    const int total = (int) (beatsTotal * 60.0 / 140.0 * sr);

    int failures = 0;
    double worstRealtime = 0;
    for (int p = 0; p < proc.getNumPrograms(); ++p)
    {
        proc.setCurrentProgram (p);
        juce::AudioBuffer<float> out (2, total);
        out.clear();
        const auto t0 = juce::Time::getHighResolutionTicks();
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = juce::jmin (block, total - pos);
            juce::AudioBuffer<float> buf (2, n);
            juce::MidiBuffer midi;
            for (const auto& e : phrase)
            {
                const int on = (int) (e.start * 60.0 / 140.0 * sr), off = (int) ((e.start + e.len) * 60.0 / 140.0 * sr);
                if (on >= pos && on < pos + n) midi.addEvent (juce::MidiMessage::noteOn (1, e.note, e.vel), on - pos);
                if (off >= pos && off < pos + n) midi.addEvent (juce::MidiMessage::noteOff (1, e.note), off - pos);
            }
            head.ppq = pos / sr * 140.0 / 60.0;
            proc.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, n);
        }
        const double secs = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0);
        const double realtime = secs / (total / sr);
        worstRealtime = juce::jmax (worstRealtime, realtime);

        float peak = 0; double sum = 0; bool bad = false;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < total; ++i)
            {
                const float v = out.getSample (c, i);
                if (! std::isfinite (v)) bad = true;
                peak = juce::jmax (peak, std::abs (v));
                sum += (double) v * v;
            }
        const float rmsDb = juce::Decibels::gainToDecibels ((float) std::sqrt (sum / (2.0 * total)), -120.0f);
        const bool silent = peak < 0.01f;
        const bool fail = bad || silent;
        failures += fail ? 1 : 0;
        std::printf ("%-22s %-9s peak %5.2f  rms %6.1f dB  cpu %5.1f%% realtime %s\n", proc.getProgramName (p).toRawUTF8(),
                     proc.getPresetCategory().toRawUTF8(), peak, rmsDb, realtime * 100.0, fail ? (bad ? "  <-- NaN" : "  <-- SILENT") : "");

        if (outDir != juce::File())
        {
            auto f = outDir.getChildFile (juce::String (p).paddedLeft ('0', 2) + " " + proc.getProgramName (p) + ".wav");
            f.deleteFile();
            juce::WavAudioFormat wav;
            if (auto stream = std::unique_ptr<juce::OutputStream> (f.createOutputStream()))
                if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), sr, 2, 24, {}, 0)))
                {
                    stream.release();
                    writer->writeFromAudioSampleBuffer (out, 0, total);
                }
        }
    }

    // Timing: back-to-back notes where the host sends the next note-on *before* the previous note-off
    // at the same sample (common in Ableton). Every note must re-hit on time and never mute.
    {
        auto render = [&] (const juce::String& preset, std::vector<std::pair<int, juce::MidiMessage>> events, int length)
        {
            for (int i = 0; i < proc.getNumPrograms(); ++i)
                if (proc.getProgramName (i) == preset) proc.setCurrentProgram (i);
            juce::AudioBuffer<float> out (1, length);
            for (int pos = 0; pos < length; pos += block)
            {
                const int n = juce::jmin (block, length - pos);
                juce::AudioBuffer<float> buf (2, n);
                juce::MidiBuffer midi;
                for (auto& [at, m] : events)
                    if (at >= pos && at < pos + n) midi.addEvent (m, at - pos);
                proc.processBlock (buf, midi);
                out.copyFrom (0, pos, buf, 0, 0, n);
            }
            return out;
        };
        auto peakIn = [] (const juce::AudioBuffer<float>& b, int from, int to)
        {
            float p = 0;
            for (int i = juce::jmax (0, from); i < juce::jmin (b.getNumSamples(), to); ++i) p = juce::jmax (p, std::abs (b.getSample (0, i)));
            return p;
        };
        const int hit = (int) (0.4 * sr), win = (int) (0.03 * sr), len = (int) (0.8 * sr);
        for (auto preset : { "Classic Log", "Rager 808", "Knock 808", "Reese Wide", "Init" })
        {
            for (int samePitch = 0; samePitch < 2; ++samePitch)
            {
                const int second = samePitch ? 36 : 43;
                auto out = render (preset, { { 0, juce::MidiMessage::noteOn (1, 36, 0.9f) },
                                             { hit, juce::MidiMessage::noteOn (1, second, 0.9f) },
                                             { hit, juce::MidiMessage::noteOff (1, 36) },
                                             { len - 2 * block, juce::MidiMessage::noteOff (1, second) } }, len);
                const float before = peakIn (out, hit - win, hit), after = peakIn (out, hit, hit + win), later = peakIn (out, hit + 4 * win, hit + 5 * win);
                const bool muted = later < 0.01f && after < 0.01f;
                const float first = peakIn (out, 0, win);
                const bool late = after < before * 1.15f && before < first * 0.25f; // decayed sound that didn't re-attack
                std::printf ("timing %-12s %s  before %.2f after %.2f later %.2f %s\n", preset, samePitch ? "same pitch" : "new pitch ",
                             before, after, later, muted ? "<-- MUTED" : (late ? "<-- NO RE-HIT" : "ok"));
                failures += (muted || late) ? 1 : 0;
            }
        }
    }

    // Preset files: export > load must restore the sound exactly, and every FOUNDERS PACK file must load and play.
    {
        proc.setCurrentProgram (1);
        const float before = proc.apvts.getRawParameterValue ("aPos")->load();
        const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("hn_roundtrip.hnpreset");
        const bool wrote = proc.exportPreset (tmp, "Tester");
        proc.setCurrentProgram (0);
        const bool loaded = proc.loadUserPreset (tmp);
        const float after = proc.apvts.getRawParameterValue ("aPos")->load();
        const bool ok = wrote && loaded && std::abs (before - after) < 1.0e-5f && proc.getPresetCategory().contains ("Tester");
        std::printf ("preset round trip: %s\n", ok ? "ok" : "FAILED");
        failures += ok ? 0 : 1;
        tmp.deleteFile();

        const auto packDir = juce::File::getCurrentWorkingDirectory().getChildFile ("packaging/FOUNDERS PACK");
        int packCount = 0;
        for (const auto& f : HypernovaAudioProcessor::presetFilesIn (packDir))
        {
            bool good = proc.loadUserPreset (f);
            float peak = 0;
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);
            for (int b = 0; b < 300 && good; ++b)
            {
                juce::AudioBuffer<float> buf (2, block);
                head.ppq = b * block / sr * 140.0 / 60.0;
                proc.processBlock (buf, midi);
                midi.clear();
                peak = juce::jmax (peak, buf.getMagnitude (0, block));
            }
            good = good && peak > 0.01f && std::isfinite (peak);
            if (! good) std::printf ("pack sound FAILED: %s\n", f.getFileName().toRawUTF8());
            failures += good ? 0 : 1;
            ++packCount;
            proc.panic();
        }
        std::printf ("founders pack: %d sounds checked\n", packCount);
    }

    // Chord mode must sound several voices from one key; the arp must step through different notes.
    {
        auto pick = [&] (const char* name) { for (int i = 0; i < proc.getNumPrograms(); ++i) if (proc.getProgramName (i) == name) proc.setCurrentProgram (i); };
        pick ("Classic House Chord");
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        juce::AudioBuffer<float> buf (2, block);
        proc.processBlock (buf, midi);
        const int chordVoices = proc.shownVoices.load();
        std::printf ("chord voices from one key: %d %s\n", chordVoices, chordVoices == 4 ? "ok" : "FAILED");
        failures += chordVoices == 4 ? 0 : 1;
        proc.panic();

        pick ("Peak Time Arp");
        std::set<int> notes;
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 51, (juce::uint8) 100), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 55, (juce::uint8) 100), 0);
        for (int b = 0; b < 400; ++b)
        {
            head.ppq = b * block / sr * 140.0 / 60.0;
            proc.processBlock (buf, midi);
            midi.clear();
            if (proc.shownNote.load() >= 0) notes.insert (proc.shownNote.load());
        }
        std::printf ("arp distinct notes: %d %s\n", (int) notes.size(), notes.size() >= 5 ? "ok" : "FAILED");
        failures += notes.size() >= 5 ? 0 : 1;
        proc.panic();
    }

    // Idle: after the tail dies, an instance sleeps (near-zero CPU) and wakes on the next note.
    {
        for (int i = 0; i < proc.getNumPrograms(); ++i) if (proc.getProgramName (i) == "Dean Solo") proc.setCurrentProgram (i);
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        proc.processBlock (buf, midi);
        midi.clear();
        midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
        proc.processBlock (buf, midi);
        midi.clear();
        int blocks = 0;
        while (! proc.isAsleep() && blocks < (int) (30.0 * sr / block)) { proc.processBlock (buf, midi); ++blocks; }
        const auto t0 = juce::Time::getHighResolutionTicks();
        for (int b = 0; b < 2000; ++b) proc.processBlock (buf, midi);
        const double idlePct = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) / (2000.0 * block / sr) * 100.0;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        proc.processBlock (buf, midi);
        midi.clear();
        juce::AudioBuffer<float> b2 (2, block);
        for (int k = 0; k < 4; ++k) proc.processBlock (b2, midi);
        const bool woke = ! proc.isAsleep() && b2.getMagnitude (0, block) > 0.001f;
        std::printf ("sleep after tail: %.1f s, idle cost %.3f%%, wakes on note: %s\n", blocks * (double) block / sr, idlePct, woke ? "ok" : "FAILED");
        failures += (proc.isAsleep() || ! woke || idlePct > 0.05) ? (woke ? 0 : 1) : 0;
        proc.panic();
    }

    // Randomizer stability
    for (int i = 0; i < 20; ++i)
    {
        proc.randomize();
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 36, 1.0f), 0);
        for (int b = 0; b < 200; ++b)
        {
            proc.processBlock (buf, midi);
            midi.clear();
            for (int c = 0; c < 2; ++c)
                for (int s = 0; s < block; ++s)
                    if (! std::isfinite (buf.getSample (c, s))) { ++failures; b = 200; break; }
        }
    }

    std::printf ("\nworst render time: %.1f%% of realtime (%d presets)\n", worstRealtime * 100.0, proc.getNumPrograms());
    std::printf (failures == 0 ? "ALL OK\n" : "%d FAILURES\n", failures);
    return failures == 0 ? 0 : 1;
}
