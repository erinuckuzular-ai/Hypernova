#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/UI/Style.h"

// Renders the installer/DMG artwork in Hypernova's cosmic look. The nebula and black hole are a CPU port of
// the plug-in's backdrop shader (Source/UI/Cosmos.h), so the disk image, the installer and the plug-in match.
//   packaging/art/dmg-background.png (+@2x)   disk image window
//   packaging/art/AppIcon.png                 installer app + volume icon (1024)
//   packaging/art/PackIcon.png, ExtrasIcon.png folder icons in the disk image
//   packaging/art/nebula.png, disk.png         layers the installer app animates
//   packaging/resources/background.png        art card in the macOS Installer
// MakeInstallerArt <repoRoot>
using namespace ab::ui;

namespace
{
    const juce::Colour ion { 0xff46e8ff }, violet { 0xff8a5cff }, gold { 0xffffa94a }, plasma { 0xffff4f9a };

    // ---- shader port ------------------------------------------------------------------------------------
    struct V3 { float r, g, b; };
    inline float fractf (float x) { return x - std::floor (x); }
    inline float hash (float x, float y)
    {
        x = fractf (x * 123.34f); y = fractf (y * 456.21f);
        const float d = x * (x + 45.32f) + y * (y + 45.32f);
        x += d; y += d;
        return fractf (x * y);
    }
    inline float smooth (float e0, float e1, float x) { const float t = juce::jlimit (0.0f, 1.0f, (x - e0) / (e1 - e0)); return t * t * (3.0f - 2.0f * t); }
    inline float noise (float x, float y)
    {
        const float ix = std::floor (x), iy = std::floor (y);
        float fx = x - ix, fy = y - iy;
        fx = fx * fx * (3.0f - 2.0f * fx); fy = fy * fy * (3.0f - 2.0f * fy);
        const float a = hash (ix, iy), b = hash (ix + 1, iy), c = hash (ix, iy + 1), d = hash (ix + 1, iy + 1);
        return (a + (b - a) * fx) + ((c + (d - c) * fx) - (a + (b - a) * fx)) * fy;
    }
    inline float fbm (float x, float y)
    {
        float v = 0, a = 0.5f;
        for (int i = 0; i < 5; ++i) { v += a * noise (x, y); x = x * 2.03f + 17.1f; y = y * 2.03f + 9.2f; a *= 0.5f; }
        return v;
    }

