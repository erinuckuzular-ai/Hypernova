#pragma once

#include "Style.h"
#include "../PluginProcessor.h"

namespace ab::ui
{

// Section colours, from the current theme (Cosmic: ion blue, violet, gold, magenta, aurora, azure, lilac, starlight).
namespace Palette
{
    inline const ThemeColour oscA { SlotOscA }, oscB { SlotOscB }, sub { SlotSub }, filter { SlotFilter },
                             env { SlotEnv }, lfo { SlotLfo }, mod { SlotMod }, fx { SlotFx };
}

//==============================================================================
// Simple 3D camera: yaw around the vertical axis, pitch tilts the floor towards the viewer.
struct Camera3D
{
    float yaw = -0.55f, pitch = 0.42f, distance = 3.4f, scale = 100.0f;
    juce::Point<float> centre;

    // Returns screen point; depth (bigger = further) through the out param.
    juce::Point<float> project (float x, float y, float z, float* depth = nullptr) const
    {
        const float cy = std::cos (yaw), sy = std::sin (yaw);
        const float x1 = x * cy - z * sy;
        const float z1 = x * sy + z * cy;
        const float cp = std::cos (pitch), sp = std::sin (pitch);
        const float y2 = y * cp + z1 * sp;  // looking down onto the scene: far things sit higher
        const float z2 = z1 * cp - y * sp;
        if (depth != nullptr) *depth = z2;
        const float p = distance / (distance + z2);
        return { centre.x + x1 * p * scale, centre.y - y2 * p * scale };
    }
};

// Drag to orbit, double-click to reset, and a slow idle sway so the scene always feels alive.
class OrbitView : public juce::Component
{
public:
    OrbitView (float defaultYaw, float defaultPitch) : yaw0 (defaultYaw), pitch0 (defaultPitch)
    {
        cam.yaw = yaw0;
        cam.pitch = pitch0;
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    void mouseDown (const juce::MouseEvent&) override { dragYaw = cam.yaw; dragPitch = cam.pitch; dragging = true; }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        userYaw = dragYaw + (float) e.getDistanceFromDragStartX() * 0.01f - swayNow();
        userPitch = juce::jlimit (-0.15f, 1.35f, dragPitch + (float) e.getDistanceFromDragStartY() * 0.008f);
        hasUser = true;
        updateCamera();
        repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent&) override { hasUser = false; updateCamera(); repaint(); }

    // The idle sway only advances while there's sound, so a silent synth draws nothing.
    void tick (bool animate)
    {
        if (animate) time += 1.0f / 30.0f;
        updateCamera();
    }

protected:
    Camera3D cam;
    float time = 0;

private:
    float yaw0, pitch0, dragYaw = 0, dragPitch = 0, userYaw = 0, userPitch = 0;
    bool hasUser = false, dragging = false;

    float swayNow() const { return dragging ? 0.0f : std::sin (time * 0.35f) * 0.12f; }
    void updateCamera()
    {
        cam.yaw = (hasUser ? userYaw : yaw0) + swayNow();
        cam.pitch = hasUser ? userPitch : pitch0 + std::sin (time * 0.23f) * 0.03f;
    }
};

//==============================================================================
// Serum-style stacked wavetable in 3D. The live (modulated) frame glows; warp is drawn onto it.
class WavetableView : public OrbitView, public juce::SettableTooltipClient, public juce::FileDragAndDropTarget
{
public:
    WavetableView (HypernovaAudioProcessor& p, int oscIndex, ThemeColour c)
        : OrbitView (-0.5f, 0.45f), proc (p), osc (oscIndex), colour (c)
    {
        prefix = osc == 0 ? "a" : "b";
        setTooltip ("Drag to spin the wavetable. Double-click to reset the view. Drop a WAV here (or right-click) to load your own wavetable.");
    }

    std::function<void (const juce::String&)> onMessage; // status line in the editor

    // Dropping audio on an oscillator turns it into that oscillator's wavetable.
    static bool isAudio (const juce::File& f) { return f.hasFileExtension ("wav;aif;aiff;flac;hnwt"); }
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (const auto& f : files) if (isAudio (juce::File (f))) return true;
        return false;
    }
    void fileDragEnter (const juce::StringArray&, int, int) override { dropHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override { dropHover = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        dropHover = false;
        for (const auto& path : files)
        {
            const juce::File f (path);
            if (! isAudio (f)) continue;
            juce::String error;
            const auto name = proc.importWavetable (f, osc, error);
            if (onMessage) onMessage (name.isNotEmpty() ? "Loaded wavetable: " + name : error);
            break;
        }
        stackKey = {};
        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { showMenu(); return; }
        OrbitView::mouseDown (e);
    }

    void showMenu()
    {
        juce::PopupMenu m;
        m.addSectionHeader ("WAVETABLE");
        m.addItem (1, "Load from an audio file...");
        const auto mine = HypernovaAudioProcessor::installedWavetables();
        if (! mine.isEmpty())
        {
            juce::PopupMenu sub;
            for (int i = 0; i < mine.size(); ++i)
                sub.addItem (100 + i, mine[i], true, mine[i] == proc.userTableName (osc));
            m.addSubMenu ("My wavetables", sub);
        }
        m.addSeparator();
        m.addItem (2, "Use the built-in table", proc.userTableName (osc).isNotEmpty());
        m.addItem (3, "Show my wavetables folder");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this, mine] (int r)
        {
            if (r == 1) chooseFile();
            else if (r == 2) { proc.setUserTable (osc, {}); stackKey = {}; repaint(); }
            else if (r == 3) { HypernovaAudioProcessor::wavetableFolder().createDirectory();
                               HypernovaAudioProcessor::wavetableFolder().revealToUser(); }
            else if (r >= 100 && r - 100 < mine.size())
            {
                proc.setUserTable (osc, mine[r - 100]);
                stackKey = {};
                repaint();
                if (onMessage) onMessage ("Wavetable: " + mine[r - 100]);
            }
        });
    }

    void chooseFile()
    {
        chooser = std::make_unique<juce::FileChooser> ("Load a wavetable or any audio file",
                                                       juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       "*.wav;*.aif;*.aiff;*.flac;*.hnwt");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f == juce::File()) return;
            juce::String error;
            const auto name = proc.importWavetable (f, osc, error);
            if (onMessage) onMessage (name.isNotEmpty() ? "Loaded wavetable: " + name : error);
            stackKey = {};
            repaint();
        });
    }

    // Repaints only when something visible changed (table, warp, live position, camera).
    void refresh (bool sounding)
    {
        tick (sounding);
        const auto k = currentKey();
        const float pos = proc.shownPos[osc].load();
        const float warpAmt = proc.apvts.getRawParameterValue (prefix + "WarpAmt")->load();
        if (k != stackKey || std::abs (pos - lastPos) > 0.0005f || std::abs (warpAmt - lastWarpAmt) > 0.0005f)
        {
            lastPos = pos;
            lastWarpAmt = warpAmt;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const bool on = proc.apvts.getRawParameterValue (prefix + "On")->load() > 0.5f;
        const int tableIndex = (int) proc.apvts.getRawParameterValue (prefix + "Table")->load();
        const int warp = (int) proc.apvts.getRawParameterValue (prefix + "Warp")->load();
        const float warpAmt = proc.apvts.getRawParameterValue (prefix + "WarpAmt")->load();
        const float pos = proc.shownPos[osc].load();
        const auto* user = proc.currentUserTable (osc);
        const auto& wt = user != nullptr ? *user : WavetableBank::get().table (tableIndex);
        cam.centre = { r.getCentreX(), r.getCentreY() + r.getHeight() * 0.06f };
        cam.scale = juce::jmin (r.getWidth() * 0.33f, r.getHeight() * 0.74f);

        // The stack of frames and the floor only change with the table or camera: draw once, reuse.
        const auto k = currentKey();
        if (k != stackKey || ! stack.isValid())
        {
            stackKey = k;
            const float sc = 2.0f;
            stack = juce::Image (juce::Image::ARGB, getWidth() * 2, getHeight() * 2, true);
            juce::Graphics sg (stack);
            sg.addTransform (juce::AffineTransform::scale (sc));
            paintStack (sg, r, wt, on);
        }
        g.drawImage (stack, r);

        // Live frame: nearest frame with the warp applied, filled and glowing.
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (r.reduced (1).toNearestInt());
        const float livePos = juce::jlimit (0.0f, 1.0f, pos) * (wtFrames - 1);
        const int nearest = juce::jlimit (0, wtFrames - 1, juce::roundToInt (livePos));
        juce::Path fill;
        const auto live = framePath (wt, nearest, zFor (pos), warp, warpAmt, &fill, 96);
        if (on)
        {
            const auto top = cam.project (0.0f, 0.36f, zFor (pos)), bottom = cam.project (0.0f, 0.0f, zFor (pos));
            g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.32f), top.x, top.y, colour.withAlpha (0.02f), bottom.x, bottom.y, false));
            g.fillPath (fill);
        }
        glowStroke (g, live, on ? colour.brighter (0.3f) : Colours::textFaint, 2.0f, on ? 1.3f : 0.3f);

        g.setColour (Colours::text.withAlpha (on ? 0.9f : 0.4f));
        g.setFont (font (11.0f, true));
        g.drawText (wt.name.toUpperCase(), r.reduced (10, 7).removeFromTop (14), juce::Justification::topLeft, false);
        g.setColour (Colours::textDim);
        g.setFont (font (10.0f));
        g.drawText (wt.blurb, r.reduced (10, 7).removeFromBottom (13), juce::Justification::bottomLeft, false);
        g.setFont (mono (10.0f));
        g.drawText ("FRAME " + juce::String (nearest + 1) + "/" + juce::String (wtFrames), r.reduced (10, 7).removeFromTop (14),
                    juce::Justification::topRight, false);

        if (dropHover)
        {
            g.setColour (colour.withAlpha (0.18f));
            g.fillRoundedRectangle (r.reduced (2), 8.0f);
            g.setColour (colour);
            g.drawRoundedRectangle (r.reduced (2), 8.0f, 2.0f);
            g.setFont (font (12.0f, true));
            g.drawText ("DROP TO USE AS THIS WAVETABLE", r, juce::Justification::centred, false);
        }
    }

