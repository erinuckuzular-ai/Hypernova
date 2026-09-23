#pragma once

#include "Components.h"
#include "../Orbit.h"

// The Orbit pad: four captured sounds at the corners, and a point you drag between them. The point follows
// the engine, so whatever moves it — your hand, automation, a modulation or a path of its own — is what you see.
namespace ab::ui
{

class OrbitPad : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit OrbitPad (HypernovaAudioProcessor& p) : proc (p)
    {
        setTooltip ("Drag the point to morph between the captured sounds. Click a corner to capture the sound as it is,"
                    " or right-click it for more.");
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    std::function<void (int corner, juce::Point<int> screenPos)> onCornerMenu;
    std::function<void (int corner)> onCapture;
    std::function<void()> onChanged;

    // Where a corner sits on the pad.
    juce::Rectangle<float> cornerBox (int corner) const
    {
        const auto r = pad();
        const float w = juce::jmin (108.0f, r.getWidth() * 0.44f), h = 34.0f;
        const float x = (corner % 2 == 0) ? r.getX() + 4.0f : r.getRight() - w - 4.0f;
        const float y = (corner < 2) ? r.getY() + 4.0f : r.getBottom() - h - 4.0f;
        return { x, y, w, h };
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = pad();
        g.setColour (Colours::inset);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (Colours::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

        // A faint grid, so the middle is easy to find.
        g.setColour (Colours::line.withAlpha (0.5f));
        g.drawLine (r.getCentreX(), r.getY() + 8.0f, r.getCentreX(), r.getBottom() - 8.0f, 1.0f);
        g.drawLine (r.getX() + 8.0f, r.getCentreY(), r.getRight() - 8.0f, r.getCentreY(), 1.0f);

        const auto w = proc.orbitWeights();
        const bool live = proc.orbitLive();
        for (int c = 0; c < ab::Orbit::NumCorners; ++c)
        {
            const auto box = cornerBox (c);
            const bool filled = proc.cornerFilled (c);
            const auto colour = cornerColour (c).get();
            const float share = live ? juce::jlimit (0.0f, 1.0f, w[(size_t) c]) : 0.0f;
            // How much of this corner you are hearing, as a wash behind its name.
            g.setColour (colour.withAlpha (filled ? 0.10f + 0.35f * share : 0.05f));
            g.fillRoundedRectangle (box, 8.0f);
            g.setColour (filled ? colour.withAlpha (0.55f + 0.4f * share) : Colours::line.get());
            g.drawRoundedRectangle (box.reduced (0.5f), 8.0f, filled ? 1.2f : 1.0f);

            auto text = box.reduced (8.0f, 4.0f);
            g.setColour (filled ? Colours::text.get() : Colours::textFaint.get());
            g.setFont (mono (9.0f).boldened().withExtraKerningFactor (0.14f));
            g.drawText (ab::Orbit::cornerNames()[c], text.removeFromTop (11.0f), juce::Justification::centredLeft, false);
            g.setFont (mono (9.0f));
            g.setColour (filled ? Colours::textDim.get() : Colours::textFaint.get());
            g.drawText (filled ? proc.cornerName (c) : juce::String ("click to capture"), text, juce::Justification::centredLeft, true);
        }

        // Leads from the point to each corner it is drawing on, so it is clear what you are hearing.
        const auto p = pointOnPad();
        if (live)
            for (int c = 0; c < ab::Orbit::NumCorners; ++c)
            {
                if (w[(size_t) c] < 0.01f) continue;
                g.setColour (cornerColour (c).withAlpha (0.12f + 0.5f * w[(size_t) c]));
                g.drawLine ({ p, cornerBox (c).getCentre() }, 0.8f + 2.2f * w[(size_t) c]);
            }

        // The point itself.
        const auto glow = live ? Colours::accent.get() : Colours::textFaint.get();
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (glow.withAlpha (0.06f * (float) i));
            g.fillEllipse (juce::Rectangle<float> (18.0f + 6.0f * (float) i, 18.0f + 6.0f * (float) i).withCentre (p));
        }
        g.setColour (glow);
        g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (p));
        g.setColour (Colours::bg0);
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (p));

