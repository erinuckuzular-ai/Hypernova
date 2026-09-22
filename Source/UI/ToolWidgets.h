#pragma once

#include "Widgets.h"

// Tool widgets: things you add from the widget library, as many as you like. Each keeps its own settings
// in the workspace (a ValueTree of properties), never in the patch. None of them change the sound except
// through the parameters they point at (the XY pad moves macros, the pinboard holds real knobs).
namespace ab::ui
{

// Callbacks a tool needs from the editor: real knobs and mod chips wired like every other one.
struct ToolServices
{
    std::function<std::unique_ptr<Knob> (const juce::String& paramId, const juce::String& label, ThemeColour, int size)> makeKnob;
    std::function<std::unique_ptr<ModChip> (int source, const juce::String& label)> makeChip;
    std::function<void (const juce::String&)> message;
    std::function<void()> changed; // the tool's settings changed: save the workspace
};

class ToolContent : public juce::Component, public juce::SettableTooltipClient
{
public:
    ToolContent (HypernovaAudioProcessor& p, juce::ValueTree cfg) : proc (p), config (cfg) {}
    virtual void tick (bool sounding) { juce::ignoreUnused (sounding); }
    // Controls that live in the widget's title row (right-aligned there by the widget).
    virtual void addHeaderControls (juce::Component& header, int designW) { juce::ignoreUnused (header, designW); }
    juce::ValueTree config;

protected:
    HypernovaAudioProcessor& proc;

    static void inset (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (Colours::inset);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (Colours::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    }

    // Which parameters a modulation source is driving, e.g. "Cutoff +60, A Position -25".
    juce::String destinationsOf (int source, int maxItems = 3) const
    {
        juce::StringArray parts;
        int more = 0;
        for (int i = 0; i < ab::NumModSlots; ++i)
        {
            const juce::String p = "mod" + juce::String (i + 1);
            if ((int) proc.apvts.getRawParameterValue (p + "Src")->load() != source) continue;
            const int d = (int) proc.apvts.getRawParameterValue (p + "Dest")->load();
            const float a = proc.apvts.getRawParameterValue (p + "Amt")->load();
            if (d == 0 || std::abs (a) < 0.005f) continue;
            if (parts.size() >= maxItems) { ++more; continue; }
            parts.add (modDestNames()[d] + " " + (a > 0 ? "+" : "") + juce::String (juce::roundToInt (a * 100.0f)));
        }
        auto s = parts.joinIntoString (", ");
        if (more > 0) s << " +" << more;
        return s;
    }
};

//==============================================================================
// Oscilloscope on the output. AUTO locks the window to two cycles of the note being played, so the
// waveform stands still while you turn knobs; the fixed windows show envelopes and rhythm. Click to freeze.
class ScopeTool : public ToolContent
{
public:
    ScopeTool (HypernovaAudioProcessor& p, juce::ValueTree cfg) : ToolContent (p, cfg)
    {
        window.setSelected (juce::jlimit (0, 3, (int) config.getProperty ("window", 0)));
        window.onChange = [this] (int i) { config.setProperty ("window", i, nullptr); repaint(); };
        setTooltip ("Click to freeze the picture");
    }

    void addHeaderControls (juce::Component& header, int designW) override
    {
        header.addAndMakeVisible (window);
        window.setBounds (designW - 214, 11, 200, 24);
    }

    void tick (bool sounding) override
    {
        if (frozen) return;
        if (! sounding && quiet > 2) return;
        quiet = sounding ? 0 : quiet + 1;
        proc.scope.latest (l.data(), r.data(), (int) l.size());
        repaint();
    }

