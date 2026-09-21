#pragma once

#include "Components.h"

// The effects rack as a row of chips in signal order. Drag a chip to move that effect (the others make
// room as you go), click its light to switch it off and on, click its name to open its controls.
// Width and mono bass always come last: they're the output stage.
namespace ab::ui
{

class FxChainView : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit FxChainView (HypernovaAudioProcessor& p) : proc (p)
    {
        setTooltip ("Drag an effect to change the order the sound goes through them. Click the light to switch one off.");
        order = proc.getFxOrder();
        for (int i = 0; i < NumFx; ++i) shownX[(size_t) i] = -1.0f;
    }

    std::function<void (int fxId)> onShowEffect;
    std::function<void (juce::Point<int> screenPos)> onMenu;

    static const char* onParam (int id)
    {
        static const char* ids[] = { "distOn", "tapeOn", "ottOn", "shiftOn", "chorusOn", "flangOn", "fxFltOn", "gateOn", "dlyOn", "verbOn", "eqOn" };
        return ids[juce::jlimit (0, NumFx - 1, id)];
    }

    // Is the effect doing anything to the sound right now (switched on and turned up)?
    bool audible (int id) const
    {
        auto v = [this] (const char* p) { return proc.apvts.getRawParameterValue (p)->load(); };
        if (v (onParam (id)) < 0.5f) return false;
        switch (id)
        {
            case FxDist:    return v ("distMix") > 0.001f;
            case FxTape:    return v ("tapeWow") > 0.001f || v ("tapeNoise") > 0.001f || v ("tapeSat") > 0.001f;
            case FxOtt:     return v ("ott") > 0.001f;
            case FxPitch:   return v ("shiftMix") > 0.001f;
            case FxChorus:  return v ("chorusMix") > 0.001f;
            case FxFlanger: return v ("flangMix") > 0.001f;
            case FxFilter:  return v ("fxFltFreq") < 19000.0f || v ("fxFltDepth") > 0.001f || (int) v ("fxFltType") != 0;
            case FxGate:    return v ("gateDepth") > 0.001f || v ("panDepth") > 0.001f;
            case FxDelay:   return v ("dlyMix") > 0.001f;
            case FxReverb:  return v ("verbMix") > 0.001f;
            case FxEq:      return std::abs (v ("eqLow")) > 0.05f || std::abs (v ("eqHigh")) > 0.05f;
            default:        return false;
        }
    }

    void refresh()
    {
        const int changes = proc.parameterChanges.load();
        if (changes == lastChanges) return;
        lastChanges = changes;
        if (dragIndex < 0) order = proc.getFxOrder();
        startTimerHz (60);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto row = chipRow();
        const float w = slotWidth();
        const auto names = fxRackNames();
        // Flow: in from the synth, out through width and mono bass.
        g.setColour (Colours::textFaint);
        g.setFont (mono (9.5f).boldened().withExtraKerningFactor (0.1f));
        g.drawText ("SYNTH", juce::Rectangle<float> (0, row.getY(), 54, row.getHeight()), juce::Justification::centred, false);
        g.drawText ("OUT", juce::Rectangle<float> (row.getRight() + 4, row.getY(), 46, row.getHeight()), juce::Justification::centred, false);
        g.setColour (Colours::line);
        g.drawHorizontalLine ((int) row.getCentreY(), 50.0f, (float) getWidth() - 44.0f);

        for (int slot = 0; slot < NumFx; ++slot)
        {
            const int id = order[(size_t) slot];
            if (dragIndex >= 0 && id == dragId) continue;
            drawChip (g, id, chipRect (shownX[(size_t) id] >= 0 ? shownX[(size_t) id] : slotX (slot), w, row), names[id], false, slot);
        }
        if (dragIndex >= 0)
            drawChip (g, dragId, chipRect (dragX, w, row).translated (0, -4.0f), names[dragId], true, dragIndex);

        g.setColour (Colours::textDim);
        g.setFont (font (10.0f));
        g.drawText ("then WIDTH and MONO BASS (the output stage). dimmed effects are on but doing nothing.",
                    getLocalBounds().toFloat().removeFromBottom (18.0f), juce::Justification::centredLeft, true);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onMenu) onMenu (e.getScreenPosition()); return; }
        pressedSlot = slotAt (e.position.x);
        pressX = e.position.x;
        dragIndex = -1;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (pressedSlot < 0) return;
        if (dragIndex < 0 && std::abs (e.position.x - pressX) < 5.0f) return;
        if (dragIndex < 0)
        {
            dragIndex = pressedSlot;
            dragId = order[(size_t) pressedSlot];
            grabOffset = pressX - slotX (pressedSlot);
        }
        dragX = juce::jlimit (chipRow().getX(), chipRow().getRight() - slotWidth(), e.position.x - grabOffset);
        // Where it would go: the others shuffle to make room.
        const int target = juce::jlimit (0, NumFx - 1, (int) std::round ((dragX - chipRow().getX()) / slotWidth()));
        if (target != dragIndex)
        {
            auto o = order;
            const auto id = o[(size_t) dragIndex];
            std::vector<juce::uint8> v (o.begin(), o.end());
            v.erase (v.begin() + dragIndex);
            v.insert (v.begin() + target, id);
            std::copy (v.begin(), v.end(), order.begin());
            dragIndex = target;
        }
        startTimerHz (60);
        repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragIndex >= 0)
        {
            proc.setFxOrder (order);
            shownX[(size_t) dragId] = dragX;
            dragIndex = -1;
            startTimerHz (60);
            repaint();
            return;
        }
        if (pressedSlot < 0) return;
        const int id = order[(size_t) pressedSlot];
        const auto chip = chipRect (slotX (pressedSlot), slotWidth(), chipRow());
        if (lightRect (chip).expanded (6.0f).contains (e.position))
        {
            if (auto* p = proc.apvts.getParameter (onParam (id)))
                p->setValueNotifyingHost (p->getValue() > 0.5f ? 0.0f : 1.0f);
        }
        else if (onShowEffect) onShowEffect (id);
        pressedSlot = -1;
    }

