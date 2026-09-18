#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Style.h"

// In-app updates. At most once a day (shared by every open instance through a small settings file) a background
// thread asks GitHub for the latest Hypernova release. If it is newer than this build, the editor shows a banner;
// one click downloads the notarized DMG to ~/Downloads and opens it, which brings up the branded installer.
// Plug-ins live in /Library and need an admin password to replace, so the installer does the actual swap.
namespace ab::ui
{

struct UpdateInfo
{
    juce::String version, dmgUrl, pageUrl;
    bool valid() const { return version.isNotEmpty() && dmgUrl.isNotEmpty(); }
};

class Updater : private juce::Thread, private juce::URL::DownloadTaskListener
{
public:
    static constexpr const char* repo = "erinuckuzular-ai/Hypernova";

    std::function<void (const UpdateInfo&)> onAvailable;   // message thread
    std::function<void (float)> onProgress;                 // message thread, 0..1
    std::function<void (bool ok, juce::String)> onFinished; // message thread

    Updater() : juce::Thread ("Hypernova update check") {}
    ~Updater() override
    {
        masterReference.clear();
        download.reset();
        stopThread (4000);
    }

    static juce::PropertiesFile::Options options()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "updates";
        o.filenameSuffix = ".xml";
        o.folderName = "Arrow/Hypernova";
        o.osxLibrarySubFolder = "Application Support";
        o.storageFormat = juce::PropertiesFile::storeAsXML;
        o.processLock = &lock();
        return o;
    }

    static bool autoCheckEnabled() { juce::PropertiesFile p (options()); return p.getBoolValue ("autoCheck", true); }
    static void setAutoCheck (bool on) { juce::PropertiesFile p (options()); p.setValue ("autoCheck", on); p.saveIfNeeded(); }

    static bool isNewer (const juce::String& candidate, const juce::String& current)
    {
        auto parts = [] (juce::String v)
        {
            v = v.trim().trimCharactersAtStart ("vV");
            juce::Array<int> n;
            for (auto& t : juce::StringArray::fromTokens (v, ".-", "")) if (t.containsOnly ("0123456789")) n.add (t.getIntValue());
            while (n.size() < 3) n.add (0);
            return n;
        };
        const auto a = parts (candidate), b = parts (current);
        for (int i = 0; i < juce::jmin (a.size(), b.size()); ++i)
            if (a[i] != b[i]) return a[i] > b[i];
        return false;
    }

    // Shows a cached result straight away, then refreshes from GitHub if the last check is over a day old
    // (or always, when the user asked).
    void check (bool userAsked)
    {
        manual = userAsked;
        {
            juce::PropertiesFile p (options());
            UpdateInfo cached { p.getValue ("latestVersion"), p.getValue ("latestDmg"), p.getValue ("latestPage") };
            if (! userAsked && cached.valid() && isNewer (cached.version, currentVersion())
                && p.getValue ("skipped") != cached.version && onAvailable)
                onAvailable (cached);
            const auto last = juce::Time (p.getValue ("lastCheck", "0").getLargeIntValue());
            if (! userAsked && (! p.getBoolValue ("autoCheck", true) || juce::Time::getCurrentTime() - last < juce::RelativeTime::hours (23)))
                return;
            p.setValue ("lastCheck", juce::String (juce::Time::currentTimeMillis()));
            p.saveIfNeeded();
        }
        if (! isThreadRunning()) startThread (juce::Thread::Priority::low);
    }

    static void skip (const juce::String& version) { juce::PropertiesFile p (options()); p.setValue ("skipped", version); p.saveIfNeeded(); }

    void downloadAndOpen (const UpdateInfo& info)
    {
        target = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Downloads")
                     .getNonexistentChildFile ("Hypernova-" + info.version, ".dmg", false);
        download = juce::URL (info.dmgUrl).downloadToFile (target, juce::URL::DownloadTaskOptions().withListener (this));
        if (download == nullptr && onFinished) onFinished (false, "Couldn't start the download.");
    }

    bool downloading() const { return download != nullptr && ! download->isFinished(); }

    static juce::String currentVersion() { return HYPERNOVA_VERSION; }

private:
    bool manual = false;
    std::unique_ptr<juce::URL::DownloadTask> download;
    juce::File target;
    juce::WeakReference<Updater>::Master masterReference;
    friend class juce::WeakReference<Updater>;

    static juce::InterProcessLock& lock() { static juce::InterProcessLock l ("HypernovaUpdates"); return l; }