    void mouseDown (const juce::MouseEvent&) override { frozen = ! frozen; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        inset (g, area);
        auto plot = area.reduced (10, 10);
        // A quiet grid: the trace is the point. The zero line is a touch stronger.
        g.setColour (Colours::line.withAlpha (0.28f));
        for (int i = 1; i < 4; ++i) if (i != 2) g.drawHorizontalLine ((int) (plot.getY() + plot.getHeight() * (float) i / 4.0f), plot.getX(), plot.getRight());
        for (int i = 1; i < 8; ++i) g.drawVerticalLine ((int) (plot.getX() + plot.getWidth() * (float) i / 8.0f), plot.getY(), plot.getBottom());
        g.setColour (Colours::line.withAlpha (0.55f));
        g.drawHorizontalLine ((int) plot.getCentreY(), plot.getX(), plot.getRight());

        const double sr = juce::jmax (8000.0, proc.getCurrentSampleRate());
        const int note = proc.shownNote.load();
        const int mode = (int) config.getProperty ("window", 0);
        int span = 0;
        juce::String label;
        if (mode == 0 && note >= 0)
        {
            const double hz = 440.0 * std::pow (2.0, (note - 69) / 12.0);
            span = (int) std::round (2.0 * sr / hz);
            label = "2 CYCLES  " + juce::MidiMessage::getMidiNoteName (note, true, true, 3);
        }
        else
        {
            static const double ms[] = { 20.0, 5.0, 20.0, 100.0 };
            span = (int) (ms[juce::jlimit (0, 3, mode)] * sr / 1000.0);
            label = juce::String (ms[juce::jlimit (0, 3, mode)], 0) + " MS";
        }
        span = juce::jlimit (32, (int) l.size() / 2, span);

        // Trigger: the last rising zero crossing that still leaves a full window after it.
        const int n = (int) l.size();
        int start = n - span;
        for (int i = n - span - 1; i > n / 4; --i)
        {
            const float a = l[(size_t) i - 1] + r[(size_t) i - 1], b = l[(size_t) i] + r[(size_t) i];
            if (a < 0.0f && b >= 0.0f) { start = i; break; }
        }

        float peak = 0.05f;
        for (int i = 0; i < span; ++i) peak = juce::jmax (peak, std::abs (l[(size_t) (start + i)]), std::abs (r[(size_t) (start + i)]));
        const float gain = 0.46f / juce::jmin (1.0f, peak * 1.1f);
        bool stereo = false;
        for (int i = 0; i < span && ! stereo; ++i) stereo = std::abs (l[(size_t) (start + i)] - r[(size_t) (start + i)]) > 0.002f;

        auto trace = [&] (const std::array<float, 4096>& s, juce::Colour c)
        {
            juce::Path p;
            const int points = juce::jmin (span, (int) plot.getWidth() * 2);
            for (int k = 0; k <= points; ++k)
            {
                const int i = start + juce::jmin (span - 1, k * span / juce::jmax (1, points));
                const juce::Point<float> pt (plot.getX() + plot.getWidth() * (float) k / (float) points,
                                             plot.getCentreY() - s[(size_t) i] * gain * plot.getHeight());
                if (k == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            glowStroke (g, p, c, 1.6f, 0.8f);
        };
        if (stereo) trace (r, Palette::oscB);
        trace (l, Palette::oscA);

        g.setColour (Colours::textDim);
        g.setFont (mono (10.0f));
        g.drawText (label + (stereo ? "   L / R" : ""), plot.reduced (4, 2), juce::Justification::topLeft, false);
        g.drawText ("x" + juce::String (gain / 0.46f, 1), plot.reduced (4, 2), juce::Justification::topRight, false);
        if (frozen)
        {
            g.setColour (Colours::accent);
            g.setFont (mono (11.0f).boldened());
            g.drawText ("FROZEN", plot.reduced (4, 2), juce::Justification::bottomRight, false);
        }
    }

private:
    Segmented window { { "AUTO", "5 MS", "20 MS", "100 MS" }, Palette::oscA };
    std::array<float, 4096> l {}, r {};
    bool frozen = false;
    int quiet = 0;
};

//==============================================================================
// Loudness to EBU R128 / ITU-R BS.1770: K-weighted momentary (400 ms), short-term (3 s) and integrated
// (gated) loudness, sample peaks with hold, and a short-term history. Click the numbers to reset.
class MeterTool : public ToolContent
{
public:
    MeterTool (HypernovaAudioProcessor& p, juce::ValueTree cfg) : ToolContent (p, cfg)
    {
        setTooltip ("Loudness in LUFS (EBU R128). Click to reset the integrated reading.");
        lastWrite = proc.scope.write.load();
    }

    void tick (bool) override
    {
        const double sr = juce::jmax (8000.0, proc.getCurrentSampleRate());
        if (std::abs (sr - rate) > 1.0) design (sr);
        const int w = proc.scope.write.load (std::memory_order_acquire);
        int n = (w - lastWrite) & (ab::ScopeRing::size - 1);
        lastWrite = w;
        const int tickSamples = (int) (sr / 30.0);
        if (n == 0 && proc.isAsleep()) n = -tickSamples; // asleep: the output is silence
        if (n > ab::ScopeRing::size / 2) n = ab::ScopeRing::size / 2;
        const bool silence = n < 0;
        n = std::abs (n);
        for (int i = 0; i < n; ++i)
        {
            float L = 0, R = 0;
            if (! silence)
            {
                const int idx = (w - n + i + ab::ScopeRing::size) & (ab::ScopeRing::size - 1);
                L = proc.scope.l[(size_t) idx];
                R = proc.scope.r[(size_t) idx];
            }
            peakNow[0] = juce::jmax (peakNow[0], std::abs (L));
            peakNow[1] = juce::jmax (peakNow[1], std::abs (R));
            const double kl = kweight (0, L), kr = kweight (1, R);
            blockSum += kl * kl + kr * kr;
            if (++blockCount >= blockLen) pushBlock();
        }
        for (int c = 0; c < 2; ++c)
        {
            const float db = juce::Decibels::gainToDecibels (peakNow[c], -90.0f);
            peakDb[c] = db > peakDb[c] ? db : peakDb[c] - 1.2f; // falls at about 36 dB/s
            if (db >= holdDb[c]) { holdDb[c] = db; holdTicks[c] = 45; }
            else if (--holdTicks[c] <= 0) holdDb[c] = juce::jmax (-90.0f, holdDb[c] - 1.5f);
            if (db > -0.1f) clipTicks = 90;
            peakNow[c] = 0;
        }
        if (clipTicks > 0) --clipTicks;
        repaint();
    }

    void mouseDown (const juce::MouseEvent&) override { gated.clear(); integrated = -90.0; history.clear(); repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        inset (g, area);
        auto r = area.reduced (12);
        auto meters = r.removeFromLeft (58);
        r.removeFromLeft (14);

        // Peak bars, -48..+3 dBFS, with their scale beside them.
        auto bars = meters.removeFromLeft (34).withTrimmedTop (10);
        auto toY = [&] (float db) { return bars.getBottom() - bars.getHeight() * juce::jlimit (0.0f, 1.0f, (db + 48.0f) / 51.0f); };
        for (int c = 0; c < 2; ++c)
        {
            auto b = juce::Rectangle<float> (bars.getX() + (float) c * 18.0f, bars.getY(), 14.0f, bars.getHeight());
            g.setColour (Colours::bg0.withAlpha (0.55f));
            g.fillRoundedRectangle (b, 3.0f);
            const float y = toY (peakDb[c]);
            juce::ColourGradient grad (juce::Colour (0xffff5a5a), 0, toY (0.0f), Palette::env, 0, b.getBottom(), false);
            grad.addColour (juce::jlimit (0.0, 1.0, (double) ((toY (-6.0f) - toY (0.0f)) / b.getHeight())), Palette::sub);
            grad.addColour (juce::jlimit (0.0, 1.0, (double) ((toY (-18.0f) - toY (0.0f)) / b.getHeight())), Palette::oscA);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (b.withTop (y), 3.0f);
            g.setColour (Colours::text.withAlpha (0.9f));
            g.fillRoundedRectangle (b.getX(), toY (holdDb[c]) - 1.0f, b.getWidth(), 2.0f, 1.0f);
        }
        g.setFont (mono (8.5f));
        for (int db : { 0, -6, -12, -24, -36, -48 })
        {
            const float y = toY ((float) db);
            g.setColour (Colours::line);
            g.drawHorizontalLine ((int) y, meters.getX(), meters.getX() + 3.0f);
            g.setColour (Colours::textFaint.withAlpha (0.9f));
            g.drawText (juce::String (db), juce::Rectangle<float> (meters.getX() + 5.0f, y - 6.0f, 20.0f, 12.0f), juce::Justification::centredLeft, false);
        }
        g.setColour (clipTicks > 0 ? juce::Colour (0xffff4a4a) : Colours::bg0.withAlpha (0.55f));
        g.fillRoundedRectangle (bars.getX(), bars.getY() - 9.0f, 32.0f, 5.0f, 2.0f);

        // The reading: short-term big, with momentary, integrated and peak as labelled figures under it.
        auto top = r.removeFromTop (juce::jmin (r.getHeight() * 0.5f, 104.0f));
        g.setColour (Colours::textDim);
        g.setFont (mono (9.5f).boldened().withExtraKerningFactor (0.1f));
        g.drawText ("SHORT-TERM", top.removeFromTop (14), juce::Justification::topLeft, false);
        auto big = top.removeFromTop (top.getHeight() * 0.58f);
        const auto bigFont = heavy (juce::jmin (40.0f, big.getHeight() * 0.9f));
        g.setColour (Colours::text);
        g.setFont (bigFont);
        const auto reading = lufs (shortTerm);
        g.drawText (reading, big, juce::Justification::centredLeft, false);
        const float unitX = big.getX() + juce::GlyphArrangement::getStringWidth (bigFont, reading) + 6.0f;
        g.setColour (Colours::textDim);
        g.setFont (mono (10.0f).boldened());
        g.drawText ("LUFS", juce::Rectangle<float> (unitX, big.getY(), 40.0f, big.getHeight() * 0.9f), juce::Justification::bottomLeft, false);
        auto stats = top.reduced (0, 2);
        const juce::String names[] = { "MOMENTARY", "INTEGRATED", "PEAK" };
        const juce::String values[] = { lufs (momentary), lufs (integrated), juce::String (juce::jmax (holdDb[0], holdDb[1]), 1) + " dB" };
        const float colW = stats.getWidth() / 3.0f;
        for (int i = 0; i < 3; ++i)
        {
            auto c = stats.withWidth (colW).translated (colW * (float) i, 0);
            g.setColour (Colours::textFaint);
            g.setFont (mono (8.0f).withExtraKerningFactor (0.08f));
            g.drawText (names[i], c.removeFromTop (11), juce::Justification::topLeft, false);
            g.setColour (Colours::text.withAlpha (0.9f));
            g.setFont (mono (11.5f).boldened());
            g.drawText (values[i], c, juce::Justification::topLeft, false);
        }

        // Short-term history, the last 20 s, -40..0 LUFS, with -14 marked (where streaming services normalise).
        auto plot = r.withTrimmedTop (8);
        g.setColour (Colours::bg0.withAlpha (0.35f));
        g.fillRoundedRectangle (plot, 6.0f);
        plot = plot.reduced (6.0f, 6.0f).withTrimmedLeft (18.0f);
        auto yOf = [&] (double l) { return plot.getBottom() - plot.getHeight() * (float) juce::jlimit (0.0, 1.0, (l + 40.0) / 40.0); };
        g.setFont (mono (8.0f));
        for (int l : { -6, -14, -23, -32 })
        {
            const float y = yOf (l);
            const bool target = l == -14;
            if (target)
            {
                const float dashes[] = { 4.0f, 3.0f };
                g.setColour (Colours::accent.withAlpha (0.7f));
                g.drawDashedLine (juce::Line<float> (plot.getX(), y, plot.getRight(), y), dashes, 2, 1.0f);
            }
            else
            {
                g.setColour (Colours::line.withAlpha (0.6f));
                g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
            }
            g.setColour (target ? Colours::accent.withAlpha (0.9f) : Colours::textFaint);
            g.drawText (juce::String (l), juce::Rectangle<float> (plot.getX() - 20.0f, y - 6.0f, 17.0f, 12.0f), juce::Justification::centredRight, false);
        }
        g.setColour (Colours::textFaint);
        g.drawText ("LAST 20 S", plot.removeFromTop (12).reduced (4, 0), juce::Justification::topRight, false);
        plot = plot.withTop (plot.getY() - 12.0f);
        if (history.size() > 1)
        {
            juce::Path line, fill;
            const int n = (int) history.size();
            for (int i = 0; i < n; ++i)
            {
                const juce::Point<float> pt (plot.getRight() - plot.getWidth() * (float) (n - 1 - i) / (float) (historyLen - 1), yOf (history[(size_t) i]));
                if (i == 0) { line.startNewSubPath (pt); fill.startNewSubPath (pt.x, plot.getBottom()); fill.lineTo (pt); }
                else { line.lineTo (pt); fill.lineTo (pt); }
            }
            fill.lineTo (plot.getRight(), plot.getBottom());
            fill.closeSubPath();
            g.setGradientFill (juce::ColourGradient (Palette::oscA.withAlpha (0.28f), 0, plot.getY(), Palette::oscA.withAlpha (0.0f), 0, plot.getBottom(), false));
            g.fillPath (fill);
            glowStroke (g, line, Palette::oscA, 1.5f, 0.6f);
        }
    }

private:
    int lastWrite = 0;
    double rate = 0;
    // Two biquads per channel: the BS.1770 head-related shelf, then the RLB high-pass.
    struct Biquad { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    Biquad shelf, highpass;
    std::array<std::array<double, 4>, 2> z1 {}, z2 {};
    double blockSum = 0;
    int blockCount = 0, blockLen = 4800;
    std::vector<double> blocks;  // 100 ms mean squares
    std::vector<double> gated;   // 400 ms loudness values, for the integrated reading
    std::vector<double> history; // short-term loudness per 100 ms
    static constexpr int historyLen = 200;
    double momentary = -90, shortTerm = -90, integrated = -90;
    float peakNow[2] {}, peakDb[2] { -90, -90 }, holdDb[2] { -90, -90 };
    int holdTicks[2] {}, clipTicks = 0;

    static juce::String lufs (double v) { return v <= -69.0 ? juce::String ("--") : juce::String (v, 1); }

    void design (double fs)
    {
        rate = fs;
        blockLen = (int) (fs / 10.0);
        const double pi = juce::MathConstants<double>::pi;
        {
            const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
            const double K = std::tan (pi * f0 / fs), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
            const double a0 = 1.0 + K / Q + K * K;
            shelf = { (Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0,
                      2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
        }
        {
            const double f0 = 38.13547087602444, Q = 0.5003270373238773;
            const double K = std::tan (pi * f0 / fs), a0 = 1.0 + K / Q + K * K;
            highpass = { 1.0, -2.0, 1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0 };
        }
        z1 = {}; z2 = {};
        blocks.clear();
    }

    double kweight (int c, float x)
    {
        auto run = [] (const Biquad& f, double in, double& s1, double& s2)
        {
            const double out = f.b0 * in + s1;
            s1 = f.b1 * in - f.a1 * out + s2;
            s2 = f.b2 * in - f.a2 * out;
            return out;
        };
        const double a = run (shelf, x, z1[(size_t) c][0], z2[(size_t) c][0]);
        return run (highpass, a, z1[(size_t) c][1], z2[(size_t) c][1]);
    }

    static double toLufs (double meanSquare) { return meanSquare <= 1e-10 ? -90.0 : -0.691 + 10.0 * std::log10 (meanSquare); }

    void pushBlock()
    {
        blocks.push_back (blockSum / juce::jmax (1, blockCount));
        blockSum = 0;
        blockCount = 0;
        if (blocks.size() > 30) blocks.erase (blocks.begin());
        auto meanOf = [&] (size_t count)
        {
            count = juce::jmin (count, blocks.size());
            double s = 0;
            for (size_t i = blocks.size() - count; i < blocks.size(); ++i) s += blocks[i];
            return s / (double) juce::jmax ((size_t) 1, count);
        };
        momentary = toLufs (meanOf (4));
        shortTerm = toLufs (meanOf (30));
        history.push_back (shortTerm);
        if ((int) history.size() > historyLen) history.erase (history.begin());
        // Integrated: 400 ms blocks at 100 ms steps, absolute gate at -70, relative gate 10 LU under.
        if (blocks.size() >= 4 && momentary > -70.0)
        {
            gated.push_back (momentary);
            if (gated.size() > 36000) gated.erase (gated.begin());
        }
        if (! gated.empty())
        {
            auto power = [] (double l) { return std::pow (10.0, (l + 0.691) / 10.0); };
            double s = 0;
            for (auto l : gated) s += power (l);
            const double relGate = toLufs (s / (double) gated.size()) - 10.0;
            double s2 = 0;
            int k = 0;
            for (auto l : gated) if (l > relGate) { s2 += power (l); ++k; }
            integrated = k > 0 ? toLufs (s2 / k) : -90.0;
        }
    }
};

//==============================================================================
// Two macros on one pad: drag the puck and both move together (and automate like any knob).
class XYTool : public ToolContent
{
public:
    XYTool (HypernovaAudioProcessor& p, juce::ValueTree cfg) : ToolContent (p, cfg)
    {
        for (auto* c : { &xBox, &yBox })
        {
            for (int m = 0; m < 4; ++m) c->addItem ("Macro " + juce::String (m + 1), m + 1);
            c->onChange = [this, c]
            {
                config.setProperty (c == &xBox ? "x" : "y", c->getSelectedId() - 1, nullptr);
                repaint();
            };
        }
        xBox.setSelectedId (juce::jlimit (0, 3, (int) config.getProperty ("x", 0)) + 1, juce::dontSendNotification);
        yBox.setSelectedId (juce::jlimit (0, 3, (int) config.getProperty ("y", 1)) + 1, juce::dontSendNotification);
        xBox.setTooltip ("Left to right moves this macro");
        yBox.setTooltip ("Bottom to top moves this macro");
        setTooltip ("Drag to move two macros at once. Double-click to put both back to zero.");
    }

    void addHeaderControls (juce::Component& header, int designW) override
    {
        header.addAndMakeVisible (xBox);
        header.addAndMakeVisible (yBox);
        xBox.setBounds (designW - 210, 11, 96, 24);
        yBox.setBounds (designW - 108, 11, 96, 24);
    }

    void tick (bool) override
    {
        const auto now = juce::Point<float> (value (axis (true)), value (axis (false)));
        if (now != last || trail.size() > 1)
        {
            if (now != last) trail.push_back (now);
            else if (! trail.empty()) trail.erase (trail.begin());
            if (trail.size() > 24) trail.erase (trail.begin());
            last = now;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        inset (g, area);
        const auto pad = padArea();
        g.setColour (Colours::line.withAlpha (0.55f));
        for (int i = 1; i < 4; ++i)
        {
            g.drawVerticalLine ((int) (pad.getX() + pad.getWidth() * (float) i / 4.0f), pad.getY(), pad.getBottom());
            g.drawHorizontalLine ((int) (pad.getY() + pad.getHeight() * (float) i / 4.0f), pad.getX(), pad.getRight());
        }
        auto toPoint = [&] (juce::Point<float> v) { return juce::Point<float> (pad.getX() + pad.getWidth() * v.x, pad.getBottom() - pad.getHeight() * v.y); };
        for (size_t i = 0; i < trail.size(); ++i)
        {
            const float a = (float) (i + 1) / (float) trail.size();
            g.setColour (Palette::mod.withAlpha (0.35f * a));
            g.fillEllipse (juce::Rectangle<float> (4.0f + 6.0f * a, 4.0f + 6.0f * a).withCentre (toPoint (trail[i])));
        }
        const auto puck = toPoint (last);
        g.setColour (Palette::mod.withAlpha (0.25f));
        g.drawVerticalLine ((int) puck.x, pad.getY(), pad.getBottom());
        g.drawHorizontalLine ((int) puck.y, pad.getX(), pad.getRight());
        glowRect (g, juce::Rectangle<float> (22, 22).withCentre (puck), 11.0f, Palette::mod, 1.2f);
        g.setColour (Palette::mod);
        g.fillEllipse (juce::Rectangle<float> (18, 18).withCentre (puck));
        g.setColour (Colours::text);
        g.fillEllipse (juce::Rectangle<float> (6, 6).withCentre (puck));

        // What each axis is: the macro's name, its value, and what it moves.
        g.setFont (mono (10.0f).boldened());
        g.setColour (Colours::text);
        const int ax = axis (true), ay = axis (false);
        g.drawText (proc.getMacroName (ax) + "  " + juce::String (juce::roundToInt (last.x * 100.0f)) + "%",
                    area.reduced (12, 6).removeFromBottom (14), juce::Justification::centredLeft, false);
        g.drawText (proc.getMacroName (ay) + "  " + juce::String (juce::roundToInt (last.y * 100.0f)) + "%",
                    area.reduced (12, 6).removeFromTop (14), juce::Justification::centredLeft, false);
        g.setFont (mono (9.0f));
        g.setColour (Colours::textDim);
        const auto dx = destinationsOf (7 + ax), dy = destinationsOf (7 + ay);
        g.drawText (dx.isEmpty() ? "not routed" : dx, area.reduced (12, 6).removeFromBottom (14), juce::Justification::centredRight, true);
        g.drawText (dy.isEmpty() ? "not routed" : dy, area.reduced (12, 6).removeFromTop (14), juce::Justification::centredRight, true);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (bool isX : { true, false }) if (auto* p = param (isX)) p->beginChangeGesture();
        dragging = true;
        mouseDrag (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        const auto pad = padArea();
        set (true, (e.position.x - pad.getX()) / pad.getWidth());
        set (false, (pad.getBottom() - e.position.y) / pad.getHeight());
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        dragging = false;
        for (bool isX : { true, false }) if (auto* p = param (isX)) p->endChangeGesture();
    }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        for (bool isX : { true, false })
            if (auto* p = param (isX)) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); }
    }

private:
    juce::ComboBox xBox, yBox;
    std::vector<juce::Point<float>> trail;
    juce::Point<float> last { -1, -1 };
    bool dragging = false;

    juce::Rectangle<float> padArea() const { return getLocalBounds().toFloat().reduced (18.0f, 33.0f); } // the puck never covers the axis labels
    int axis (bool isX) const { return juce::jlimit (0, 3, (int) config.getProperty (isX ? "x" : "y", isX ? 0 : 1)); }
    juce::RangedAudioParameter* param (bool isX) const { return proc.apvts.getParameter ("macro" + juce::String (axis (isX) + 1)); }
    float value (int macro) const { return proc.apvts.getRawParameterValue ("macro" + juce::String (macro + 1))->load(); }
    void set (bool isX, float v)
    {
        if (auto* p = param (isX)) p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v));
    }
};

//==============================================================================
// Every modulation source that is doing something, as a live trace: what it is, what it moves, and
// where it is right now. The name chips drag onto knobs like the others.
class ModMonitorTool : public ToolContent
{
public:
    ModMonitorTool (HypernovaAudioProcessor& p, juce::ValueTree cfg, ToolServices s) : ToolContent (p, cfg), services (std::move (s))
    {
        for (auto& h : history) h.fill (0.0f);
    }

    void tick (bool sounding) override
    {
        for (int s = 1; s < 11; ++s)
        {
            auto& h = history[(size_t) s];
            std::rotate (h.begin(), h.begin() + 1, h.end());
            h.back() = proc.shownModSource[(size_t) s].load();
        }
        const auto active = activeSources();
        if (active != shown) rebuild (active);
        if (sounding || ! quiet) repaint();
        quiet = ! sounding;
    }

    void resized() override { placeChips(); }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        inset (g, area);
        if (shown.empty())
        {
            g.setColour (Colours::textDim);
            g.setFont (font (12.0f));
            g.drawFittedText ("Nothing is modulating yet. Drag an LFO, envelope or macro chip onto a knob, and it shows up here.",
                              getLocalBounds().reduced (24), juce::Justification::centred, 3);
            return;
        }
        for (size_t i = 0; i < shown.size(); ++i)
        {
            const int src = shown[i];
            auto lane = laneArea (i);
            // Source chip, then what it moves, side by side so any number of lanes fits.
            auto info = lane.removeFromLeft (infoWidth (lane.getWidth())).withTrimmedLeft ((float) chipW + 8.0f);
            lane.removeFromLeft (8.0f);
            g.setColour (Colours::text.withAlpha (0.72f));
            g.setFont (font (10.5f));
            g.drawFittedText (destinationsOf (src), info.toNearestInt(), juce::Justification::centredLeft, lane.getHeight() > 30.0f ? 2 : 1, 0.85f);
            if (i > 0)
            {
                g.setColour (Colours::line.withAlpha (0.5f));
                g.drawHorizontalLine ((int) (lane.getY() - 4.0f), area.getX() + 10.0f, area.getRight() - 10.0f);
            }
            g.setColour (Colours::line);
            g.drawHorizontalLine ((int) lane.getCentreY(), lane.getX(), lane.getRight());
            juce::Path p;
            const auto& h = history[(size_t) src];
            const bool bipolar = src == 1 || src == 2 || src == 6;
            for (size_t k = 0; k < h.size(); ++k)
            {
                const float v = bipolar ? h[k] * 0.5f : h[k] - 0.5f;
                const juce::Point<float> pt (lane.getX() + lane.getWidth() * (float) k / (float) (h.size() - 1),
                                             lane.getCentreY() - v * lane.getHeight() * 0.9f);
                if (k == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            glowStroke (g, p, colourFor (src), 1.5f, 0.7f);
            g.setColour (Colours::text);
            g.fillEllipse (juce::Rectangle<float> (5, 5).withCentre (p.getCurrentPosition()));
        }
    }

private:
    ToolServices services;
    std::array<std::array<float, 120>, 12> history {};
    std::vector<int> shown;
    std::vector<std::unique_ptr<ModChip>> chips;
    bool quiet = false;

    static ThemeColour colourFor (int src)
    {
        switch (src) { case 1: case 2: return Palette::lfo; case 3: return Palette::env; case 4: return Palette::sub; default: return Palette::mod; }
    }

    std::vector<int> activeSources() const
    {
        std::vector<int> out;
        for (int i = 0; i < ab::NumModSlots; ++i)
        {
            const juce::String p = "mod" + juce::String (i + 1);
            const int s = (int) proc.apvts.getRawParameterValue (p + "Src")->load();
            const int d = (int) proc.apvts.getRawParameterValue (p + "Dest")->load();
            const float a = proc.apvts.getRawParameterValue (p + "Amt")->load();
            if (s > 0 && s < 11 && d > 0 && std::abs (a) > 0.005f && std::find (out.begin(), out.end(), s) == out.end()) out.push_back (s);
        }
        std::sort (out.begin(), out.end());
        return out;
    }

    void rebuild (const std::vector<int>& active)
    {
        shown = active;
        chips.clear();
        for (int s : shown)
        {
            chips.push_back (services.makeChip (s, modSrcNames()[s].toUpperCase()));
            addAndMakeVisible (*chips.back());
        }
        placeChips();
        repaint();
    }

    static constexpr int chipW = 104;
    juce::Rectangle<float> laneArea (size_t i) const
    {
        const auto area = getLocalBounds().toFloat().reduced (0, 4.0f);
        const float laneH = area.getHeight() / (float) juce::jmax ((size_t) 1, shown.size());
        return juce::Rectangle<float> (area.getX(), area.getY() + laneH * (float) i, area.getWidth(), laneH).reduced (10.0f, 4.0f);
    }
    static float infoWidth (float laneWidth) { return juce::jmin (270.0f, laneWidth * 0.46f); }

    void placeChips()
    {
        if (shown.empty()) return;
        for (size_t i = 0; i < chips.size(); ++i)
        {
            const auto lane = laneArea (i);
            chips[i]->setBounds (juce::Rectangle<float> (lane.getX(), lane.getCentreY() - 10.0f, (float) chipW, 20.0f).toNearestInt());
        }
    }
};

//==============================================================================
// The four macros, big: rename them in place, see what each one moves, drag one onto any knob.
class MacroTool : public ToolContent
{
public:
    MacroTool (HypernovaAudioProcessor& p, juce::ValueTree cfg, ToolServices s) : ToolContent (p, cfg), services (std::move (s))
    {
        for (int m = 0; m < 4; ++m)
        {
            auto& c = cols[(size_t) m];
            c.knob = services.makeKnob ("macro" + juce::String (m + 1), proc.getMacroName (m), Palette::fx, 56);
            c.knob->setShowValue (true);
            addAndMakeVisible (*c.knob);
            c.name.setText (proc.getMacroName (m), juce::dontSendNotification);
            c.name.setEditable (false, true, false);
            c.name.setJustificationType (juce::Justification::centred);
            c.name.setFont (mono (11.0f).boldened());
            c.name.setTooltip ("Double-click to rename this macro (saved with the sound)");
            c.name.onTextChange = [this, m] { proc.setMacroName (m, cols[(size_t) m].name.getText().trim().toUpperCase()); };
            addAndMakeVisible (c.name);
            c.chip = services.makeChip (7 + m, "DRAG");
            addAndMakeVisible (*c.chip);
        }
    }

    void tick (bool) override
    {
        const int v = proc.presetVersion.load();
        const int changes = proc.parameterChanges.load();
        if (v != lastVersion || changes != lastChanges)
        {
            lastVersion = v;
            lastChanges = changes;
            for (int m = 0; m < 4; ++m)
            {
                const auto n = proc.getMacroName (m);
                if (! cols[(size_t) m].name.isBeingEdited()) cols[(size_t) m].name.setText (n, juce::dontSendNotification);
                cols[(size_t) m].knob->setLabel (n);
            }
            repaint();
        }
        for (auto& c : cols) c.knob->ageTrail();
    }

    void resized() override
    {
        const float w = (float) getWidth() / 4.0f;
        for (int m = 0; m < 4; ++m)
        {
            auto col = juce::Rectangle<float> (w * (float) m, 0, w, (float) getHeight()).reduced (8, 8).toNearestInt();
            auto& c = cols[(size_t) m];
            c.name.setBounds (col.removeFromTop (22));
            c.knob->setBounds (col.removeFromTop (92).withSizeKeepingCentre (84, 92));
            c.chip->setBounds (col.removeFromTop (24).withSizeKeepingCentre (64, 20));
            c.text = col.reduced (2, 4);
        }
    }

    void paint (juce::Graphics& g) override
    {
        const float w = (float) getWidth() / 4.0f;
        for (int m = 0; m < 4; ++m)
        {
            auto col = juce::Rectangle<float> (w * (float) m, 0, w, (float) getHeight()).reduced (4);
            inset (g, col);
            g.setColour (Colours::textDim);
            g.setFont (font (10.0f));
            const auto d = destinationsOf (7 + m, 4);
            g.drawFittedText (d.isEmpty() ? "Moves nothing yet: drag DRAG onto a knob" : d.replace (", ", "\n"),
                              cols[(size_t) m].text, juce::Justification::centredTop, 5, 0.8f);
        }
    }

private:
    ToolServices services;
    struct Column { std::unique_ptr<Knob> knob; juce::Label name; std::unique_ptr<ModChip> chip; juce::Rectangle<int> text; };
    std::array<Column, 4> cols;
    int lastVersion = -1, lastChanges = -1;
};

//==============================================================================
// Your own panel: pin any knob here (right-click it, Pin to pinboard) and it appears as a real control.
class PinboardTool : public ToolContent
{
public:
    PinboardTool (HypernovaAudioProcessor& p, juce::ValueTree cfg, ToolServices s) : ToolContent (p, cfg), services (std::move (s)) { rebuild(); }

    juce::StringArray params() const
    {
        auto a = juce::StringArray::fromTokens (config.getProperty ("params").toString(), ",", {});
        a.removeEmptyStrings();
        return a;
    }
    bool has (const juce::String& id) const { return params().contains (id); }
    void pin (const juce::String& id)   { auto a = params(); a.addIfNotAlreadyThere (id); config.setProperty ("params", a.joinIntoString (","), nullptr); rebuild(); }
    void unpin (const juce::String& id) { auto a = params(); a.removeString (id); config.setProperty ("params", a.joinIntoString (","), nullptr); rebuild(); }

    void tick (bool) override { for (auto& k : knobs) k->ageTrail(); }

    void resized() override
    {
        const int cellW = 78, cellH = 86;
        const int perRow = juce::jmax (1, getWidth() / cellW);
        int i = 0;
        const int offsetX = (getWidth() - juce::jmin (perRow, (int) cells.size()) * cellW) / 2;
        for (auto* c : cells)
        {
            const int row = i / perRow, col = i % perRow;
            auto cell = juce::Rectangle<int> (offsetX + col * cellW, 6 + row * cellH, cellW, cellH);
            if (dynamic_cast<Knob*> (c) != nullptr) c->setBounds (cell.withTrimmedTop (4).withHeight (74));
            else c->setBounds (cell.withSizeKeepingCentre (cellW - 8, 24));
            ++i;
        }
    }

    void paint (juce::Graphics& g) override
    {
        inset (g, getLocalBounds().toFloat());
        if (cells.empty())
        {
            g.setColour (Colours::textDim);
            g.setFont (font (12.0f));
            g.drawFittedText ("Right-click any knob and choose Pin to pinboard. Build a panel of the controls you reach for.",
                              getLocalBounds().reduced (20), juce::Justification::centred, 3);
        }
    }

private:
    ToolServices services;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<juce::ComboBox>> boxes;
    std::vector<std::unique_ptr<juce::ToggleButton>> buttons;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> boxAttach;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttach;
    std::vector<juce::Component*> cells;

    void rebuild()
    {
        cells.clear();
        boxAttach.clear(); buttonAttach.clear();
        knobs.clear(); boxes.clear(); buttons.clear();
        for (auto& id : params())
        {
            auto* p = proc.apvts.getParameter (id);
            if (p == nullptr) continue;
            const auto label = p->getName (14).toUpperCase();
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (p))
            {
                boxes.push_back (std::make_unique<juce::ComboBox>());
                boxes.back()->addItemList (choice->choices, 1);
                boxes.back()->setTooltip (p->getName (64));
                boxAttach.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, *boxes.back()));
                cells.push_back (boxes.back().get());
            }
            else if (dynamic_cast<juce::AudioParameterBool*> (p) != nullptr)
            {
                buttons.push_back (std::make_unique<PillToggle> (label, Palette::mod));
                buttonAttach.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, *buttons.back()));
                cells.push_back (buttons.back().get());
            }
            else
            {
                knobs.push_back (services.makeKnob (id, label, Palette::mod, 44));
                cells.push_back (knobs.back().get());
            }
            addAndMakeVisible (cells.back());
        }
        resized();
        repaint();
    }
};

