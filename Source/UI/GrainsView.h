#pragma once

#include "Components.h"

// Where the grains are reading from: the recording drawn small, with the window the cloud is drawn from
// (POSITION, widened by SPRAY) shaded over it, and the grains going off as they are played.
namespace ab::ui
{

class GrainsView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit GrainsView (HypernovaAudioProcessor& p) : proc (p)
    {
        setTooltip ("Where in the recording the grains come from. Drag to move POSITION; the shaded band is SPRAY.");
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    }

    void refresh()
    {
        const int v = proc.sampleVersion.load(), changes = proc.parameterChanges.load();
        if (v != version) { version = v; peaks.clear(); }
        if (changes != lastChanges || sparkle > 0) { lastChanges = changes; repaint(); }
    }

    void resized() override { peaks.clear(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (Colours::inset);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (Colours::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);

        const auto sample = proc.sampleForUi();
        auto plot = r.reduced (8.0f, 6.0f);
        if (sample == nullptr)
        {
            g.setColour (Colours::textFaint);
            g.setFont (mono (9.5f));
            g.drawText ("load a sample, or record this sound into the sampler", plot.toNearestInt(), juce::Justification::centred, false);
            return;
        }
        buildPeaks (*sample, (int) plot.getWidth());

        const float position = value ("grainPos"), spray = value ("grainSpray");
        const float mid = plot.getCentreY(), half = plot.getHeight() * 0.44f;
        const bool on = value ("grainOn") > 0.5f;

        // The band the grains are drawn from.
        const float from = juce::jlimit (0.0f, 1.0f, position - spray), to = juce::jlimit (0.0f, 1.0f, position + spray);
        auto xOf = [&] (float v) { return plot.getX() + plot.getWidth() * v; };
        g.setColour ((on ? Palette::oscA.get() : Colours::textFaint.get()).withAlpha (0.12f));
        g.fillRect (juce::Rectangle<float> (xOf (from), plot.getY(), juce::jmax (2.0f, xOf (to) - xOf (from)), plot.getHeight()));

        if (peaks.size() > 1)
        {
            juce::Path shape;
            const float step = plot.getWidth() / (float) (peaks.size() - 1);
            for (size_t i = 0; i < peaks.size(); ++i)
                shape.addLineSegment ({ plot.getX() + step * (float) i, mid - juce::jmax (peaks[i].second * half, 0.4f),
                                        plot.getX() + step * (float) i, mid - juce::jmin (peaks[i].first * half, -0.4f) }, 1.0f);
            g.setColour ((on ? Palette::oscA.get() : Colours::textFaint.get()).withAlpha (0.55f));
            g.fillPath (shape);
        }

        // The grains themselves: one mark per grain sounding, spread across the band.
        if (on)
        {
            const int voices = juce::jlimit (0, 8, proc.shownVoices.load());
            juce::Random r ((int) (juce::Time::getMillisecondCounter() / 60) * 2654435761u);
            for (int i = 0; i < voices * 3; ++i)
            {
                const float at = juce::jlimit (0.0f, 1.0f, position + spray * (r.nextFloat() * 2.0f - 1.0f));
                const float y = mid + (r.nextFloat() * 2.0f - 1.0f) * half * 0.8f;
                g.setColour (Palette::oscA.withAlpha (0.25f + 0.45f * r.nextFloat()));
                g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre ({ xOf (at), y }));
            }
        }

        g.setColour ((on ? Colours::text.get() : Colours::textFaint.get()).withAlpha (0.9f));
        g.fillRect (xOf (position) - 0.75f, plot.getY(), 1.5f, plot.getHeight());
        g.setColour (Colours::textFaint);
        g.setFont (mono (8.5f).withExtraKerningFactor (0.1f));
        g.drawText (on ? juce::String (juce::roundToInt (value ("grainRate"))) + "/S   "
                             + juce::String (juce::roundToInt (value ("grainSize") * 1000.0f)) + " MS"
                       : juce::String ("GRAINS OFF"),
                    plot.toNearestInt().reduced (2, 1), juce::Justification::bottomLeft, false);
    }

    void mouseDown (const juce::MouseEvent& e) override { dragTo (e); }
    void mouseDrag (const juce::MouseEvent& e) override { dragTo (e); }

private:
    HypernovaAudioProcessor& proc;
    std::vector<std::pair<float, float>> peaks;
    int version = -1, lastChanges = -1, sparkle = 0;

    float value (const char* id) const
    {
        auto* v = proc.apvts.getRawParameterValue (id);
        return v != nullptr ? v->load() : 0.0f;
    }

    void dragTo (const juce::MouseEvent& e)
    {
        auto plot = getLocalBounds().toFloat().reduced (9.0f, 7.0f);
        const float at = juce::jlimit (0.0f, 1.0f, (e.position.x - plot.getX()) / juce::jmax (1.0f, plot.getWidth()));
        if (auto* p = proc.apvts.getParameter ("grainPos")) p->setValueNotifyingHost (p->convertTo0to1 (at));
        repaint();
    }

    void buildPeaks (const SampleData& s, int width)
    {
        if ((int) peaks.size() == width || width <= 0) return;
        peaks.assign ((size_t) width, { 0.0f, 0.0f });
        for (int x = 0; x < width; ++x)
        {
            const int a = (int) ((juce::int64) s.length * x / width), b = juce::jmax (a + 1, (int) ((juce::int64) s.length * (x + 1) / width));
            float lo = 0, hi = 0;
            for (int i = a; i < b && i < s.length; ++i)
            {
                const float v = 0.5f * (s.l[(size_t) i + 2] + s.r[(size_t) i + 2]);
                lo = juce::jmin (lo, v);
                hi = juce::jmax (hi, v);
            }
            peaks[(size_t) x] = { lo, hi };
        }
    }
};

} // namespace ab::ui
