#pragma once

#include "BinaryData.h"

#include <juce_gui_basics/juce_gui_basics.h>

// Visual language: cosmic cinematic. Deep space behind smoked-glass panels, ion blue / nebula violet /
// accretion gold / plasma magenta light, planet-like knobs. The nebula, stars and black hole behind the
// panels are drawn on the GPU (UI/Cosmos.h); panels are translucent so the space shows through.
namespace ab::ui
{

//==============================================================================
// Themes. Every colour in the UI is read through the current theme, so switching theme repaints the whole
// plug-in in place. Colours::text etc. are small proxies that convert to juce::Colour on use.
enum ThemeSlot
{
    SlotBg0, SlotBg1, SlotPanel, SlotPanelHi, SlotInset, SlotLine, SlotLineHi, SlotText, SlotTextDim, SlotTextFaint,
    SlotAccent, SlotAccent2, SlotWarm, SlotPlasma,
    SlotOscA, SlotOscB, SlotSub, SlotFilter, SlotEnv, SlotLfo, SlotMod, SlotFx,
    NumThemeSlots
};

enum BackdropStyle { BackdropCosmos, BackdropHorizon, BackdropFlat };
enum KnobStyle { KnobPlanet, KnobRing, KnobMinimal };
enum PanelStyle { PanelGlass, PanelFlat, PanelOutline };

struct Theme
{
    juce::String name;
    std::array<juce::Colour, NumThemeSlots> c;
    int backdrop = BackdropCosmos;
    float backdropStrength = 0.7f; // how bright the backdrop is behind the panels
    int knobStyle = KnobPlanet, panelStyle = PanelGlass;
    bool light = false;
};

inline std::vector<Theme> builtInThemes()
{
    auto make = [] (const char* name, std::initializer_list<juce::uint32> cols, int backdrop, float strength, int knobs, int panels, bool light)
    {
        Theme t;
        t.name = name;
        int i = 0;
        for (auto v : cols) t.c[(size_t) i++] = juce::Colour (v);
        t.backdrop = backdrop; t.backdropStrength = strength; t.knobStyle = knobs; t.panelStyle = panels; t.light = light;
        return t;
    };
    return {
        // Paper: the house look. Warm grey, white panels, black ink and one orange, like the website.
        make ("Paper", { 0xfff2f1ee, 0xffe9e8e4, 0xffffffff, 0xffffffff, 0xffeeede9, 0x1f000000, 0x40000000,
                         0xff111111, 0xff4a4a4a, 0xff8b8b88, 0xffff5a1f, 0xff2f5bff, 0xffff5a1f, 0xffe8363d,
                         0xffff5a1f, 0xff2f5bff, 0xffe0a100, 0xffe8363d, 0xff16a37b, 0xff2f5bff, 0xff7a5cff, 0xff111111 },
              BackdropFlat, 0.0f, KnobRing, PanelFlat, true),
        // Cosmic: deep space, but with panels that read as surfaces and a backdrop that sits back.
        make ("Cosmic", { 0xff03040a, 0xff070914, 0xc40d1022, 0xff181d35, 0xc206081a, 0x24ffffff, 0x45ffffff,
                          0xfff4f5ff, 0xffa3a8d0, 0xff676d96, 0xff46e8ff, 0xff8a5cff, 0xffffa94a, 0xffff4f9a,
                          0xff46e8ff, 0xffb07cff, 0xffffa94a, 0xffff4f9a, 0xff7dffcf, 0xff5aa9ff, 0xffc9a8ff, 0xffffd36b },
              BackdropCosmos, 0.6f, KnobPlanet, PanelGlass, false),
        // Blackout: near-black, one accent, no nebula.
        make ("Blackout", { 0xff050505, 0xff0b0b0c, 0xff111113, 0xff1b1b1f, 0xff0a0a0b, 0x1fffffff, 0x3dffffff,
                            0xffededed, 0xff9a9a9e, 0xff5e5e63, 0xfff2f2f2, 0xff8f8f95, 0xfff2f2f2, 0xffff5a5a,
                            0xfff2f2f2, 0xffcfcfd4, 0xffe0e0e0, 0xffff5a5a, 0xffd8d8d8, 0xffbdbdc2, 0xffa8a8ae, 0xffe8e8e8 },
                BackdropFlat, 0.0f, KnobMinimal, PanelFlat, false),
        // Daylight: light panels and dark text for bright rooms.
        make ("Daylight", { 0xffdfe2ea, 0xffe9ebf1, 0xfff6f7fa, 0xffffffff, 0xffe6e9f0, 0x22000000, 0x40000000,
                            0xff15171f, 0xff4a4f63, 0xff8a8fa3, 0xff0a84d8, 0xff6b4be0, 0xffd9770a, 0xffd6246e,
                            0xff0a84d8, 0xff6b4be0, 0xffd9770a, 0xffd6246e, 0xff0f9f78, 0xff2f6fd6, 0xff8a5cd0, 0xffc79200 },
                BackdropFlat, 0.0f, KnobRing, PanelOutline, true),
        // Vapor: magenta and cyan on deep purple over a neon grid horizon.
        make ("Vapor", { 0xff12051f, 0xff1d0930, 0xc8230c3a, 0xff2e1149, 0xc2140620, 0x33ff7ad9, 0x66ff7ad9,
                         0xfffff0ff, 0xffd6a6e8, 0xff9a6cb0, 0xff2ef2ff, 0xffff3fb8, 0xffffc857, 0xffff3fb8,
                         0xff2ef2ff, 0xffff3fb8, 0xffffc857, 0xffff5c8a, 0xff7affd8, 0xff9f7bff, 0xffff9ae8, 0xffffe27a },
             BackdropHorizon, 0.75f, KnobRing, PanelGlass, false),
    };
}

// The live theme plus the user's own tweaks on top of it (accent override, backdrop, knob and panel style).
struct ThemeState
{
    Theme base;
    juce::Colour accentOverride; // transparent = use the theme's own
    int backdrop = -1, knobStyle = -1, panelStyle = -1;
    float backdropStrength = -1.0f;
    bool alwaysShowValues = false; // otherwise knob values appear on hover, which keeps panels calm
    int version = 0; // bumped on every change so views can drop cached images