//==============================================================================
// The widget library: every kind of widget, what it's for, and whether it's on screen. Click one to add
// it where there's most room, or drag it to exactly where it should go.
class WidgetLibrary : public juce::Component
{
public:
    struct Entry
    {
        juce::String type, name, group, description;
        bool multi = false;
        int state = 0; // 0 not on screen, 1 on screen, 2 behind a tab
        ThemeColour colour { SlotAccent };
    };
    std::function<std::vector<Entry>()> entries;
    std::function<void (const juce::String& type)> onAdd;
    std::function<void (const juce::String& type, juce::Image ghost, const juce::MouseEvent&)> onDragStart;
    std::function<void (const juce::MouseEvent&)> onDragMove, onDragEnd;
    std::function<void()> onClose;

    WidgetLibrary()
    {
        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&list, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (6);
        addAndMakeVisible (close);
        close.onClick = [this] { if (onClose) onClose(); };
    }

    void refresh()
    {
        cards.clear();
        list.removeAllChildren();
        const auto all = entries ? entries() : std::vector<Entry>();
        int y = 0;
        juce::String group;
        headers.clear();
        for (auto& e : all)
        {
            if (e.group != group)
            {
                group = e.group;
                headers.push_back ({ y, group });
                y += 26;
            }
            cards.push_back (std::make_unique<Card> (*this, e));
            list.addAndMakeVisible (*cards.back());
            cards.back()->setBounds (0, y, 292, 62);
            y += 66;
        }
        list.headers = headers;
        list.setSize (292, y + 8);
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto top = r.removeFromTop (34);
        close.setBounds (top.removeFromRight (30).withSizeKeepingCentre (26, 24));
        viewport.setBounds (r);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        for (int i = 4; i >= 1; --i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.07f * (float) (5 - i)));
            g.fillRoundedRectangle (r.translated (-2.0f * (float) i, 2.0f * (float) i).expanded ((float) i), 16.0f);
        }
        g.setColour (Colours::bg0);
        g.fillRoundedRectangle (r, 14.0f);
        g.setColour (Colours::panel);
        g.fillRoundedRectangle (r, 14.0f);
        g.setColour (Colours::lineHi);
        g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);
        g.setColour (Colours::text);
        g.setFont (mono (12.0f).boldened().withExtraKerningFactor (0.1f));
        g.drawText ("WIDGET LIBRARY", juce::Rectangle<int> (16, 12, 220, 34), juce::Justification::centredLeft, false);
        g.setColour (Colours::textDim);
        g.setFont (font (10.0f));
        g.drawText ("click to add, or drag into place", juce::Rectangle<int> (16, 32, 260, 16), juce::Justification::centredLeft, false);
    }