private:
    HypernovaAudioProcessor& proc;
    int osc;
    ThemeColour colour;
    juce::String prefix;
    juce::Image stack;
    juce::String stackKey;
    float lastPos = -1, lastWarpAmt = -1;
    bool dropHover = false;
    std::unique_ptr<juce::FileChooser> chooser;

    static float zFor (float framePos) { return -0.75f + 1.5f * framePos; } // frame 0 at the front

    juce::String currentKey() const
    {
        const int table = (int) proc.apvts.getRawParameterValue (prefix + "Table")->load();
        const bool on = proc.apvts.getRawParameterValue (prefix + "On")->load() > 0.5f;
        return proc.userTableName (osc) + juce::String (table) + (on ? "+" : "-") + juce::String (juce::roundToInt (cam.yaw * 200.0f)) + ":"
             + juce::String (juce::roundToInt (cam.pitch * 200.0f)) + "@" + juce::String (getWidth()) + "x" + juce::String (getHeight());
    }

    juce::Path framePath (const Wavetable& wt, int f, float z, int warp, float warpAmt, juce::Path* fill, int points) const
    {
        juce::Path p;
        const float* d = wt.frame (f, 0);
        for (int i = 0; i <= points; ++i)
        {
            double ph = (double) i / points;
            if (fill != nullptr && warp != WarpOff && warp != WarpFm)
                ph = juce::jlimit (0.0, 0.9999, dsp::warpPhase (warp, juce::jmin (ph, 0.9999), warpAmt, std::exp2 (8.0f - warpAmt * 6.0f), 0.0f));
            const float v = d[juce::jlimit (0, wtBaseSize - 1, (int) (ph * wtBaseSize))];
            const auto pt = cam.project (-1.0f + 2.0f * i / points, v * 0.36f, z);
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        if (fill != nullptr)
        {
            *fill = p;
            fill->lineTo (cam.project (1.0f, 0.0f, z));
            fill->lineTo (cam.project (-1.0f, 0.0f, z));
            fill->closeSubPath();
        }
        return p;
    }

    void paintStack (juce::Graphics& g, juce::Rectangle<float> r, const Wavetable& wt, bool on) const
    {
        g.setColour (Colours::inset);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (Colours::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (r.reduced (1).toNearestInt());

        juce::Path grid;
        for (int i = 0; i <= 8; ++i)
        {
            const float z = -0.75f + 1.5f * i / 8.0f;
            grid.startNewSubPath (cam.project (-1.0f, -0.42f, z)); grid.lineTo (cam.project (1.0f, -0.42f, z));
            const float x = -1.0f + 2.0f * i / 8.0f;
            grid.startNewSubPath (cam.project (x, -0.42f, -0.75f)); grid.lineTo (cam.project (x, -0.42f, 0.75f));
        }
        g.setColour (Colours::line);
        g.strokePath (grid, juce::PathStrokeType (0.7f));

        // Every other frame, back to front, fading with depth.
        const auto base = on ? colour : Colours::textFaint;
        for (int f = wtFrames - 1; f >= 0; f -= 2)
        {
            const float depth = (float) f / (wtFrames - 1);
            g.setColour (base.withAlpha (on ? 0.12f + 0.26f * (1.0f - depth) : 0.08f));
            g.strokePath (framePath (wt, f, zFor (depth), WarpOff, 0, nullptr, 72), juce::PathStrokeType (1.0f));
        }
    }
};

//==============================================================================
// The big 3D window: SPECTRUM = a flying waterfall of the output spectrum,
// ORBIT = the waveform unfolded in phase space (x = now, y = a quarter-cycle ago, z = time) — a sine is a tube.
class SoundSpace : public OrbitView, public juce::SettableTooltipClient
{
public:
    enum Mode { Spectrum, Orbit, Stereo };

    explicit SoundSpace (HypernovaAudioProcessor& p) : OrbitView (-0.42f, 0.5f), proc (p)
    {
        for (auto& row : history) row.fill (0.0f);
        setTooltip ("Drag to fly around the sound. Double-click to reset.");
    }

    void setMode (Mode m) { mode = m; repaint(); }
    Mode getMode() const { return mode; }

    // Skips all work when the output has been silent long enough for the waterfall to settle.
    void refresh (bool sounding)
    {
        tick (sounding);
        proc.scope.latest (sampL.data(), sampR.data(), fftSize);
        float peak = 0;
        for (int i = fftSize - 1024; i < fftSize; ++i) peak = juce::jmax (peak, std::abs (sampL[(size_t) i]));
        if (peak < 1.0e-4f && ! sounding)
        {
            if (++quietFrames > rows + 4) return;
        }
        else quietFrames = 0;

        // Spectrum row
        for (int i = 0; i < fftSize; ++i)
        {
            const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / (fftSize - 1));
            fftData[(size_t) i] = 0.5f * (sampL[(size_t) i] + sampR[(size_t) i]) * w;
        }
        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform (fftData.data());

        const double sr = juce::jmax (8000.0, proc.getCurrentSampleRate());
        std::array<float, cols> row {};
        for (int c = 0; c < cols; ++c)
        {
            const double f0 = 25.0 * std::pow (16000.0 / 25.0, (double) c / cols);
            const double f1 = 25.0 * std::pow (16000.0 / 25.0, (double) (c + 1) / cols);
            const int b0 = juce::jlimit (1, fftSize / 2 - 1, (int) (f0 * fftSize / sr));
            const int b1 = juce::jlimit (b0, fftSize / 2 - 1, (int) (f1 * fftSize / sr));
            float mag = 0;
            for (int b = b0; b <= b1; ++b) mag = juce::jmax (mag, fftData[(size_t) b]);
            const float db = juce::Decibels::gainToDecibels (mag / (fftSize * 0.25f), -90.0f);
            const float v = juce::jlimit (0.0f, 1.0f, (db + 78.0f) / 78.0f);
            row[(size_t) c] = juce::jmax (v, smoothRow[(size_t) c] * 0.72f);
            smoothRow[(size_t) c] = row[(size_t) c];
        }
        head = (head + 1) % rows;
        history[(size_t) head] = row;

        if (mode == Stereo) analyseStereo();

        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient (Colours::inset.brighter (0.05f), r.getCentreX(), r.getY(), Colours::bg0, r.getCentreX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (Colours::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);

        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (r.reduced (1).toNearestInt());
        cam.centre = { r.getCentreX(), r.getCentreY() + r.getHeight() * (mode == Spectrum ? 0.08f : 0.0f) };
        cam.scale = juce::jmin (r.getWidth() * 0.36f, r.getHeight() * 0.5f);

        if (mode == Spectrum) paintSpectrum (g);
        else if (mode == Orbit) paintOrbit (g);
        else paintStereo (g);
    }

private:
    static constexpr int fftOrder = 11, fftSize = 1 << fftOrder, cols = 60, rows = 26;
    HypernovaAudioProcessor& proc;
    Mode mode = Spectrum;
    juce::dsp::FFT fft { fftOrder };
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize> sampL {}, sampR {};
    std::array<std::array<float, cols>, rows> history {};
    std::array<float, cols> smoothRow {}, bandWidth {}, bandLevel {};
    std::array<std::array<float, fftSize * 2>, 2> stereoFft {};
    float correlation = 1.0f, stereoPeak = 0.1f;
    int head = 0, quietFrames = 0;

    // Per-band stereo width and overall correlation, measured from the same window the spectrum uses.
    void analyseStereo()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto& src = ch == 0 ? sampL : sampR;
            for (int i = 0; i < fftSize; ++i)
            {
                const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / (fftSize - 1));
                stereoFft[(size_t) ch][(size_t) i] = src[(size_t) i] * w;
            }
            std::fill (stereoFft[(size_t) ch].begin() + fftSize, stereoFft[(size_t) ch].end(), 0.0f);
            fft.performFrequencyOnlyForwardTransform (stereoFft[(size_t) ch].data());
        }
        const double sr = juce::jmax (8000.0, proc.getCurrentSampleRate());
        for (int c = 0; c < cols; ++c)
        {
            const double f0 = 25.0 * std::pow (16000.0 / 25.0, (double) c / cols);
            const double f1 = 25.0 * std::pow (16000.0 / 25.0, (double) (c + 1) / cols);
            const int b0 = juce::jlimit (1, fftSize / 2 - 1, (int) (f0 * fftSize / sr));
            const int b1 = juce::jlimit (b0, fftSize / 2 - 1, (int) (f1 * fftSize / sr));
            float l = 0, r = 0;
            for (int b = b0; b <= b1; ++b)
            {
                l = juce::jmax (l, stereoFft[0][(size_t) b]);
                r = juce::jmax (r, stereoFft[1][(size_t) b]);
            }
            const float sum = l + r;
            const float width = sum > 1.0e-6f ? std::abs (l - r) / sum : 0.0f;       // 0 mono, 1 hard one-sided
            const float level = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (sum / (fftSize * 0.25f), -90.0f) + 78.0f) / 78.0f);
            bandWidth[(size_t) c] = bandWidth[(size_t) c] * 0.7f + width * 0.3f;
            bandLevel[(size_t) c] = juce::jmax (level, bandLevel[(size_t) c] * 0.72f);
        }

        double dot = 0, el = 0, er = 0, peak = 0;
        for (int i = 0; i < fftSize; ++i)
        {
            const double l = sampL[(size_t) i], r = sampR[(size_t) i];
            dot += l * r; el += l * l; er += r * r;
            peak = juce::jmax (peak, juce::jmax (std::abs (l), std::abs (r)));
        }
        const double denom = std::sqrt (el * er);
        const float corr = denom > 1.0e-9 ? (float) (dot / denom) : 1.0f;
        correlation = correlation * 0.8f + corr * 0.2f;
        stereoPeak = juce::jmax ((float) peak, stereoPeak * 0.9f);
    }

    // Goniometer, correlation and per-band width: how the sound sits across the stereo field.
    void paintStereo (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat().reduced (10.0f);
        auto right = r.removeFromRight (juce::jmax (110.0f, r.getWidth() * 0.34f));
        auto meter = r.removeFromBottom (46.0f);
        const auto scope = r.reduced (4.0f);

        // Goniometer: left/right rotated 45 degrees, so mono sits on the vertical axis.
        const auto centre = scope.getCentre();
        const float rad = juce::jmin (scope.getWidth(), scope.getHeight()) * 0.5f;
        g.setColour (Colours::line);
        g.drawEllipse (juce::Rectangle<float> (rad * 2, rad * 2).withCentre (centre), 1.0f);
        for (int i = 0; i < 4; ++i)
        {
            const float a = juce::MathConstants<float>::pi * (0.25f + 0.5f * i);
            g.drawLine (juce::Line<float> (centre, centre.getPointOnCircumference (rad, a)), 0.6f);
        }
        g.setColour (Colours::textFaint);
        g.setFont (mono (9.0f));
        g.drawText ("L", juce::Rectangle<float> (16, 12).withCentre (centre.translated (-rad * 0.72f, -rad * 0.72f)), juce::Justification::centred, false);
        g.drawText ("R", juce::Rectangle<float> (16, 12).withCentre (centre.translated (rad * 0.72f, -rad * 0.72f)), juce::Justification::centred, false);
        g.drawText ("MONO", juce::Rectangle<float> (40, 12).withCentre (centre.translated (0, -rad - 8.0f)), juce::Justification::centred, false);

        const float norm = 1.0f / juce::jmax (0.05f, stereoPeak);
        juce::Path trace;
        bool started = false;
        for (int i = fftSize - 1024; i < fftSize; ++i)
        {
            const float l = sampL[(size_t) i] * norm, rr = sampR[(size_t) i] * norm;
            const auto pt = centre.translated ((rr - l) * rad * 0.7f, -(rr + l) * rad * 0.5f);
            if (! started) { trace.startNewSubPath (pt); started = true; }
            else trace.lineTo (pt);
        }
        glowStroke (g, trace, Palette::oscA, 1.0f, 0.8f);

        // Correlation: +1 mono, 0 wide, -1 out of phase.
        const float t = juce::jlimit (-1.0f, 1.0f, correlation);
        auto labels = meter.removeFromTop (14.0f);
        g.setColour (Colours::textDim);
        g.setFont (font (9.5f, true).withExtraKerningFactor (0.18f));
        g.drawText ("PHASE", labels, juce::Justification::centredLeft, false);
        g.setColour (t < -0.1f ? Palette::filter : Colours::text);
        g.setFont (mono (10.0f));
        g.drawText (juce::String (t, 2), labels, juce::Justification::centredRight, false);

        auto corrBar = meter.removeFromTop (14.0f);
        g.setColour (Colours::inset);
        g.fillRoundedRectangle (corrBar, 4.0f);
        const float mid = corrBar.getCentreX();
        auto fill = juce::Rectangle<float> (juce::jmin (mid, mid + t * corrBar.getWidth() * 0.5f), corrBar.getY(),
                                            juce::jmax (2.0f, std::abs (t) * corrBar.getWidth() * 0.5f), corrBar.getHeight());
        g.setColour ((t < -0.1f ? Palette::filter : Palette::oscA).withAlpha (0.65f));
        g.fillRoundedRectangle (fill, 4.0f);
        g.setColour (Colours::lineHi);
        g.drawLine (mid, corrBar.getY(), mid, corrBar.getBottom(), 1.0f);

        g.setColour (Colours::textFaint);
        g.setFont (font (9.0f));
        g.drawText ("out of phase", meter, juce::Justification::centredLeft, false);
        g.drawText ("wide", meter, juce::Justification::centred, false);
        g.drawText ("mono", meter, juce::Justification::centredRight, false);

        // Per-band width: how wide each part of the spectrum is, low to high.
        g.setColour (Colours::textDim);
        g.setFont (font (9.5f, true).withExtraKerningFactor (0.18f));
        g.drawText ("WIDTH BY FREQUENCY", right.removeFromTop (14.0f), juce::Justification::centredLeft, false);
        const float rowH = right.getHeight() / (float) cols;
        for (int c = 0; c < cols; ++c)
        {
            const float w = bandWidth[(size_t) c], lvl = bandLevel[(size_t) c];
            auto row = juce::Rectangle<float> (right.getX(), right.getY() + c * rowH, right.getWidth() * juce::jmax (0.02f, w), juce::jmax (1.0f, rowH - 1.0f));
            g.setColour (heat (w).withAlpha (0.25f + 0.65f * lvl));
            g.fillRect (row);
        }
        g.setColour (Colours::textFaint);
        g.setFont (mono (8.5f));
        g.drawText ("25 Hz", right.removeFromTop (12.0f), juce::Justification::centredRight, false);
        g.drawText ("16 kHz", right.removeFromBottom (12.0f), juce::Justification::centredRight, false);
    }

    static juce::Colour heat (float v)
    {
        // cyan -> violet -> hot pink -> warm as the level rises
        if (v < 0.5f) return Palette::oscA.interpolatedWith (Palette::oscB, v * 2.0f);
        if (v < 0.8f) return Palette::oscB.interpolatedWith (Palette::filter, (v - 0.5f) / 0.3f);
        return Palette::filter.interpolatedWith (Palette::sub, juce::jmin (1.0f, (v - 0.8f) / 0.2f));
    }

    void paintSpectrum (juce::Graphics& g)
    {
        // Floor grid with frequency lines.
        g.setColour (Colours::line);
        for (int i = 0; i <= 10; ++i)
        {
            const float z = -1.0f + 2.0f * i / 10.0f;
            g.drawLine (juce::Line<float> (cam.project (-1.0f, 0, z), cam.project (1.0f, 0, z)), 0.6f);
        }
        g.setFont (mono (9.0f));
        for (float f : { 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
        {
            const float x = -1.0f + 2.0f * (float) (std::log (f / 25.0) / std::log (16000.0 / 25.0));
            g.setColour (Colours::line);
            g.drawLine (juce::Line<float> (cam.project (x, 0, -1.0f), cam.project (x, 0, 1.0f)), 0.6f);
            g.setColour (Colours::textFaint);
            const auto p = cam.project (x, 0, -1.1f);
            g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000)) + "k" : juce::String ((int) f), juce::Rectangle<float> (40, 12).withCentre (p.translated (0, 8)),
                        juce::Justification::centred, false);
        }

        // Oldest (back) to newest (front).
        for (int k = rows - 1; k >= 0; --k)
        {
            const auto& row = history[(size_t) ((head - k + rows) % rows)];
            const float age = (float) k / (rows - 1);
            const float z = -1.0f + 2.0f * age; // newest at the front
            float peak = 0;
            juce::Path line, fill;
            for (int c = 0; c < cols; ++c)
            {
                const float x = -1.0f + 2.0f * c / (cols - 1);
                const auto pt = cam.project (x, row[(size_t) c] * 0.62f, z);
                peak = juce::jmax (peak, row[(size_t) c]);
                if (c == 0) { line.startNewSubPath (pt); fill.startNewSubPath (cam.project (x, 0, z)); }
                else line.lineTo (pt);
                fill.lineTo (pt);
            }
            fill.lineTo (cam.project (1.0f, 0, z));
            fill.closeSubPath();

            const float alpha = (1.0f - age) * 0.9f + 0.05f;
            const auto c = heat (peak);
            g.setColour (Colours::bg0.withAlpha (0.55f * alpha + 0.2f));
            g.fillPath (fill);
            g.setColour (c.withAlpha (0.10f * alpha));
            g.fillPath (fill);
            if (k == 0) glowStroke (g, line, c.brighter (0.2f), 1.8f, 1.2f);
            else
            {
                g.setColour (c.withAlpha (alpha * 0.75f));
                g.strokePath (line, juce::PathStrokeType (1.0f));
            }
        }
    }

    void paintOrbit (juce::Graphics& g)
    {
        const int note = proc.shownNote.load();
        const double sr = juce::jmax (8000.0, proc.getCurrentSampleRate());
        const double freq = note >= 0 ? 440.0 * std::pow (2.0, (note - 69) / 12.0) : 110.0;
        const int tau = juce::jlimit (1, 400, (int) std::round (sr / freq * 0.25));
        const int span = juce::jlimit (256, fftSize - tau - 1, (int) (sr / freq * 6.0));

        float peak = 0.02f;
        for (int i = fftSize - span - tau; i < fftSize; ++i)
            peak = juce::jmax (peak, std::abs (sampL[(size_t) i]), std::abs (sampR[(size_t) i]));
        const float k = 0.85f / peak;

        // Axis tube guide
        g.setColour (Colours::line);
        for (int i = 0; i <= 6; ++i)
        {
            const float z = -1.0f + 2.0f * i / 6.0f;
            juce::Path ring;
            for (int a = 0; a <= 48; ++a)
            {
                const float ang = juce::MathConstants<float>::twoPi * a / 48;
                const auto pt = cam.project (std::cos (ang) * 0.9f, std::sin (ang) * 0.9f, z);
                if (a == 0) ring.startNewSubPath (pt); else ring.lineTo (pt);
            }
            g.strokePath (ring, juce::PathStrokeType (0.6f));
        }

        constexpr int segments = 24;
        const int start = fftSize - span;
        const int per = juce::jmax (2, span / segments);
        for (int s = 0; s < segments; ++s)
        {
            juce::Path p;
            const int i0 = start + s * per, i1 = juce::jmin (fftSize - 1, i0 + per + 1);
            for (int i = i0; i < i1; ++i)
            {
                const float mono = 0.5f * (sampL[(size_t) i] + sampR[(size_t) i]);
                const float prev = 0.5f * (sampL[(size_t) (i - tau)] + sampR[(size_t) (i - tau)]);
                const float side = 0.5f * (sampL[(size_t) i] - sampR[(size_t) i]);
                const float z = 1.0f - 2.0f * (float) (i - start) / span;
                const auto pt = cam.project ((mono + side * 0.6f) * k, prev * k, z);
                if (i == i0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            const float t = (float) s / (segments - 1);
            const auto c = Palette::oscB.interpolatedWith (Palette::oscA, t);
            if (s == segments - 1) glowStroke (g, p, c, 1.8f, 1.0f);
            else
            {
                g.setColour (c.withAlpha (0.25f + 0.65f * t));
                g.strokePath (p, juce::PathStrokeType (1.1f + t * 0.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
        }
    }
};

//==============================================================================
// A knob, and a drop target for modulation: drag an LFO or envelope chip onto it to modulate it. The ring
// around the knob shows how much modulation is arriving and from where.
class Knob : public juce::Component, public juce::DragAndDropTarget
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& label, ThemeColour c, int knobSize = 44)
        : name (label), colour (c), size (knobSize), id (paramId)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setColour (juce::Slider::rotarySliderFillColourId, colour);
        slider.setMouseDragSensitivity (220);
        slider.setDoubleClickReturnValue (true, 0.0); // replaced below with the real default
        addAndMakeVisible (slider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramId, slider);
        param = state.getParameter (paramId);
        if (param != nullptr)
        {
            slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
            slider.setTooltip (param->getName (64));
        }
        slider.onValueChange = [this] { repaint(); };
    }

    // Set by the editor: what modulation is reaching this knob, and what to do when a source is dropped on it.
    struct ModInfo { float depth = 0; float live = 0; juce::Colour colour; int slot = -1; bool highlight = false; };
    std::function<ModInfo (const juce::String& paramId)> modLookup;
    std::function<void (const juce::String& source, const juce::String& paramId)> onModDrop;
    std::function<void (const juce::String& paramId)> onModMenu;

    void resized() override
    {
        auto r = getLocalBounds();
        slider.setBounds (r.removeFromTop (size).withSizeKeepingCentre (size, size));
    }

    void paint (juce::Graphics& g) override
    {
        slider.setColour (juce::Slider::rotarySliderFillColourId, colour); // follows the theme
        auto full = getLocalBounds().toFloat();
        auto r = full;
        r.removeFromTop ((float) size - 1.0f);
        const bool active = slider.isMouseOverOrDragging();
        g.setColour (active ? Colours::text : Colours::textDim);
        g.setFont (font (9.5f, true).withExtraKerningFactor (0.12f));
        g.drawFittedText (active && param != nullptr ? param->getCurrentValueAsText() : name, r.removeFromTop (13).toNearestInt(),
                          juce::Justification::centred, 1, 0.7f);
        if (showValue && ! active)
        {
            g.setColour (Colours::text.withAlpha (0.85f));
            g.setFont (mono (10.5f));
            g.drawFittedText (param != nullptr ? param->getCurrentValueAsText() : juce::String(), r.removeFromTop (13).toNearestInt(),
                              juce::Justification::centred, 1, 0.7f);
        }

        // Modulation ring: an arc from the knob's own value, in the source's colour, with a moving dot
        // showing where the modulation has the parameter right now.
        const auto info = modLookup ? modLookup (id) : ModInfo();
        if (std::abs (info.depth) > 0.001f || dropHover)
        {
            const auto knobArea = slider.getBounds().toFloat().reduced (2.0f);
            const float radius = juce::jmin (knobArea.getWidth(), knobArea.getHeight()) * 0.5f + 3.0f;
            const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
            const float here = a0 + (a1 - a0) * (float) slider.valueToProportionOfLength (slider.getValue());
            const float to = juce::jlimit (a0, a1, here + (a1 - a0) * info.depth);
            juce::Path arc;
            arc.addCentredArc (knobArea.getCentreX(), knobArea.getCentreY(), radius, radius, 0.0f,
                               juce::jmin (here, to), juce::jmax (here, to), true);
            g.setColour ((dropHover ? Colours::accent : info.colour).withAlpha (dropHover ? 0.9f : info.highlight ? 1.0f : 0.7f));
            g.strokePath (arc, juce::PathStrokeType (info.highlight ? 3.0f : 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            if (std::abs (info.depth) > 0.001f)
            {
                const float now = juce::jlimit (a0, a1, here + (a1 - a0) * info.depth * info.live);
                const auto dot = juce::Point<float> (knobArea.getCentreX(), knobArea.getCentreY())
                                     .getPointOnCircumference (radius, now - juce::MathConstants<float>::halfPi);
                g.setColour (info.colour.brighter (0.4f));
                g.fillEllipse (juce::Rectangle<float> (4.4f, 4.4f).withCentre (dot));
            }
        }
        else if (info.highlight)
        {
            // This knob is a target of the source being hovered, but at zero depth: outline it faintly.
            g.setColour (info.colour.withAlpha (0.5f));
            g.drawRoundedRectangle (full.reduced (1.0f), 8.0f, 1.0f);
        }
        if (dropHover)
        {
            g.setColour (Colours::accent.withAlpha (0.5f));
            g.drawRoundedRectangle (full.reduced (1.0f), 8.0f, 1.2f);
        }
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && onModMenu) onModMenu (id);
    }

    // Drag and drop: chips carry "mod:<source index>".
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("mod:"); }
    void itemDragEnter (const SourceDetails&) override { dropHover = true; repaint(); }
    void itemDragExit (const SourceDetails&) override { dropHover = false; repaint(); }
    void itemDropped (const SourceDetails& d) override
    {
        dropHover = false;
        if (onModDrop) onModDrop (d.description.toString(), id);
        repaint();
    }

    void setShowValue (bool b) { showValue = b; }
    void setLabel (const juce::String& s) { if (s != name) { name = s; repaint(); } }
    const juce::String& paramId() const { return id; }
    juce::Slider slider;

private:
    juce::String name;
    ThemeColour colour;
    int size;
    juce::String id;
    bool showValue = true, dropHover = false;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
// A click target with no appearance of its own (used over the painted logo).
class InvisibleButton : public juce::Button
{
public:
    InvisibleButton() : juce::Button ("invisible") {}
    void paintButton (juce::Graphics&, bool, bool) override {}
};

//==============================================================================
// A padlock: a locked section is left alone by the dice.
class LockButton : public juce::Button
{
public:
    explicit LockButton (ThemeColour c) : juce::Button ("lock"), colour (c)
    {
        setClickingTogglesState (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const bool locked = getToggleState();
        auto r = getLocalBounds().toFloat().withSizeKeepingCentre (11.0f, 13.0f);
        auto body = r.removeFromBottom (7.5f);
        g.setColour (locked ? colour.withAlpha (over ? 1.0f : 0.85f) : Colours::textFaint.withAlpha (over ? 0.9f : 0.45f));
        g.fillRoundedRectangle (body, 2.0f);
        juce::Path shackle;
        const float w = r.getWidth() * (locked ? 0.62f : 0.72f);
        shackle.addCentredArc (body.getCentreX() + (locked ? 0.0f : 1.5f), body.getY(), w * 0.5f, r.getHeight() * 0.85f,
                               0.0f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        g.strokePath (shackle, juce::PathStrokeType (1.4f));
    }

private:
    ThemeColour colour;
};

//==============================================================================
// The little grab handle on a modulation source. Drag it onto any knob to modulate that knob.
class ModChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    ModChip (const juce::String& labelText, int sourceIndex, ThemeColour c)
        : label (labelText), source (sourceIndex), colour (c)
    {
        setTooltip ("Drag onto any knob to modulate it");
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (colour.withAlpha (isMouseOver() ? 0.3f : 0.16f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (colour.withAlpha (isMouseOver() ? 0.95f : 0.6f));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
        g.setFont (font (9.0f, true).withExtraKerningFactor (0.12f));
        g.drawText (label, r, juce::Justification::centred, false);
    }

    std::function<void (int source)> onHover; // -1 when the mouse leaves

    void mouseEnter (const juce::MouseEvent&) override { if (onHover) onHover (source); repaint(); }
    void mouseExit (const juce::MouseEvent&) override { if (onHover) onHover (-1); repaint(); }

    void mouseDrag (const juce::MouseEvent&) override
    {
        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
            if (! c->isDragAndDropActive())
                c->startDragging ("mod:" + juce::String (source), this);
    }

private:
    juce::String label;
    int source;
    ThemeColour colour;
};

//==============================================================================
class PowerLed : public juce::ToggleButton
{
public:
    explicit PowerLed (ThemeColour c) : colour (c) {}
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().withSizeKeepingCentre (14, 14);
        if (getToggleState())
        {
            for (int i = 3; i >= 1; --i)
            {
                g.setColour (colour.withAlpha (0.08f * (float) (4 - i)));
                g.fillEllipse (r.expanded ((float) i * 2.2f));
            }
            g.setGradientFill (juce::ColourGradient (colour.brighter (0.6f), r.getCentreX(), r.getY(), colour, r.getCentreX(), r.getBottom(), false));
            g.fillEllipse (r);
        }
        else
        {
            g.setColour (Colours::inset);
            g.fillEllipse (r);
            g.setColour (over ? colour.withAlpha (0.6f) : Colours::lineHi);
            g.drawEllipse (r.reduced (0.5f), 1.2f);
        }
    }
private:
    ThemeColour colour;
};

class PillToggle : public juce::ToggleButton
{
public:
    PillToggle (const juce::String& text, ThemeColour c) : juce::ToggleButton (text), colour (c) {}
    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool on = getToggleState();
        g.setColour (on ? colour.withAlpha (0.14f) : Colours::inset);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (on ? colour.withAlpha (0.75f) : (over ? Colours::lineHi : Colours::line));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
        auto dot = r.removeFromLeft (r.getHeight()).withSizeKeepingCentre (6, 6);
        g.setColour (on ? colour : Colours::textFaint);
        g.fillEllipse (dot);
        g.setColour (on ? Colours::text : Colours::textDim);
        g.setFont (font (9.5f, true).withExtraKerningFactor (0.12f));
        g.drawText (getButtonText(), r.withTrimmedRight (6), juce::Justification::centredLeft, false);
    }
private:
    ThemeColour colour;
};

//==============================================================================
class IconButton : public juce::Button
{
public:
    enum Kind { Dice, Prev, Next, Save, Undo, Redo, Gear, Expand, PopOut, Close };
    IconButton (Kind k, ThemeColour c = Colours::text) : juce::Button ({}), kind (k), colour (c) {}

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        panel (g, r, 9.0f, over ? Colours::panelHi.brighter (0.06f) : Colours::panelHi);
        auto icon = r.withSizeKeepingCentre (r.getHeight() * 0.5f, r.getHeight() * 0.5f).translated (0, down ? 0.5f : 0.0f);
        const auto c = colour.withAlpha (over ? 1.0f : 0.85f);
        g.setColour (c);
        juce::Path p;
        switch (kind)
        {
            case Dice:
            {
                g.drawRoundedRectangle (icon, icon.getWidth() * 0.24f, 1.5f);
                const float d = icon.getWidth() * 0.16f;
                for (auto pt : { icon.getCentre(), icon.getTopLeft() + juce::Point<float> (icon.getWidth() * 0.28f, icon.getHeight() * 0.28f),
                                 icon.getBottomRight() - juce::Point<float> (icon.getWidth() * 0.28f, icon.getHeight() * 0.28f) })
                    g.fillEllipse (juce::Rectangle<float> (d, d).withCentre (pt));
                return;
            }
            case Prev:
                p.startNewSubPath (icon.getRight() - icon.getWidth() * 0.3f, icon.getY());
                p.lineTo (icon.getX() + icon.getWidth() * 0.3f, icon.getCentreY());
                p.lineTo (icon.getRight() - icon.getWidth() * 0.3f, icon.getBottom());
                break;
            case Next:
                p.startNewSubPath (icon.getX() + icon.getWidth() * 0.3f, icon.getY());
                p.lineTo (icon.getRight() - icon.getWidth() * 0.3f, icon.getCentreY());
                p.lineTo (icon.getX() + icon.getWidth() * 0.3f, icon.getBottom());
                break;
            case Undo:
            case Redo:
            {
                const auto ctr = icon.getCentre();
                const float rad = icon.getWidth() * 0.36f;
                const bool redo = kind == Redo;
                p.addCentredArc (ctr.x, ctr.y, rad, rad, 0.0f, redo ? 2.2f : -2.2f, redo ? -1.4f : 1.4f, true);
                g.strokePath (p, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                const auto tip = ctr.getPointOnCircumference (rad, redo ? 2.2f : -2.2f);
                juce::Path head;
                const float d = redo ? -1.0f : 1.0f;
                head.addTriangle (tip.x - 4.0f * d, tip.y - 1.5f, tip.x + 3.0f * d, tip.y - 4.0f, tip.x + 0.5f * d, tip.y + 4.0f);
                g.fillPath (head);
                return;
            }
            case Gear:
            {
                const auto ctr = icon.getCentre();
                const float ro = icon.getWidth() * 0.46f, ri = icon.getWidth() * 0.32f;
                juce::Path cog;
                for (int i = 0; i < 16; ++i)
                {
                    const float a = juce::MathConstants<float>::twoPi * i / 16.0f;
                    const auto pt = ctr.getPointOnCircumference ((i / 2) % 2 == 0 ? ro : ri, a);
                    if (i == 0) cog.startNewSubPath (pt); else cog.lineTo (pt);
                }
                cog.closeSubPath();
                g.strokePath (cog, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.drawEllipse (juce::Rectangle<float> (icon.getWidth() * 0.26f, icon.getWidth() * 0.26f).withCentre (ctr), 1.5f);
                return;
            }
            case Expand: // four corners pointing out
            {
                const float k = icon.getWidth() * 0.38f;
                for (int corner = 0; corner < 4; ++corner)
                {
                    const float sx = (corner & 1) ? 1.0f : -1.0f, sy = (corner & 2) ? 1.0f : -1.0f;
                    const auto c0 = icon.getCentre().translated (sx * icon.getWidth() * 0.5f, sy * icon.getHeight() * 0.5f);
                    p.startNewSubPath (c0);
                    p.lineTo (c0.translated (-sx * k, 0));
                    p.startNewSubPath (c0);
                    p.lineTo (c0.translated (0, -sy * k));
                }
                break;
            }
            case PopOut: // a small window lifting off a larger one
            {
                auto back = icon.withTrimmedRight (icon.getWidth() * 0.3f).withTrimmedBottom (icon.getHeight() * 0.3f);
                auto front = icon.withTrimmedLeft (icon.getWidth() * 0.3f).withTrimmedTop (icon.getHeight() * 0.3f);
                g.drawRoundedRectangle (back, 2.0f, 1.4f);
                g.setColour (Colours::panelHi);
                g.fillRoundedRectangle (front, 2.0f);
                g.setColour (c);
                g.drawRoundedRectangle (front, 2.0f, 1.6f);
                return;
            }
            case Close:
                p.startNewSubPath (icon.getTopLeft());
                p.lineTo (icon.getBottomRight());
                p.startNewSubPath (icon.getTopRight());
                p.lineTo (icon.getBottomLeft());
                break;
            case Save:
                p.startNewSubPath (icon.getCentreX(), icon.getY());
                p.lineTo (icon.getCentreX(), icon.getBottom() - icon.getHeight() * 0.3f);
                p.startNewSubPath (icon.getCentreX() - icon.getWidth() * 0.28f, icon.getCentreY());
                p.lineTo (icon.getCentreX(), icon.getBottom() - icon.getHeight() * 0.3f);
                p.lineTo (icon.getCentreX() + icon.getWidth() * 0.28f, icon.getCentreY());
                p.startNewSubPath (icon.getX(), icon.getBottom());
                p.lineTo (icon.getRight(), icon.getBottom());
                break;
        }
        g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    Kind kind;
    ThemeColour colour;
};

//==============================================================================
// An effect's name, used as its on/off switch: click the title to bypass that effect. Off dims the name and
// strikes it through, so it reads at a glance which effects are doing anything.
class SectionToggle : public juce::Button
{
public:
    SectionToggle (const juce::String& title, ThemeColour c) : juce::Button (title), name (title), colour (c)
    {
        setClickingTogglesState (true);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        const bool on = getToggleState();
        auto r = getLocalBounds().toFloat();
        auto dot = r.removeFromLeft (14.0f).withSizeKeepingCentre (7.0f, 7.0f);
        if (on)
        {
            g.setColour (colour.withAlpha (0.3f));
            g.fillEllipse (dot.expanded (2.0f));
            g.setColour (colour);
            g.fillEllipse (dot);
        }
        else
        {
            g.setColour (Colours::textFaint);
            g.drawEllipse (dot, 1.0f);
        }

        const auto f = font (10.5f, true).withExtraKerningFactor (0.22f);
        g.setColour (on ? (over ? Colours::text : colour) : Colours::textFaint);
        g.setFont (f);
        g.drawText (name, r, juce::Justification::centredLeft, false);
        if (! on)
        {
            const float w = juce::jmin (r.getWidth(), (float) juce::GlyphArrangement::getStringWidthInt (f, name) + 2.0f);
            g.drawLine (r.getX(), r.getCentreY(), r.getX() + w, r.getCentreY(), 1.0f);
        }
    }

private:
    juce::String name;
    ThemeColour colour;
};

//==============================================================================
// Two-button segmented switch (e.g. SPECTRUM / ORBIT).
class Segmented : public juce::Component
{
public:
    Segmented (juce::StringArray items, ThemeColour c) : labels (std::move (items)), colour (c) {}
    std::function<void (int)> onChange;
    void setSelected (int i) { selected = i; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (Colours::inset);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (Colours::line);
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
        const float w = r.getWidth() / (float) labels.size();
        for (int i = 0; i < labels.size(); ++i)
        {
            auto cell = juce::Rectangle<float> (r.getX() + w * i, r.getY(), w, r.getHeight()).reduced (2);
            if (i == selected)
            {
                g.setColour (colour.withAlpha (0.18f));
                g.fillRoundedRectangle (cell, cell.getHeight() * 0.5f);
                g.setColour (colour.withAlpha (0.8f));
                g.drawRoundedRectangle (cell, cell.getHeight() * 0.5f, 1.0f);
            }
            g.setColour (i == selected ? Colours::text : Colours::textDim);
            g.setFont (font (9.5f, true).withExtraKerningFactor (0.14f));
            g.drawFittedText (labels[i], cell.toNearestInt().reduced (4, 0), juce::Justification::centred, 1, 0.7f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = juce::jlimit (0, labels.size() - 1, (int) (e.position.x / ((float) getWidth() / (float) labels.size())));
        setSelected (i);
        if (onChange) onChange (i);
    }

private:
    juce::StringArray labels;
    ThemeColour colour;
    int selected = 0;
};

//==============================================================================
class LfoView : public juce::Component
{
public:
    LfoView (HypernovaAudioProcessor& p, int index) : proc (p), lfo (index) {}

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::inset);
        g.fillRoundedRectangle (r, 8.0f);
        auto area = r.reduced (8, 7);
        g.setColour (Colours::line);
        g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

        const int shape = (int) proc.apvts.getRawParameterValue ("lfo" + juce::String (lfo + 1) + "Shape")->load();
        juce::Path p;
        juce::Random rnd (7);
        std::array<float, 9> held {};
        for (auto& h : held) h = rnd.nextFloat() * 2.0f - 1.0f;
        constexpr int n = 120;
        for (int i = 0; i <= n; ++i)
        {
            const double ph = (double) i / n;
            const int step = juce::jmin (7, (int) (ph * 8.0));
            const double local = ph * 8.0 - step;
            float v;
            if (shape == LSnH) v = held[(size_t) step];
            else if (shape == LSmooth) v = dsp::lfoShape (LSmooth, local, held[(size_t) step + 1], held[(size_t) step]);
            else v = dsp::lfoShape (shape, ph, 0, 0);
            const juce::Point<float> pt (area.getX() + area.getWidth() * (float) ph, area.getCentreY() - v * area.getHeight() * 0.46f);
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        glowStroke (g, p, Palette::lfo, 1.5f, 0.7f);

        if (shape != LSnH && shape != LSmooth)
        {
            const float ph = proc.shownLfoPhase[lfo].load();
            const float v = proc.shownLfo[lfo].load();
            const juce::Point<float> dot (area.getX() + area.getWidth() * ph, area.getCentreY() - v * area.getHeight() * 0.46f);
            g.setColour (Palette::lfo.withAlpha (0.25f));
            g.fillEllipse (juce::Rectangle<float> (12, 12).withCentre (dot));
            g.setColour (Colours::text);
            g.fillEllipse (juce::Rectangle<float> (5, 5).withCentre (dot));
        }
    }

private:
    HypernovaAudioProcessor& proc;
    int lfo;
};

//==============================================================================
class EnvView : public juce::Component, public juce::SettableTooltipClient
{
public:
    EnvView (HypernovaAudioProcessor& p, const juce::String& prefixIn, ThemeColour c, bool live) : proc (p), prefix (prefixIn), colour (c), showLive (live)
    {
        setTooltip ("Drag the handles: attack, decay/sustain, release. Double-click a handle to reset it.");
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::inset);
        g.fillRoundedRectangle (r, 8.0f);
        const auto sh = shape();

        juce::Path p;
        p.startNewSubPath (sh.x0, sh.yb);
        p.lineTo (sh.xa, sh.yb - sh.h);
        for (int i = 1; i <= 24; ++i)
        {
            const float t = (float) i / 24;
            p.lineTo (sh.xa + (sh.xd - sh.xa) * t, sh.yb - sh.h * (sh.s + (1.0f - sh.s) * std::pow (0.001f, t)));
        }
        p.lineTo (sh.xs, sh.yb - sh.h * sh.s);
        for (int i = 1; i <= 24; ++i)
        {
            const float t = (float) i / 24;
            p.lineTo (sh.xs + (sh.xr - sh.xs) * t, sh.yb - sh.h * sh.s * std::pow (0.001f, t));
        }
        juce::Path fill (p);
        fill.lineTo (sh.xr, sh.yb);
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.22f), 0, sh.top, colour.withAlpha (0.0f), 0, sh.yb, false));
        g.fillPath (fill);
        glowStroke (g, p, colour, 1.5f, 0.7f);

        // Drag handles, brighter under the mouse.
        for (int i = 0; i < 3; ++i)
        {
            const auto pt = handle (sh, i);
            const bool hot = i == hover || i == dragging;
            g.setColour (colour.withAlpha (hot ? 0.9f : 0.45f));
            g.fillEllipse (juce::Rectangle<float> (hot ? 8.0f : 6.0f, hot ? 8.0f : 6.0f).withCentre (pt));
            g.setColour (Colours::bg0.withAlpha (0.8f));
            g.drawEllipse (juce::Rectangle<float> (hot ? 8.0f : 6.0f, hot ? 8.0f : 6.0f).withCentre (pt), 1.0f);
        }

        if (showLive)
        {
            const float lvl = proc.shownEnv.load();
            if (lvl > 0.001f)
            {
                g.setColour (colour.withAlpha (0.6f));
                g.fillRect (juce::Rectangle<float> (sh.area.getRight() - 3, sh.yb - sh.h * lvl, 3, sh.h * lvl));
            }
        }
    }

    void mouseMove (const juce::MouseEvent& e) override { setHover (nearest (e.position)); }
    void mouseExit (const juce::MouseEvent&) override { setHover (-1); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = nearest (e.position);
        dragStart = e.position;
        if (dragging >= 0)
        {
            startA = value ("A"); startD = value ("D"); startS = value ("S"); startR = value ("R");
            proc.undoManager.beginNewTransaction ("envelope");
            for (auto* id : { "A", "D", "S", "R" })
                if (auto* p = proc.apvts.getParameter (prefix + id)) p->beginChangeGesture();
        }
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging < 0) return;
        const auto d = e.position - dragStart;
        const float w = juce::jmax (1.0f, getWidth() - 16.0f);
        // Times move on the same square-root scale the curve is drawn with, so dragging feels linear.
        auto timeDrag = [&] (float start, float dx, const char* id)
        {
            const float t = std::sqrt (juce::jmax (0.0f, start)) + dx / w * 2.4f;
            setValue (id, t * t);
        };
        switch (dragging)
        {
            case 0: timeDrag (startA, d.x, "A"); break;
            case 1: timeDrag (startD, d.x, "D");
                    setValue ("S", juce::jlimit (0.0f, 1.0f, startS - d.y / juce::jmax (1.0f, getHeight() - 14.0f))); break;
            default: timeDrag (startR, d.x, "R"); break;
        }
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging >= 0)
            for (auto* id : { "A", "D", "S", "R" })
                if (auto* p = proc.apvts.getParameter (prefix + id)) p->endChangeGesture();
        dragging = -1;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int h = nearest (e.position);
        if (h < 0) return;
        auto reset = [&] (const char* id)
        {
            if (auto* p = proc.apvts.getParameter (prefix + id))
                p->setValueNotifyingHost (p->getDefaultValue());
        };
        if (h == 0) reset ("A");
        else if (h == 1) { reset ("D"); reset ("S"); }
        else reset ("R");
        repaint();
    }

private:
    struct Shape { juce::Rectangle<float> area; float x0, xa, xd, xs, xr, yb, top, h, s; };

    Shape shape() const
    {
        auto area = getLocalBounds().toFloat().reduced (8, 7);
        const float a = value ("A"), d = value ("D"), s = value ("S"), rl = value ("R");
        auto tw = [] (float t) { return std::sqrt (juce::jmax (0.0f, t)); };
        const float total = tw (a) + tw (d) + 0.35f + tw (rl);
        const float k = area.getWidth() / juce::jmax (0.0001f, total);
        Shape sh;
        sh.area = area;
        sh.x0 = area.getX();
        sh.xa = sh.x0 + tw (a) * k;
        sh.xd = sh.xa + tw (d) * k;
        sh.xs = sh.xd + 0.35f * k;
        sh.xr = sh.xs + tw (rl) * k;
        sh.yb = area.getBottom();
        sh.top = area.getY();
        sh.h = area.getHeight();
        sh.s = s;
        return sh;
    }

    static juce::Point<float> handle (const Shape& sh, int i)
    {
        if (i == 0) return { sh.xa, sh.yb - sh.h };
        if (i == 1) return { sh.xd, sh.yb - sh.h * sh.s };
        return { sh.xr, sh.yb };
    }

    int nearest (juce::Point<float> p) const
    {
        const auto sh = shape();
        int best = -1;
        float bestD = 14.0f;
        for (int i = 0; i < 3; ++i)
        {
            const float d = handle (sh, i).getDistanceFrom (p);
            if (d < bestD) { bestD = d; best = i; }
        }
        return best;
    }

    void setHover (int h) { if (h != hover) { hover = h; repaint(); } }

    float value (const char* id) const { return proc.apvts.getRawParameterValue (prefix + id)->load(); }

    void setValue (const char* id, float v)
    {
        if (auto* p = proc.apvts.getParameter (prefix + id))
            p->setValueNotifyingHost (p->convertTo0to1 (v));
    }

    HypernovaAudioProcessor& proc;
    juce::String prefix;
    ThemeColour colour;
    bool showLive;
    int hover = -1, dragging = -1;
    juce::Point<float> dragStart;
    float startA = 0, startD = 0, startS = 0, startR = 0;
};