    void run() override
    {
        UpdateInfo info;
        const auto url = juce::URL ("https://api.github.com/repos/" + juce::String (repo) + "/releases/latest");
        if (auto stream = url.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                     .withExtraHeaders ("Accept: application/vnd.github+json\r\nUser-Agent: Hypernova")
                                                     .withConnectionTimeoutMs (8000)))
        {
            const auto json = juce::JSON::parse (stream->readEntireStreamAsString());
            info.version = json["tag_name"].toString().trimCharactersAtStart ("vV");
            info.pageUrl = json["html_url"].toString();
            if (auto* assets = json["assets"].getArray())
                for (const auto& a : *assets)
                    if (a["name"].toString().endsWithIgnoreCase (".dmg")) { info.dmgUrl = a["browser_download_url"].toString(); break; }
        }
        if (threadShouldExit()) return;

        const bool newer = info.valid() && isNewer (info.version, currentVersion());
        if (info.valid())
        {
            juce::PropertiesFile p (options());
            p.setValue ("latestVersion", info.version);
            p.setValue ("latestDmg", info.dmgUrl);
            p.setValue ("latestPage", info.pageUrl);
            p.saveIfNeeded();
        }
        juce::MessageManager::callAsync ([weak = juce::WeakReference<Updater> (this), info, newer, ok = info.valid()]
        {
            auto* self = weak.get();
            if (self == nullptr) return;
            if (newer && self->onAvailable) self->onAvailable (info);
            else if (self->manual && self->onFinished)
                self->onFinished (ok, ok ? "You're on the latest version (" + currentVersion() + ")." : "Couldn't reach the update server.");
        });
    }

    void progress (juce::URL::DownloadTask*, juce::int64 done, juce::int64 total) override
    {
        const float f = total > 0 ? (float) done / (float) total : 0.0f;
        juce::MessageManager::callAsync ([weak = juce::WeakReference<Updater> (this), f] { if (auto* s = weak.get()) if (s->onProgress) s->onProgress (f); });
    }

    void finished (juce::URL::DownloadTask* task, bool success) override
    {
        const bool ok = success && task->statusCode() < 400 && target.getSize() > 1000000;
        if (ok) target.startAsProcess(); // mounts the disk image; its window holds the installer
        else target.deleteFile();
        juce::MessageManager::callAsync ([weak = juce::WeakReference<Updater> (this), ok]
        {
            if (auto* s = weak.get())
                if (s->onFinished) s->onFinished (ok, ok ? "Downloaded. Open the installer in the window that just appeared." : "Download failed. Try again later.");
        });
    }
};

// The pill that appears under the top bar when an update is out.
class UpdateBanner : public juce::Component
{
public:
    std::function<void()> onUpdate, onDismiss;

    void setInfo (const UpdateInfo& i) { info = i; progress = -1.0f; repaint(); }
    void setProgress (float p) { progress = p; repaint(); }
    const UpdateInfo& getInfo() const { return info; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (r.translated (0, 3), r.getHeight() * 0.5f);
        g.setGradientFill (juce::ColourGradient (Colours::accent.withAlpha (0.28f), r.getX(), 0, Colours::accent2.withAlpha (0.28f), r.getRight(), 0, false));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (Colours::panelHi.withAlpha (0.82f));
        g.fillRoundedRectangle (r.reduced (1.5f), r.getHeight() * 0.5f - 1.5f);
        if (progress >= 0.0f)
        {
            g.setColour (Colours::accent.withAlpha (0.25f));
            g.fillRoundedRectangle (r.reduced (1.5f).withWidth ((r.getWidth() - 3.0f) * juce::jlimit (0.02f, 1.0f, progress)), r.getHeight() * 0.5f - 1.5f);
        }
        g.setColour (Colours::accent.withAlpha (isMouseOver() ? 0.9f : 0.6f));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);

        auto text = r.reduced (16, 0);
        auto close = text.removeFromRight (18);
        g.setColour (Colours::textDim);
        g.drawLine (close.getCentreX() - 4, close.getCentreY() - 4, close.getCentreX() + 4, close.getCentreY() + 4, 1.4f);
        g.drawLine (close.getCentreX() - 4, close.getCentreY() + 4, close.getCentreX() + 4, close.getCentreY() - 4, 1.4f);
        g.setColour (Colours::text);
        g.setFont (font (13.0f, true));
        const auto msg = progress >= 0.0f ? "Downloading Hypernova " + info.version + "...  " + juce::String (juce::roundToInt (progress * 100)) + "%"
                                          : "Hypernova " + info.version + " is out  /  click to update";
        g.drawText (msg, text, juce::Justification::centred, true);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! getLocalBounds().contains (e.getPosition())) return;
        if (e.x > getWidth() - 40) { if (onDismiss) onDismiss(); }
        else if (progress < 0.0f && onUpdate) onUpdate();
    }

private:
    UpdateInfo info;
    float progress = -1.0f;
};

} // namespace ab::ui
