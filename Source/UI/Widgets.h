#pragma once

#include "Components.h"

// Widgets: every panel of the synth is a widget that keeps its own look and contents. Widgets can be moved,
// resized (the contents scale, the aspect stays), collapsed to their title row, hidden, and stacked into tab
// groups. That only happens in layout mode: in normal use the knobs work and nothing moves.
//
// A widget's contents are laid out once at a design size, in the same coordinates the panel always used.
// Hiding or moving a widget never touches the sound; layouts are stored per user, apart from the patch.
namespace ab::ui
{

class Widget;

// The contents of a widget: a plain component whose painter draws the panel's own titles and captions in
// design coordinates, so they scale with the knobs.
class WidgetContent : public juce::Component
{
public:
    std::function<void (juce::Graphics&)> painter;
    void paint (juce::Graphics& g) override { if (painter) painter (g); }
};

// Tabs for a stack of widgets sharing one place (the old deck tabs).
class StackTabs : public juce::Component
{
public:
    std::function<void (int)> onSelect;

    void setTabs (juce::StringArray names, int active, ThemeColour c)
    {
        labels = std::move (names);
        current = active;
        colour = c;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (Colours::inset);
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (Colours::line);
        g.drawRoundedRectangle (r.reduced (0.5f), r.getHeight() * 0.5f, 1.0f);
        const int n = juce::jmax (1, labels.size());
        const float w = r.getWidth() / (float) n;
        for (int i = 0; i < labels.size(); ++i)
        {
            auto cell = juce::Rectangle<float> (r.getX() + w * (float) i, r.getY(), w, r.getHeight()).reduced (2.0f);
            if (i == current)
            {
                g.setColour (colour.withAlpha (0.16f));
                g.fillRoundedRectangle (cell, cell.getHeight() * 0.5f);
                g.setColour (colour.withAlpha (0.7f));
                g.drawRoundedRectangle (cell, cell.getHeight() * 0.5f, 1.0f);
            }
            g.setColour (i == current ? Colours::text : Colours::textDim);
            g.setFont (font (10.0f, true).withExtraKerningFactor (0.14f));
            g.drawFittedText (labels[i], cell.toNearestInt(), juce::Justification::centred, 1, 0.7f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (labels.isEmpty()) return;
        const int i = juce::jlimit (0, labels.size() - 1, (int) ((float) e.x / ((float) getWidth() / (float) labels.size())));
        if (i != current && onSelect) onSelect (i);
    }

private:
    juce::StringArray labels;
    int current = 0;
    ThemeColour colour { SlotMod };
};

class Widget : public juce::Component
{
public:
    // What the host can be asked to do from the widget's own chrome in layout mode.
    enum class Edit { MoveStart, Move, MoveEnd, ResizeStart, Resize, ResizeEnd };
    std::function<void (Widget&, Edit, juce::Point<int> canvasDelta)> onEdit;
    std::function<void (Widget&)> onCollapse, onHide, onUnstack;

    Widget (const juce::String& widgetId, const juce::String& widgetTitle, ThemeColour c, int designWidth, int designHeight,
            int titleOffsetX = 14, int headerHeight = 40)
        : id (widgetId), title (widgetTitle), colour (c), designW (designWidth), designH (designHeight),
          titleX (titleOffsetX), headerH (headerHeight)
    {
        addAndMakeVisible (content);
        content.setBounds (0, 0, designW, designH);
        content.addChildComponent (tabs);
        tabs.setBounds (12, 9, 500, 26);
        content.painter = [this] (juce::Graphics& g)
        {
            if (! tabs.isVisible()) sectionLabel (g, title, { (float) titleX, 10.0f, 260.0f, 24.0f }, colour);
            if (extraPaint) extraPaint (g);
        };
        setInterceptsMouseClicks (true, true);
    }

    const juce::String id, title;
    ThemeColour colour;
    const int designW, designH, titleX, headerH;
    WidgetContent content;
    StackTabs tabs;
    std::function<void (juce::Graphics&)> extraPaint; // the panel's own captions and dividers, design coordinates
    juce::String stack;                               // widgets with the same stack share a place, one shown
    bool collapsed = false;
    bool shown = true;                                // part of the layout (hidden widgets can be added back)
    bool stackActive = true;                          // the member of its stack currently in front

    float scale() const { return (float) getWidth() / (float) designW; }
    int heightForWidth (int w) const
    {
        const float k = (float) w / (float) designW;
        return juce::roundToInt ((float) (collapsed ? headerH : designH) * k);
    }
    int minWidth() const { return juce::roundToInt ((float) designW * 0.6f); }
    int maxWidth() const { return juce::roundToInt ((float) designW * 1.6f); }

    void setEditing (bool e)
    {
        editing = e;
        // In layout mode the controls are locked, so dragging anywhere moves the widget and nothing else.
        content.setInterceptsMouseClicks (! e, ! e);
        setMouseCursor (e ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
    bool isEditing() const { return editing; }
    void setInvalidDrop (bool b) { if (b != invalidDrop) { invalidDrop = b; repaint(); } }

    void resized() override
    {
        content.setTransform (juce::AffineTransform::scale (scale()));
        content.setVisible (true);
    }

    void paint (juce::Graphics& g) override
    {
        const float k = scale();
        panel (g, getLocalBounds().toFloat(), 14.0f * k);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        if (! editing) return;
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const auto accent = invalidDrop ? juce::Colour (0xffe8363d) : Colours::accent.get();
        g.setColour (accent.withAlpha (0.06f));
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (accent.withAlpha (0.85f));
        juce::Path outline;
        outline.addRoundedRectangle (r, 12.0f);
        const float dashes[] = { 6.0f, 4.0f };
        juce::PathStrokeType (1.5f).createDashedStroke (outline, outline, dashes, 2);
        g.fillPath (outline);

        // Header chrome covers the widget's own title row (its controls are locked in layout mode anyway):
        // grip, title, then collapse / unstack / hide buttons on the right.
        auto header = getLocalBounds().removeFromTop (juce::jmin (getHeight(), juce::jmax (30, juce::roundToInt ((float) headerH * scale()) - 4)));
        {
            juce::Path top;
            top.addRoundedRectangle ((float) header.getX() + 2.0f, (float) header.getY() + 2.0f, (float) header.getWidth() - 4.0f, (float) header.getHeight() - 2.0f,
                                     12.0f, 12.0f, true, true, collapsed, collapsed);
            g.setColour (Colours::bg0);
            g.fillPath (top);
            g.setColour (accent.withAlpha (0.14f));
            g.fillPath (top);
        }
        g.setColour (Colours::text);
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 2; ++j)
                g.fillEllipse ((float) header.getX() + 14.0f + (float) i * 5.0f, (float) header.getCentreY() - 4.0f + (float) j * 6.0f, 2.4f, 2.4f);
        g.setFont (mono (11.0f).boldened());
        g.drawText (stack.isNotEmpty() ? title + "  +TABS" : title, header.withTrimmedLeft (36).withTrimmedRight (90), juce::Justification::centredLeft, true);
        drawButton (g, collapseButton(), collapsed ? "+" : "-");
        if (stack.isNotEmpty()) drawButton (g, unstackButton(), ">");
        drawButton (g, hideButton(), "x");

        // Resize grip, bottom right.
        if (! collapsed)
        {
            auto grip = resizeGrip().toFloat();
            g.setColour (Colours::text.withAlpha (0.8f));
            for (int i = 0; i < 3; ++i)
                g.drawLine (grip.getRight() - 3.0f - (float) i * 4.0f, grip.getBottom() - 3.0f, grip.getRight() - 3.0f, grip.getBottom() - 3.0f - (float) i * 4.0f, 1.3f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! editing) return;
        const auto p = e.getPosition();
        if (hideButton().contains (p))           { if (onHide) onHide (*this); return; }
        if (collapseButton().contains (p))       { if (onCollapse) onCollapse (*this); return; }
        if (stack.isNotEmpty() && unstackButton().contains (p)) { if (onUnstack) onUnstack (*this); return; }
        resizing = ! collapsed && resizeGrip().contains (p);
        dragOrigin = e.getScreenPosition();
        if (onEdit) onEdit (*this, resizing ? Edit::ResizeStart : Edit::MoveStart, {});
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! editing || ! onEdit) return;
        onEdit (*this, resizing ? Edit::Resize : Edit::Move, toCanvasDelta (e.getScreenPosition() - dragOrigin));
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! editing || ! onEdit) return;
        onEdit (*this, resizing ? Edit::ResizeEnd : Edit::MoveEnd, toCanvasDelta (e.getScreenPosition() - dragOrigin));
        resizing = false;
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        if (! editing) return;
        const bool onGrip = ! collapsed && resizeGrip().contains (e.getPosition());
        const bool onButton = hideButton().contains (e.getPosition()) || collapseButton().contains (e.getPosition())
                              || (stack.isNotEmpty() && unstackButton().contains (e.getPosition()));
        setMouseCursor (onGrip ? juce::MouseCursor::BottomRightCornerResizeCursor
                               : onButton ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::DraggingHandCursor);
    }

private:
    bool editing = false, resizing = false, invalidDrop = false;
    juce::Point<int> dragOrigin;

    juce::Rectangle<int> headerButton (int indexFromRight) const
    {
        const int h = juce::jmin (getHeight(), juce::jmax (30, juce::roundToInt ((float) headerH * scale()) - 4));
        return { getWidth() - 30 - indexFromRight * 28, (h - 22) / 2 + 1, 24, 22 };
    }
    juce::Rectangle<int> hideButton() const     { return headerButton (0); }
    juce::Rectangle<int> collapseButton() const { return headerButton (1); }
    juce::Rectangle<int> unstackButton() const  { return headerButton (2); }
    juce::Rectangle<int> resizeGrip() const     { return { getWidth() - 20, getHeight() - 20, 20, 20 }; }

    void drawButton (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& glyph) const
    {
        g.setColour (Colours::panelHi);
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
        g.setColour (Colours::lineHi);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 5.0f, 1.0f);
        g.setColour (Colours::text);
        g.setFont (mono (12.0f).boldened());
        g.drawText (glyph, r, juce::Justification::centred, false);
    }

    // Screen-space mouse travel in the canvas's own (unscaled) units.
    juce::Point<int> toCanvasDelta (juce::Point<int> screenDelta) const
    {
        const float k = getParentComponent() != nullptr ? juce::Component::getApproximateScaleFactorForComponent (getParentComponent()) : 1.0f;
        return { juce::roundToInt ((float) screenDelta.x / k), juce::roundToInt ((float) screenDelta.y / k) };
    }
};

//==============================================================================
// Named workspaces, saved per user next to the look settings. A workspace is only an arrangement of widgets:
// loading one never touches the sound.
struct WorkspaceStore
{
    static juce::PropertiesFile::Options options()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "workspaces";
        o.filenameSuffix = ".xml";
        o.folderName = "Arrow/Hypernova";
        // Tests point this somewhere scratch so they never touch the user's own workspaces.
        const auto testFolder = juce::SystemStats::getEnvironmentVariable ("HYPERNOVA_WORKSPACE_FOLDER", {});
        if (testFolder.isNotEmpty()) o.folderName = testFolder;
        o.osxLibrarySubFolder = "Application Support";
        o.storageFormat = juce::PropertiesFile::storeAsXML;
        return o;
    }

