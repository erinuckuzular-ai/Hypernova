#pragma once

#include "Components.h"

// Low End: the output's spectrum split at the crossover. Everything left of the line stays clean (no effects);
// everything right of it goes through the rack. Drag the line to move the crossover.
namespace ab::ui
{

class LowEndView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit LowEndView (HypernovaAudioProcessor& p) : proc (p)
    {
        setTooltip ("Drag the line to set the crossover. Below it: the clean sub. Above it: what the effects get.");
        window.resize ((size_t) fftSize);
        for (int i = 0; i < fftSize; ++i)
            window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (fftSize - 1));
        fftData.assign ((size_t) fftSize * 2, 0.0f);
        smoothed.assign ((size_t) fftSize / 2, -90.0f);
    }

    void refresh (bool sounding)
    {
        if (! sounding && quiet > 30) return;
        quiet = sounding ? 0 : quiet + 1;
        proc.scope.latest (scopeL.data(), scopeR.data(), fftSize);
        std::fill (fftData.begin(), fftData.end(), 0.0f);
        for (int i = 0; i < fftSize; ++i) fftData[(size_t) i] = 0.5f * (scopeL[(size_t) i] + scopeR[(size_t) i]) * window[(size_t) i];
        fft.performFrequencyOnlyForwardTransform (fftData.data());
        for (int k = 1; k < fftSize / 2; ++k)
        {
            const float db = juce::Decibels::gainToDecibels (fftData[(size_t) k] * 4.0f / (float) fftSize, -90.0f);
            auto& s = smoothed[(size_t) k];
            s = db > s ? db : s + (db - s) * 0.25f;
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::inset);
        g.fillRoundedRectangle (r, 8.0f);
        const auto plot = r.reduced (8.0f, 6.0f);
        const bool on = value ("lowOn") > 0.5f;
        const float xover = value ("lowXover");
        const float xx = xOf (xover, plot);

        // The two bands.
        g.setColour ((on ? Palette::sub : Colours::textFaint).withAlpha (on ? 0.14f : 0.05f));
        g.fillRect (juce::Rectangle<float> (plot.getX(), plot.getY(), xx - plot.getX(), plot.getHeight()));
        g.setColour (Colours::line);
        for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f })
            g.drawVerticalLine ((int) xOf (hz, plot), plot.getY(), plot.getBottom());
        g.setColour (Colours::textFaint);
        g.setFont (mono (8.5f));
        for (auto [hz, label] : { std::pair<float, const char*> { 50.0f, "50" }, { 100.0f, "100" }, { 200.0f, "200" }, { 500.0f, "500" }, { 1000.0f, "1k" }, { 5000.0f, "5k" } })
            g.drawText (label, juce::Rectangle<float> (xOf (hz, plot) + 2.0f, plot.getBottom() - 12.0f, 30.0f, 12.0f), juce::Justification::centredLeft, false);

        // Spectrum, coloured by band.
        const double sr = juce::jmax (8000.0, proc.getCurrentSampleRate());
        juce::Path lowPath, highPath;
        bool lowStarted = false, highStarted = false;
        for (int k = 1; k < fftSize / 2; ++k)
        {
            const float hz = (float) (k * sr / fftSize);
            if (hz < 20.0f || hz > 12000.0f) continue;
            const float x = xOf (hz, plot);
            const float y = plot.getBottom() - plot.getHeight() * juce::jlimit (0.0f, 1.0f, (smoothed[(size_t) k] + 72.0f) / 72.0f);
            auto& path = hz <= xover ? lowPath : highPath;
            auto& started = hz <= xover ? lowStarted : highStarted;
            if (! started) { path.startNewSubPath (x, y); started = true; } else path.lineTo (x, y);
        }
        glowStroke (g, lowPath, on ? Palette::sub.get() : Colours::textDim.get(), 1.6f, 0.7f);
        glowStroke (g, highPath, Palette::fx, 1.4f, 0.5f);

        // Crossover line and labels.
        g.setColour (on ? Palette::sub.get() : Colours::textDim.get());
        g.fillRect (xx - 1.0f, plot.getY(), 2.0f, plot.getHeight());
        g.fillEllipse (juce::Rectangle<float> (10, 10).withCentre ({ xx, plot.getY() + 8.0f }));
        g.setFont (mono (10.0f).boldened());
        g.drawText (juce::String (juce::roundToInt (xover)) + " Hz", juce::Rectangle<float> (xx + 8.0f, plot.getY() + 2.0f, 70.0f, 14.0f),
                    juce::Justification::centredLeft, false);
        g.setFont (mono (9.5f).boldened().withExtraKerningFactor (0.1f));
        g.setColour (on ? Palette::sub.get() : Colours::textFaint.get());
        g.drawText (on ? "CLEAN SUB" : "LOW END OFF", juce::Rectangle<float> (plot.getX() + 4.0f, plot.getY() + 18.0f, xx - plot.getX() - 8.0f, 14.0f),
                    juce::Justification::centredLeft, true);
        g.setColour (Palette::fx);
        g.drawText ("TO THE EFFECTS", juce::Rectangle<float> (xx + 8.0f, plot.getY() + 18.0f, 140.0f, 14.0f), juce::Justification::centredLeft, false);

        // Band balance: how much of the output sits below the line.
        if (on)
        {
            const float lo = proc.shownLowRms.load(), hi = proc.shownHighRms.load();
            const float share = lo + hi > 1.0e-5f ? lo / (lo + hi) : 0.0f;
            auto bar = juce::Rectangle<float> (plot.getRight() - 110.0f, plot.getY() + 4.0f, 100.0f, 6.0f);
            g.setColour (Colours::bg0.withAlpha (0.5f));
            g.fillRoundedRectangle (bar, 3.0f);
            g.setColour (Palette::sub);
            g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * share), 3.0f);
            g.setColour (Colours::textDim);
            g.setFont (mono (8.5f));
            g.drawText ("SUB " + juce::String (juce::roundToInt (share * 100.0f)) + "%", bar.translated (0, 8.0f).withHeight (12.0f),
                        juce::Justification::centredRight, false);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (auto* p = proc.apvts.getParameter ("lowXover")) p->beginChangeGesture();
        dragging = true;
        mouseDrag (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        const auto plot = getLocalBounds().toFloat().reduced (8.0f, 6.0f);
        const float hz = hzOf (e.position.x, plot);
        if (auto* p = proc.apvts.getParameter ("lowXover")) p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (40.0f, 300.0f, hz)));
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (auto* p = proc.apvts.getParameter ("lowXover")) p->endChangeGesture();
        dragging = false;
    }

private:
    HypernovaAudioProcessor& proc;
    static constexpr int fftOrder = 12, fftSize = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    std::vector<float> window, fftData, smoothed;
    std::array<float, fftSize> scopeL {}, scopeR {};
    int quiet = 0;
    bool dragging = false;

    float value (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }
    static float xOf (float hz, juce::Rectangle<float> plot)
    {
        return plot.getX() + plot.getWidth() * std::log (hz / 20.0f) / std::log (12000.0f / 20.0f);
    }
    static float hzOf (float x, juce::Rectangle<float> plot)
    {
        return 20.0f * std::pow (12000.0f / 20.0f, (x - plot.getX()) / plot.getWidth());
    }
};

} // namespace ab::ui
