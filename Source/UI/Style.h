#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Visual language shared with Arrow Keys: deep navy glass, neon colours, chunky hardware-style knobs.
namespace ab::ui
{

namespace Colours
{
    const juce::Colour bg0       { 0xff06080d };
    const juce::Colour bg1       { 0xff0b0f17 };
    const juce::Colour panel     { 0xff111724 };
    const juce::Colour panelHi   { 0xff182032 };
    const juce::Colour inset     { 0xff080b12 };
    const juce::Colour line      { 0x14ffffff };
    const juce::Colour lineHi    { 0x26ffffff };
    const juce::Colour text      { 0xffe9eef8 };
    const juce::Colour textDim   { 0xff8190ab };
    const juce::Colour textFaint { 0xff4c5870 };
    const juce::Colour accent    { 0xff2ff3e0 };
    const juce::Colour accent2   { 0xff8b6cff };
    const juce::Colour warm      { 0xffffb547 };

    inline juce::Colour lane (int part)
    {
        static const juce::uint32 c[] = { 0xffffb547, 0xffff5c8a, 0xff8b6cff, 0xff36d6ff, 0xff4dffb0, 0xff9db4ff, 0xffffe066 };
        return juce::Colour (c[juce::jlimit (0, 6, part)]);
    }
}

inline juce::Font font (float height, bool bold = false)
{
    return juce::Font (juce::FontOptions().withName ("Avenir Next").withHeight (height).withStyle (bold ? "Demi Bold" : "Medium"));
}

inline juce::Font heavy (float height)
{
    return juce::Font (juce::FontOptions().withName ("Avenir Next").withHeight (height).withStyle ("Heavy"));
}

inline juce::Font mono (float height)
{
    return juce::Font (juce::FontOptions().withName ("Menlo").withHeight (height));
}

// Soft neon glow: a stroked path drawn wide and faint, then crisp.
inline void glowStroke (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width, float glow = 1.0f)
{
    g.setColour (c.withAlpha (0.07f * glow));
    g.strokePath (p, juce::PathStrokeType (width * 4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c.withAlpha (0.16f * glow));
    g.strokePath (p, juce::PathStrokeType (width * 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

inline void glowRect (juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour c, float strength = 1.0f)
{
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (c.withAlpha (0.05f * strength * (float) (4 - i)));
        g.fillRoundedRectangle (r.expanded ((float) i * 2.5f), radius + (float) i * 2.5f);
    }
}

// Glassy panel with a hairline border and a faint top sheen.
inline void panel (juce::Graphics& g, juce::Rectangle<float> r, float radius = 12.0f, juce::Colour base = Colours::panel)
{
    g.setGradientFill (juce::ColourGradient (base.brighter (0.06f), r.getX(), r.getY(), base.darker (0.25f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);
    g.setColour (Colours::line);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.fillRect (r.reduced (radius, 0).withHeight (1.0f).translated (0, 1.0f));
}

inline void sectionLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, juce::Colour c = Colours::textDim)
{
    g.setColour (c);
    g.setFont (font (10.5f, true).withExtraKerningFactor (0.22f));
    g.drawText (text, area, juce::Justification::centredLeft, false);
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, Colours::panelHi);
        setColour (juce::PopupMenu::textColourId, Colours::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Colours::accent.withAlpha (0.18f));
        setColour (juce::PopupMenu::highlightedTextColourId, Colours::text);
        setColour (juce::PopupMenu::headerTextColourId, Colours::textDim);
        setColour (juce::ComboBox::textColourId, Colours::text);
        setColour (juce::ComboBox::backgroundColourId, Colours::inset);
        setColour (juce::ComboBox::outlineColourId, Colours::line);
        setColour (juce::ComboBox::arrowColourId, Colours::textDim);
        setColour (juce::TooltipWindow::backgroundColourId, Colours::panelHi);
        setColour (juce::TooltipWindow::textColourId, Colours::text);
        setColour (juce::TooltipWindow::outlineColourId, Colours::lineHi);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (13.0f, true); }
    juce::Font getPopupMenuFont() override { return font (14.0f); }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
        g.setColour (Colours::inset);
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (box.isMouseOver (true) ? Colours::lineHi : Colours::line);
        g.drawRoundedRectangle (r, 7.0f, 1.0f);

        juce::Path chevron;
        const float cx = (float) width - 13.0f, cy = (float) height * 0.5f;
        chevron.startNewSubPath (cx - 3.5f, cy - 1.5f);
        chevron.lineTo (cx, cy + 2.0f);
        chevron.lineTo (cx + 3.5f, cy - 1.5f);
        g.setColour (Colours::textDim);
        g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 0, box.getWidth() - 26, box.getHeight());
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (juce::Justification::centredLeft);
    }

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        g.fillAll (Colours::panelHi);
        g.setColour (Colours::lineHi);
        g.drawRect (0, 0, width, height, 1);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        const auto tint = b.findColour (juce::TextButton::buttonOnColourId);
        g.setColour (on ? tint.withAlpha (0.16f) : (over ? Colours::panelHi.brighter (0.05f) : Colours::panelHi));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (on ? tint.withAlpha (0.8f) : (over ? Colours::lineHi : Colours::line));
        g.drawRoundedRectangle (r, 8.0f, on ? 1.4f : 1.0f);
        if (down)
        {
            g.setColour (juce::Colours::white.withAlpha (0.04f));
            g.fillRoundedRectangle (r, 8.0f);
        }
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const auto c = s.findColour (juce::Slider::rotarySliderFillColourId);
        const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (5.0f);
        const float radius = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
        const float cx = r.getCentreX(), cy = r.getCentreY();

        juce::Path track;
        track.addCentredArc (cx, cy, radius, radius, 0.0f, a0, a1, true);
        g.setColour (Colours::inset.brighter (0.12f));
        g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Bipolar parameters fill from the centre.
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float zero = bipolar ? (float) s.valueToProportionOfLength (0.0) : 0.0f;
        if (std::abs (pos - zero) > 0.001f)
        {
            juce::Path value;
            const float from = a0 + zero * (a1 - a0), to = a0 + pos * (a1 - a0);
            value.addCentredArc (cx, cy, radius, radius, 0.0f, juce::jmin (from, to), juce::jmax (from, to), true);
            glowStroke (g, value, c, 3.0f, s.isMouseOverOrDragging() ? 1.6f : 1.0f);
        }

        const float capR = radius - juce::jmax (5.0f, radius * 0.3f);
        auto cap = juce::Rectangle<float> (capR * 2, capR * 2).withCentre ({ cx, cy });
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (cap.translated (0, 2.5f).expanded (1.5f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c3549), cx, cap.getY(), juce::Colour (0xff10151f), cx, cap.getBottom(), false));
        g.fillEllipse (cap);
        g.setColour (juce::Colours::white.withAlpha (0.09f));
        g.drawEllipse (cap.reduced (0.5f), 1.0f);
        auto inner = cap.reduced (capR * 0.22f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff222a3a), cx, inner.getBottom(), juce::Colour (0xff171d2a), cx, inner.getY(), false));
        g.fillEllipse (inner);

        const float angle = a0 + pos * (a1 - a0);
        juce::Path pointer;
        pointer.startNewSubPath (juce::Point<float> (cx, cy).getPointOnCircumference (capR * 0.35f, angle));
        pointer.lineTo (juce::Point<float> (cx, cy).getPointOnCircumference (capR * 0.82f, angle));
        glowStroke (g, pointer, c.brighter (0.4f), 2.2f, 0.8f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override { return font (12.0f, true).withExtraKerningFactor (0.12f); }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const bool on = b.getToggleState();
        g.setColour (on ? b.findColour (juce::TextButton::buttonOnColourId).brighter (0.3f) : Colours::text.withAlpha (b.isEnabled() ? 0.9f : 0.35f));
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height);
        g.setColour (Colours::panelHi);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Colours::lineHi);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        g.setColour (Colours::text);
        g.setFont (font (13.0f));
        g.drawFittedText (text, r.reduced (8, 4).toNearestInt(), juce::Justification::centredLeft, 3);
    }
};

} // namespace ab::ui