    static juce::StringArray builtIn() { return { "Sound Design", "Effects" }; }

    static juce::String current()
    {
        juce::PropertiesFile p (options());
        return p.getValue ("current", "Sound Design");
    }

    static void setCurrent (const juce::String& name)
    {
        juce::PropertiesFile p (options());
        p.setValue ("current", name);
        p.saveIfNeeded();
    }

    static juce::StringArray names()
    {
        auto all = builtIn();
        juce::PropertiesFile p (options());
        for (auto& key : p.getAllProperties().getAllKeys())
            if (key.startsWith ("ws:")) all.addIfNotAlreadyThere (key.fromFirstOccurrenceOf ("ws:", false, false));
        return all;
    }

    static juce::ValueTree load (const juce::String& name)
    {
        juce::PropertiesFile p (options());
        if (auto xml = juce::parseXML (p.getValue ("ws:" + name)))
            return juce::ValueTree::fromXml (*xml);
        return {};
    }

    static void save (const juce::String& name, const juce::ValueTree& layout)
    {
        juce::PropertiesFile p (options());
        p.setValue ("ws:" + name, layout.toXmlString());
        p.saveIfNeeded();
    }

    static void remove (const juce::String& name)
    {
        juce::PropertiesFile p (options());
        p.removeValue ("ws:" + name);
        p.saveIfNeeded();
    }
};

} // namespace ab::ui