    static ThemeState& get()
    {
        static ThemeState t { builtInThemes().front() };
        return t;
    }

    juce::Colour colour (int slot) const
    {
        if (! accentOverride.isTransparent() && (slot == SlotAccent || slot == SlotOscA)) return accentOverride;
        return base.c[(size_t) slot];
    }
    int backdropStyle() const { return backdrop >= 0 ? backdrop : base.backdrop; }
    float strength() const { return backdropStrength >= 0.0f ? backdropStrength : base.backdropStrength; }
    int knobs() const { return knobStyle >= 0 ? knobStyle : base.knobStyle; }
    int panels() const { return panelStyle >= 0 ? panelStyle : base.panelStyle; }
};

// The look is a per-user preference (not per session), kept next to the presets.
struct LookSettings
{
    static juce::PropertiesFile::Options options()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "look";
        o.filenameSuffix = ".xml";
        o.folderName = "Arrow/Hypernova";
        o.osxLibrarySubFolder = "Application Support";
        o.storageFormat = juce::PropertiesFile::storeAsXML;
        return o;
    }

    static void load()
    {
        juce::PropertiesFile p (options());
        auto& t = ThemeState::get();
        auto name = p.getValue ("theme", "Cosmic");
        const auto forced = juce::SystemStats::getEnvironmentVariable ("HYPERNOVA_THEME", {}); // for snapshots
        if (forced.isNotEmpty()) name = forced;
        for (const auto& th : builtInThemes()) if (th.name == name) t.base = th;
        const auto accent = p.getValue ("accent");
        t.accentOverride = accent.isNotEmpty() ? juce::Colour::fromString (accent) : juce::Colour();
        t.backdrop = p.getIntValue ("backdrop", -1);
        t.backdropStrength = (float) p.getDoubleValue ("strength", -1.0);
        t.knobStyle = p.getIntValue ("knobs", -1);
        t.panelStyle = p.getIntValue ("panels", -1);
        t.alwaysShowValues = p.getBoolValue ("values", false);
        ++t.version;
    }

    static void save()
    {
        juce::PropertiesFile p (options());
        const auto& t = ThemeState::get();
        p.setValue ("theme", t.base.name);
        p.setValue ("accent", t.accentOverride.isTransparent() ? juce::String() : t.accentOverride.toString());
        p.setValue ("backdrop", t.backdrop);
        p.setValue ("strength", (double) t.backdropStrength);
        p.setValue ("knobs", t.knobStyle);
        p.setValue ("panels", t.panelStyle);
        p.setValue ("values", t.alwaysShowValues);
        p.saveIfNeeded();
    }

    static bool flag (const char* key, bool def) { juce::PropertiesFile p (options()); return p.getBoolValue (key, def); }
    static void setFlag (const char* key, bool v) { juce::PropertiesFile p (options()); p.setValue (key, v); p.saveIfNeeded(); }
};

