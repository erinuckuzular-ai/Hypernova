#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include <set>

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
            const bool high = cat == "Lead" || cat == "House Stabs" || cat == "Techno & Euro" || cat == "Soundscape" || cat == "FX";
            const int base = high ? 60 : 36;
            const int total = (int) (4.0 * rate);
            std::vector<float> mono ((size_t) total);
            for (int pos = 0; pos < total; pos += 256)
            {
                juce::AudioBuffer<float> buf (2, 256);
                juce::MidiBuffer midi;
                const bool slow = cat == "Soundscape" || cat == "FX";
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