private:
    struct Header { int y; juce::String text; };
    std::vector<Header> headers;

    class List : public juce::Component
    {
    public:
        std::vector<Header> headers;
        void paint (juce::Graphics& g) override
        {
            for (auto& h : headers)
            {
                g.setColour (Colours::textDim);
                g.setFont (mono (10.0f).boldened().withExtraKerningFactor (0.14f));
                g.drawText (h.text, juce::Rectangle<int> (4, h.y + 6, 280, 18), juce::Justification::centredLeft, false);
            }
        }
    } list;

    class Card : public juce::Component, public juce::SettableTooltipClient
    {
    public:
        Card (WidgetLibrary& o, Entry e) : owner (o), entry (std::move (e))
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
            setTooltip (entry.description);
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (isMouseOver() ? Colours::panelHi.brighter (0.05f) : Colours::panelHi.get());
            g.fillRoundedRectangle (r, 10.0f);
            g.setColour (isMouseOver() ? entry.colour.withAlpha (0.8f) : Colours::line.get());
            g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
            auto icon = r.removeFromLeft (54).reduced (9);
            drawIcon (g, icon);
            auto text = r.reduced (4, 8);
            auto badge = text.removeFromRight (70);
            g.setColour (Colours::text);
            g.setFont (font (12.0f, true));
            g.drawText (entry.name, text.removeFromTop (18), juce::Justification::centredLeft, true);
            g.setColour (Colours::textDim);
            g.setFont (font (10.0f));
            g.drawFittedText (entry.description, text.toNearestInt(), juce::Justification::topLeft, 2, 0.85f);
            const juce::String b = entry.multi ? "+ ADD" : entry.state == 1 ? "ON SCREEN" : entry.state == 2 ? "IN A TAB" : "+ ADD";
            const bool active = entry.multi || entry.state != 1;
            auto chip = badge.withSizeKeepingCentre (66, 20);
            g.setColour (active ? entry.colour.withAlpha (0.2f) : Colours::inset.get());
            g.fillRoundedRectangle (chip, 10.0f);
            g.setColour (active ? entry.colour.get() : Colours::textFaint.get());
            g.setFont (mono (9.0f).boldened());
            g.drawText (b, chip, juce::Justification::centred, false);
        }

        void mouseEnter (const juce::MouseEvent&) override { repaint(); }
        void mouseExit (const juce::MouseEvent&) override { repaint(); }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! dragging && e.getDistanceFromDragStart() > 6)
            {
                dragging = true;
                if (owner.onDragStart) owner.onDragStart (entry.type, createComponentSnapshot (getLocalBounds(), true, 2.0f), e);
                owner.setAlpha (0.25f);
            }
            if (dragging && owner.onDragMove) owner.onDragMove (e);
        }
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (dragging)
            {
                dragging = false;
                owner.setAlpha (1.0f);
                if (owner.onDragEnd) owner.onDragEnd (e);
                return;
            }
            if (getLocalBounds().contains (e.getPosition()) && owner.onAdd) owner.onAdd (entry.type);
        }

    private:
        WidgetLibrary& owner;
        Entry entry;
        bool dragging = false;

        void drawIcon (juce::Graphics& g, juce::Rectangle<float> r) const
        {
            g.setColour (entry.colour.withAlpha (0.14f));
            g.fillRoundedRectangle (r, 8.0f);
            const auto c = entry.colour.get();
            auto in = r.reduced (7);
            juce::Path p;
            const auto& t = entry.type;
            if (t == "scope" || t == "oscA" || t == "oscB")
            {
                for (int i = 0; i <= 24; ++i)
                {
                    const float x = in.getX() + in.getWidth() * (float) i / 24.0f;
                    const float y = in.getCentreY() - std::sin ((float) i / 24.0f * 6.283f * (t == "scope" ? 2.0f : 1.0f)) * in.getHeight() * 0.4f;
                    if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                }
            }
            else if (t == "meter")
            {
                for (int i = 0; i < 2; ++i)
                    p.addRoundedRectangle (in.getX() + (float) i * in.getWidth() * 0.55f, in.getY() + in.getHeight() * (i == 0 ? 0.25f : 0.45f),
                                           in.getWidth() * 0.35f, in.getHeight() * (i == 0 ? 0.75f : 0.55f), 2.0f);
                g.setColour (c);
                g.fillPath (p);
                return;
            }
            else if (t == "xy")
            {
                p.addRectangle (in);
                g.setColour (c.withAlpha (0.6f));
                g.strokePath (p, juce::PathStrokeType (1.0f));
                g.setColour (c);
                g.fillEllipse (juce::Rectangle<float> (8, 8).withCentre (in.getRelativePoint (0.65f, 0.35f)));
                return;
            }
            else if (t == "filter")
            {
                p.startNewSubPath (in.getX(), in.getCentreY());
                p.lineTo (in.getX() + in.getWidth() * 0.55f, in.getCentreY());
                p.quadraticTo (in.getX() + in.getWidth() * 0.7f, in.getY(), in.getRight(), in.getBottom());
            }
            else if (t == "env")
            {
                p.startNewSubPath (in.getBottomLeft());
                p.lineTo (in.getX() + in.getWidth() * 0.2f, in.getY());
                p.lineTo (in.getX() + in.getWidth() * 0.5f, in.getCentreY());
                p.lineTo (in.getX() + in.getWidth() * 0.8f, in.getCentreY());
                p.lineTo (in.getBottomRight());
            }
            else if (t == "pinboard" || t == "macros" || t == "fx" || t == "morefx" || t == "play" || t == "sub" || t == "pitch")
            {
                const int n = t == "macros" ? 4 : 3;
                for (int i = 0; i < n; ++i)
                {
                    const float d = in.getWidth() / (float) n;
                    auto k = juce::Rectangle<float> (d * 0.8f, d * 0.8f).withCentre ({ in.getX() + d * ((float) i + 0.5f), in.getCentreY() });
                    p.addEllipse (k);
                }
                g.setColour (c);
                g.strokePath (p, juce::PathStrokeType (1.6f));
                return;
            }
            else if (t == "space")
            {
                for (int i = 0; i < 3; ++i) p.addEllipse (in.reduced ((float) i * 4.0f));
            }
            else // modulation, monitor
            {
                for (int i = 0; i <= 24; ++i)
                {
                    const float x = in.getX() + in.getWidth() * (float) i / 24.0f;
                    const float y = in.getCentreY() + ((i / 6) % 2 == 0 ? -1.0f : 1.0f) * in.getHeight() * 0.3f;
                    if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
                }
            }
            g.setColour (c);
            g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    };

    std::vector<std::unique_ptr<Card>> cards;
    juce::Viewport viewport;
    IconButton close { IconButton::Close };
};

} // namespace ab::ui