struct ThemeColour
{
    int slot;
    juce::Colour get() const { return ThemeState::get().colour (slot); }
    operator juce::Colour() const { return get(); }
    juce::Colour withAlpha (float a) const { return get().withAlpha (a); }
    juce::Colour withMultipliedAlpha (float a) const { return get().withMultipliedAlpha (a); }
    juce::Colour brighter (float a = 0.4f) const { return get().brighter (a); }
    juce::Colour darker (float a = 0.4f) const { return get().darker (a); }
    juce::Colour interpolatedWith (juce::Colour o, float p) const { return get().interpolatedWith (o, p); }
    juce::Colour contrasting (float a = 1.0f) const { return get().contrasting (a); }
};

namespace Colours
{
    inline const ThemeColour bg0 { SlotBg0 }, bg1 { SlotBg1 }, panel { SlotPanel }, panelHi { SlotPanelHi }, inset { SlotInset },
                             line { SlotLine }, lineHi { SlotLineHi }, text { SlotText }, textDim { SlotTextDim },
                             textFaint { SlotTextFaint }, accent { SlotAccent }, accent2 { SlotAccent2 }, warm { SlotWarm },
                             plasma { SlotPlasma };

inline juce::Colour lane (int part)
    {
        static const juce::uint32 c[] = { 0xffffa94a, 0xffff4f9a, 0xff8a5cff, 0xff46e8ff, 0xff7dffcf, 0xffc9a8ff, 0xffffd36b };
        return juce::Colour (c[juce::jlimit (0, 6, part)]);
    }
}

//==============================================================================
// Bundled typefaces (all SIL Open Font License). HYPERNOVA_FONTSET picks a set while we compare them:
//   0 Michroma + Space Grotesk + Space Mono   (wide sci-fi)
//   1 Chakra Petch + Share Tech Mono          (angular techno)
//   2 Orbitron + Bai Jamjuree + Space Mono    (rounded retro-futuristic)
struct Fonts
{
    juce::Typeface::Ptr display, body, bodyBold, code;

    static Fonts& get()
    {
        static Fonts f;
        return f;
    }

    static int set()
    {
        static const int chosen = juce::jlimit (0, 2, juce::SystemStats::getEnvironmentVariable ("HYPERNOVA_FONTSET", "0").getIntValue());
        return chosen;
    }

private:
    Fonts()
    {
        auto load = [] (const char* name) -> juce::Typeface::Ptr
        {
            int size = 0;
            if (const auto* data = BinaryData::getNamedResource (name, size))
                return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
            return {};
        };
        switch (set())
        {
            case 1:
                display = load ("ChakraPetchSemiBold_ttf");
                body = load ("ChakraPetchRegular_ttf");
                bodyBold = load ("ChakraPetchSemiBold_ttf");
                code = load ("ShareTechMonoRegular_ttf");
                break;
            case 2:
                display = load ("Orbitronvar_ttf");
                body = load ("BaiJamjureeRegular_ttf");
                bodyBold = load ("BaiJamjureeSemiBold_ttf");
                code = load ("SpaceMonoRegular_ttf");
                break;
            default:
                display = load ("SpaceGroteskvar_ttf");
                body = load ("SpaceGroteskvar_ttf");
                bodyBold = load ("SpaceGroteskvar_ttf");
                code = load ("SpaceMonoRegular_ttf");
                break;
        }
    }
};

inline juce::Font font (float height, bool bold = false)
{
    const auto& f = Fonts::get();
    const auto face = bold ? f.bodyBold : f.body;
    if (face == nullptr)
        return juce::Font (juce::FontOptions().withName ("Avenir Next").withHeight (height).withStyle (bold ? "Demi Bold" : "Medium"));
    auto out = juce::Font (juce::FontOptions (face).withHeight (height));
    // The variable-weight faces need the bold nudge applied by hand.
    return bold ? out.boldened() : out;
}

inline juce::Font heavy (float height)
{
    const auto& f = Fonts::get();
    if (f.display == nullptr)
        return juce::Font (juce::FontOptions().withName ("Avenir Next").withHeight (height).withStyle ("Heavy"));
    return juce::Font (juce::FontOptions (f.display).withHeight (height)).boldened();
}

// Wide display face, for the logo and section headings.
inline juce::Font display (float height)
{
    return heavy (height);
}

