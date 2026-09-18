#include <juce_gui_basics/juce_gui_basics.h>
#include "../Source/UI/Style.h"

// Renders the installer artwork with the plug-in's own look (Source/UI/Style.h):
//   packaging/art/dmg-background.png (+@2x)   the disk image window
//   packaging/art/AppIcon.png                 1024 px icon for the installer app and the volume
//   packaging/resources/background.png        left-hand art in the macOS Installer (transparent elsewhere)
// MakeInstallerArt <repoRoot>
using namespace ab::ui;

namespace
{
    const juce::Colour cyan { 0xff2ff3e0 }, violet { 0xff8b6cff }, pink { 0xffff5c8a };

    void backdrop (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setGradientFill (juce::ColourGradient (Colours::bg1, 0, 0, Colours::bg0, 0, r.getBottom(), false));
        g.fillRect (r);
        g.setGradientFill (juce::ColourGradient (violet.withAlpha (0.20f), r.getWidth() * 0.18f, r.getHeight() * 0.1f,
                                                 violet.withAlpha (0.0f), r.getWidth() * 0.7f, r.getHeight() * 0.8f, true));
        g.fillRect (r);
        g.setGradientFill (juce::ColourGradient (cyan.withAlpha (0.13f), r.getWidth() * 0.9f, r.getHeight() * 0.35f,
                                                 cyan.withAlpha (0.0f), r.getWidth() * 0.4f, r.getHeight(), true));
        g.fillRect (r);
        juce::Random rnd (7);
        for (int i = 0; i < 140; ++i)
        {
            const float x = rnd.nextFloat() * r.getWidth(), y = rnd.nextFloat() * r.getHeight();
            const float s = 0.6f + rnd.nextFloat() * 1.4f;
            g.setColour (juce::Colours::white.withAlpha (0.08f + rnd.nextFloat() * 0.35f));
            g.fillEllipse (x, y, s, s);
        }
    }

    // The nova mark: glowing key-cap, orbit ring and four-point star.
    void novaMark (juce::Graphics& g, juce::Rectangle<float> cap)
    {
        const float k = cap.getWidth() / 48.0f;
        glowRect (g, cap, 12.0f * k, violet, 1.3f);
        g.setGradientFill (juce::ColourGradient (violet, cap.getX(), cap.getY(), cyan, cap.getRight(), cap.getBottom(), false));
        g.fillRoundedRectangle (cap, 12.0f * k);
        g.setColour (Colours::bg0.withAlpha (0.9f));
        g.fillRoundedRectangle (cap.reduced (3.0f * k), 9.5f * k);
        const auto c = cap.getCentre();
        juce::Path ring;
        ring.addEllipse (juce::Rectangle<float> (26 * k, 12 * k).withCentre (c));
        ring.applyTransform (juce::AffineTransform::rotation (-0.45f, c.x, c.y));
        glowStroke (g, ring, violet.brighter (0.3f), 1.2f * k, 0.7f);
        juce::Path star;
        star.addStar (c, 4, 2.2f * k, 13.0f * k, 0.0f);
        g.setColour (cyan.withAlpha (0.25f));
        g.fillPath (star);
        glowStroke (g, star, cyan, 1.6f * k);
        g.setColour (Colours::text);
        g.fillEllipse (juce::Rectangle<float> (4 * k, 4 * k).withCentre (c));
    }

    juce::Image dmgBackground (float scale)
    {
        const int w = 660, h = 440;
        juce::Image img (juce::Image::ARGB, (int) (w * scale), (int) (h * scale), true);
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (scale));
        const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
        backdrop (g, r);

        novaMark (g, { 34, 30, 44, 44 });
        g.setColour (Colours::text);
        g.setFont (heavy (26.0f).withExtraKerningFactor (0.14f));
        g.drawText ("HYPERNOVA", juce::Rectangle<float> (92, 28, 400, 30), juce::Justification::centredLeft, false);
        g.setColour (Colours::textDim);
        g.setFont (font (10.0f, true).withExtraKerningFactor (0.32f));
        g.drawText ("WAVETABLE SPACE SYNTH", juce::Rectangle<float> (93, 58, 400, 16), juce::Justification::centredLeft, false);

        // Three plates. Finder puts the icons' centres at (130|330|530, 206) and their names just below,
        // so each plate has a pale band under the icon where the dark Finder label stays readable.
        struct Plate { float x; juce::Colour c; const char* caption; };
        const Plate plates[] = { { 130, cyan, "1  DOUBLE-CLICK TO INSTALL" }, { 330, pink, "FREE EXCLUSIVE SOUNDS" }, { 530, violet, "PKG + READ ME" } };
        for (const auto& p : plates)
        {
            auto card = juce::Rectangle<float> (170, 212).withCentre ({ p.x, 226 });
            glowRect (g, card, 18.0f, p.c, p.x == 130 ? 1.4f : 0.6f);
            panel (g, card, 18.0f, Colours::panel.withAlpha (0.92f));
            g.setColour (p.c.withAlpha (p.x == 130 ? 0.9f : 0.45f));
            g.drawRoundedRectangle (card.reduced (0.5f), 18.0f, p.x == 130 ? 1.6f : 1.0f);
            g.setColour (juce::Colour (0xffeef2fb));
            g.fillRoundedRectangle (juce::Rectangle<float> (150, 24).withCentre ({ p.x, 272 }), 12.0f);
            g.setColour (p.c);
            g.setFont (font (8.5f, true).withExtraKerningFactor (0.2f));
            g.drawText (p.caption, juce::Rectangle<float> (170, 16).withCentre ({ p.x, 306 }), juce::Justification::centred, false);
        }

