#pragma once

#include "Components.h"
#include "../DSP/Resonator.h"

// What the resonator is tuned to: each partial as a line, where it sits against the note and how strongly it
// rings, with the decay drawn behind them. Moving STRUCTURE pulls the partials off the whole numbers, which is
// what turns a string into a bell.
namespace ab::ui
{

class ResonatorView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit ResonatorView (HypernovaAudioProcessor& p) : proc (p)
    {
        setTooltip ("Where the resonator rings: each line is a partial, its height how strongly it sounds."
                    " The curve behind them is how long it rings for.");
    }

    void refresh()
    {
        const int changes = proc.parameterChanges.load();
        if (changes != lastChanges) { lastChanges = changes; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (Colours::inset);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (Colours::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

        ab::dsp::Resonator::Settings s;
        s.model = (int) value ("resModel");
        s.structure = value ("resStruct");
        s.bright = value ("resBright");
        s.decay = value ("resDecay");
        s.position = value ("resPos");
        s.mix = value ("resMix");
        const bool on = value ("resOn") > 0.5f && s.mix > 0.001f;

        auto plot = r.reduced (10.0f, 8.0f);
        const float base = plot.getBottom() - 12.0f;

        // How long it rings: a decay curve across the panel, faint, behind the partials.
        const float seconds = 0.05f * std::pow (400.0f, juce::jlimit (0.0f, 1.0f, s.decay));
        {
            juce::Path curve;
            const float shown = juce::jmax (1.0f, seconds);
            for (int i = 0; i <= 64; ++i)
            {
                const float t = (float) i / 64.0f * shown;
                const float level = std::exp (-6.9078f * t / juce::jmax (0.05f, seconds));
                const juce::Point<float> at (plot.getX() + plot.getWidth() * (float) i / 64.0f, base - (base - plot.getY()) * level);
                if (i == 0) curve.startNewSubPath (at); else curve.lineTo (at);
            }
            g.setColour (Palette::lfo.withAlpha (on ? 0.18f : 0.08f));
            g.strokePath (curve, juce::PathStrokeType (1.5f));
        }

        // The partials: where they sit against the note, and how loudly each one rings.
        const auto lines = ab::dsp::Resonator::partials (s);
        float furthest = 1.0f;
        for (auto& [ratio, level] : lines) furthest = juce::jmax (furthest, ratio);
        for (auto& [ratio, level] : lines)
        {
            const float x = plot.getX() + plot.getWidth() * (ratio - 1.0f) / juce::jmax (0.001f, furthest - 1.0f) * 0.96f + 4.0f;
            const float top = base - (base - plot.getY()) * juce::jlimit (0.03f, 1.0f, level);
            g.setColour ((on ? Palette::lfo.get() : Colours::textFaint.get()).withAlpha (0.35f + 0.6f * level));
            g.fillRect (juce::Rectangle<float> (x - 1.0f, top, 2.0f, base - top));
        }

        g.setColour (Colours::line.withAlpha (0.8f));
        g.drawHorizontalLine ((int) base, plot.getX(), plot.getRight());
        g.setColour (Colours::textFaint);
        g.setFont (mono (8.5f).withExtraKerningFactor (0.1f));
        g.drawText (ab::dsp::Resonator::modelNames()[juce::jlimit (0, ab::dsp::Resonator::NumModels - 1, s.model)].toUpperCase()
                        + "   RINGS FOR " + (seconds < 1.0f ? juce::String (juce::roundToInt (seconds * 1000.0f)) + " MS"
                                                            : juce::String (seconds, seconds < 10.0f ? 1 : 0) + " S"),
                    plot.withTrimmedTop (plot.getHeight() - 12.0f).toNearestInt(), juce::Justification::centredLeft, false);
        if (! on)
        {
            g.setColour (Colours::textFaint);
            g.setFont (mono (9.0f));
            g.drawText ("send a source into it and turn MIX up", plot.toNearestInt(), juce::Justification::centred, false);
        }
    }

private:
    HypernovaAudioProcessor& proc;
    int lastChanges = -1;
    float value (const char* id) const
    {
        auto* v = proc.apvts.getRawParameterValue (id);
        return v != nullptr ? v->load() : 0.0f;
    }
};

} // namespace ab::ui
