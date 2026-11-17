// Hardware-port check (HARDWARE_AUDIT.md, step 0).
//
// Runs every DSP block that can live without JUCE through a fixed workout and writes the raw output bits to a
// file. It is built twice: DspCoreCheckJuce (HN_HAS_JUCE=1, the plug-in's own helpers) and DspCoreCheckNoJuce
// (HN_HAS_JUCE=0, no JUCE anywhere). scripts/compare_dsp_core.py then requires the two files to match bit for
// bit, which proves the JUCE-free helpers in Core.h compute exactly what the plug-in computes.
//
//   DspCoreCheckJuce   juce.bin
//   DspCoreCheckNoJuce nojuce.bin
//   python3 scripts/compare_dsp_core.py juce.bin nojuce.bin
//
// Not covered here: Freeze (needs an FFT), the wavetable builder (desktop only) and Voice::render, which reads
// WavetableBank::get(); those need the steps that follow this one.

#include "../Source/DSP/Synth.h"
#include "../Source/DSP/ExtraFx.h"
#include "../Source/DSP/Resonator.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    std::FILE* out = nullptr;
    int sections = 0;

    // One section: name, byte count, bytes. Values are written as their exact bit patterns.
    void emit (const std::string& name, const void* data, size_t bytes)
    {
        const auto n = (std::uint32_t) name.size();
        const auto b = (std::uint64_t) bytes;
        std::fwrite (&n, sizeof n, 1, out);
        std::fwrite (name.data(), 1, name.size(), out);
        std::fwrite (&b, sizeof b, 1, out);
        if (bytes > 0) std::fwrite (data, 1, bytes, out);
        ++sections;
    }
    template <typename T> void emit (const std::string& name, const std::vector<T>& v) { emit (name, v.data(), v.size() * sizeof (T)); }

    // Deterministic test signal that doesn't depend on any of the code under test.
    struct Lcg
    {
        std::uint32_t s;
        explicit Lcg (std::uint32_t seed) : s (seed) {}
        float next() { s = s * 1664525u + 1013904223u; return (float) (s >> 8) * (2.0f / 16777216.0f) - 1.0f; }
    };

    std::vector<float> testSignal (int n, std::uint32_t seed)
    {
        Lcg r (seed);
        std::vector<float> v ((size_t) n);
        for (int i = 0; i < n; ++i)
            v[(size_t) i] = 0.5f * std::sin (0.031f * (float) i) + 0.3f * r.next() + (i % 997 == 0 ? 0.9f : 0.0f);
        return v;
    }

    constexpr double sr = 48000.0;
    constexpr int blockSize = 48; // the Daisy's default callback size

    template <typename Fx, typename Process>
    void runStereo (const std::string& name, Fx& fx, int n, Process&& process)
    {
        auto L = testSignal (n, 11u), R = testSignal (n, 23u);
        for (int pos = 0; pos < n; pos += blockSize)
        {
            const int len = hn::jmin (blockSize, n - pos);
            process (fx, L.data() + pos, R.data() + pos, len, pos);
        }
        emit (name + ".L", L);
        emit (name + ".R", R);
    }

    //==========================================================================
    void helpers()
    {
        std::vector<double> d;
        const double xs[] = { -2.5, -1.0, -0.0, 0.0, 0.25, 0.999999, 1.0, 1.5, 1.0e30, -1.0e-30,
                              std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN() };
        for (double x : xs)
        {
            d.push_back (hn::jlimit (-1.0, 1.0, x));
            d.push_back (hn::jmax (0.0, x));
            d.push_back (hn::jmin (0.0, x));
            d.push_back (hn::jmax (x, 0.5, -0.5));
            d.push_back (hn::jmin (x, 0.5, -0.5));
            d.push_back ((double) hn::jlimit (-1.0f, 1.0f, (float) x));
            d.push_back ((double) hn::isPositiveAndBelow (x, 1.0));
            if (! std::isnan (x)) d.push_back ((double) hn::isPositiveAndBelow ((int) hn::jlimit (-5.0, 5.0, x), 3)); // NaN to int is undefined
            d.push_back (hn::decibelsToGain (x * 40.0));
        }
        d.push_back (hn::MathConstants<double>::pi);
        d.push_back (hn::MathConstants<double>::twoPi);
        d.push_back (hn::MathConstants<double>::halfPi);
        d.push_back ((double) hn::MathConstants<float>::pi);
        d.push_back ((double) hn::MathConstants<float>::twoPi);
        d.push_back ((double) hn::MathConstants<float>::halfPi);
        d.push_back ((double) sizeof (hn::uint16));
        d.push_back ((double) sizeof (hn::uint32));
        d.push_back ((double) sizeof (hn::uint64));
        emit ("helpers", d);

        std::vector<float> r;
        hn::Rng rng;
        for (int i = 0; i < 4096; ++i) r.push_back (rng.next());
        emit ("rng.xorshift", r);
    }

    void randomGen()
    {
        const hn::int64 seeds[] = { 1, 0x5eed, 0x9e3779b9, 1984, -7, 0x7fffffffffffLL };
        for (auto seed : seeds)
        {
            hn::Random r (seed);
            std::vector<std::int64_t> v;
            for (int i = 0; i < 256; ++i)
            {
                v.push_back (r.nextInt());
                v.push_back (r.nextInt (1000));
                float f = r.nextFloat();
                std::int32_t fb; std::memcpy (&fb, &f, 4); v.push_back (fb);
                double dd = r.nextDouble();
                std::int64_t db; std::memcpy (&db, &dd, 8); v.push_back (db);
                v.push_back (r.nextInt64());
                v.push_back (r.nextBool() ? 1 : 0);
            }
            r.setSeed (r.nextInt64() | 1); // what GrainCloud::reset does
            for (int i = 0; i < 64; ++i) v.push_back (r.nextInt());
            emit ("random." + std::to_string (seed), v);
        }
    }

    void voiceParts()
    {
        using namespace ab;
        using namespace ab::dsp;

        // Noise: every flavour.
        for (int type = 0; type < NumNoiseTypes; ++type)
        {
            NoiseGen g;
            g.reset (0x2545f491u + (hn::uint32) type);
            std::vector<float> v;
            for (int i = 0; i < 8192; ++i) v.push_back (g.tick (type, sr));
            emit ("noise." + std::to_string (type), v);
        }

        // Envelopes: attack, decay, sustain, release, and the fast de-click fade.
        const EnvSettings envs[] = { {}, { 0.0f, 0.05f, 0.3f, 0.5f }, { 0.25f, 1.0f, 0.0f, 0.01f }, { 2.0f, 0.001f, 0.8f, 3.0f } };
        int e = 0;
        for (const auto& es : envs)
        {
            Env env;
            env.set (es, sr);
            env.noteOn();
            std::vector<float> v;
            for (int i = 0; i < 20000; ++i) v.push_back (env.tick());
            env.noteOff();
            for (int i = 0; i < 20000; ++i) v.push_back (env.tick());
            env.noteOn();
            for (int i = 0; i < 2000; ++i) v.push_back (env.tick());
            env.fadeOut (sr);
            for (int i = 0; i < 2000; ++i) v.push_back (env.tick());
            emit ("env." + std::to_string (e++), v);
        }

        // State-variable filter across cutoffs and resonances.
        {
            const auto x = testSignal (8192, 5u);
            std::vector<float> v;
            for (float cutoff : { 40.0f, 800.0f, 6000.0f, 20000.0f })
                for (float k : { 2.0f, 0.7f, 0.05f })
                {
                    SVF f;
                    f.set (std::tan (hn::MathConstants<float>::pi * cutoff / (float) sr), k);
                    for (float s : x) { float lo, bp, hp; f.tick (s, lo, bp, hp); v.push_back (lo); v.push_back (bp); v.push_back (hp); }
                }
            emit ("svf", v);
        }

        // Small shaping functions.
        {
            std::vector<float> table ((size_t) LfoTableSize + 1);
            for (int i = 0; i <= LfoTableSize; ++i) table[(size_t) i] = std::sin (0.07f * (float) i) * 0.8f;
            std::vector<double> v;
            for (int i = -50; i <= 1050; ++i)
            {
                const double p = i / 1000.0;
                for (int shape = 0; shape < NumLfoShapes; ++shape)
                    v.push_back (lfoShape (shape, p, 0.3f, -0.6f, table.data()));
                for (int w = 0; w < NumWarps; ++w)
                    v.push_back (warpPhase (w, hn::jlimit (0.0, 0.999999, p), 0.37f, 6.0f, 0.2f));
                for (int shape = 0; shape < NumModShapes; ++shape)
                    v.push_back (shapeMod (shape, (float) (p * 2.0 - 1.0)));
                v.push_back (polyBlep (p, 0.01));
                v.push_back (dsp::frac (p * 3.3));
            }
            emit ("shapes", v);
        }

        // Wavetable reads: a synthetic table (so no FFT is needed), every level and morph position.
        {
            Wavetable wt;
            size_t offset = 0;
            for (int l = 0; l < wtLevels; ++l) { wt.levelOffset[(size_t) l] = offset; offset += (size_t) wtLevelSize (l) + 1; }
            wt.frameStride = offset;
            wt.data.assign (wt.frameStride * wtFrames, 0.0f);
            for (int f = 0; f < wtFrames; ++f)
                for (int l = 0; l < wtLevels; ++l)
                {
                    const int size = wtLevelSize (l);
                    float* dst = wt.data.data() + (size_t) f * wt.frameStride + wt.levelOffset[(size_t) l];
                    for (int i = 0; i < size; ++i)
                        dst[i] = std::sin (6.2831853f * (float) i / (float) size) * (1.0f - (float) f / 40.0f)
                                 + 0.3f * std::sin (6.2831853f * (float) (3 + f % 5) * (float) i / (float) size) / (float) (1 + l);
                    dst[size] = dst[0];
                }
            std::vector<double> v;
            for (double level : { 0.0, 0.5, 0.8, 1.9, 4.25, 8.9, 9.0 })
                for (float pos : { 0.0f, 0.13f, 0.5f, 0.97f, 1.0f })
                {
                    FrameCursor c;
                    c.set (wt, level, pos);
                    v.push_back (c.mix); v.push_back (c.levelMix);
                    for (int i = 0; i < 300; ++i) v.push_back (c.read (i / 300.0 + 0.0013));
                }
            for (double f : { 20.0, 110.0, 880.0, 4000.0, 15000.0 })
                for (double rate : { 48000.0, 96000.0, 192000.0 })
                {
                    v.push_back (WavetableBank::exactLevel (f, rate));
                    v.push_back ((double) WavetableBank::levelFor (f, rate));
                }
            emit ("wavetable", v);
        }
    }

    void resonator()
    {
        using namespace ab::dsp;
        for (int model = 0; model < Resonator::NumModels; ++model)
        {
            Resonator r;
            r.prepare (sr);
            Resonator::Settings s;
            s.model = model; s.freq = 110.0f + 50.0f * (float) model; s.structure = 0.4f; s.bright = 0.55f; s.decay = 0.6f; s.position = 0.27f; s.mix = 0.8f;
            r.update (s);
            const auto x = testSignal (16384, 31u + (std::uint32_t) model);
            std::vector<float> v;
            for (int i = 0; i < (int) x.size(); ++i)
            {
                if (i % 4096 == 0) { s.freq *= 1.5f; s.bright = 1.0f - s.bright; r.update (s); }
                v.push_back (r.tick (i < 64 ? x[(size_t) i] : x[(size_t) i] * 0.05f, s));
            }
            v.push_back (r.ringingSeconds());
            emit ("resonator." + std::to_string (model), v);
        }
    }

    void effects()
    {
        using namespace ab::dsp;
        const int n = 48000;

        { Flanger fx; fx.prepare (sr);
          runStereo ("flanger", fx, n, [] (Flanger& f, float* L, float* R, int len, int) { f.process (L, R, len, 0.7f, 0.6f, 0.5f, 0.8f); }); }

        { TapeDegrade fx; fx.prepare (sr);
          runStereo ("tape", fx, n, [] (TapeDegrade& f, float* L, float* R, int len, int) { f.process (L, R, len, 0.8f, 0.4f, 0.7f); }); }

        { PitchShifter fx; fx.prepare (sr);
          runStereo ("pitch", fx, n, [] (PitchShifter& f, float* L, float* R, int len, int pos) { f.process (L, R, len, pos < n / 2 ? 7.0f : -12.0f, 0.9f); }); }

        for (int type = 0; type < Speaker::NumTypes; ++type)
        { Speaker fx; fx.prepare (sr);
          runStereo ("speaker." + std::to_string (type), fx, n / 4, [type] (Speaker& f, float* L, float* R, int len, int pos) { f.process (L, R, len, type, pos < n / 8 ? 0.3f : 0.9f, 1.0f); }); }

        for (int type = 0; type < 3; ++type)
            for (bool synced : { false, true })
            { FxFilter fx; fx.prepare (sr);
              runStereo ("fxfilter." + std::to_string (type) + (synced ? ".sync" : ""), fx, n / 2,
                         [type, synced] (FxFilter& f, float* L, float* R, int len, int pos)
                         { f.process (L, R, len, type, 1200.0f, 0.6f, 0.7f, 4, pos / sr * (128.0 / 60.0), 128.0, synced); }); }

        for (int pattern = 0; pattern < 5; ++pattern)
        { GateAndPan fx; fx.prepare (sr);
          runStereo ("gatepan." + std::to_string (pattern), fx, n / 2,
                     [pattern] (GateAndPan& f, float* L, float* R, int len, int pos)
                     { f.process (L, R, len, pos / sr * (140.0 / 60.0), 140.0, true, 0.9f, 6, pattern, 0.4f, 0.7f, 3); }); }

        std::vector<double> beats;
        for (int i = 0; i < 9; ++i) beats.push_back (syncRateBeats (i));
        emit ("syncRateBeats", beats);
    }

    void sampler()
    {
        using namespace ab;
        using namespace ab::dsp;

        SampleData data;
        data.rate = 44100.0;
        data.length = 44100;
        const auto mono = testSignal (data.length, 77u);
        data.l.assign ((size_t) data.length + 4, 0.0f);
        data.r.assign ((size_t) data.length + 4, 0.0f);
        for (int i = 0; i < data.length; ++i) { data.l[(size_t) i + 2] = mono[(size_t) i]; data.r[(size_t) i + 2] = -0.5f * mono[(size_t) i]; }

        for (int loop = 0; loop < NumLoopModes; ++loop)
            for (bool reverse : { false, true })
            {
                SamplerSettings s;
                s.on = true; s.data = &data; s.loop = loop; s.reverse = reverse;
                s.start = 0.1f; s.end = 0.6f; s.loopStart = 0.3f; s.loopEnd = 0.9f;
                SamplePlayer p;
                p.start (s);
                std::vector<float> v;
                for (int i = 0; i < 60000; ++i) { float L = 0, R = 0; p.tick (s, 1.37, 0.8f, 0.6f, L, R); v.push_back (L); v.push_back (R); }
                emit ("sampleplayer." + std::to_string (loop) + (reverse ? ".rev" : ""), v);
            }

        {
            SamplerSettings s;
            s.on = true; s.data = &data;
            GrainSettings g;
            g.on = true; g.position = 0.4f; g.size = 0.05f; g.rate = 35.0f; g.spray = 0.2f; g.pitchSpread = 5.0f; g.reverse = 0.3f; g.drift = 0.1f;
            GrainCloud cloud;
            cloud.reset();
            std::vector<float> v;
            for (int i = 0; i < 96000; ++i) { float L = 0, R = 0; cloud.tick (s, g, 1.0, 0.9f, 0.9f, L, R, sr); v.push_back (L); v.push_back (R); }
            emit ("grains", v);
        }

        emit ("onsets", findOnsets (mono, data.rate, 16));
        std::vector<float> pitch;
        for (int from : { 0, 4410, 22050 }) pitch.push_back (detectPitch (mono, data.rate, from));
        emit ("pitch", pitch);
    }
}

int main (int argc, char** argv)
{
    if (argc != 2) { std::fprintf (stderr, "usage: %s <out.bin>\n", argv[0]); return 2; }
    out = std::fopen (argv[1], "wb");
    if (out == nullptr) { std::fprintf (stderr, "can't write %s\n", argv[1]); return 2; }

    helpers();
    randomGen();
    voiceParts();
    resonator();
    effects();
    sampler();

   #if HN_HAS_JUCE
    // In the JUCE build, also check the two replicas the plug-in doesn't route through hn:: yet.
    int mismatches = 0;
    for (int i = -2000; i <= 400; ++i)
    {
        const float db = (float) i * 0.1f;
        if (hn::decibelsToGain (db) != juce::Decibels::decibelsToGain (db)) ++mismatches;
    }
    std::printf ("decibelsToGain vs juce::Decibels: %d mismatches\n", mismatches);
   #endif

    std::fclose (out);
    std::printf ("%s: %d sections written (HN_HAS_JUCE=%d)\n", argv[1], sections, (int) HN_HAS_JUCE);
   #if HN_HAS_JUCE
    return mismatches == 0 ? 0 : 1;
   #else
    return 0;
   #endif
}