        g.setColour (Colours::textDim);
        g.setFont (font (11.0f));
        g.drawText ("Installs VST3 + AU for Ableton Live, the standalone app and the FOUNDERS PACK. Updates replace the old version.",
                    juce::Rectangle<float> (24, 368, 612, 18), juce::Justification::centred, false);
        g.setColour (Colours::textFaint);
        g.drawText ("Share your sounds: drag .hnpreset files onto Hypernova to import them.",
                    juce::Rectangle<float> (24, 388, 612, 18), juce::Justification::centred, false);
        return img;
    }

    juce::Image appIcon()
    {
        const int s = 1024;
        juce::Image img (juce::Image::ARGB, s, s, true);
        juce::Graphics g (img);
        // macOS icon grid: 824 px rounded square centred in 1024.
        auto body = juce::Rectangle<float> (824, 824).withCentre ({ 512, 512 });
        juce::Path clip;
        clip.addRoundedRectangle (body, 186.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillPath (clip, juce::AffineTransform::translation (0, 14));
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (clip);
            backdrop (g, body);
            g.setGradientFill (juce::ColourGradient (cyan.withAlpha (0.25f), 512, 512, cyan.withAlpha (0.0f), 512, 120, true));
            g.fillRect (body);
        }
        g.setGradientFill (juce::ColourGradient (violet.withAlpha (0.9f), body.getX(), body.getY(), cyan.withAlpha (0.9f), body.getRight(), body.getBottom(), false));
        g.strokePath (clip, juce::PathStrokeType (10.0f));
        const auto c = body.getCentre();
        juce::Path ring;
        ring.addEllipse (juce::Rectangle<float> (620, 250).withCentre (c));
        ring.applyTransform (juce::AffineTransform::rotation (-0.45f, c.x, c.y));
        glowStroke (g, ring, violet.brighter (0.35f), 16.0f, 1.2f);
        juce::Path star;
        star.addStar (c, 4, 44.0f, 300.0f, 0.0f);
        g.setColour (cyan.withAlpha (0.22f));
        g.fillPath (star);
        glowStroke (g, star, cyan, 18.0f, 1.4f);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white, c.x, c.y, cyan.withAlpha (0.0f), c.x + 110, c.y, true));
        g.fillEllipse (juce::Rectangle<float> (220, 220).withCentre (c));
        return img;
    }

    juce::Image installerArt()
    {
        // The macOS Installer draws this behind its whole window, bottom-left. Its step list sits top-left,
        // so the art is a small card in the bottom-left corner and everything else stays transparent.
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
            backdrop (g, card);
        }
        g.setColour (violet.withAlpha (0.5f));
        g.strokePath (clip, juce::PathStrokeType (1.0f));
        novaMark (g, juce::Rectangle<float> (50, 50).withCentre ({ card.getCentreX(), card.getY() + 46 }));
        g.setColour (Colours::text);
        g.setFont (heavy (15.0f).withExtraKerningFactor (0.12f));
        g.drawText ("HYPERNOVA", juce::Rectangle<float> (card.getX(), card.getY() + 84, card.getWidth(), 22), juce::Justification::centred, false);
        g.setColour (Colours::textDim);
        g.setFont (font (6.5f, true).withExtraKerningFactor (0.3f));
        g.drawText ("WAVETABLE SPACE SYNTH", juce::Rectangle<float> (card.getX(), card.getY() + 106, card.getWidth(), 12), juce::Justification::centred, false);
        return img;
    }

    // Neon folder icon (FOUNDERS PACK = star inside, Everything else = document lines).
    juce::Image folderIcon (juce::Colour c, bool star)
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
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillPath (folder, juce::AffineTransform::translation (0, 16));
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (folder);
            backdrop (g, body.expanded (0, 80));
            g.setGradientFill (juce::ColourGradient (c.withAlpha (0.28f), 512, 560, c.withAlpha (0.0f), 512, 160, true));
            g.fillRect (body.expanded (0, 80));
        }
        g.setGradientFill (juce::ColourGradient (c, body.getX(), body.getY(), c.interpolatedWith (juce::Colours::white, 0.3f), body.getRight(), body.getBottom(), false));
        g.strokePath (folder, juce::PathStrokeType (16.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const juce::Point<float> ctr (512, 600);
        if (star)
        {
            juce::Path st;
            st.addStar (ctr, 4, 34.0f, 220.0f, 0.0f);
            g.setColour (c.withAlpha (0.22f));
            g.fillPath (st);
            glowStroke (g, st, c, 16.0f, 1.3f);
            g.setGradientFill (juce::ColourGradient (juce::Colours::white, ctr.x, ctr.y, c.withAlpha (0.0f), ctr.x + 80, ctr.y, true));
            g.fillEllipse (juce::Rectangle<float> (160, 160).withCentre (ctr));
        }
        else
        {
            for (int i = 0; i < 4; ++i)
            {
                juce::Path line;
                const float y = 470.0f + i * 80.0f, w = i == 3 ? 260.0f : 420.0f;
                line.startNewSubPath (ctr.x - 210, y);
                line.lineTo (ctr.x - 210 + w, y);
                glowStroke (g, line, c, 22.0f, 1.0f);
            }
        }
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
    save (installerArt(), root.getChildFile ("packaging/resources/background.png"));
    save (folderIcon (pink, true), root.getChildFile ("packaging/art/PackIcon.png"));
    save (folderIcon (violet, false), root.getChildFile ("packaging/art/ExtrasIcon.png"));
    return 0;
}
