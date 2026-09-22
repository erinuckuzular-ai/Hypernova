#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/UI/Style.h"

// Renders the installer/DMG artwork.
//   packaging/art/dmg-background.png (+@2x)   disk image window, in the Paper house style (docs/style.css)
//   packaging/resources/background.png        art card in the macOS Installer, Paper style
//   packaging/art/AppIcon.png                 installer app + volume icon (1024): white tile, the orange nova mark
//   packaging/art/PackIcon.png, ExtrasIcon.png folder icons in the disk image, paper white
// MakeInstallerArt <repoRoot>
using namespace ab::ui;

namespace
{
    const juce::Colour violet { 0xff8a5cff }, gold { 0xffffa94a };

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

    // ---- Paper --------------------------------------------------------------------------------------------
    namespace paper
    {
        const juce::Colour bg { 0xfff2f1ee }, card { 0xffffffff }, rule { 0xffd9d8d3 },
                           ink { 0xff111111 }, ink2 { 0xff4a4a4a }, ink3 { 0xff8b8b88 }, accent { 0xffff5a1f };

        // Space Grotesk (variable; its named instances resolve once registered with CoreText) and Space Mono.
        void registerFonts (const juce::File& root) { juce::Typeface::scanFolderForFonts (root.getChildFile ("Resources/Fonts")); }
        juce::Font grotesk (float h, bool bold = true) { return juce::Font (juce::FontOptions ("Space Grotesk", bold ? "Bold" : "Regular", h)); }
        juce::Font mono (float h, bool bold = false) { return juce::Font (juce::FontOptions ("Space Mono", bold ? "Bold" : "Regular", h)); }