//==============================================================================
// Filter response computed from the same analog prototype the SVF models, following the live cutoff.
class FilterView : public juce::Component
{
public:
    explicit FilterView (HypernovaAudioProcessor& p) : proc (p) {}

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::inset);
        g.fillRoundedRectangle (r, 8.0f);
        auto area = r.reduced (6, 6);

        g.setColour (Colours::line);
        for (float f : { 100.0f, 1000.0f, 10000.0f })
        {
            const float x = area.getX() + area.getWidth() * (float) (std::log (f / 20.0) / std::log (1000.0));
            g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
        }

        const bool on = proc.apvts.getRawParameterValue ("fltOn")->load() > 0.5f;
        const int type = (int) proc.apvts.getRawParameterValue ("fltType")->load();
        const float res = proc.apvts.getRawParameterValue ("res")->load();
        const float fc = on ? proc.shownCutoff.load() : 20000.0f;
        const float k = 2.0f - 1.96f * res;

        using C = std::complex<float>;
        auto biquad = [] (C s, float kk) { return 1.0f / (s * s + kk * s + 1.0f); };
        juce::Path p;
        constexpr int n = 90;
        for (int i = 0; i <= n; ++i)
        {
            const float f = 20.0f * std::pow (1000.0f, (float) i / n);
            const C s (0.0f, f / juce::jmax (20.0f, fc));
            C h;
            switch (type)
            {
                case FLp12:  h = biquad (s, k); break;
                case FHp12:  h = s * s * biquad (s, k); break;
                case FBp:    h = s * biquad (s, k) * (0.5f + k * 0.5f); break;
                case FNotch: h = (s * s + 1.0f) * biquad (s, k); break;
                default:     h = biquad (s, 1.414f) * biquad (s, k); break;
            }
            if (! on) h = 1.0f;
            const float db = juce::jlimit (-36.0f, 24.0f, juce::Decibels::gainToDecibels (std::abs (h), -60.0f));
            const juce::Point<float> pt (area.getX() + area.getWidth() * (float) i / n, area.getY() + area.getHeight() * (1.0f - (db + 36.0f) / 60.0f));
            if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
        }
        juce::Path fill (p);
        fill.lineTo (area.getBottomRight());
        fill.lineTo (area.getBottomLeft());
        fill.closeSubPath();
        const auto c = on ? Palette::filter : Colours::textFaint;
        g.setGradientFill (juce::ColourGradient (c.withAlpha (0.25f), 0, area.getY(), c.withAlpha (0.0f), 0, area.getBottom(), false));
        g.fillPath (fill);
        glowStroke (g, p, c, 1.6f, on ? 0.8f : 0.2f);
    }

