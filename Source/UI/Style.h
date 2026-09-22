#pragma once

#include "BinaryData.h"
#include <unordered_map>
#include <tuple>

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
enum KnobStyle { KnobPlanet, KnobRing, KnobMinimal, KnobMachined };
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
              BackdropFlat, 0.0f, KnobMachined, PanelFlat, true),
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
        // Snapshots: HYPERNOVA_THEME picks a theme as it ships, without this machine's own tweaks.
        const auto forced = juce::SystemStats::getEnvironmentVariable ("HYPERNOVA_THEME", {});
        if (forced.isNotEmpty())
        {
            for (const auto& th : builtInThemes()) if (th.name.equalsIgnoreCase (forced)) t.base = th;
            t.accentOverride = {};
            t.backdrop = t.knobStyle = t.panelStyle = -1;
            t.backdropStrength = -1.0f;
            t.alwaysShowValues = false;
            ++t.version;
            return;
        }
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
        // A card lifted off the page: layered soft shadow, solid face, a lit top edge.
        const float rad = radius * 0.6f;
        for (int i = 4; i >= 1; --i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.018f * (float) (5 - i)));
            g.fillRoundedRectangle (r.translated (0, 1.5f * (float) i).expanded (0.5f * (float) i), rad + (float) i);
        }
        g.setColour (base.withAlpha (1.0f));
        g.fillRoundedRectangle (r, rad);
        g.setColour (Colours::line);
        g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);
        g.setColour (juce::Colours::white.withAlpha (ThemeState::get().base.light ? 0.9f : 0.06f));
        g.drawHorizontalLine ((int) r.getY() + 1, r.getX() + rad, r.getRight() - rad);
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
    // Layered, neutral shadow that stays within 3 px below the face (a single hard offset looked cut off).
    // The face covers the middle, so only the rim is painted.
    {
        juce::Graphics::ScopedSaveState keep (g);
        g.excludeClipRegion (r.reduced (radius + 1.0f, radius * 0.5f).toNearestInt());
        for (int i = 3; i >= 1; --i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.07f * (float) (4 - i)));
            g.fillRoundedRectangle (r.translated (0, (float) i).expanded (0.4f * (float) i, 0), radius + 0.5f * (float) i);
        }
    }
    // Face: lit from above, a touch darker towards the bottom. Drawn as fine solid bands inside the
    // panel's shape: JUCE's gradient fills cost several times more, and faces redraw whenever a panel
    // settles at a new size. The grain laid on top hides any stepping.
    juce::Path shape;
    shape.addRoundedRectangle (r, radius);
    {
        juce::Graphics::ScopedSaveState keep (g);
        g.reduceClipRegion (shape);
        const int bands = 32;
        const auto top = base.brighter (0.07f), bottom = base.darker (0.28f);
        // Edges on whole device pixels (the face is often translucent, so bands must meet without overlapping).
        const float k = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
        auto snap = [k] (float v) { return std::round (v * k) / k; };
        for (int i = 0; i < bands; ++i)
        {
            const float y0 = snap (r.getY() + r.getHeight() * (float) i / (float) bands);
            const float y1 = i == bands - 1 ? r.getBottom() + 1.0f : snap (r.getY() + r.getHeight() * (float) (i + 1) / (float) bands);
            g.setColour (top.interpolatedWith (bottom, ((float) i + 0.5f) / (float) bands));
            g.fillRect (juce::Rectangle<float> (r.getX(), y0, r.getWidth(), y1 - y0));
        }
    }
    // Edge: catches light along the top and falls into shadow below; the accent glints off the top-left.
    auto strokeIn = [&] (juce::Rectangle<float> clip, juce::Colour c)
    {
        juce::Graphics::ScopedSaveState keep (g);
        g.reduceClipRegion (clip.toNearestInt());
        g.setColour (c);
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
    };
    const float lit = juce::jmin (r.getHeight() * 0.3f, radius + 10.0f);
    strokeIn (r.withHeight (lit), juce::Colours::white.withAlpha (0.15f));
    strokeIn (r.withTrimmedTop (lit).withHeight (r.getHeight() * 0.25f), juce::Colours::white.withAlpha (0.04f));
    strokeIn (r.withTrimmedTop (lit + r.getHeight() * 0.25f), juce::Colours::black.withAlpha (0.3f));
    // The glint fades out in overlapping steps, so it has no hard end.
    for (auto [w, h, a] : { std::tuple<float, float, float> { 0.3f, 0.2f, 0.1f }, { 0.45f, 0.32f, 0.08f }, { 0.62f, 0.46f, 0.06f }, { 0.8f, 0.62f, 0.04f } })
        strokeIn (r.withSize (r.getWidth() * w, r.getHeight() * h), Colours::accent2.withAlpha (a));
    // A fine specular line just inside the top edge, brightest left of centre.
    {
        juce::ColourGradient spec (juce::Colours::white.withAlpha (0.0f), r.getX() + radius, 0, juce::Colours::white.withAlpha (0.0f), r.getRight() - radius, 0, false);
        spec.addColour (0.35, juce::Colours::white.withAlpha (0.13f));
        g.setGradientFill (spec);
        g.fillRect (r.reduced (radius, 0).withHeight (1.0f).translated (0, 1.0f));
    }
}