        // The site's drawText, with a warning if the line would not fit (so a copy change can't clip silently).
        void text (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Justification j)
        {
            if (juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), t) > r.getWidth())
                std::printf ("warning: \"%s\" is wider than %.0f px\n", t.toRawUTF8(), r.getWidth());
            g.drawText (t, r, j, false);
        }

        // White card, 1px rule border, soft lift shadow.
        void cardAt (juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour border = rule, float borderW = 1.0f)
        {
            juce::Path p;
            p.addRoundedRectangle (r, radius);
            juce::DropShadow (juce::Colours::black.withAlpha (0.10f), 26, { 0, 12 }).drawForPath (g, p);
            juce::DropShadow (juce::Colours::black.withAlpha (0.06f), 3, { 0, 1 }).drawForPath (g, p);
            g.setColour (card);
            g.fillPath (p);
            g.setColour (border);
            g.drawRoundedRectangle (r.reduced (borderW * 0.5f), radius, borderW);
        }

        void dot (juce::Graphics& g, juce::Point<float> c, float d)
        {
            g.setColour (accent);
            g.fillEllipse (juce::Rectangle<float> (d, d).withCentre (c));
        }
    }

    juce::Image dmgBackground (float scale)
    {
        using namespace paper;
        const int w = 660, h = 440;
        juce::Image img (juce::Image::RGB, juce::roundToInt (w * scale), juce::roundToInt (h * scale), false);
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (scale));
        g.fillAll (bg);

        // Wordmark, top-left: the nova mark, then lowercase hypernova; the sub-line in mono.
        drawNovaMark (g, { 38, 41 }, 5.0f, accent);
        g.setColour (ink);
        g.setFont (grotesk (27).withExtraKerningFactor (-0.03f));
        text (g, "hypernova", { 53, 24, 300, 34 }, juce::Justification::centredLeft);
        g.setColour (ink3);
        g.setFont (mono (10.5f));
        text (g, "wavetable synthesizer", { 54, 55, 300, 16 }, juce::Justification::centredLeft);
        g.setColour (ink2);
        g.setFont (mono (10.0f));
        text (g, "vst3 / au / standalone / free", { 326, 34, 300, 14 }, juce::Justification::centredRight);

        // Finder draws each icon (104 px) centred at (130|330|530, 200) with its name just below; the cards sit
        // behind them. Positions come from scripts/make-dmg.sh.
        struct Plate { float x; const char* caption; bool first; };
        const Plate plates[] = { { 130, "double-click to install", true }, { 330, "free exclusive sounds", false }, { 530, "pkg, uninstall, read me", false } };
        for (const auto& p : plates)
        {
            auto c = juce::Rectangle<float> (170, 212).withCentre ({ p.x, 226 });
            cardAt (g, c, 16.0f, p.first ? accent : rule, p.first ? 1.5f : 1.0f);
            g.setColour (rule);
            g.fillRect (juce::Rectangle<float> (c.getX() + 16, 288, c.getWidth() - 32, 1));
            g.setFont (mono (10.5f, p.first));
            const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), p.caption);
            if (p.first)
                dot (g, { p.x - tw * 0.5f - 5.0f, 306 }, 6);
            g.setColour (p.first ? ink : ink2);
            if (p.first)
            {
                text (g, p.caption, juce::Rectangle<float> (tw + 2, 16).withCentre ({ p.x + 6.0f, 306 }), juce::Justification::centred);
            }
            else
                text (g, p.caption, juce::Rectangle<float> (c.getWidth() - 16, 16).withCentre ({ p.x, 306 }), juce::Justification::centred);
        }

        // Footer: what it installs, and the older-Mac hint.
        g.setColour (rule);
        g.fillRect (juce::Rectangle<float> (34, 358, 592, 1));
        g.setColour (ink);
        g.setFont (grotesk (12.0f, false));
        text (g, "installs the vst3 + au for ableton live, the standalone app and the founders pack. updates replace the old version.",
              { 34, 370, 592, 18 }, juce::Justification::centredLeft);
        g.setColour (ink2);
        g.setFont (mono (10.0f));
        text (g, "older mac (macOS 10.13 to 11)? open everything else and double-click install hypernova.pkg.",
              { 34, 392, 592, 16 }, juce::Justification::centredLeft);
        return img;
    }

    // App icon: a white machined tile with the orange dot, raised off a soft shadow, like the plug-in's knobs.
    juce::Image appIcon()
    {
        using namespace paper;
        const int s = 1024;
        juce::Image img (juce::Image::ARGB, s, s, true);
        juce::Graphics g (img);
        auto body = juce::Rectangle<float> (824, 824).withCentre ({ 512, 500 });
        for (int i = 6; i >= 1; --i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.025f * (float) (7 - i)));
            g.fillRoundedRectangle (body.translated (0, 6.0f * (float) i).expanded (2.0f * (float) i), 186.0f + (float) i * 2.0f);
        }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffffff), 512, body.getY(), juce::Colour (0xffeceae5), 512, body.getBottom(), false));
        g.fillRoundedRectangle (body, 186.0f);
        g.setColour (rule);
        g.drawRoundedRectangle (body.reduced (2.0f), 184.0f, 4.0f);
        // The nova mark, big, with a soft shadow under it.
        const juce::Point<float> c (512, 492);
        {
            juce::Image shade (juce::Image::ARGB, s, s, true);
            juce::Graphics sg (shade);
            drawNovaMark (sg, c.translated (0, 22), 170.0f, juce::Colours::black.withAlpha (0.16f), false);
            g.drawImageAt (shade.rescaled (s / 8, s / 8).rescaled (s, s), 0, 0);
        }
        drawNovaMark (g, c, 170.0f, accent);
        return img;
    }

    // Folder icons: a white folder with a thin rule and a lifted shadow; the pack gets an orange star,
    // the extras folder three ink lines.
    juce::Image folderIcon (juce::Colour c, bool star, float)
    {
        using namespace paper;
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
        for (int i = 5; i >= 1; --i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.03f * (float) (6 - i)));
            g.fillPath (folder, juce::AffineTransform::translation (0, 6.0f * (float) i));
        }
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xffffffff), 512, body.getY(), juce::Colour (0xffeeede9), 512, body.getBottom(), false));
        g.fillPath (folder);
        g.setColour (rule.darker (0.1f));
        g.strokePath (folder, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const juce::Point<float> ctr (512, 600);
        if (star)
        {
            juce::Path st;
            st.addStar (ctr, 4, 34.0f, 200.0f, 0.0f);
            g.setColour (c);
            g.fillPath (st);
        }
        else
            for (int i = 0; i < 3; ++i)
            {
                const float y = 500.0f + i * 90.0f, lw = i == 2 ? 260.0f : 420.0f;
                g.setColour (ink);
                g.fillRoundedRectangle (juce::Rectangle<float> (ctr.x - 210, y - 12, lw, 24), 12.0f);
            }
        return img;
    }

    juce::Image installerArt()
    {
        // The macOS Installer draws this behind its window, bottom-left: a small card, the rest transparent.
        using namespace paper;
        const int w = 620, h = 418;
        juce::Image img (juce::Image::ARGB, w * 2, h * 2, true);
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (2.0f));
        auto c = juce::Rectangle<float> (12, 262, 136, 144);
        cardAt (g, c, 14.0f);
        const float cx = c.getCentreX();
        drawNovaMark (g, { cx, c.getY() + 50 }, 12.0f, accent);
        g.setColour (ink);
        g.setFont (grotesk (19.0f).withExtraKerningFactor (-0.03f));
        text (g, "hypernova", { c.getX(), c.getY() + 84, c.getWidth(), 24 }, juce::Justification::centred);
        g.setColour (ink2);
        g.setFont (mono (8.5f));
        text (g, "wavetable synthesizer", { c.getX() + 6, c.getY() + 110, c.getWidth() - 12, 12 }, juce::Justification::centred);
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
    paper::registerFonts (root);
    save (dmgBackground (1.0f), root.getChildFile ("packaging/art/dmg-background.png"));
    save (dmgBackground (2.0f), root.getChildFile ("packaging/art/dmg-background@2x.png"));
    save (appIcon(), root.getChildFile ("packaging/art/AppIcon.png"));
    save (folderIcon (paper::accent, true, 0.0f), root.getChildFile ("packaging/art/PackIcon.png"));
    save (folderIcon (paper::ink, false, 0.0f), root.getChildFile ("packaging/art/ExtrasIcon.png"));
    save (installerArt(), root.getChildFile ("packaging/resources/background.png"));
    return 0;
}
