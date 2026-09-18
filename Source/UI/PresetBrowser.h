#pragma once

#include "Components.h"

// Full preset browser: categories and packs on the left, search on top, a list you can arrow through
// (each sound loads as you land on it), stars for favourites, and the save/share actions at the bottom.
namespace ab::ui
{

class PresetBrowser : public juce::Component, private juce::ListBoxModel, private juce::TextEditor::Listener
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
        addAndMakeVisible (search);

        list.setModel (this);
        list.setRowHeight (30);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        list.getViewport()->setScrollBarThickness (8);
        addAndMakeVisible (list);

        for (auto* b : { &saveB, &exportB, &importB, &folderB, &closeB })
        {
            b->setColour (juce::TextButton::buttonOnColourId, Colours::accent);
            addAndMakeVisible (b);
        }
        saveB.onClick = [this] { if (onSave) onSave(); };
        exportB.onClick = [this] { if (onExport) onExport(); };
        importB.onClick = [this] { if (onImport) onImport(); };
        folderB.onClick = [] { HypernovaAudioProcessor::userPresetFolder().createDirectory(); HypernovaAudioProcessor::userPresetFolder().revealToUser(); };
        closeB.onClick = [this] { if (onClose) onClose(); };
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

        groups.clear();
        groups.add ("All sounds");
        groups.add ("Favourites");
        for (const auto& e : entries)
            if (e.source == "Factory") groups.addIfNotAlreadyThere (e.category);
        for (const auto& e : entries)
            if (e.source != "Factory") groups.addIfNotAlreadyThere (e.category);
        applyFilter();
    }

    void grabSearchFocus() { search.grabKeyboardFocus(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (r.translated (0, 6), 18.0f);
        panel (g, r, 18.0f, juce::Colour (0xf00a0c19));
        sectionLabel (g, "SOUNDS", { r.getX() + 20, r.getY() + 16, 200, 24 }, Colours::text);

        // Group list
        auto col = groupArea();
        for (int i = 0; i < groups.size(); ++i)
        {
            auto row = juce::Rectangle<float> ((float) col.getX(), (float) col.getY() + i * 26.0f, (float) col.getWidth(), 24.0f);
            if (row.getBottom() > (float) col.getBottom()) break;
            const bool sel = i == group;
            if (sel)
            {
                g.setColour (Colours::accent.withAlpha (0.14f));
                g.fillRoundedRectangle (row, 8.0f);
                g.setColour (Colours::accent.withAlpha (0.6f));
                g.drawRoundedRectangle (row.reduced (0.5f), 8.0f, 1.0f);
            }
            g.setColour (sel ? Colours::text : Colours::textDim);
            g.setFont (font (13.0f, sel));
            g.drawText (groups[i], row.reduced (12, 0), juce::Justification::centredLeft, true);
            g.setColour (Colours::textFaint);
            g.setFont (mono (10.5f));
            g.drawText (juce::String (countIn (i)), row.reduced (10, 0), juce::Justification::centredRight, false);
        }
        g.setColour (Colours::textFaint);
        g.setFont (font (11.0f));
        g.drawText (juce::String ((int) shown.size()) + " sounds   /   arrow keys browse, double-click or Enter loads and closes, click the star to favourite",
                    juce::Rectangle<float> ((float) list.getX(), (float) list.getBottom() + 8, (float) list.getWidth(), 18), juce::Justification::centredLeft, true);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (20, 16);
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
        groupBounds = r.removeFromLeft (216);
        r.removeFromLeft (14);
        list.setBounds (r);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto col = groupArea();
        if (! col.contains (e.getPosition())) return;
        const int i = (e.y - col.getY()) / 26;
        if (i >= 0 && i < groups.size()) { group = i; applyFilter(); repaint(); }
    }

    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey) { if (onClose) onClose(); return true; }
        if (k == juce::KeyPress::returnKey) { if (onClose) onClose(); return true; }
        if (k == juce::KeyPress::downKey || k == juce::KeyPress::upKey)
        {
            const int n = (int) shown.size();
            if (n == 0) return true;
            int row = list.getSelectedRow();
            row = juce::jlimit (0, n - 1, row + (k == juce::KeyPress::downKey ? 1 : -1));
            list.selectRow (row);
            return true;
        }
        return false;
    }

private:
    struct Entry { juce::String name, category, source; int factoryIndex; juce::File file; };

    HypernovaAudioProcessor& proc;
    juce::TextEditor search;
    juce::ListBox list;
    juce::TextButton saveB { "SAVE SOUND" }, exportB { "EXPORT TO SHARE" }, importB { "IMPORT" }, folderB { "SHOW FOLDER" }, closeB { "CLOSE" };
    std::vector<Entry> entries;
    std::vector<int> shown;
    juce::StringArray groups;
    int group = 0;
    juce::Rectangle<int> groupBounds;
    juce::StringArray favourites;

    juce::Rectangle<int> groupArea() const { return groupBounds; }

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
            if (! (e.name.containsIgnoreCase (word) || e.category.containsIgnoreCase (word))) return false;
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
        list.updateContent();
        list.deselectAllRows();
        list.repaint();
    }

    void textEditorTextChanged (juce::TextEditor&) override { applyFilter(); repaint(); }
    void textEditorReturnKeyPressed (juce::TextEditor&) override { if (! shown.empty()) list.selectRow (0); }

    int getNumRows() override { return (int) shown.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, (int) shown.size())) return;
        const auto& e = entries[(size_t) shown[(size_t) row]];
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (2, 1);
        const bool current = e.name == proc.getPresetName();
        if (selected || current)
        {
            g.setColour ((selected ? Colours::accent : Colours::accent2).withAlpha (0.14f));
            g.fillRoundedRectangle (r, 7.0f);
        }
        const bool fav = favourites.contains (keyFor (e));
        auto star = r.removeFromRight (34).withSizeKeepingCentre (14, 14);
        juce::Path sp;
        sp.addStar (star.getCentre(), 5, 3.2f, 7.5f);
        g.setColour (fav ? Colours::warm : Colours::textFaint.withAlpha (0.6f));
        if (fav) g.fillPath (sp); else g.strokePath (sp, juce::PathStrokeType (1.1f));
        g.setColour (Colours::textDim);
        g.setFont (font (11.0f));
        g.drawText (e.category + (e.source == "Factory" ? "" : "  /  " + e.source), r.removeFromRight (220), juce::Justification::centredRight, true);
        g.setColour (selected || current ? Colours::text : Colours::text.withAlpha (0.85f));
        g.setFont (font (14.0f, selected || current));
        g.drawText (e.name, r.reduced (12, 0), juce::Justification::centredLeft, true);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent& e) override
    {
        if (e.x > list.getWidth() - 44 && juce::isPositiveAndBelow (row, (int) shown.size()))
        {
            const auto key = keyFor (entries[(size_t) shown[(size_t) row]]);
            if (favourites.contains (key)) favourites.removeString (key); else favourites.add (key);
            saveFavourites();
            list.repaintRow (row);
            repaint();
        }
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override { load (row); if (onClose) onClose(); }
    void selectedRowsChanged (int row) override { load (row); } // audition as you browse
    void returnKeyPressed (int row) override { load (row); if (onClose) onClose(); }

    void load (int row)
    {
        if (! juce::isPositiveAndBelow (row, (int) shown.size())) return;
        const auto& e = entries[(size_t) shown[(size_t) row]];
        if (e.source == "Factory") proc.loadFactoryPreset (e.factoryIndex);
        else proc.loadUserPreset (e.file);
        list.repaint();
    }
};

} // namespace ab::ui