    // hole: centre in logical px (y down), radius px. seed shifts the nebula so each image differs.
    juce::Image cosmos (int w, int h, float scale, juce::Point<float> hole, float holeR, float seed, bool stars = true)
    {
        const int W = juce::roundToInt ((float) w * scale), H = juce::roundToInt ((float) h * scale);
        juce::Image img (juce::Image::ARGB, W, H, false);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        const V3 vio { 0.34f, 0.19f, 0.86f }, mag { 0.86f, 0.17f, 0.52f }, io { 0.16f, 0.74f, 1.0f }, gd { 1.0f, 0.63f, 0.27f };
        for (int py = 0; py < H; ++py)
            for (int px = 0; px < W; ++px)
            {
                const float fx = (float) px, fy = (float) (H - py);          // shader space: y up
                const float ux = fx / (float) H + seed, uy = fy / (float) H;
                const float qx = ux * 1.5f, qy = uy * 1.5f;
                const float wx = fbm (qx, qy), wy = fbm (qx + 5.2f, qy + 1.3f);
                const float n = fbm (qx + 1.8f * wx, qy + 1.8f * wy);
                const float n2 = fbm (qx * 2.2f - wx + 11.0f, qy * 2.2f - wy + 11.0f);
                V3 c { 0.014f, 0.016f, 0.040f };
                auto add = [&] (V3 k, float a) { c.r += k.r * a; c.g += k.g * a; c.b += k.b * a; };
                add (vio, smooth (0.30f, 0.90f, n) * 0.95f);
                add (mag, smooth (0.42f, 0.95f, n * wx * 1.8f) * 0.65f);
                add (io, smooth (0.55f, 0.95f, n2) * 0.40f);
                add (gd, smooth (0.72f, 1.0f, n * n2 * 1.9f) * 0.25f);

                if (stars)
                    for (int L = 0; L < 2; ++L)
                    {
                        const float cell = (L == 0 ? 70.0f : 34.0f) * scale;
                        const float sx = fx / cell, sy = fy / cell;
                        const float idx = std::floor (sx), idy = std::floor (sy);
                        const float hh = hash (idx + (float) L * 13.0f, idy + (float) L * 13.0f);
                        if (hh > 0.82f)
                        {
                            const float ox = hash (idx + 3.1f, idy + 3.1f) - 0.5f, oy = hash (idx + 7.7f, idy + 7.7f) - 0.5f;
                            const float dx = (sx - idx - 0.5f - ox * 0.7f), dy = (sy - idy - 0.5f - oy * 0.7f);
                            const float d = std::sqrt (dx * dx + dy * dy) * cell / scale;
                            const float size = L == 0 ? 1.6f : 0.9f;
                            const float tw = 0.55f + 0.45f * std::sin (hh * 60.0f);
                            add ({ 0.82f, 0.86f, 1.0f }, smooth (size, 0.0f, d) * tw * (L == 0 ? 1.0f : 0.6f));
                        }
                    }

                if (holeR > 0)
                {
                    const float hx = fx - hole.x * scale, hy = fy - ((float) h - hole.y) * scale, R = holeR * scale;
                    const float dpy = hy * 3.0f;
                    const float rd = std::sqrt (hx * hx + dpy * dpy) / R;
                    const float ang = std::atan2 (dpy, hx);
                    const float disk = smooth (2.7f, 1.35f, rd) * smooth (0.95f, 1.3f, rd);
                    const float streak = 0.5f + 0.5f * std::sin (ang * 5.0f + rd * 7.0f) * (0.6f + 0.4f * noise (ang * 3.0f, rd * 4.0f));
                    const float doppler = 0.6f + 0.4f * std::cos (ang - 0.4f);
                    const float t = juce::jlimit (0.0f, 1.0f, rd - 1.3f);
                    const V3 dc { gd.r + (mag.r - gd.r) * t, gd.g + (mag.g - gd.g) * t, gd.b + (mag.b - gd.b) * t };
                    const float r = std::sqrt (hx * hx + hy * hy) / R;
                    add (gd, std::exp (-juce::jmax (r - 1.0f, 0.0f) * 1.2f) * 0.22f);
                    add ({ 1.0f, 0.82f, 0.58f }, std::exp (-std::pow ((r - 1.06f) * 9.0f, 2.0f)) * 0.95f);
                    const float horizon = smooth (1.02f, 0.9f, r);
                    c.r *= 1 - horizon; c.g *= 1 - horizon; c.b *= 1 - horizon;
                    const float front = hy < 0 ? 1.0f : (r > 1.0f ? 1.0f : 0.25f);
                    add (dc, disk * streak * doppler * 1.1f * front);
                }

                const float vx = fx / (float) W - 0.5f, vy = fy / (float) H - 0.5f;
                const float vig = 1.0f - (vx * vx + vy * vy) * 0.85f;
                auto to8 = [] (float v) { return (juce::uint8) juce::jlimit (0, 255, juce::roundToInt (v * 255.0f)); };
                bd.setPixelColour (px, py, juce::Colour (to8 (c.r * vig), to8 (c.g * vig), to8 (c.b * vig)));
            }
        return img;
    }

