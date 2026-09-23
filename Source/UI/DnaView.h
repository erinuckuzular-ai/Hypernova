#pragma once

#include "Components.h"

// Sound DNA: six children of the sounds you have. Click one to hear it, right-click it to breed from it
// again. Nothing is saved until you keep the sound yourself, so a whole family can be tried out and left.
namespace ab::ui
{

class DnaView : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit DnaView (HypernovaAudioProcessor& p) : proc (p)
    {
        setTooltip ("Click a child to hear it. Right-click one to breed from it again. Undo puts the sound back.");
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    std::function<void (const juce::String&)> onMessage;
    float mutation = 0.35f;

    void refresh()
    {
        const int changes = proc.parameterChanges.load();
        if (changes != lastChanges || proc.childPlaying() != lastHeard)
        {
            lastChanges = changes;
            lastHeard = proc.childPlaying();
            repaint();
        }
    }

    juce::Rectangle<float> tile (int i) const
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        const float gap = 6.0f;
        const float w = (r.getWidth() - gap * 5.0f) / 6.0f;
        return { r.getX() + (w + gap) * (float) i, r.getY(), w, r.getHeight() };
    }

    void paint (juce::Graphics& g) override
    {
        const bool any = proc.hasChildren();
        for (int i = 0; i < HypernovaAudioProcessor::NumChildren; ++i)
        {
            const auto box = tile (i);
            const bool playing = proc.childPlaying() == i;
            const bool over = hover == i;
            g.setColour (playing ? Palette::oscB.withAlpha (0.28f)
                                 : (over && any ? Colours::panelHi.brighter (0.06f) : Colours::panelHi.withAlpha (any ? 0.8f : 0.35f)));
            g.fillRoundedRectangle (box, 8.0f);
            g.setColour (playing ? Palette::oscB.get() : Colours::line.get());
            g.drawRoundedRectangle (box.reduced (0.5f), 8.0f, playing ? 1.4f : 1.0f);

            // A little strand for each child, drawn from its own number so it always looks the same.
            if (any)
            {
                juce::Random r (i * 7919 + generation * 104729);
                const auto strand = box.reduced (10.0f, 14.0f);
                for (int k = 0; k < 7; ++k)
                {
                    const float t = (float) k / 6.0f;
                    const float y = strand.getY() + strand.getHeight() * t;
                    const float wob = std::sin (t * 6.0f + (float) i) * strand.getWidth() * 0.28f;
                    const float size = 3.0f + r.nextFloat() * 2.0f;
                    g.setColour ((playing ? Palette::oscB.get() : Colours::textDim.get()).withAlpha (0.35f + 0.5f * r.nextFloat()));
                    g.fillEllipse (juce::Rectangle<float> (size, size).withCentre ({ strand.getCentreX() + wob, y }));
                    g.fillEllipse (juce::Rectangle<float> (size, size).withCentre ({ strand.getCentreX() - wob, y }));
                    g.setColour (Colours::line.withAlpha (0.5f));
                    g.drawLine (strand.getCentreX() - wob, y, strand.getCentreX() + wob, y, 0.8f);
                }
            }
            g.setColour (any ? (playing ? Colours::text.get() : Colours::textDim.get()) : Colours::textFaint.get());
            g.setFont (mono (9.0f).boldened());
            g.drawText (juce::String (i + 1), box.withTrimmedTop (box.getHeight() - 14.0f), juce::Justification::centred, false);
        }
        if (! any)
        {
            g.setColour (Colours::textFaint);
            g.setFont (mono (9.5f));
            g.drawText ("press BREED to cross the sounds you have captured", getLocalBounds(), juce::Justification::centred, false);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int was = hover;
        hover = -1;
        for (int i = 0; i < HypernovaAudioProcessor::NumChildren; ++i) if (tile (i).contains (e.position)) hover = i;
        if (hover != was) repaint();
    }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! proc.hasChildren()) return;
        for (int i = 0; i < HypernovaAudioProcessor::NumChildren; ++i)
        {
            if (! tile (i).contains (e.position)) continue;
            if (e.mods.isPopupMenu())
            {
                ++generation;
                proc.breedFromChild (i, mutation);
                if (onMessage) onMessage ("Six more from that one");
            }
            else
            {
                proc.hearChild (i);
                if (onMessage) onMessage ("Child " + juce::String (i + 1) + ": keep playing with it, or undo to go back");
            }
            repaint();
            return;
        }
    }

    void newGeneration() { ++generation; repaint(); }

private:
    HypernovaAudioProcessor& proc;
    int hover = -1, lastChanges = -1, lastHeard = -1, generation = 0;
};

} // namespace ab::ui