inline juce::Font mono (float height)
{
    const auto& f = Fonts::get();
    if (f.code == nullptr)
        return juce::Font (juce::FontOptions().withName ("Menlo").withHeight (height));
    return juce::Font (juce::FontOptions (f.code).withHeight (height));
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

// Smoked-glass panel: translucent body, a rim of starlight along the top-left edge fading out, faint sheen.
inline void panel (juce::Graphics& g, juce::Rectangle<float> r, float radius = 12.0f, juce::Colour base = Colours::panel)
{
    const int style = ThemeState::get().panels();
    if (style == PanelFlat)
    {
        g.setColour (base.withAlpha (1.0f));
        g.fillRoundedRectangle (r, radius * 0.6f);
        g.setColour (Colours::line);
        g.drawRoundedRectangle (r.reduced (0.5f), radius * 0.6f, 1.0f);
        return;
    }
    if (style == PanelOutline)
    {
        g.setColour (base.withAlpha (juce::jmin (1.0f, base.getFloatAlpha() + 0.2f)));
        g.fillRoundedRectangle (r, radius);
        g.setColour (Colours::lineHi);
        g.drawRoundedRectangle (r.reduced (0.75f), radius, 1.5f);
        return;
    }
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (r.translated (0, 3.0f), radius);
    g.setGradientFill (juce::ColourGradient (base.brighter (0.08f), r.getX(), r.getY(), base.darker (0.3f), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, radius);
    juce::ColourGradient rim (Colours::accent2.withAlpha (0.45f), r.getX(), r.getY(),
                              Colours::accent.withAlpha (0.02f), r.getX() + r.getWidth() * 0.7f, r.getBottom(), false);
    rim.addColour (0.35, Colours::accent.withAlpha (0.14f));
    g.setGradientFill (rim);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), r.getX() + radius, r.getY(),
                                             juce::Colours::white.withAlpha (0.0f), r.getCentreX(), r.getY(), false));
    g.fillRect (r.reduced (radius, 0).withHeight (1.0f).translated (0, 1.0f));
}