private:
    HypernovaAudioProcessor& proc;
    FxOrder order;
    std::array<float, NumFx> shownX {};
    int lastChanges = -1, pressedSlot = -1, dragIndex = -1, dragId = 0;
    float pressX = 0, dragX = 0, grabOffset = 0;

    juce::Rectangle<float> chipRow() const { return getLocalBounds().toFloat().withTrimmedBottom (24.0f).reduced (54.0f, 4.0f).withTrimmedRight (0); }
    float slotWidth() const { return chipRow().getWidth() / (float) NumFx; }
    float slotX (int slot) const { return chipRow().getX() + slotWidth() * (float) slot; }
    int slotAt (float x) const
    {
        const int s = (int) std::floor ((x - chipRow().getX()) / slotWidth());
        return juce::isPositiveAndBelow (s, NumFx) ? s : -1;
    }
    static juce::Rectangle<float> chipRect (float x, float w, juce::Rectangle<float> row)
    {
        return juce::Rectangle<float> (x + 3.0f, row.getY(), w - 6.0f, row.getHeight());
    }
    static juce::Rectangle<float> lightRect (juce::Rectangle<float> chip)
    {
        return juce::Rectangle<float> (10, 10).withCentre ({ chip.getX() + 14.0f, chip.getY() + 14.0f });
    }

    void timerCallback() override
    {
        // Chips glide to their slots.
        bool moving = false;
        for (int slot = 0; slot < NumFx; ++slot)
        {
            const int id = order[(size_t) slot];
            auto& x = shownX[(size_t) id];
            const float target = slotX (slot);
            if (x < 0.0f) x = target;
            x += (target - x) * 0.3f;
            if (std::abs (target - x) > 0.3f) moving = true; else x = target;
        }
        repaint();
        if (! moving && dragIndex < 0) stopTimer();
    }

    void drawChip (juce::Graphics& g, int id, juce::Rectangle<float> r, const juce::String& name, bool lifted, int slot) const
    {
        const bool on = proc.apvts.getRawParameterValue (onParam (id))->load() > 0.5f;
        const bool live = audible (id);
        if (lifted)
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (juce::Colours::black.withAlpha (0.07f * (float) (4 - i)));
                g.fillRoundedRectangle (r.translated (0, 2.0f * (float) i).expanded ((float) i), 10.0f);
            }
        g.setColour (live ? Palette::fx.withAlpha (0.2f) : Colours::panelHi.withAlpha (on ? 0.9f : 0.5f));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (lifted ? Colours::accent.get() : live ? Palette::fx.withAlpha (0.8f) : Colours::line.get());
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, lifted ? 1.6f : 1.0f);
        const auto light = lightRect (r);
        g.setColour (on ? Palette::fx.get() : Colours::textFaint.withAlpha (0.5f));
        g.fillEllipse (light);
        g.setColour (Colours::textFaint);
        g.setFont (mono (9.0f));
        g.drawText (juce::String (slot + 1), r.withTrimmedLeft (r.getWidth() - 22.0f).withHeight (28.0f), juce::Justification::centred, false);
        g.setColour (live ? Colours::text.get() : on ? Colours::textDim.get() : Colours::textFaint.get());
        g.setFont (font (11.0f, true).withExtraKerningFactor (0.1f));
        g.drawFittedText (name, r.withTrimmedTop (r.getHeight() * 0.45f).toNearestInt().reduced (4, 0), juce::Justification::centred, 1, 0.6f);
    }
};

} // namespace ab::ui
