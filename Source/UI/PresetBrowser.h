#pragma once

#include "Components.h"

// Full preset browser: categories and packs on the left, search on top, a list you can arrow through
// (each sound loads shortly after you land on it), stars for favourites, and the save/share actions at the bottom.
// The component covers the editor below the top bar: a dimmed backdrop with the panel inset in it, so a click
// on the backdrop closes the browser.
namespace ab::ui
{

class PresetBrowser : public juce::Component, private juce::ListBoxModel, private juce::TextEditor::Listener,
                      private juce::KeyListener, private juce::Timer
{
public:
    std::function<void()> onClose, onSave, onExport, onImport;

    explicit PresetBrowser (HypernovaAudioProcessor& p) : proc (p)
    {
        search.setTextToShowWhenEmpty ("Search sounds, categories, packs...", Colours::textFaint);
        search.setFont (font (15.0f));
        search.setColour (juce::TextEditor::backgroundColourId, Colours::inset);
        search.setColour (juce::TextEditor::outlineColourId, Colours::line);
        search.setColour (juce::TextEditor::focusedOutlineColourId, Colours::accent.withAlpha (0.6f));
        search.setColour (juce::TextEditor::textColourId, Colours::text);
        search.setIndents (12, 8);
        search.addListener (this);
        search.addKeyListener (this);
        addAndMakeVisible (search);

        list.setModel (this);
        list.setRowHeight (30);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        list.getViewport()->setScrollBarThickness (8);
        list.addMouseListener (this, true);
        list.addKeyListener (this);
        addAndMakeVisible (list);

        groupList.setModel (&groupModel);
        groupList.setRowHeight (27);
        groupList.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        groupList.getViewport()->setScrollBarThickness (6);
        groupList.addMouseListener (this, true);
        addAndMakeVisible (groupList);

        for (auto* b : { &saveB, &exportB, &importB, &folderB, &closeB })
        {
            b->setColour (juce::TextButton::buttonOnColourId, Colours::accent);
            b->setMouseCursor (juce::MouseCursor::PointingHandCursor);
            addAndMakeVisible (b);
        }
        saveB.onClick = [this] { if (onSave) onSave(); };
        exportB.onClick = [this] { if (onExport) onExport(); };
        importB.onClick = [this] { if (onImport) onImport(); };
        folderB.onClick = [] { HypernovaAudioProcessor::userPresetFolder().createDirectory(); HypernovaAudioProcessor::userPresetFolder().revealToUser(); };
        closeB.onClick = [this] { close(); };
        loadFavourites();
    }

    // Rebuilds the catalogue (factory, installed packs, the user's own and imported sounds).
    void refresh()
    {
        entries.clear();
        const auto& f = factoryPresets();
        for (int i = 1; i < (int) f.size(); ++i)
            entries.push_back ({ f[(size_t) i].name, f[(size_t) i].category, "Factory", i, {} });
        for (const auto& file : HypernovaAudioProcessor::presetFilesIn (HypernovaAudioProcessor::packFolder()))
            entries.push_back ({ file.getFileNameWithoutExtension(), file.getParentDirectory().getFileName(), "Pack", -1, file });
        for (const auto& file : HypernovaAudioProcessor::presetFilesIn (HypernovaAudioProcessor::userPresetFolder()))
        {
            const bool top = file.getParentDirectory() == HypernovaAudioProcessor::userPresetFolder();
            entries.push_back ({ file.getFileNameWithoutExtension(), top ? juce::String ("My Sounds") : file.getParentDirectory().getFileName(), "User", -1, file });
        }

        const auto previous = groups[group];
        groups.clear();
        groups.add ("All sounds");
        groups.add ("Favourites");
        for (const auto& e : entries)
            if (e.source == "Factory") groups.addIfNotAlreadyThere (e.category);
        for (const auto& e : entries)
            if (e.source != "Factory") groups.addIfNotAlreadyThere (e.category);
        group = juce::jmax (0, groups.indexOf (previous));
        counts.clearQuick();
        for (int i = 0; i < groups.size(); ++i) counts.add (countIn (i));
        groupList.updateContent();
        groupList.repaint();
        applyFilter();
    }

    // Called when the browser opens: lands on the sound that is playing, without reloading it.
    void showCurrent()
    {
        pendingRow = -1;
        const auto name = proc.getPresetName();
        auto rowOf = [&] { for (int r = 0; r < (int) shown.size(); ++r) if (entries[(size_t) shown[(size_t) r]].name == name) return r; return -1; };
        int row = rowOf();
        if (row < 0 && group != 0 && search.isEmpty()) { group = 0; groupList.repaint(); applyFilter(); row = rowOf(); }
        if (row >= 0)
        {
            const juce::ScopedValueSetter<bool> quiet (suppressLoad, true);
            list.selectRow (row, true, true);
            // centre it rather than pinning it to the top edge
            const int visibleRows = juce::jmax (1, list.getHeight() / list.getRowHeight());
            list.scrollToEnsureRowIsOnscreen (juce::jmax (0, row - visibleRows / 2));
            list.scrollToEnsureRowIsOnscreen (juce::jmin ((int) shown.size() - 1, row + visibleRows / 2));
        }
        groupList.scrollToEnsureRowIsOnscreen (group);
    }

    void grabSearchFocus() { search.grabKeyboardFocus(); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (ThemeState::get().base.light ? Colours::bg0.withAlpha (0.7f) : juce::Colours::black.withAlpha (0.45f)); // backdrop over the editor
        auto r = panelBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (r.translated (0, 6), 18.0f);
        panel (g, r, 18.0f, Colours::panelHi.withAlpha (0.97f));
        sectionLabel (g, "SOUNDS", { r.getX() + 20, r.getY() + 16, 200, 24 }, Colours::text);

        g.setColour (Colours::textFaint);
        g.setFont (font (11.0f));
        g.drawText (juce::String ((int) shown.size()) + (shown.size() == 1 ? " sound" : " sounds")
                        + "   /   arrow keys browse, Enter or double-click loads and closes, click a star to favourite",
                    juce::Rectangle<float> ((float) list.getX(), (float) list.getBottom() + 8, (float) list.getWidth(), 18), juce::Justification::centredLeft, true);

        if (shown.empty())
        {
            const auto q = search.getText().trim();
            g.setColour (Colours::textDim);
            g.setFont (font (15.0f));
            g.drawText (groups[group] == "Favourites" && q.isEmpty() ? "No favourites yet: click the star next to any sound."
                                                                        : "No sounds match \"" + q + "\" here.",
                        list.getBounds().withHeight (80), juce::Justification::centred, true);
        }
    }

    void resized() override
    {
        auto r = panelBounds().reduced (20, 16);
        auto top = r.removeFromTop (40);
        closeB.setBounds (top.removeFromRight (80).reduced (0, 4));
        top.removeFromRight (10);
        search.setBounds (top.withTrimmedLeft (230).reduced (0, 2));
        r.removeFromTop (12);
        auto bottom = r.removeFromBottom (40);
        for (auto* b : { &saveB, &exportB, &importB, &folderB })
        {
            b->setBounds (bottom.removeFromLeft (170).reduced (0, 4));
            bottom.removeFromLeft (10);
        }
        r.removeFromBottom (30);
        groupList.setBounds (r.removeFromLeft (216));
        r.removeFromLeft (14);
        list.setBounds (r);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.eventComponent == this && ! panelBounds().contains (e.getPosition())) close();
    }