// The Hypernova mark: a lit core with a tilted orbit ring passing in front of it (the same idea as the
// orbit and 3D views). R is the core's radius; the ring reaches about 2R either side. One colour, so it
// works at favicon size. Mirrored in docs/favicon.svg and the site's .brand mark.
inline void drawNovaMark (juce::Graphics& g, juce::Point<float> c, float R, juce::Colour col, bool shaded = true)
{
    constexpr float pi = juce::MathConstants<float>::pi;
    const auto place = juce::AffineTransform::rotation (-0.36f).translated (c);
    auto half = [&] (bool front)
    {
        juce::Path p;
        const float rx = 2.1f * R, ry = 0.55f * R;
        for (int i = 0; i <= 48; ++i)
        {
            const float a = (front ? 0.0f : pi) + pi * (float) i / 48.0f;
            const juce::Point<float> pt (rx * std::cos (a), ry * std::sin (a));
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        p.applyTransform (place);
        return p;
    };
    const juce::PathStrokeType ring (0.2f * R, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    const auto back = half (false), front = half (true);
    g.setColour (col.withMultipliedAlpha (0.8f));
    g.strokePath (back, ring);
    {
        // The core, with a gap cut where the front of the ring crosses it.
        // The gap is the band between two ellipses around the ring's front half (a simple shape, unlike a
        // stroke outline, whose tight ends fold over themselves and confuse the even-odd cut).
        juce::Path gap, clip;
        const float w = 0.2f * R, rx = 2.1f * R, ry = 0.55f * R;
        for (int i = 0; i <= 48; ++i)
        {
            const float a = pi * (float) i / 48.0f;
            const juce::Point<float> pt ((rx + w) * std::cos (a), (ry + w) * std::sin (a));
            if (i == 0) gap.startNewSubPath (pt); else gap.lineTo (pt);
        }
        for (int i = 48; i >= 0; --i)
        {
            const float a = pi * (float) i / 48.0f;
            gap.lineTo ((rx - w) * std::cos (a), (ry - w) * std::sin (a));
        }
        gap.closeSubPath();
        gap.applyTransform (place);
        clip.addRectangle (juce::Rectangle<float> (6.0f * R, 6.0f * R).withCentre (c));
        clip.addPath (gap);
        clip.setUsingNonZeroWinding (false);
        juce::Graphics::ScopedSaveState keep (g);
        g.reduceClipRegion (clip);
        const auto core = juce::Rectangle<float> (2.0f * R, 2.0f * R).withCentre (c);
        if (shaded) g.setGradientFill (juce::ColourGradient (col.brighter (0.35f), c.x - R * 0.5f, c.y - R * 0.6f, col.darker (0.12f), c.x + R, c.y + R, true));
        else g.setColour (col);
        g.fillEllipse (core);
    }
    g.setColour (col);
    g.strokePath (front, ring);
}

// A whisper of grain on the face, so large panels read as a material rather than flat fill. One small
// noise tile, made once, laid over the face in a single fill (panels re-render every frame while they move).
inline void panelGrain (juce::Graphics& g, juce::Rectangle<float> r, float radius)
{
    const bool light = ThemeState::get().base.light;
    static juce::Image tiles[2];
    auto& tile = tiles[light ? 1 : 0];
    if (! tile.isValid())
    {
        tile = juce::Image (juce::Image::ARGB, 96, 96, true);
        juce::Image::BitmapData bd (tile, juce::Image::BitmapData::writeOnly);
        juce::Random rnd (1234);
        for (int y = 0; y < 96; ++y)
            for (int x = 0; x < 96; ++x)
            {
                const float n = rnd.nextFloat();
                if (n < 0.12f) bd.setPixelColour (x, y, juce::Colours::black.withAlpha (light ? 0.04f : 0.08f));
                else if (n > 0.95f) bd.setPixelColour (x, y, juce::Colours::white.withAlpha (light ? 0.04f : 0.03f));
            }
    }
    g.setTiledImageFill (tile, 0, 0, 1.0f);
    g.fillRoundedRectangle (r, radius);
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

    std::unordered_map<juce::String, juce::Image> knobBodies; // cached machined knob bodies

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
        g.setFont (font (14.0f)); // same weight for every row: the dot and colour mark the current one
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
            // Pressed in: darker face with a shadow along the top edge.
            g.setColour (juce::Colours::black.withAlpha (0.18f));
            g.fillRoundedRectangle (r, 8.0f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.22f), 0, r.getY(), juce::Colours::transparentBlack, 0, r.getY() + 6.0f, false));
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
        if (knobStyle == KnobMachined)
        {
            // Machined: a solid cylinder with a coloured cap, lit from the top left, sitting on its own shadow.
            // The body never changes with the value, so it is drawn once per size/colour/theme and cached;
            // only the pointer is drawn live.
            const float capR = radius - juce::jmax (6.0f, radius * 0.3f);
            const float depth = juce::jmax (2.5f, capR * 0.22f);
            const bool over = s.isMouseOverOrDragging();
            const auto centre = juce::Point<float> (cx, cy - depth * 0.35f);
            const float scale = juce::jmax (1.0f, (float) g.getInternalContext().getPhysicalPixelScaleFactor());
            const auto key = juce::String (capR, 1) + ":" + c.toString() + (over ? "h" : "") + ":" + juce::String (scale, 2)
                           + ":" + juce::String (ThemeState::get().version);
            auto& body = knobBodies[key];
            const float pad = depth + 8.0f;
            if (! body.isValid())
            {
                const int sz = (int) std::ceil ((capR + pad) * 2.0f * scale);
                body = juce::Image (juce::Image::ARGB, sz, sz, true);
                juce::Graphics bg (body);
                bg.addTransform (juce::AffineTransform::scale (scale));
                const auto ctr = juce::Point<float> (capR + pad, capR + pad);
                auto cap = juce::Rectangle<float> (capR * 2, capR * 2).withCentre (ctr);
                for (int i = 3; i >= 1; --i)
                {
                    bg.setColour (juce::Colours::black.withAlpha (0.045f * (float) i));
                    bg.fillEllipse (cap.translated (0, depth + 1.5f * (float) i).expanded (0.8f * (float) (4 - i)));
                }
                const auto side = c.darker (0.55f);
                bg.setGradientFill (juce::ColourGradient (side.brighter (0.15f), cap.getX(), ctr.y, side.darker (0.4f), cap.getRight(), ctr.y, false));
                bg.fillEllipse (cap.translated (0, depth));
                bg.fillRect (cap.withTrimmedTop (capR).withHeight (depth));
                bg.setGradientFill (juce::ColourGradient (c.brighter (over ? 0.35f : 0.22f), cap.getX() + capR * 0.4f, cap.getY() + capR * 0.3f,
                                                          c.darker (0.18f), cap.getRight(), cap.getBottom(), true));
                bg.fillEllipse (cap);
                bg.setColour (juce::Colours::black.withAlpha (0.05f));
                for (float rr = capR * 0.25f; rr < capR; rr += juce::jmax (1.6f, capR * 0.12f))
                    bg.drawEllipse (juce::Rectangle<float> (rr * 2, rr * 2).withCentre (ctr), 0.6f);
                bg.setColour (juce::Colours::white.withAlpha (0.35f));
                juce::Path rim;
                rim.addCentredArc (ctr.x, ctr.y, capR - 0.6f, capR - 0.6f, 0.0f, -2.4f, -0.4f, true);
                bg.strokePath (rim, juce::PathStrokeType (1.0f));
                bg.setColour (juce::Colours::white.withAlpha (0.28f));
                bg.fillEllipse (juce::Rectangle<float> (capR * 0.7f, capR * 0.38f).withCentre (ctr.translated (-capR * 0.28f, -capR * 0.42f)));
                if (knobBodies.size() > 400) knobBodies.clear(); // theme churn: don't let it grow forever
            }
            g.drawImage (body, juce::Rectangle<float> (centre.x - capR - pad, centre.y - capR - pad, (capR + pad) * 2, (capR + pad) * 2));
            const auto ink = c.getPerceivedBrightness() > 0.6f ? juce::Colour (0xff111111) : juce::Colours::white;
            const auto tip = centre.getPointOnCircumference (capR * 0.86f, angle);
            const auto root = centre.getPointOnCircumference (capR * 0.42f, angle);
            g.setColour (ink.withAlpha (0.9f));
            g.drawLine (juce::Line<float> (root, tip), juce::jmax (1.8f, capR * 0.14f));
            return;
        }
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