inline void sectionLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, juce::Colour c = Colours::textDim)
{
    g.setColour (c);
    g.setFont (mono (11.0f).boldened().withExtraKerningFactor (0.06f));
    g.drawFittedText (text, area.toNearestInt(), juce::Justification::centredLeft, 1, 0.75f);
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel() { refreshColours(); }

    // Re-reads the theme into JUCE's colour ids (menus, combo boxes, tooltips, text editors).
    void refreshColours()
    {
        setColour (juce::TextEditor::backgroundColourId, Colours::inset);
        setColour (juce::TextEditor::textColourId, Colours::text);
        setColour (juce::TextEditor::outlineColourId, Colours::line);
        setColour (juce::TextEditor::focusedOutlineColourId, Colours::accent.withAlpha (0.6f));
        setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
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
        const auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height);
        g.setGradientFill (juce::ColourGradient (Colours::panelHi.brighter (0.04f), 0, 0, Colours::panelHi.darker (0.15f), 0, (float) height, false));
        g.fillRect (r);
        g.setColour (Colours::lineHi);
        g.drawRect (r, 1.0f);
    }

    int getPopupMenuBorderSize() override { return 6; }

    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardHeight, int& w, int& h) override
    {
        if (isSeparator) { w = 50; h = 9; return; }
        juce::ignoreUnused (standardHeight);
        h = 28;
        w = juce::GlyphArrangement::getStringWidthInt (getPopupMenuFont(), text) + 64;
    }

    void drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name) override
    {
        g.setColour (Colours::textDim);
        g.setFont (font (10.5f, true).withExtraKerningFactor (0.2f));
        g.drawText (name.toUpperCase(), area.withTrimmedLeft (14).withTrimmedTop (4), juce::Justification::centredLeft, true);
    }

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable*, const juce::Colour*) override
    {
        if (isSeparator)
        {
            g.setColour (Colours::line);
            g.fillRect (area.reduced (10, 0).withSizeKeepingCentre (area.getWidth() - 20, 1));
            return;
        }
        auto r = area.toFloat().reduced (4.0f, 1.5f);
        if (isHighlighted && isActive)
        {
            g.setColour (Colours::accent.withAlpha (0.16f));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (Colours::accent.withAlpha (0.45f));
            g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        }
        if (isTicked)
        {
            g.setColour (Colours::accent);
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getX() + 11.0f, r.getCentreY() }));
        }
        auto textArea = r.withTrimmedLeft (22.0f).withTrimmedRight (hasSubMenu ? 22.0f : 10.0f);
        if (hasSubMenu)
        {
            juce::Path chevron;
            const float cx = r.getRight() - 12.0f, cy = r.getCentreY();
            chevron.startNewSubPath (cx - 2.0f, cy - 4.0f);
            chevron.lineTo (cx + 2.0f, cy);
            chevron.lineTo (cx - 2.0f, cy + 4.0f);
            g.setColour (isHighlighted ? Colours::text : Colours::textDim);
            g.strokePath (chevron, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setFont (font (14.0f, isTicked));
        g.setColour (! isActive ? Colours::textFaint : (isHighlighted || isTicked) ? Colours::text : Colours::text.withAlpha (0.86f));
        if (shortcutKeyText.isNotEmpty())
        {
            g.setColour (Colours::textFaint);
            g.drawText (shortcutKeyText, textArea, juce::Justification::centredRight, true);
        }
        g.setColour (! isActive ? Colours::textFaint : (isHighlighted || isTicked) ? Colours::text : Colours::text.withAlpha (0.86f));
        g.drawFittedText (text, textArea.toNearestInt(), juce::Justification::centredLeft, 1, 0.9f);
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
        g.strokePath (track, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Bipolar parameters fill from the centre.
        const bool bipolar = s.getMinimum() < 0.0 && s.getMaximum() > 0.0;
        const float zero = bipolar ? (float) s.valueToProportionOfLength (0.0) : 0.0f;
        if (std::abs (pos - zero) > 0.001f)
        {
            juce::Path value;
            const float from = a0 + zero * (a1 - a0), to = a0 + pos * (a1 - a0);
            value.addCentredArc (cx, cy, radius, radius, 0.0f, juce::jmin (from, to), juce::jmax (from, to), true);
            glowStroke (g, value, c, 2.4f, s.isMouseOverOrDragging() ? 1.6f : 1.0f);
        }

        const int knobStyle = ThemeState::get().knobs();
        const float angle = a0 + pos * (a1 - a0);
        if (knobStyle == KnobRing)
        {
            // Ring: a flat disc with a bold pointer line, the arc does the talking.
            const float capR = radius - juce::jmax (6.0f, radius * 0.34f);
            auto cap = juce::Rectangle<float> (capR * 2, capR * 2).withCentre ({ cx, cy });
            g.setColour (Colours::panelHi);
            g.fillEllipse (cap);
            g.setColour (Colours::lineHi);
            g.drawEllipse (cap, 1.0f);
            const auto tip = juce::Point<float> (cx, cy).getPointOnCircumference (capR * 0.92f, angle);
            const auto root = juce::Point<float> (cx, cy).getPointOnCircumference (capR * 0.25f, angle);
            g.setColour (c);
            g.drawLine (juce::Line<float> (root, tip), 2.2f);
            return;
        }
        if (knobStyle == KnobMinimal)
        {
            // Minimal: just the arc and a dot.
            const auto dot = juce::Point<float> (cx, cy).getPointOnCircumference (radius - 6.0f, angle);
            g.setColour (Colours::text);
            g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (dot));
            return;
        }

        // A small planet: dark sphere lit from the top-left, rim light in the knob's colour, a moon for a pointer.
        const float capR = radius - juce::jmax (5.0f, radius * 0.3f);
        auto cap = juce::Rectangle<float> (capR * 2, capR * 2).withCentre ({ cx, cy });
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (cap.translated (0, 2.0f).expanded (1.0f));
        juce::ColourGradient sphere (juce::Colour (0xff2a2f4d), cap.getX() + capR * 0.55f, cap.getY() + capR * 0.45f,
                                     juce::Colour (0xff07080f), cap.getRight(), cap.getBottom(), true);
        g.setGradientFill (sphere);
        g.fillEllipse (cap);
        g.setGradientFill (juce::ColourGradient (c.withAlpha (0.0f), cx, cy, c.withAlpha (s.isMouseOverOrDragging() ? 0.55f : 0.32f),
                                                 cx + capR, cy, true));
        g.drawEllipse (cap.reduced (0.6f), 1.2f);

        const auto moon = juce::Point<float> (cx, cy).getPointOnCircumference (capR * 0.62f, angle);
        g.setColour (c.withAlpha (0.25f));
        g.fillEllipse (juce::Rectangle<float> (capR * 0.62f, capR * 0.62f).withCentre (moon));
        g.setColour (c.brighter (0.5f));
        g.fillEllipse (juce::Rectangle<float> (capR * 0.3f, capR * 0.3f).withCentre (moon));
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