    void mouseMove (const juce::MouseEvent& e) override { trackHover (e); }
    void mouseDrag (const juce::MouseEvent& e) override { trackHover (e); }
    void mouseExit (const juce::MouseEvent& e) override { trackHover (e); }

    bool keyPressed (const juce::KeyPress& k) override { return handleKey (k); }

private:
    struct Entry { juce::String name, category, source; int factoryIndex; juce::File file; };

    // Category / pack column.
    struct GroupModel : juce::ListBoxModel
    {
        PresetBrowser& b;
        explicit GroupModel (PresetBrowser& owner) : b (owner) {}
        int getNumRows() override { return b.groups.size(); }
        void paintListBoxItem (int i, juce::Graphics& g, int w, int h, bool) override
        {
            if (! juce::isPositiveAndBelow (i, b.groups.size())) return;
            auto row = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (2, 1.5f);
            const bool sel = i == b.group, over = i == b.hoverGroup;
            if (sel || over)
            {
                g.setColour (Colours::accent.withAlpha (sel ? 0.14f : 0.06f));
                g.fillRoundedRectangle (row, 8.0f);
            }
            if (sel)
            {
                g.setColour (Colours::accent.withAlpha (0.6f));
                g.drawRoundedRectangle (row.reduced (0.5f), 8.0f, 1.0f);
            }
            g.setColour (sel ? Colours::text : over ? Colours::text.withAlpha (0.9f) : Colours::textDim);
            g.setFont (font (13.0f, sel));
            g.drawText (b.groups[i], row.reduced (12, 0), juce::Justification::centredLeft, true);
            g.setColour (Colours::textFaint);
            g.setFont (mono (10.5f));
            g.drawText (juce::String (b.counts[i]), row.reduced (10, 0), juce::Justification::centredRight, false);
        }
        void listBoxItemClicked (int i, const juce::MouseEvent&) override
        {
            if (! juce::isPositiveAndBelow (i, b.groups.size()) || i == b.group) return;
            b.group = i;
            b.groupList.repaint();
            b.applyFilter();
            b.reselectCurrent();
        }
    };