private:
    HypernovaAudioProcessor& proc;
};

//==============================================================================
class BipolarBar : public juce::Slider
{
public:
    explicit BipolarBar (ThemeColour c) : colour (c)
    {
        setSliderStyle (juce::Slider::LinearBar);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setVelocityBasedMode (false);
        setDoubleClickReturnValue (true, 0.0);
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0, 3);
        g.setColour (Colours::inset);
        g.fillRoundedRectangle (r, 5.0f);
        g.setColour (Colours::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
        const float mid = r.getCentreX();
        const float v = (float) getValue();
        const float x = r.getX() + r.getWidth() * (float) valueToProportionOfLength (v);
        auto fill = juce::Rectangle<float> (juce::jmin (mid, x), r.getY() + 3, std::abs (x - mid), r.getHeight() - 6);
        g.setColour (colour.withAlpha (std::abs (v) > 0.001f ? 0.85f : 0.0f));
        g.fillRoundedRectangle (fill, 2.5f);
        g.setColour (Colours::lineHi);
        g.drawVerticalLine ((int) mid, r.getY() + 2, r.getBottom() - 2);
        g.setColour (Colours::text.withAlpha (0.9f));
        g.setFont (mono (9.5f));
        const int pct = juce::roundToInt (v * 100.0f);
        g.drawText ((pct > 0 ? "+" : "") + juce::String (pct), r.reduced (6, 0), v < 0 ? juce::Justification::centredRight : juce::Justification::centredLeft, false);
    }

private:
    ThemeColour colour;
};

class ModRow : public juce::Component
{
public:
    ModRow (juce::AudioProcessorValueTreeState& state, int slot) : bar (Palette::mod)
    {
        const juce::String p = "mod" + juce::String (slot + 1);
        src.addItemList (modSrcNames(), 1);
        dest.addItemList (modDestNames(), 1);
        for (auto* c : { &src, &dest }) addAndMakeVisible (c);
        addAndMakeVisible (bar);
        srcAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, p + "Src", src);
        destAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, p + "Dest", dest);
        barAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, p + "Amt", bar);
        bar.setDoubleClickReturnValue (true, 0.0);
        bar.setTooltip ("Amount. Drag left for negative, double-click to zero.");
    }

    void resized() override
    {
        auto r = getLocalBounds();
        const int w = r.getWidth();
        src.setBounds (r.removeFromLeft (w * 30 / 100).reduced (0, 1));
        r.removeFromLeft (5);
        dest.setBounds (r.removeFromLeft (w * 37 / 100).reduced (0, 1));
        r.removeFromLeft (5);
        bar.setBounds (r);
    }

    juce::ComboBox src, dest;
    BipolarBar bar;

private:
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> srcAttach, destAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> barAttach;
};

} // namespace ab::ui