    // Glass plate like the plug-in's panels, with a starlight rim.
    void glass (juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour rimColour, float strength)
    {
        glowRect (g, r, radius, rimColour, strength);
        g.setColour (juce::Colour (0x8c0b0d1a));
        g.fillRoundedRectangle (r, radius);
        juce::ColourGradient rim (rimColour.withAlpha (0.85f), r.getX(), r.getY(), rimColour.withAlpha (0.1f), r.getRight(), r.getBottom(), false);
        g.setGradientFill (rim);
        g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.3f);
    }

    juce::Image dmgBackground (float scale)
    {
        const int w = 660, h = 440;
        auto img = cosmos (w, h, scale, { 590, 58 }, 20.0f, 0.3f);
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (scale));

        g.setGradientFill (juce::ColourGradient (Colours::text, 34, 26, juce::Colour (0xffd9c8ff), 360, 60, false));
        g.setFont (heavy (30.0f).withExtraKerningFactor (0.18f));
        g.drawText ("HYPERNOVA", juce::Rectangle<float> (34, 24, 460, 36), juce::Justification::centredLeft, false);
        g.setColour (Colours::textDim);
        g.setFont (font (10.0f, true).withExtraKerningFactor (0.34f));
        g.drawText ("WAVETABLE SPACE SYNTH", juce::Rectangle<float> (36, 60, 400, 16), juce::Justification::centredLeft, false);

        // Finder draws each icon centred at (130|330|530, 200) with its name just below, on a pale band.
        struct Plate { float x; juce::Colour c; const char* caption; };
        const Plate plates[] = { { 130, ion, "1  DOUBLE-CLICK TO INSTALL" }, { 330, gold, "FREE EXCLUSIVE SOUNDS" }, { 530, violet, "PKG, UNINSTALL, READ ME" } };
        for (const auto& p : plates)
        {
            auto card = juce::Rectangle<float> (170, 212).withCentre ({ p.x, 226 });
            glass (g, card, 20.0f, p.c, p.x == 130 ? 1.2f : 0.5f);
            g.setColour (juce::Colour (0xffeef0ff));
            g.fillRoundedRectangle (juce::Rectangle<float> (150, 24).withCentre ({ p.x, 272 }), 12.0f);
            g.setColour (p.c);
            g.setFont (font (8.5f, true).withExtraKerningFactor (0.2f));
            g.drawText (p.caption, juce::Rectangle<float> (170, 16).withCentre ({ p.x, 306 }), juce::Justification::centred, false);
        }
        g.setColour (juce::Colours::black.withAlpha (0.45f)); // keeps the notes readable over the bright nebula
        g.fillRoundedRectangle (juce::Rectangle<float> (40, 362, 580, 50), 12.0f);
        g.setColour (Colours::text.withAlpha (0.85f));
        g.setFont (font (11.0f));
        g.drawText ("Installs the VST3 + AU for Ableton Live, the standalone app and the FOUNDERS PACK. Updates replace the old version.",
                    juce::Rectangle<float> (24, 368, 612, 18), juce::Justification::centred, false);
        g.setColour (Colours::text.withAlpha (0.85f));
        g.drawText ("Older Mac (macOS 10.13 to 11)? Open Everything else and double-click Install Hypernova.pkg.",
                    juce::Rectangle<float> (24, 388, 612, 18), juce::Justification::centred, false);
        return img;
    }

    juce::Image appIcon()
    {
        const int s = 1024;
        juce::Image img (juce::Image::ARGB, s, s, true);
        juce::Graphics g (img);
        auto body = juce::Rectangle<float> (824, 824).withCentre ({ 512, 512 });
        juce::Path clip;
        clip.addRoundedRectangle (body, 186.0f);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillPath (clip, juce::AffineTransform::translation (0, 14));
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (clip);
            const auto space = cosmos (412, 412, 2.0f, { 206, 206 }, 62.0f, 1.7f);
            g.setOpacity (1.0f);
            g.drawImage (space, body);
        }
        g.setGradientFill (juce::ColourGradient (gold, body.getX(), body.getY(), violet, body.getRight(), body.getBottom(), false));
        g.strokePath (clip, juce::PathStrokeType (10.0f));
        return img;
    }

    // Folder shape filled with space, rimmed in colour; the pack gets a star, the extras folder lines.
    juce::Image folderIcon (juce::Colour c, bool star, float seed)
    {
        const int s = 1024;
        juce::Image img (juce::Image::ARGB, s, s, true);
        juce::Graphics g (img);
        auto body = juce::Rectangle<float> (860, 640).withCentre ({ 512, 560 });
        juce::Path folder;
        folder.startNewSubPath (body.getX(), body.getY() + 60);
        folder.lineTo (body.getX(), body.getY() - 20);
        folder.quadraticTo (body.getX(), body.getY() - 70, body.getX() + 50, body.getY() - 70);
        folder.lineTo (body.getX() + 300, body.getY() - 70);
        folder.lineTo (body.getX() + 360, body.getY() - 10);
        folder.lineTo (body.getRight() - 50, body.getY() - 10);
        folder.quadraticTo (body.getRight(), body.getY() - 10, body.getRight(), body.getY() + 40);
        folder.lineTo (body.getRight(), body.getBottom() - 60);
        folder.quadraticTo (body.getRight(), body.getBottom(), body.getRight() - 60, body.getBottom());
        folder.lineTo (body.getX() + 60, body.getBottom());
        folder.quadraticTo (body.getX(), body.getBottom(), body.getX(), body.getBottom() - 60);
        folder.closeSubPath();
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillPath (folder, juce::AffineTransform::translation (0, 16));
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (folder);
            g.setOpacity (1.0f);
            g.drawImage (cosmos (430, 400, 2.0f, {}, 0.0f, seed), body.expanded (0, 80));
            g.setGradientFill (juce::ColourGradient (c.withAlpha (0.3f), 512, 600, c.withAlpha (0.0f), 512, 180, true));
            g.fillRect (body.expanded (0, 80));
        }
        g.setGradientFill (juce::ColourGradient (c, body.getX(), body.getY(), c.interpolatedWith (juce::Colours::white, 0.35f), body.getRight(), body.getBottom(), false));
        g.strokePath (folder, juce::PathStrokeType (16.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const juce::Point<float> ctr (512, 600);
        if (star)
        {
            juce::Path st;
            st.addStar (ctr, 4, 30.0f, 210.0f, 0.0f);
            g.setColour (c.withAlpha (0.25f));
            g.fillPath (st);
            glowStroke (g, st, c.interpolatedWith (juce::Colours::white, 0.2f), 14.0f, 1.3f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white, ctr.x, ctr.y, c.withAlpha (0.0f), ctr.x + 80, ctr.y, true));
            g.fillEllipse (juce::Rectangle<float> (160, 160).withCentre (ctr));
        }
        else
            for (int i = 0; i < 4; ++i)
            {
                juce::Path line;
                const float y = 470.0f + i * 80.0f, lw = i == 3 ? 260.0f : 420.0f;
                line.startNewSubPath (ctr.x - 210, y);
                line.lineTo (ctr.x - 210 + lw, y);
                glowStroke (g, line, c, 20.0f, 1.0f);
            }
        return img;
    }

    // Face-on accretion disk (transparent): the installer spins it and squashes it into perspective.
    juce::Image diskImage()
    {
        const int s = 1024;
        juce::Image img (juce::Image::ARGB, s, s, true);
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < s; ++y)
            for (int x = 0; x < s; ++x)
            {
                const float dx = (float) x - 512.0f, dy = (float) y - 512.0f;
                const float rd = std::sqrt (dx * dx + dy * dy) / 190.0f;
                const float ang = std::atan2 (dy, dx);
                const float disk = smooth (2.7f, 1.35f, rd) * smooth (0.95f, 1.3f, rd);
                const float streak = 0.5f + 0.5f * std::sin (ang * 5.0f + rd * 7.0f) * (0.6f + 0.4f * noise (ang * 3.0f + 9.0f, rd * 4.0f));
                const float t = juce::jlimit (0.0f, 1.0f, rd - 1.3f);
                const float a = juce::jlimit (0.0f, 1.0f, disk * streak * 1.3f);
                const juce::Colour col = gold.interpolatedWith (plasma, t);
                bd.setPixelColour (x, y, col.withAlpha (a));
            }
        return img;
    }

    juce::Image installerArt()
    {
        // The macOS Installer draws this behind its window, bottom-left: a small card, the rest transparent.
        const int w = 620, h = 418;
        juce::Image img (juce::Image::ARGB, w * 2, h * 2, true);
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (2.0f));
        auto card = juce::Rectangle<float> (12, 262, 136, 144);
        juce::Path clip;
        clip.addRoundedRectangle (card, 16.0f);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (clip);
            g.setOpacity (1.0f);
            g.drawImage (cosmos (136, 144, 2.0f, { 68, 50 }, 16.0f, 2.4f), card);
        }
        g.setGradientFill (juce::ColourGradient (gold.withAlpha (0.8f), card.getX(), card.getY(), violet.withAlpha (0.4f), card.getRight(), card.getBottom(), false));
        g.strokePath (clip, juce::PathStrokeType (1.0f));
        g.setColour (Colours::text);
        g.setFont (heavy (15.0f).withExtraKerningFactor (0.12f));
        g.drawText ("HYPERNOVA", juce::Rectangle<float> (card.getX(), card.getY() + 90, card.getWidth(), 22), juce::Justification::centred, false);
        g.setColour (Colours::textDim);
        g.setFont (font (6.5f, true).withExtraKerningFactor (0.3f));
        g.drawText ("WAVETABLE SPACE SYNTH", juce::Rectangle<float> (card.getX(), card.getY() + 112, card.getWidth(), 12), juce::Justification::centred, false);
        return img;
    }

    void save (const juce::Image& img, const juce::File& f)
    {
        f.getParentDirectory().createDirectory();
        f.deleteFile();
        juce::FileOutputStream out (f);
        juce::PNGImageFormat().writeImageToStream (img, out);
        std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File root (argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]) : juce::File::getCurrentWorkingDirectory());
    save (dmgBackground (1.0f), root.getChildFile ("packaging/art/dmg-background.png"));
    save (dmgBackground (2.0f), root.getChildFile ("packaging/art/dmg-background@2x.png"));
    save (appIcon(), root.getChildFile ("packaging/art/AppIcon.png"));
    save (folderIcon (gold, true, 4.1f), root.getChildFile ("packaging/art/PackIcon.png"));
    save (folderIcon (violet, false, 6.3f), root.getChildFile ("packaging/art/ExtrasIcon.png"));
    save (cosmos (680, 460, 2.0f, {}, 0.0f, 0.9f, false), root.getChildFile ("packaging/art/nebula.png"));
    save (diskImage(), root.getChildFile ("packaging/art/disk.png"));
    save (installerArt(), root.getChildFile ("packaging/resources/background.png"));
    return 0;
}