    HypernovaAudioProcessor& proc;
    juce::TextEditor search;
    juce::ListBox list;
    GroupModel groupModel { *this };
    juce::ListBox groupList;
    juce::TextButton saveB { "SAVE SOUND" }, exportB { "EXPORT TO SHARE" }, importB { "IMPORT" }, folderB { "SHOW FOLDER" }, closeB { "CLOSE" };
    std::vector<Entry> entries;
    std::vector<int> shown;
    juce::StringArray groups;
    juce::Array<int> counts;
    int group = 0, hoverRow = -1, hoverGroup = -1, pendingRow = -1;
    bool suppressLoad = false;
    juce::StringArray favourites;

    static constexpr int starWidth = 44;

    juce::Rectangle<int> panelBounds() const { return getLocalBounds().reduced (24, 0).withTrimmedBottom (88); }

    void close()
    {
        if (pendingRow >= 0) timerCallback(); // don't drop a sound that was about to load
        if (onClose) onClose();
    }

    static juce::File favouritesFile() { return HypernovaAudioProcessor::userPresetFolder().getParentDirectory().getChildFile ("favourites.txt"); }
    juce::String keyFor (const Entry& e) const { return e.source == "Factory" ? "factory:" + e.name : "file:" + e.file.getFullPathName(); }
    void loadFavourites() { favourites.clear(); favourites.addLines (favouritesFile().loadFileAsString()); favourites.removeEmptyStrings(); }
    void saveFavourites() const { favouritesFile().getParentDirectory().createDirectory(); favouritesFile().replaceWithText (favourites.joinIntoString ("\n")); }

    bool matches (const Entry& e) const
    {
        const auto g = groups[group];
        if (g == "Favourites") { if (! favourites.contains (keyFor (e))) return false; }
        else if (g != "All sounds" && e.category != g) return false;
        const auto q = search.getText().trim();
        if (q.isEmpty()) return true;
        for (const auto& word : juce::StringArray::fromTokens (q, " ", ""))
            if (! (e.name.containsIgnoreCase (word) || e.category.containsIgnoreCase (word) || e.source.containsIgnoreCase (word))) return false;
        return true;
    }

    int countIn (int g) const
    {
        const auto name = groups[g];
        int n = 0;
        for (const auto& e : entries)
            if (name == "All sounds" || (name == "Favourites" ? favourites.contains (keyFor (e)) : e.category == name)) ++n;
        return n;
    }

    void applyFilter()
    {
        shown.clear();
        for (int i = 0; i < (int) entries.size(); ++i)
            if (matches (entries[(size_t) i])) shown.push_back (i);
        hoverRow = -1;
        list.updateContent();
        list.deselectAllRows();
        list.getViewport()->setViewPosition (0, 0);
        list.repaint();
        repaint();
    }

    // After the list changes, keep the playing sound highlighted if it is in view (without reloading it).
    void reselectCurrent()
    {
        const auto name = proc.getPresetName();
        for (int r = 0; r < (int) shown.size(); ++r)
            if (entries[(size_t) shown[(size_t) r]].name == name)
            {
                const juce::ScopedValueSetter<bool> quiet (suppressLoad, true);
                list.selectRow (r, false, true);
                return;
            }
    }