        if (! live)
        {
            g.setColour (Colours::textFaint);
            g.setFont (mono (9.5f));
            g.drawText (proc.cornersFilled() == 0 ? "capture a sound into a corner to start"
                                                  : "switch ORBIT on to hear the blend",
                        r.withTrimmedTop (r.getHeight() * 0.5f + 16.0f).withHeight (16.0f), juce::Justification::centred, false);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int c = 0; c < ab::Orbit::NumCorners; ++c)
            if (cornerBox (c).contains (e.position))
            {
                if (e.mods.isPopupMenu()) { if (onCornerMenu) onCornerMenu (c, e.getScreenPosition()); return; }
                if (! proc.cornerFilled (c)) { if (onCapture) onCapture (c); return; }
                // A filled corner: take the point there, so you hear that sound on its own.
                setPoint ({ c % 2 == 0 ? 0.0f : 1.0f, c < 2 ? 0.0f : 1.0f }, true);
                return;
            }
        if (e.mods.isPopupMenu()) return;
        dragging = true;
        proc.undoManager.beginNewTransaction ("Morph");
        setPoint (fromPad (e.position), false);
    }

    void mouseDrag (const juce::MouseEvent& e) override { if (dragging) setPoint (fromPad (e.position), false); }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }

    void refresh()
    {
        const auto now = proc.orbitPoint();
        if (std::abs (now.x - lastDrawn.x) > 0.002f || std::abs (now.y - lastDrawn.y) > 0.002f || proc.orbitLive() != wasLive)
        {
            lastDrawn = now;
            wasLive = proc.orbitLive();
            repaint();
        }
    }

    static ThemeColour cornerColour (int corner)
    {
        static const int slots[] = { SlotOscA, SlotOscB, SlotSub, SlotLfo };
        return ThemeColour { slots[juce::jlimit (0, 3, corner)] };
    }

private:
    HypernovaAudioProcessor& proc;
    bool dragging = false, wasLive = false;
    juce::Point<float> lastDrawn;

    // The square stays square whatever shape the panel is, so the corners keep their meaning.
    juce::Rectangle<float> pad() const
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        const float side = juce::jmin (r.getWidth(), r.getHeight());
        return juce::Rectangle<float> (side, side).withCentre (r.getCentre());
    }

    // The point in the square, in the pad's own pixels: kept inside the corner boxes so it never hides one.
    juce::Point<float> pointOnPad() const
    {
        const auto r = pad().reduced (16.0f);
        const auto p = proc.orbitLive() ? proc.orbitPoint()
                                        : juce::Point<float> (value ("orbitX"), value ("orbitY"));
        return { r.getX() + r.getWidth() * juce::jlimit (0.0f, 1.0f, p.x), r.getY() + r.getHeight() * juce::jlimit (0.0f, 1.0f, p.y) };
    }

    juce::Point<float> fromPad (juce::Point<float> where) const
    {
        const auto r = pad().reduced (16.0f);
        return { juce::jlimit (0.0f, 1.0f, (where.x - r.getX()) / juce::jmax (1.0f, r.getWidth())),
                 juce::jlimit (0.0f, 1.0f, (where.y - r.getY()) / juce::jmax (1.0f, r.getHeight())) };
    }

    float value (const char* id) const
    {
        auto* v = proc.apvts.getRawParameterValue (id);
        return v != nullptr ? v->load() : 0.0f;
    }

    void setPoint (juce::Point<float> p, bool asStep)
    {
        if (asStep) proc.undoManager.beginNewTransaction ("Morph to a corner");
        proc.setParam ("orbitX", p.x);
        proc.setParam ("orbitY", p.y);
        if (onChanged) onChanged();
        repaint();
    }
};

} // namespace ab::ui
