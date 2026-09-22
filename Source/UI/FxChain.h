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

    bool isScrollable() const { return scrollable(); }
    float scrollPosition() const { return scroll; }
    float chipWidth() const { return slotWidth(); }

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

        {
            juce::Graphics::ScopedSaveState clip (g);
            g.reduceClipRegion (row.expanded (2.0f, 10.0f).toNearestInt());
            for (int slot = 0; slot < NumFx; ++slot)
            {
                const int id = order[(size_t) slot];
                if (dragIndex >= 0 && id == dragId) continue;
                drawChip (g, id, chipRect (shownX[(size_t) id] >= 0 ? shownX[(size_t) id] : slotX (slot), w, row), names[id], false, slot);
            }
        }
        if (scrollable())
        {
            // Soft edges where chips run off, and a thin bar showing where you are.
            auto fade = [&] (juce::Rectangle<float> r, bool left)
            {
                juce::ColourGradient grad (Colours::panel.withAlpha (left ? 1.0f : 0.0f), r.getX(), 0, Colours::panel.withAlpha (left ? 0.0f : 1.0f), r.getRight(), 0, false);
                g.setGradientFill (grad);
                g.fillRect (r);
            };
            if (scroll > 0.5f) fade (row.withWidth (28.0f), true);
            if (scroll < maxScroll() - 0.5f) fade (row.withLeft (row.getRight() - 28.0f), false);
            const float frac = row.getWidth() / contentWidth();
            auto track = juce::Rectangle<float> (row.getX(), row.getBottom() + 5.0f, row.getWidth(), 3.0f);
            g.setColour (Colours::line);
            g.fillRoundedRectangle (track, 1.5f);
            g.setColour (Colours::textDim);
            g.fillRoundedRectangle (track.withWidth (track.getWidth() * frac).withX (track.getX() + track.getWidth() * (1.0f - frac) * (scroll / maxScroll())), 1.5f);
        }
        if (dragIndex >= 0)
            drawChip (g, dragId, chipRect (dragX, w, row).translated (0, -4.0f), names[dragId], true, dragIndex);

        g.setColour (Colours::textDim);
        g.setFont (font (10.0f));
        g.drawText (scrollable() ? "scroll or swipe sideways for more. then WIDTH and MONO BASS (the output stage)."
                                 : "then WIDTH and MONO BASS (the output stage). dimmed effects are on but doing nothing.",
                    getLocalBounds().toFloat().removeFromBottom (18.0f), juce::Justification::centredLeft, true);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (! scrollable()) { juce::Component::mouseWheelMove (e, w); return; }
        // Trackpads scroll sideways directly; a mouse wheel's up/down moves the row too.
        const float delta = std::abs (w.deltaX) > std::abs (w.deltaY) ? w.deltaX : w.deltaY;
        setScroll (scroll - delta * (w.isReversed ? -1.0f : 1.0f) * 240.0f);
    }

    void resized() override { setScroll (scroll); }

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
        lastMouseX = e.position.x;
        dragX = juce::jlimit (chipRow().getX(), chipRow().getRight() - slotWidth(), e.position.x - grabOffset);
        updateDragTarget();
        startTimerHz (60);
        repaint();
    }

    // Where the dragged chip would go: the others shuffle to make room.
    void updateDragTarget()
    {
        const int target = juce::jlimit (0, NumFx - 1, (int) std::round ((dragX + scroll - chipRow().getX()) / slotWidth()));
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
        // A chip that's partly off the edge scrolls into view.
        if (slotX (pressedSlot) < chipRow().getX()) setScroll (scroll - (chipRow().getX() - slotX (pressedSlot)));
        else if (slotX (pressedSlot) + slotWidth() > chipRow().getRight()) setScroll (scroll + (slotX (pressedSlot) + slotWidth() - chipRow().getRight()));
        pressedSlot = -1;
    }

private:
    HypernovaAudioProcessor& proc;
    FxOrder order;
    std::array<float, NumFx> shownX {};
    int lastChanges = -1, pressedSlot = -1, dragIndex = -1, dragId = 0;
    float pressX = 0, dragX = 0, grabOffset = 0, scroll = 0, lastMouseX = 0;

    // The visible strip; when it's narrower than the chips need, the row scrolls sideways.
    static constexpr float minSlot = 96.0f;
    juce::Rectangle<float> chipRow() const { return getLocalBounds().toFloat().withTrimmedBottom (24.0f).reduced (54.0f, 4.0f); }
    float contentWidth() const { return juce::jmax (chipRow().getWidth(), minSlot * (float) NumFx); }
    float maxScroll() const { return contentWidth() - chipRow().getWidth(); }
    bool scrollable() const { return maxScroll() > 0.5f; }
    float slotWidth() const { return contentWidth() / (float) NumFx; }
    float slotX (int slot) const { return chipRow().getX() + slotWidth() * (float) slot - scroll; }
    int slotAt (float x) const
    {
        if (x < chipRow().getX() || x > chipRow().getRight()) return -1;
        const int s = (int) std::floor ((x + scroll - chipRow().getX()) / slotWidth());
        return juce::isPositiveAndBelow (s, NumFx) ? s : -1;
    }
    void setScroll (float v)
    {
        const float clamped = juce::jlimit (0.0f, juce::jmax (0.0f, maxScroll()), v);
        if (std::abs (clamped - scroll) < 0.01f) return;
        const float shift = scroll - clamped;
        scroll = clamped;
        for (auto& x : shownX) if (x >= 0.0f) x += shift; // chips move with the row, not glide
        repaint();
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
        // Dragging near either end of a scrolling row scrolls it.
        if (dragIndex >= 0 && scrollable())
        {
            const auto row = chipRow();
            const float edge = 36.0f;
            float step = 0.0f;
            if (lastMouseX < row.getX() + edge) step = -(row.getX() + edge - lastMouseX) * 0.35f;
            else if (lastMouseX > row.getRight() - edge) step = (lastMouseX - (row.getRight() - edge)) * 0.35f;
            if (step != 0.0f)
            {
                const float before = scroll;
                setScroll (scroll + step);
                // The dragged chip stays under the mouse while the row slides.
                if (std::abs (scroll - before) > 0.01f) updateDragTarget();
            }
        }
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