    void trackHover (const juce::MouseEvent& e)
    {
        const auto inList = e.getEventRelativeTo (&list).getPosition();
        const int row = list.getLocalBounds().contains (inList) ? list.getRowContainingPosition (inList.x, inList.y) : -1;
        if (row != hoverRow)
        {
            if (hoverRow >= 0) list.repaintRow (hoverRow);
            hoverRow = row;
            if (hoverRow >= 0) list.repaintRow (hoverRow);
        }
        const auto inGroups = e.getEventRelativeTo (&groupList).getPosition();
        const int g = groupList.getLocalBounds().contains (inGroups) ? groupList.getRowContainingPosition (inGroups.x, inGroups.y) : -1;
        if (g != hoverGroup)
        {
            hoverGroup = g;
            groupList.repaint();
        }
        const bool overStar = hoverRow >= 0 && inList.x > list.getWidth() - starWidth - list.getViewport()->getScrollBarThickness();
        list.setMouseCursor (overStar ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }

    bool handleKey (const juce::KeyPress& k)
    {
        if (k == juce::KeyPress::escapeKey)
        {
            if (search.hasKeyboardFocus (false) && ! search.isEmpty()) { search.clear(); applyFilter(); reselectCurrent(); }
            else close();
            return true;
        }
        if (k == juce::KeyPress::returnKey)
        {
            if (list.getSelectedRow() < 0 && ! shown.empty()) list.selectRow (0);
            close();
            return true;
        }
        const bool down = k == juce::KeyPress::downKey, up = k == juce::KeyPress::upKey;
        const bool pageDown = k == juce::KeyPress::pageDownKey, pageUp = k == juce::KeyPress::pageUpKey;
        if (down || up || pageDown || pageUp)
        {
            const int n = (int) shown.size();
            if (n == 0) return true;
            const int page = juce::jmax (1, list.getHeight() / list.getRowHeight() - 1);
            const int step = down ? 1 : up ? -1 : pageDown ? page : -page;
            const int cur = list.getSelectedRow();
            const int row = cur < 0 ? (step > 0 ? 0 : n - 1) : juce::jlimit (0, n - 1, cur + step);
            list.selectRow (row);
            return true;
        }
        return false;
    }

    bool keyPressed (const juce::KeyPress& k, juce::Component*) override { return handleKey (k); }

    void textEditorTextChanged (juce::TextEditor&) override { applyFilter(); reselectCurrent(); }

    int getNumRows() override { return (int) shown.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, (int) shown.size())) return;
        const auto& e = entries[(size_t) shown[(size_t) row]];
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (2, 1);
        const bool current = e.name == proc.getPresetName();
        const bool over = row == hoverRow;
        if (selected || current || over)
        {
            g.setColour ((selected || current ? Colours::accent : Colours::text).withAlpha (selected ? 0.16f : current ? 0.1f : 0.05f));
            g.fillRoundedRectangle (r, 7.0f);
        }
        if (current)
        {
            g.setColour (Colours::accent);
            g.fillRoundedRectangle (r.withWidth (3.0f).reduced (0, 7), 1.5f);
        }
        const bool fav = favourites.contains (keyFor (e));
        auto star = r.removeFromRight ((float) starWidth - 10).withSizeKeepingCentre (14, 14);
        juce::Path sp;
        sp.addStar (star.getCentre(), 5, 3.2f, 7.5f);
        g.setColour (fav ? Colours::warm : Colours::textFaint.withAlpha (over ? 0.9f : 0.45f));
        if (fav) g.fillPath (sp); else g.strokePath (sp, juce::PathStrokeType (1.1f));
        g.setColour (Colours::textDim);
        g.setFont (font (11.0f));
        g.drawText (e.category + (e.source == "Factory" ? "" : "  /  " + e.source), r.removeFromRight (220), juce::Justification::centredRight, true);
        g.setColour (selected || current ? Colours::text : Colours::text.withAlpha (0.85f));
        g.setFont (font (14.0f, selected || current));
        g.drawText (e.name, r.reduced (14, 0), juce::Justification::centredLeft, true);
    }

    bool clickIsOnStar (const juce::MouseEvent& e)
    {
        return e.getEventRelativeTo (&list).x > list.getWidth() - starWidth - list.getViewport()->getScrollBarThickness();
    }

    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        if (! clickIsOnStar (e) || ! juce::isPositiveAndBelow (row, (int) shown.size())) return;
        const auto key = keyFor (entries[(size_t) shown[(size_t) row]]);
        if (favourites.contains (key)) favourites.removeString (key); else favourites.add (key);
        saveFavourites();
        counts.set (1, countIn (1));
        groupList.repaintRow (1);
        list.repaintRow (row);
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent& e) override
    {
        if (clickIsOnStar (e)) return;
        pendingRow = row;
        close();
    }

    void returnKeyPressed (int) override { close(); }

    // Audition as you browse, but only once the selection settles: holding an arrow key skims the list
    // without loading (and re-voicing) every sound it passes.
    void selectedRowsChanged (int row) override
    {
        if (suppressLoad || row < 0) return;
        if (juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown() && list.isMouseOver (true)
            && list.getMouseXYRelative().x > list.getWidth() - starWidth - list.getViewport()->getScrollBarThickness())
        {
            // a click on a star selects the row too; put the selection back instead of loading that sound
            const juce::ScopedValueSetter<bool> quiet (suppressLoad, true);
            list.deselectAllRows();
            reselectCurrent();
            return;
        }
        pendingRow = row;
        startTimer (70);
    }

    void timerCallback() override
    {
        stopTimer();
        const int row = pendingRow;
        pendingRow = -1;
        if (! juce::isPositiveAndBelow (row, (int) shown.size())) return;
        const auto& e = entries[(size_t) shown[(size_t) row]];
        if (e.source == "Factory") proc.loadFactoryPreset (e.factoryIndex);
        else proc.loadUserPreset (e.file);
        list.repaint();
    }
};

} // namespace ab::ui
