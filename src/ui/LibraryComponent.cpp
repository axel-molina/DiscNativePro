#include "LibraryComponent.h"
#include "BpmFormat.h"
#include "DeckComponent.h"
#include "YouTubeLibraryView.h"
#include "UsbLibraryView.h"
#include "SoundCloudLibraryView.h"
#include "../services/YouTubeService.h"
#include "../services/YouTubeAudioDownloader.h"
#include "../services/AsyncFolderScanner.h"
#include "../services/LibraryCacheManager.h"
#include "../audio/SampleTrackGenerator.h"
#include "../audio/BeatDetector.h"
#include "../audio/KeyDetector.h"
#include "CoverArtGenerator.h"
#include <cstdio>

namespace
{
    juce::String formatDuration(double seconds)
    {
        if (seconds <= 0.0)
            return "--:--";
        int totalSecs = static_cast<int>(std::round(seconds));
        int hours = totalSecs / 3600;
        int mins = (totalSecs % 3600) / 60;
        int secs = totalSecs % 60;
        if (hours > 0)
            return juce::String::formatted("%d:%02d:%02d", hours, mins, secs);
        return juce::String::formatted("%02d:%02d", mins, secs);
    }

    // Custom button strip for Column 9 ("CARGAR 1 / 2")
    class LoadButtonsComponent : public juce::Component
    {
    public:
        LoadButtonsComponent(std::function<void(int)> onClickCallback)
            : onDeckLoad(onClickCallback)
        {
            btn1.setButtonText("1");
            btn1.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(0, 180, 216).withAlpha(0.15f));
            btn1.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(0, 210, 255));
            btn1.onClick = [this]() { if (onDeckLoad) onDeckLoad(0); };
            addAndMakeVisible(btn1);

            btn2.setButtonText("2");
            btn2.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(0, 229, 255).withAlpha(0.15f));
            btn2.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(0, 229, 255));
            btn2.onClick = [this]() { if (onDeckLoad) onDeckLoad(1); };
            addAndMakeVisible(btn2);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(2, 2);
            btn1.setBounds(area.removeFromLeft(30).reduced(1, 1));
            area.removeFromLeft(2);
            btn2.setBounds(area.removeFromLeft(30).reduced(1, 1));
        }

    private:
        juce::TextButton btn1;
        juce::TextButton btn2;
        std::function<void(int)> onDeckLoad;
    };
}

class LibraryComponent::AutomixQueueView : public juce::Component,
                                           public juce::DragAndDropTarget
{
public:
    class DeleteButton : public juce::Component
    {
    public:
        std::function<void()> onClick;

        DeleteButton()
        {
            setMouseCursor(juce::MouseCursor::PointingHandCursor);
        }

        void mouseEnter(const juce::MouseEvent&) override { isHover = true; repaint(); }
        void mouseExit(const juce::MouseEvent&) override  { isHover = false; repaint(); }
        void mouseUp(const juce::MouseEvent&) override    { if (onClick) onClick(); }

        void paint(juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat().reduced(2.0f);
            if (isHover)
            {
                g.setColour(juce::Colour::fromRGB(244, 63, 94).withAlpha(0.25f));
                g.fillEllipse(b);
            }
            g.setColour(isHover ? juce::Colour::fromRGB(244, 63, 94) : juce::Colour::fromRGB(110, 118, 135));
            auto centre = b.getCentre();
            float r = 3.2f;
            g.drawLine(centre.x - r, centre.y - r, centre.x + r, centre.y + r, 1.4f);
            g.drawLine(centre.x - r, centre.y + r, centre.x + r, centre.y - r, 1.4f);
        }

    private:
        bool isHover { false };
    };

    class RowItem : public juce::Component
    {
    public:
        RowItem(LibraryComponent& parentLibrary, int index, const TrackItem& item, bool isPlaying, bool isNext)
            : owner(parentLibrary), rowIndex(index), track(item), playing(isPlaying), next(isNext)
        {
            deleteBtn.onClick = [this]() {
                owner.deleteTrackFromAutomixQueue(rowIndex);
            };
            addAndMakeVisible(deleteBtn);
        }

        void mouseEnter(const juce::MouseEvent&) override { isHover = true; repaint(); }
        void mouseExit(const juce::MouseEvent&) override  { isHover = false; repaint(); }

        void mouseDown(const juce::MouseEvent& e) override
        {
            mouseDownPos = e.position;
            hasStartedDrag = false;
        }

        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (!hasStartedDrag && e.getDistanceFromDragStart() > 4)
            {
                hasStartedDrag = true;
                if (auto* ddc = juce::DragAndDropContainer::findParentDragContainerFor(this))
                {
                    ddc->startDragging("automix_reorder:" + juce::String(rowIndex), this);
                }
            }
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            hasStartedDrag = false;
        }

        void paint(juce::Graphics& g) override
        {
            auto itemRect = getLocalBounds().reduced(2, 1);

            // Alternating row background
            juce::Colour bg = (rowIndex % 2 == 0) ? juce::Colour::fromRGB(18, 20, 28) : juce::Colour::fromRGB(14, 16, 22);
            if (isHover)
                bg = bg.brighter(0.08f);
            g.setColour(bg);
            g.fillRoundedRectangle(itemRect.toFloat(), 4.0f);

            // Highlight playing or next track
            if (playing)
            {
                g.setColour(juce::Colour::fromRGB(96, 165, 250).withAlpha(0.18f));
                g.fillRoundedRectangle(itemRect.toFloat(), 4.0f);
                g.setColour(juce::Colour::fromRGB(96, 165, 250).withAlpha(0.6f));
                g.drawRoundedRectangle(itemRect.toFloat().reduced(0.5f), 4.0f, 1.2f);
            }
            else if (next)
            {
                g.setColour(juce::Colour::fromRGB(0, 229, 255).withAlpha(0.08f));
                g.fillRoundedRectangle(itemRect.toFloat(), 4.0f);
                g.setColour(juce::Colour::fromRGB(0, 229, 255).withAlpha(0.35f));
                g.drawRoundedRectangle(itemRect.toFloat().reduced(0.5f), 4.0f, 1.0f);
            }

            // Track number
            g.setColour(playing ? juce::Colour::fromRGB(96, 165, 250) : juce::Colour::fromRGB(100, 108, 125));
            g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
            auto numArea = itemRect;
            g.drawText(juce::String(rowIndex + 1), numArea.removeFromLeft(20).reduced(2, 0), juce::Justification::centred, false);

            // Delete button on the right (20px reserved)
            numArea.removeFromRight(20);

            // BPM badge or status tag
            auto badgeArea = numArea.removeFromRight(36).reduced(1, 6);
            if (playing)
            {
                g.setColour(juce::Colour::fromRGB(96, 165, 250).withAlpha(0.2f));
                g.fillRoundedRectangle(badgeArea.toFloat(), 3.0f);
                g.setColour(juce::Colour::fromRGB(147, 197, 253));
                g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
                g.drawText("ON AIR", badgeArea, juce::Justification::centred, false);
            }
            else if (next)
            {
                g.setColour(juce::Colour::fromRGB(0, 229, 255).withAlpha(0.12f));
                g.fillRoundedRectangle(badgeArea.toFloat(), 3.0f);
                g.setColour(juce::Colour::fromRGB(0, 229, 255));
                g.setFont(juce::FontOptions(8.0f, juce::Font::bold));
                g.drawText("NEXT", badgeArea, juce::Justification::centred, false);
            }
            else
            {
                g.setColour(juce::Colour::fromRGB(0, 180, 216).withAlpha(0.1f));
                g.fillRoundedRectangle(badgeArea.toFloat(), 3.0f);
                g.setColour(juce::Colour::fromRGB(0, 229, 255));
                g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
                g.drawText(formatBpm(track.bpm), badgeArea, juce::Justification::centred, false);
            }

            // Title + Artist
            auto textArea = numArea.reduced(4, 1);
            g.setColour(playing ? juce::Colour::fromRGB(224, 242, 254) : juce::Colours::white);
            g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
            g.drawText(track.title, textArea.removeFromTop(14), juce::Justification::centredLeft, true);

            g.setColour(juce::Colour::fromRGB(142, 149, 165));
            g.setFont(juce::FontOptions(9.0f));
            g.drawText(track.artist, textArea, juce::Justification::centredLeft, true);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            deleteBtn.setBounds(r.removeFromRight(20).reduced(2, 8));
        }

    private:
        LibraryComponent& owner;
        int rowIndex;
        TrackItem track;
        bool playing { false };
        bool next { false };
        bool isHover { false };
        bool hasStartedDrag { false };
        juce::Point<float> mouseDownPos;
        DeleteButton deleteBtn;
    };

    class ListContentComponent : public juce::Component
    {
    public:
        ListContentComponent(AutomixQueueView& parent) : view(parent) {}

        void updateRows(LibraryComponent& libOwner)
        {
            rows.clear();
            int idx = 0;
            for (const auto& item : libOwner.automixQueue)
            {
                bool isPlaying = (libOwner.automixRunning && idx == libOwner.automixPlayingIndex);
                bool isNext = (libOwner.automixRunning && (
                    (libOwner.automixPlayingIndex == -1 && idx == 0) ||
                    (libOwner.automixPlayingIndex >= 0 && idx == libOwner.automixPlayingIndex + 1)
                ));
                auto r = std::make_unique<RowItem>(libOwner, idx, item, isPlaying, isNext);
                addAndMakeVisible(*r);
                rows.push_back(std::move(r));
                ++idx;
            }
            resized();
            repaint();
        }

        int calculateTotalHeight() const
        {
            return (int)rows.size() * 34;
        }

        void setInsertLine(int index)
        {
            if (insertLineIndex != index)
            {
                insertLineIndex = index;
                repaint();
            }
        }

        void paintOverChildren(juce::Graphics& g) override
        {
            if (insertLineIndex >= 0 && insertLineIndex <= (int)rows.size())
            {
                float y = (float)(insertLineIndex * 34);
                g.setColour(juce::Colour::fromRGB(0, 229, 255));
                g.drawLine(4.0f, y, (float)getWidth() - 4.0f, y, 2.0f);
                g.fillEllipse(2.0f, y - 3.0f, 6.0f, 6.0f);
            }
        }

        void resized() override
        {
            int y = 0;
            int w = getWidth();
            for (auto& row : rows)
            {
                row->setBounds(0, y, w, 34);
                y += 34;
            }
        }

    private:
        [[maybe_unused]] AutomixQueueView& view;
        std::vector<std::unique_ptr<RowItem>> rows;
        int insertLineIndex { -1 };
    };

    AutomixQueueView(LibraryComponent& parent)
        : owner(parent), contentComp(*this)
    {
        viewport.setViewedComponent(&contentComp, false);
        viewport.setScrollBarsShown(true, false);
        viewport.setScrollBarThickness(6);
        addAndMakeVisible(viewport);
    }

    void updateContent()
    {
        bool hasItems = !owner.automixQueue.empty();
        viewport.setVisible(hasItems);
        if (hasItems)
        {
            contentComp.updateRows(owner);
            int totalH = contentComp.calculateTotalHeight();
            contentComp.setSize(std::max(10, viewport.getWidth() - 6), totalH);
        }
        repaint();
    }

    void resized() override
    {
        viewport.setBounds(getLocalBounds().reduced(2));
        if (!owner.automixQueue.empty())
        {
            int totalH = contentComp.calculateTotalHeight();
            contentComp.setSize(std::max(10, viewport.getWidth() - 6), totalH);
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        bool isHover = owner.automixDropHover || externalDropHover;
        g.setColour(isHover ? juce::Colour::fromRGB(37, 99, 235).withAlpha(0.12f) : juce::Colour::fromRGB(16, 18, 25));
        g.fillRoundedRectangle(bounds, 8.0f);

        g.setColour(isHover ? juce::Colour::fromRGB(96, 165, 250).withAlpha(0.6f) : juce::Colour::fromRGB(36, 42, 56));
        g.drawRoundedRectangle(bounds, 8.0f, isHover ? 2.0f : 1.0f);

        if (owner.automixQueue.empty())
        {
            // Empty state placeholder
            float iconCentreX = bounds.getCentreX();
            float totalBlockHeight = 94.0f;
            float contentTop = juce::jmax(bounds.getY() + 14.0f,
                                          bounds.getY() + (bounds.getHeight() - totalBlockHeight) * 0.45f);
            float iconCentreY = contentTop + 16.0f;

            g.setColour(juce::Colour::fromRGB(24, 28, 38));
            g.fillEllipse(iconCentreX - 16.0f, iconCentreY - 16.0f, 32.0f, 32.0f);
            LucideIcons::draw(g, LucideIcons::IconType::Music,
                              juce::Rectangle<float>(iconCentreX - 8.0f, iconCentreY - 8.0f, 16.0f, 16.0f),
                              juce::Colour::fromRGB(142, 149, 165), 1.6f);

            g.setColour(juce::Colour::fromRGB(203, 213, 225));
            g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
            g.drawText(LanguageManager::trUtf8("Arrastra canciones aquí", "Drop tracks here"),
                       (int)bounds.getX(), (int)(iconCentreY + 22.0f), (int)bounds.getWidth(), 18,
                       juce::Justification::centred, false);

            g.setColour(juce::Colour::fromRGB(100, 108, 125));
            g.setFont(juce::FontOptions(9.5f));
            g.drawText(LanguageManager::trUtf8("Usa Shift o Cmd+A para seleccionar\ny arrastrar varias pistas.",
                                               "Use Shift or Cmd+A to select\nand drag multiple tracks."),
                       (int)bounds.getX() + 8, (int)(iconCentreY + 42.0f), (int)bounds.getWidth() - 16, 30,
                       juce::Justification::centred, true);
        }
    }

    bool isInterestedInDragSource(const SourceDetails& details) override
    {
        return details.description.toString().startsWith("automix_reorder:") ||
               details.description.toString().isNotEmpty();
    }

    void itemDragEnter(const SourceDetails& details) override
    {
        itemDragMove(details);
    }

    void itemDragMove(const SourceDetails& details) override
    {
        if (details.description.toString().startsWith("automix_reorder:"))
        {
            auto posInContent = contentComp.getLocalPoint(this, details.localPosition);
            int idx = juce::jlimit(0, (int)owner.automixQueue.size(), (int)((posInContent.y + 17) / 34));
            contentComp.setInsertLine(idx);
            externalDropHover = false;
        }
        else
        {
            contentComp.setInsertLine(-1);
            if (!externalDropHover)
            {
                externalDropHover = true;
                repaint();
            }
        }
    }

    void itemDragExit(const SourceDetails&) override
    {
        contentComp.setInsertLine(-1);
        if (externalDropHover)
        {
            externalDropHover = false;
            repaint();
        }
    }

    void itemDropped(const SourceDetails& details) override
    {
        contentComp.setInsertLine(-1);
        externalDropHover = false;
        repaint();

        if (details.description.toString().startsWith("automix_reorder:"))
        {
            int fromIdx = details.description.toString().substring(16).getIntValue();
            auto posInContent = contentComp.getLocalPoint(this, details.localPosition);
            int toIdx = juce::jlimit(0, (int)owner.automixQueue.size() - 1, (int)((posInContent.y + 17) / 34));
            owner.reorderAutomixQueue(fromIdx, toIdx);
        }
        else
        {
            owner.handleAutomixDropTracks(details.description.toString());
        }
    }

private:
    LibraryComponent& owner;
    juce::Viewport viewport;
    ListContentComponent contentComp;
    bool externalDropHover { false };
};

class LibraryComponent::DownloadsEmptyOverlay : public juce::Component
{
public:
    DownloadsEmptyOverlay()
    {
        titleLabel.setFont(juce::FontOptions(13.5f, juce::Font::bold));
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        titleLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(titleLabel);

        descLabel.setFont(juce::FontOptions(11.5f, juce::Font::plain));
        descLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(148, 163, 184));
        descLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(descLabel);

        actionBtn.setFontSize(11.0f);
        actionBtn.setCornerRadius(6.0f);
        addAndMakeVisible(actionBtn);
    }

    void updateState(bool saveEnabled)
    {
        isSaveEnabled = saveEnabled;
        if (!isSaveEnabled)
        {
            titleLabel.setText(LanguageManager::trUtf8("Carpeta de Descargas vacía", "Downloads folder is empty"), juce::dontSendNotification);
            descLabel.setText(LanguageManager::trUtf8("Habilita el guardado de las descargas de audio en la configuración General para ver tus canciones.",
                                                      "Enable saving audio downloads in General settings to see your tracks."), juce::dontSendNotification);
            actionBtn.setIcon(LucideIcons::IconType::Settings, 13.0f);
            actionBtn.setText(LanguageManager::trUtf8("Abrir Configuración General", "Open General Settings"));
            actionBtn.setCustomColours(juce::Colour::fromRGB(22, 26, 35),
                                       juce::Colour::fromRGB(0, 229, 255),
                                       juce::Colour::fromRGB(0, 229, 255),
                                       juce::Colour::fromRGB(38, 43, 56));
            actionBtn.onClick = [this]() {
                if (onOpenSettings) onOpenSettings();
            };
        }
        else
        {
            titleLabel.setText(LanguageManager::trUtf8("Aún no tienes canciones descargadas", "No downloaded tracks yet"), juce::dontSendNotification);
            descLabel.setText(LanguageManager::trUtf8("Busca temas en la pestaña YouTube y pulsa Descargar para guardarlos en esta carpeta.",
                                                      "Search for tracks on the YouTube tab and click Download to save them to this folder."), juce::dontSendNotification);
            actionBtn.setIcon(LucideIcons::IconType::YouTube, 13.0f);
            actionBtn.setText(LanguageManager::trUtf8("Ir a YouTube", "Go to YouTube"));
            actionBtn.setCustomColours(juce::Colour::fromRGB(26, 20, 24),
                                       juce::Colour::fromRGB(239, 68, 68),
                                       juce::Colour::fromRGB(239, 68, 68),
                                       juce::Colour::fromRGB(48, 30, 36));
            actionBtn.onClick = [this]() {
                if (onGoToYouTube) onGoToYouTube();
            };
        }
        resized();
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.fillAll(juce::Colour::fromRGB(12, 13, 18));

        // Circular badge for icon
        float centreX = bounds.getCentreX();
        float startY = juce::jmax(10.0f, bounds.getCentreY() - 80.0f);
        auto badgeRect = juce::Rectangle<float>(centreX - 24.0f, startY, 48.0f, 48.0f);

        if (!isSaveEnabled)
        {
            g.setColour(juce::Colour::fromRGB(0, 180, 216).withAlpha(0.12f));
            g.fillRoundedRectangle(badgeRect, 24.0f);
            g.setColour(juce::Colour::fromRGB(0, 180, 216).withAlpha(0.35f));
            g.drawRoundedRectangle(badgeRect, 24.0f, 1.2f);
            LucideIcons::draw(g, LucideIcons::IconType::Download, badgeRect.reduced(13.0f), juce::Colour::fromRGB(0, 229, 255), 1.8f);
        }
        else
        {
            g.setColour(juce::Colour::fromRGB(16, 185, 129).withAlpha(0.12f));
            g.fillRoundedRectangle(badgeRect, 24.0f);
            g.setColour(juce::Colour::fromRGB(16, 185, 129).withAlpha(0.35f));
            g.drawRoundedRectangle(badgeRect, 24.0f, 1.2f);
            LucideIcons::draw(g, LucideIcons::IconType::Download, badgeRect.reduced(13.0f), juce::Colour::fromRGB(52, 211, 153), 1.8f);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        int centreX = bounds.getCentreX();
        int startY = juce::jmax(10, bounds.getCentreY() - 80);

        int y = startY + 56;
        titleLabel.setBounds(centreX - 250, y, 500, 22);
        y += 24;
        descLabel.setBounds(centreX - 260, y, 520, 20);
        y += 28;
        actionBtn.setBounds(centreX - 110, y, 220, 30);
    }

    std::function<void()> onOpenSettings;
    std::function<void()> onGoToYouTube;

private:
    juce::Label titleLabel;
    juce::Label descLabel;
    DjButton actionBtn;
    bool isSaveEnabled { false };
};

class LibraryComponent::BackgroundBpmScanner : public juce::Thread
{
public:
    BackgroundBpmScanner(juce::AudioFormatManager& fm, std::function<void(const juce::File&, double, const juce::String&)> onAnalysisReady)
        : juce::Thread("AudioBackgroundScanner"), formatManager(fm), callback(std::move(onAnalysisReady))
    {
    }

    ~BackgroundBpmScanner() override
    {
        signalThreadShouldExit();
        notify();
        stopThread(2000);
    }

    void queueTrack(const juce::File& file)
    {
        {
            const juce::ScopedLock sl(queueLock);
            if (std::find(pendingFiles.begin(), pendingFiles.end(), file) == pendingFiles.end())
                pendingFiles.push_back(file);
        }
        notify();
        if (!isThreadRunning())
            startThread(juce::Thread::Priority::low);
    }

    void clearQueue()
    {
        const juce::ScopedLock sl(queueLock);
        pendingFiles.clear();
    }

    void run() override
    {
        while (!threadShouldExit())
        {
            juce::File nextFile;
            {
                const juce::ScopedLock sl(queueLock);
                if (pendingFiles.empty())
                {
                    wait(500);
                    if (pendingFiles.empty() || threadShouldExit())
                        break;
                }
                if (threadShouldExit()) break;
                nextFile = pendingFiles.front();
                pendingFiles.erase(pendingFiles.begin());
            }

            if (nextFile.existsAsFile() && !threadShouldExit())
            {
                std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(nextFile));
                if (reader != nullptr && !threadShouldExit())
                {
                    auto beatRes = BeatDetector::analyze(reader.get());
                    juce::String detectedKey;
                    if (!threadShouldExit())
                    {
                        auto keyRes = KeyDetector::analyze(reader.get(), 45.0);
                        detectedKey = (keyRes.formattedKey.isNotEmpty() && keyRes.formattedKey != "--")
                                      ? keyRes.formattedKey
                                      : keyRes.camelot;
                    }

                    if (callback && !threadShouldExit())
                    {
                        double detectedBpm = (beatRes.bpm >= 40.0 && beatRes.bpm <= 250.0) ? beatRes.bpm : 0.0;
                        LibraryCacheManager::getInstance().updateBpmAndKey(nextFile, detectedBpm, detectedKey);

                        juce::MessageManager::callAsync([cb = callback, f = nextFile, bpm = detectedBpm, key = detectedKey]() {
                            cb(f, bpm, key);
                        });
                    }
                }
            }

            wait(40);
        }
    }

private:
    juce::AudioFormatManager& formatManager;
    std::function<void(const juce::File&, double, const juce::String&)> callback;
    juce::CriticalSection queueLock;
    std::vector<juce::File> pendingFiles;
};

LibraryComponent::LibraryComponent(juce::AudioFormatManager& fm)
    : formatManager(fm)
{
    bpmScanner = std::make_unique<BackgroundBpmScanner>(formatManager, [this](const juce::File& file, double bpm, const juce::String& key) {
        updateTrackBpmAndKey(file, bpm, key);
    });

    folderScanner = std::make_unique<AsyncFolderScanner>(formatManager);
    folderScanner->onTracksDiscovered = [this](const std::vector<TrackItem>& batch, uint64_t /*scanId*/) {
        for (auto item : batch)
        {
            auto path = item.file.getFullPathName();
            if (knownTrackPaths.find(path) == knownTrackPaths.end())
            {
                knownTrackPaths.insert(path);
                item.id = juce::String(allTracks.size() + 1);
                allTracks.push_back(item);

                if ((item.bpm <= 0.0 || item.key == "--" || item.key.isEmpty()) && !item.isDemo)
                {
                    if (bpmScanner != nullptr)
                        bpmScanner->queueTrack(item.file);
                }
            }
        }
        applySorting();
        filterTracks(searchBox.getText());
        folderTree.updateTree(allTracks);
        updateSyncStatus((int)filteredTracks.size(), true);
    };

    folderScanner->onProgress = [this](int totalFound, const juce::String& currentFolderName, uint64_t /*scanId*/) {
        updateSyncStatus(totalFound, true, currentFolderName);
    };

    folderScanner->onScanFinished = [this](int /*totalFound*/, uint64_t /*scanId*/) {
        isFolderSyncing = false;
        applySorting();
        filterTracks(searchBox.getText());
        folderTree.updateTree(allTracks);
        updateSyncStatus((int)filteredTracks.size(), false);
    };

    // 1. Far-Left Vertical Icon Rail
    auto setupRailIcon = [this](DjButton& btn, LucideIcons::IconType iconType, juce::Colour col, bool active) {
        btn.setIcon(iconType, 16.0f);
        btn.setCornerRadius(6.0f);
        btn.setCustomColours(active ? juce::Colour::fromRGB(0, 180, 216).withAlpha(0.2f) : juce::Colour::fromRGB(18, 20, 26),
                             col,
                             col,
                             active ? juce::Colour::fromRGB(0, 180, 216).withAlpha(0.4f) : juce::Colour::fromRGB(30, 34, 44));
        addAndMakeVisible(btn);
    };
    setupRailIcon(railLocalBtn, LucideIcons::IconType::Music, juce::Colour::fromRGB(0, 229, 255), true); // Active music
    setupRailIcon(railUsbBtn, LucideIcons::IconType::Usb, juce::Colour::fromRGB(52, 211, 153), false); // USB Library
    setupRailIcon(railAiBtn, LucideIcons::IconType::Sparkles, juce::Colour::fromRGB(192, 132, 252), false); // AI Smart Match
    setupRailIcon(railYtBtn, LucideIcons::IconType::YouTube, juce::Colour::fromRGB(239, 68, 68), false); // YouTube
    setupRailIcon(railScBtn, LucideIcons::IconType::SoundCloud, juce::Colour::fromRGB(255, 85, 0), false); // SoundCloud

    railLocalBtn.onClick = [this]() { setLibraryMode(LibraryMode::Local); };
    railUsbBtn.onClick = [this]() { setLibraryMode(LibraryMode::USB); };
    railAiBtn.onClick = [this]() { setLibraryMode(LibraryMode::AIMatch); };
    railYtBtn.onClick = [this]() { setLibraryMode(LibraryMode::YouTube); };
    railScBtn.onClick = [this]() { setLibraryMode(LibraryMode::SoundCloud); };

    // USB View
    usbView = std::make_unique<UsbLibraryView>(formatManager);
    usbView->onLoadTrack = [this](int deckIndex, const juce::File& file) {
        if (onLoadTrack)
            onLoadTrack(deckIndex, file);
    };
    addChildComponent(*usbView);

    // YouTube View
    youtubeView = std::make_unique<YouTubeLibraryView>();
    youtubeView->onLoadTrack = [this](int deckIndex, const YouTubeSearchResult& video) {
        if (onLoadYouTubeTrack)
            onLoadYouTubeTrack(deckIndex, video);
    };
    youtubeView->onOpenSettings = [this]() {
        if (onOpenSettings) onOpenSettings(5);
    };
    addChildComponent(*youtubeView);

    // AI Match View
    aiMatchView = std::make_unique<AIMatchView>();
    aiMatchView->onLoadLocalTrack = [this](const juce::File& file, int targetDeck) {
        if (onLoadTrack)
            onLoadTrack(targetDeck, file);
    };
    aiMatchView->onSearchYouTube = [this](const juce::String& query, int /*targetDeck*/) {
        setLibraryMode(LibraryMode::YouTube);
        if (youtubeView)
            youtubeView->performSearch(query);
    };
    aiMatchView->onOpenSettings = [this]() {
        if (onOpenSettings) onOpenSettings(6);
    };
    addChildComponent(*aiMatchView);

    // SoundCloud View
    soundcloudView = std::make_unique<SoundCloudLibraryView>();
    soundcloudView->onLoadTrack = [this](int deckIndex, const juce::File& file, const juce::String&, const juce::String&) {
        if (onLoadTrack)
            onLoadTrack(deckIndex, file);
    };
    addChildComponent(*soundcloudView);

    // 2. "Mis Archivos" Hierarchical Folder Tree
    folderTree.onSelectFolder = [this](const juce::String& folderPath) {
        currentSelectedFolder = folderPath;
        filterTracks(searchBox.getText());
        updateDownloadsEmptyState();
    };
    folderTree.onAddFolder = [this]() {
        importFolder();
    };
    folderTree.onUnsyncFolder = [this](const juce::String& folderPath) {
        unsyncFolder(folderPath);
    };
    addAndMakeVisible(folderTree);

    // 3. Center Table Area Header
    tableHeaderTitle.setText(juce::String::fromUTF8("Todas las Pistas  0 Canciones"), juce::dontSendNotification);
    tableHeaderTitle.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    tableHeaderTitle.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(tableHeaderTitle);

    searchBox.setTextToShowWhenEmpty("Explora Mis Archivos...", juce::Colour::fromRGB(100, 108, 125));
    searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGB(16, 18, 24));
    searchBox.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colour::fromRGB(36, 40, 54));
    searchBox.onTextChange = [this]() { filterTracks(searchBox.getText()); };
    addAndMakeVisible(searchBox);

    addBtn.setIcon(LucideIcons::IconType::FolderPlus, 13.0f);
    addBtn.setText(juce::String::fromUTF8("Añadir"));
    addBtn.setFontSize(11.0f);
    addBtn.setCornerRadius(5.0f);
    addBtn.setCustomColours(juce::Colour::fromRGB(22, 26, 35),
                            juce::Colour::fromRGB(0, 229, 255),
                            juce::Colour::fromRGB(0, 229, 255),
                            juce::Colour::fromRGB(38, 43, 56));
    addBtn.onClick = [this]() { importFiles(); };
    addAndMakeVisible(addBtn);

    // Setup Table Columns
    table.setModel(this);
    table.setColour(juce::TableListBox::backgroundColourId, juce::Colour::fromRGB(12, 13, 18));
    table.getHeader().addColumn("#", 1, 38, 30, 48, juce::TableHeaderComponent::notSortable);
    table.getHeader().addColumn("", 2, 34, 28, 42, juce::TableHeaderComponent::notSortable);
    table.getHeader().addColumn(juce::String::fromUTF8("TÍTULO"), 3, 200, 110, 350);
    table.getHeader().addColumn(juce::String::fromUTF8("ARTISTA"), 4, 140, 90, 250);
    table.getHeader().addColumn(juce::String::fromUTF8("ÁLBUM"), 5, 130, 80, 250);
    table.getHeader().addColumn(juce::String::fromUTF8("DURACIÓN"), 6, 68, 52, 90);
    table.getHeader().addColumn("BPM", 7, 62, 50, 80);
    table.getHeader().addColumn("KEY", 8, 76, 60, 100);
    table.getHeader().addColumn("CARGAR", 9, 74, 68, 85, juce::TableHeaderComponent::notSortable);
    table.getHeader().setColour(juce::TableHeaderComponent::backgroundColourId, juce::Colour::fromRGB(16, 18, 24));
    table.getHeader().setColour(juce::TableHeaderComponent::textColourId, juce::Colour::fromRGB(142, 149, 165));
    table.setRowHeight(34);
    table.setMultipleSelectionEnabled(true);
    addAndMakeVisible(table);

    // Overlay para estado vacío de Descargas
    downloadsEmptyOverlay = std::make_unique<DownloadsEmptyOverlay>();
    downloadsEmptyOverlay->onOpenSettings = [this]() {
        if (onOpenSettings) onOpenSettings(0);
    };
    downloadsEmptyOverlay->onGoToYouTube = [this]() {
        setLibraryMode(LibraryMode::YouTube);
    };
    downloadsEmptyOverlay->setVisible(false);
    addChildComponent(*downloadsEmptyOverlay);

    // 4. Right Automix Panel
    automixHeader.setIcon(LucideIcons::IconType::Sparkles, 14.0f);
    automixHeader.setText("Automix");
    automixHeader.setFontSize(12.5f);
    automixHeader.setInterceptsMouseClicks(false, false);
    automixHeader.setCustomColours(juce::Colours::transparentBlack,
                                   juce::Colour::fromRGB(96, 165, 250),
                                   juce::Colour::fromRGB(96, 165, 250),
                                   juce::Colours::transparentBlack);
    addAndMakeVisible(automixHeader);

    startAutomixBtn.setIcon(LucideIcons::IconType::Sparkles, 12.0f);
    startAutomixBtn.setText("Comenzar Automix");
    startAutomixBtn.setFontSize(11.0f);
    startAutomixBtn.setCornerRadius(5.0f);
    startAutomixBtn.setCustomColours(juce::Colour::fromRGB(22, 25, 34),
                                     juce::Colour::fromRGB(142, 149, 165),
                                     juce::Colour::fromRGB(142, 149, 165),
                                     juce::Colour::fromRGB(38, 43, 56));
    startAutomixBtn.onClick = [this]() {
        if (automixRunning)
        {
            if (onStopAutomix) onStopAutomix();
        }
        else if (!automixQueue.empty() && onStartAutomix)
        {
            onStartAutomix(automixQueue);
        }
    };
    addAndMakeVisible(startAutomixBtn);

    smartAutomixBtn.setIcon(LucideIcons::IconType::Zap, 12.0f);
    smartAutomixBtn.setFontSize(11.0f);
    smartAutomixBtn.setCornerRadius(5.0f);
    smartAutomixBtn.setCustomColours(juce::Colour::fromRGB(22, 25, 34),
                                     juce::Colour::fromRGB(142, 149, 165),
                                     juce::Colour::fromRGB(142, 149, 165),
                                     juce::Colour::fromRGB(38, 43, 56));
    smartAutomixBtn.onClick = [this]() {
        if (!smartBusy && onSmartAutomix)
            onSmartAutomix();
    };
    addAndMakeVisible(smartAutomixBtn);

    clearQueueBtn.setIcon(LucideIcons::IconType::Minus, 10.0f);
    clearQueueBtn.setCornerRadius(4.0f);
    clearQueueBtn.setCustomColours(juce::Colours::transparentBlack,
                                   juce::Colour::fromRGB(100, 108, 125),
                                   juce::Colour::fromRGB(244, 63, 94),
                                   juce::Colours::transparentBlack);
    clearQueueBtn.onClick = [this]() {
        if (!automixRunning)
            clearAutomixQueue();
    };
    clearQueueBtn.setVisible(false);
    addAndMakeVisible(clearQueueBtn);

    queueTitle.setText(juce::String::fromUTF8("COLA DE REPRODUCCIÓN"), juce::dontSendNotification);
    queueTitle.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    queueTitle.setColour(juce::Label::textColourId, juce::Colour::fromRGB(100, 108, 125));
    addAndMakeVisible(queueTitle);

    automixQueueView = std::make_unique<AutomixQueueView>(*this);
    addAndMakeVisible(*automixQueueView);

    setWantsKeyboardFocus(true);

    // Escaneo inicial de la carpeta de Descargas persistentes
    scanDownloadsFolder();

    // Listener para registrar inmediatamente nuevas canciones descargadas de YouTube
    YouTubeAudioDownloader::getInstance().onTrackSavedToDownloads = [this](const juce::File& file, const juce::String& /*id*/, const juce::String& /*title*/, const juce::String& /*artist*/) {
        auto dlFolder = YouTubeService::getDownloadsFolder();
        addTrack(file, "Descargas", dlFolder.getFullPathName(), false);
        folderTree.updateTree(allTracks);
        filterTracks(searchBox.getText());
    };

    filteredTracks = allTracks;
    folderTree.updateTree(allTracks);
    table.updateContent();

    LanguageManager::getInstance().addListener(this);
    refreshLanguage();

    setLibraryMode(LibraryMode::Local);
}

LibraryComponent::~LibraryComponent()
{
    stopTimer();
    if (folderScanner != nullptr)
        folderScanner->cancelScan();

    if (bpmScanner != nullptr)
    {
        bpmScanner->clearQueue();
        bpmScanner->signalThreadShouldExit();
        bpmScanner->notify();
        bpmScanner->stopThread(2000);
        bpmScanner.reset();
    }
    LanguageManager::getInstance().removeListener(this);
}

void LibraryComponent::timerCallback()
{
    if (tableNeedsRepaint)
    {
        tableNeedsRepaint = false;
        table.updateContent();
        table.repaint();
        if (automixQueueView != nullptr)
            automixQueueView->updateContent();
    }
    else
    {
        stopTimer();
    }
}

void LibraryComponent::updateSyncStatus(int count, bool isScanning, const juce::String& currentFolder)
{
    isFolderSyncing = isScanning;
    juce::String folderDisplay = currentSelectedFolder;
    if (currentSelectedFolder == "Todas las Pistas")
        folderDisplay = LanguageManager::trUtf8("Todas las Pistas", "All Tracks");
    else if (currentSelectedFolder == "Descargas")
        folderDisplay = LanguageManager::trUtf8("Descargas", "Downloads");

    if (isScanning)
    {
        tableHeaderTitle.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 229, 255));
        juce::String status = folderDisplay + "  " + LanguageManager::trUtf8("Sincronizando: ", "Syncing: ") + juce::String(count) + " " + LanguageManager::trUtf8("Canciones...", "Tracks...");
        if (currentFolder.isNotEmpty())
            status += " (" + currentFolder + ")";
        tableHeaderTitle.setText(status, juce::dontSendNotification);
    }
    else
    {
        tableHeaderTitle.setColour(juce::Label::textColourId, juce::Colours::white);
        tableHeaderTitle.setText(folderDisplay + "  " + juce::String(count) + " " + LanguageManager::trUtf8("Canciones", "Tracks"), juce::dontSendNotification);
    }
}

void LibraryComponent::languageChanged()
{
    refreshLanguage();
}

void LibraryComponent::refreshLanguage()
{
    table.getHeader().setColumnName(3, LanguageManager::trUtf8("TÍTULO", "TITLE"));
    table.getHeader().setColumnName(4, LanguageManager::trUtf8("ARTISTA", "ARTIST"));
    table.getHeader().setColumnName(5, LanguageManager::trUtf8("ÁLBUM", "ALBUM"));
    table.getHeader().setColumnName(6, LanguageManager::trUtf8("DURACIÓN", "DURATION"));
    table.getHeader().setColumnName(9, LanguageManager::trUtf8("CARGAR", "LOAD"));

    addBtn.setText(LanguageManager::trUtf8("Añadir", "Add"));
    searchBox.setTextToShowWhenEmpty(LanguageManager::trUtf8("Explora Mis Archivos...", "Browse My Files..."), juce::Colour::fromRGB(100, 108, 125));

    if (automixRunning)
        startAutomixBtn.setText(LanguageManager::trUtf8("Detener Automix", "Stop Automix"));
    else
        startAutomixBtn.setText(LanguageManager::trUtf8("Comenzar Automix", "Start Automix"));

    smartAutomixBtn.setText(smartBusy ? LanguageManager::trUtf8("Consultando IA…", "Asking AI…")
                                      : LanguageManager::trUtf8("AutoMix IA", "Smart AutoMix"));

    if (automixQueue.empty())
        queueTitle.setText(LanguageManager::trUtf8("COLA DE REPRODUCCIÓN", "PLAYBACK QUEUE"), juce::dontSendNotification);
    else
        queueTitle.setText(LanguageManager::trUtf8("COLA · ", "QUEUE · ") + juce::String((int)automixQueue.size()) + LanguageManager::trUtf8(" pistas", " tracks"), juce::dontSendNotification);

    railLocalBtn.setTooltip(LanguageManager::trUtf8("Biblioteca Local (Mis Archivos)", "Local Library (My Files)"));
    railUsbBtn.setTooltip(LanguageManager::trUtf8("Biblioteca USB", "USB Library"));
    railAiBtn.setTooltip(LanguageManager::trUtf8("Recomendaciones IA (AI Match)", "AI Match Recommendations"));
    railYtBtn.setTooltip(LanguageManager::trUtf8("YouTube DJ Streaming", "YouTube DJ Streaming"));
    railScBtn.setTooltip(LanguageManager::trUtf8("SoundCloud DJ Streaming", "SoundCloud DJ Streaming"));

    juce::String folderDisplay = currentSelectedFolder;
    if (currentSelectedFolder == "Todas las Pistas")
        folderDisplay = LanguageManager::trUtf8("Todas las Pistas", "All Tracks");
    else if (currentSelectedFolder == "Descargas")
        folderDisplay = LanguageManager::trUtf8("Descargas", "Downloads");
    tableHeaderTitle.setText(folderDisplay + "  " + juce::String((int)filteredTracks.size()) + " " + LanguageManager::trUtf8("Canciones", "Tracks"), juce::dontSendNotification);

    table.repaint();
    updateDownloadsEmptyState();
    repaint();
}

void LibraryComponent::importFiles()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        LanguageManager::trUtf8("Seleccionar archivos de musica...", "Select music files..."),
        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        "*.wav;*.mp3;*.flac;*.aiff;*.aif;*.m4a;*.ogg;*.aac");

    auto folderFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems;
    fileChooser->launchAsync(folderFlags, [this](const juce::FileChooser& fc) {
        auto results = fc.getResults();
        for (const auto& file : results)
        {
            addTrack(file, "Mis Archivos", {}, false);
        }
        folderTree.updateTree(allTracks);
        filterTracks(searchBox.getText());
    });
}

void LibraryComponent::importFolder()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        LanguageManager::trUtf8("Seleccionar carpeta con musica...", "Select music folder..."),
        juce::File::getSpecialLocation(juce::File::userMusicDirectory));

    auto folderFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
    fileChooser->launchAsync(folderFlags, [this](const juce::FileChooser& fc) {
        auto folder = fc.getResult();
        if (folder.isDirectory())
        {
            scanFolderRecursive(folder);
            folderTree.updateTree(allTracks);
            folderTree.expandFolder(folder.getFileName());
            currentSelectedFolder = folder.getFileName();
            folderTree.setSelectedFolder(currentSelectedFolder);
            filterTracks(searchBox.getText());
        }
    });
}

void LibraryComponent::scanFolderRecursive(const juce::File& folder)
{
    if (!folder.isDirectory())
        return;

    auto rootName = folder.getFileName();
    currentSelectedFolder = rootName;
    folderTree.expandFolder(rootName);
    folderTree.setSelectedFolder(currentSelectedFolder);

    if (folderScanner != nullptr)
    {
        isFolderSyncing = true;
        updateSyncStatus((int)filteredTracks.size(), true);
        folderScanner->startScan(folder, rootName);
    }
}

void LibraryComponent::scanDownloadsFolder()
{
    auto dlFolder = YouTubeService::getDownloadsFolder();
    if (!dlFolder.exists())
        dlFolder.createDirectory();

    juce::Array<juce::File> files;
    dlFolder.findChildFiles(files, juce::File::findFiles, false, "*.wav;*.mp3;*.flac;*.aiff;*.aif;*.m4a;*.ogg;*.aac");

    for (const auto& f : files)
    {
        addTrack(f, "Descargas", dlFolder.getFullPathName(), false);
    }
}

void LibraryComponent::addTrack(const juce::File& file, const juce::String& folderTag, const juce::String& rootFolder, bool isDemo)
{
    // Fast O(1) duplicate check
    auto path = file.getFullPathName();
    if (knownTrackPaths.find(path) != knownTrackPaths.end())
        return;
    knownTrackPaths.insert(path);

    TrackItem item;
    item.file = file;
    item.folder = folderTag;
    item.rootFolderPath = rootFolder;
    item.isDemo = isDemo;
    item.id = juce::String(allTracks.size() + 1);

    // Check persistent cache first
    CachedTrackMetadata cachedMeta;
    if (!isDemo && LibraryCacheManager::getInstance().getTrack(file, cachedMeta))
    {
        item.title = cachedMeta.title;
        item.artist = cachedMeta.artist;
        item.album = cachedMeta.album;
        item.duration = cachedMeta.duration;
        item.bpm = cachedMeta.bpm;
        item.key = cachedMeta.key;
        allTracks.push_back(item);

        if ((item.bpm <= 0.0 || item.key == "--" || item.key.isEmpty()) && bpmScanner != nullptr)
            bpmScanner->queueTrack(file);
        return;
    }

    // Try reading audio metadata and detect BPM
    bool bpmFound = false;
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    if (reader != nullptr)
    {
        item.duration = (double)reader->lengthInSamples / reader->sampleRate;
        auto title = reader->metadataValues.getValue("title", "");
        auto artist = reader->metadataValues.getValue("artist", "");
        auto album = reader->metadataValues.getValue("album", "");

        item.title = title.isNotEmpty() ? title : file.getFileNameWithoutExtension().replaceCharacter('_', ' ');
        item.artist = artist.isNotEmpty() ? artist : "Artista Local";
        item.album = album.isNotEmpty() ? album : "Local Audio";

        item.title = DeckComponent::sanitizeTrackString(item.title);
        item.artist = DeckComponent::sanitizeTrackString(item.artist);
        item.album = DeckComponent::sanitizeTrackString(item.album);

        if (item.artist.isEmpty() || item.artist == "Artista Local" || item.artist == "Local Artist")
        {
            juce::String splitArtist, splitTitle;
            if (DeckComponent::splitArtistTitle(item.title, splitArtist, splitTitle))
            {
                item.artist = splitArtist;
                item.title = splitTitle;
            }
            else
            {
                item.title = DeckComponent::stripTrackNumberPrefix(item.title);
            }
        }

        // 1. Try reading BPM from metadata tags (ID3 TBPM, bpm, tempo)
        auto bpmStr = reader->metadataValues.getValue("bpm", "");
        if (bpmStr.isEmpty()) bpmStr = reader->metadataValues.getValue("TBPM", "");
        if (bpmStr.isEmpty()) bpmStr = reader->metadataValues.getValue("tempo", "");
        if (bpmStr.isNotEmpty())
        {
            double val = bpmStr.getDoubleValue();
            if (val >= 40.0 && val <= 250.0)
            {
                item.bpm = val;
                bpmFound = true;
            }
        }

        // 2. Try reading Key from metadata
        bool keyFound = false;
        auto keyStr = reader->metadataValues.getValue("initialkey", "");
        if (keyStr.isEmpty()) keyStr = reader->metadataValues.getValue("TKEY", "");
        if (keyStr.isEmpty()) keyStr = reader->metadataValues.getValue("key", "");
        if (keyStr.isNotEmpty())
        {
            auto formatted = KeyDetector::formatKeyString(keyStr);
            if (formatted.isNotEmpty() && formatted != "--")
            {
                item.key = formatted;
                keyFound = true;
            }
        }
        if (!keyFound)
            item.key = "--";

        // 3. If BPM or Key not in metadata, queue for asynchronous background analysis (zero UI freeze)
        if ((!bpmFound || !keyFound) && !isDemo)
        {
            if (bpmScanner != nullptr)
                bpmScanner->queueTrack(file);
        }
    }
    else
    {
        item.title = file.getFileNameWithoutExtension().replaceCharacter('_', ' ');
        juce::String splitArtist, splitTitle;
        if (DeckComponent::splitArtistTitle(item.title, splitArtist, splitTitle))
        {
            item.artist = splitArtist;
            item.title = splitTitle;
        }
        else
        {
            item.title = DeckComponent::stripTrackNumberPrefix(item.title);
            item.artist = "Artista Local";
        }
        item.album = "Local Audio";
        item.duration = 0.0;
        item.key = "--";
    }

    if (isDemo && !bpmFound)
    {
        if (item.title.containsIgnoreCase("Latin") || item.title.containsIgnoreCase("Groove"))
        {
            item.bpm = 124.0;
            item.key = "8A / Am";
            item.album = "Club Sessions Vol. 1";
        }
        else if (item.title.containsIgnoreCase("Sunset") || item.title.containsIgnoreCase("Beach"))
        {
            item.bpm = 126.0;
            item.key = "11A / F#m";
            item.album = "Ibiza Opening 2026";
        }
        else
        {
            item.bpm = 124.0;
            item.key = "8A / Am";
        }
    }
    else if (!bpmFound)
    {
        item.bpm = 0.0;
        if (item.key.isEmpty())
            item.key = "--";
    }

    allTracks.push_back(item);
}

void LibraryComponent::setLibraryMode(LibraryMode mode)
{
    currentMode = mode;
    bool isLocal  = (currentMode == LibraryMode::Local);
    bool isUsb    = (currentMode == LibraryMode::USB);
    bool isYt     = (currentMode == LibraryMode::YouTube);
    bool isAi     = (currentMode == LibraryMode::AIMatch);
    bool isSc     = (currentMode == LibraryMode::SoundCloud);

    railLocalBtn.setCustomColours(isLocal ? juce::Colour::fromRGB(0, 180, 216).withAlpha(0.2f) : juce::Colour::fromRGB(18, 20, 26),
                               juce::Colour::fromRGB(0, 229, 255),
                               juce::Colour::fromRGB(0, 229, 255),
                               isLocal ? juce::Colour::fromRGB(0, 180, 216).withAlpha(0.4f) : juce::Colour::fromRGB(30, 34, 44));

    railUsbBtn.setCustomColours(isUsb ? juce::Colour::fromRGB(52, 211, 153).withAlpha(0.25f) : juce::Colour::fromRGB(18, 20, 26),
                               isUsb ? juce::Colour::fromRGB(110, 231, 183) : juce::Colour::fromRGB(148, 163, 184),
                               isUsb ? juce::Colour::fromRGB(110, 231, 183) : juce::Colour::fromRGB(148, 163, 184),
                               isUsb ? juce::Colour::fromRGB(52, 211, 153).withAlpha(0.4f) : juce::Colour::fromRGB(30, 34, 44));

    railAiBtn.setCustomColours(isAi ? juce::Colour::fromRGB(147, 51, 234).withAlpha(0.25f) : juce::Colour::fromRGB(18, 20, 26),
                               isAi ? juce::Colour::fromRGB(192, 132, 252) : juce::Colour::fromRGB(148, 163, 184),
                               isAi ? juce::Colour::fromRGB(192, 132, 252) : juce::Colour::fromRGB(148, 163, 184),
                               isAi ? juce::Colour::fromRGB(147, 51, 234).withAlpha(0.4f) : juce::Colour::fromRGB(30, 34, 44));

    railYtBtn.setCustomColours(isYt ? juce::Colour::fromRGB(220, 38, 38).withAlpha(0.2f) : juce::Colour::fromRGB(18, 20, 26),
                               isYt ? juce::Colour::fromRGB(239, 68, 68) : juce::Colour::fromRGB(148, 163, 184),
                               isYt ? juce::Colour::fromRGB(239, 68, 68) : juce::Colour::fromRGB(148, 163, 184),
                               isYt ? juce::Colour::fromRGB(220, 38, 38).withAlpha(0.4f) : juce::Colour::fromRGB(30, 34, 44));

    railScBtn.setCustomColours(isSc ? juce::Colour::fromRGB(255, 85, 0).withAlpha(0.25f) : juce::Colour::fromRGB(18, 20, 26),
                               isSc ? juce::Colour::fromRGB(255, 120, 40) : juce::Colour::fromRGB(148, 163, 184),
                               isSc ? juce::Colour::fromRGB(255, 120, 40) : juce::Colour::fromRGB(148, 163, 184),
                                isSc ? juce::Colour::fromRGB(255, 85, 0).withAlpha(0.4f) : juce::Colour::fromRGB(30, 34, 44));

    folderTree.setVisible(isLocal);
    tableHeaderTitle.setVisible(isLocal);
    searchBox.setVisible(isLocal);
    addBtn.setVisible(isLocal);
    table.setVisible(isLocal);
    automixHeader.setVisible(isLocal);
    startAutomixBtn.setVisible(isLocal);
    smartAutomixBtn.setVisible(isLocal);
    clearQueueBtn.setVisible(isLocal && !automixQueue.empty());
    queueTitle.setVisible(isLocal);
    if (automixQueueView != nullptr)
        automixQueueView->setVisible(isLocal);

    updateDownloadsEmptyState();

    if (usbView != nullptr)
    {
        usbView->setVisible(isUsb);
        if (isUsb)
            usbView->refreshDrives();
    }

    if (youtubeView != nullptr)
    {
        youtubeView->setVisible(isYt);
        if (isYt)
            youtubeView->updateApiStatus();
    }

    if (aiMatchView != nullptr)
    {
        aiMatchView->setVisible(isAi);
        if (isAi)
        {
            std::vector<juce::File> files;
            for (const auto& t : allTracks)
                files.push_back(t.file);
            aiMatchView->setLocalLibraryFiles(files);
        }
    }

    if (soundcloudView != nullptr)
    {
        soundcloudView->setVisible(isSc);
    }

    resized();
    repaint();
}

void LibraryComponent::updateActiveTrackContext(const CurrentTrackContext& ctx)
{
    if (aiMatchView != nullptr)
    {
        aiMatchView->updateActiveTrack(ctx);
    }
}

void LibraryComponent::filterTracks(const juce::String& query)
{
    filteredTracks.clear();
    auto q = query.trim().toLowerCase();

    for (const auto& t : allTracks)
    {
        // 1. Check folder matching
        bool matchesFolder = false;
        if (currentSelectedFolder == "Todas las Pistas")
        {
            matchesFolder = true;
        }
        else if (currentSelectedFolder == "Descargas")
        {
            matchesFolder = (t.folder == "Descargas" || t.folder.startsWith("Descargas / "));
        }
        else
        {
            // Explicit user preference: "Mostrar únicamente los temas que estén directamente en la raíz de esa carpeta"
            matchesFolder = (t.folder == currentSelectedFolder);
        }

        if (!matchesFolder)
            continue;

        // 2. Check search query matching
        if (q.isNotEmpty())
        {
            if (!t.title.toLowerCase().contains(q) &&
                !t.artist.toLowerCase().contains(q) &&
                !t.album.toLowerCase().contains(q))
            {
                continue;
            }
        }

        filteredTracks.push_back(t);
    }

    applySorting();

    // Update table header title
    juce::String folderDisplay = currentSelectedFolder;
    if (currentSelectedFolder == "Todas las Pistas")
        folderDisplay = LanguageManager::trUtf8("Todas las Pistas", "All Tracks");
    else if (currentSelectedFolder == "Descargas")
        folderDisplay = LanguageManager::trUtf8("Descargas", "Downloads");
    tableHeaderTitle.setText(folderDisplay + "  " + juce::String((int)filteredTracks.size()) + " " + LanguageManager::trUtf8("Canciones", "Tracks"), juce::dontSendNotification);

    table.updateContent();
    table.repaint();
    updateDownloadsEmptyState();
}

void LibraryComponent::updateDownloadsEmptyState()
{
    if (downloadsEmptyOverlay == nullptr)
        return;

    bool shouldShow = (currentMode == LibraryMode::Local &&
                       currentSelectedFolder == "Descargas" &&
                       filteredTracks.empty());

    if (shouldShow)
    {
        downloadsEmptyOverlay->updateState(YouTubeService::isSaveDownloadsEnabled());
        downloadsEmptyOverlay->setVisible(true);
        downloadsEmptyOverlay->toFront(false);
    }
    else
    {
        downloadsEmptyOverlay->setVisible(false);
    }
}

void LibraryComponent::unsyncFolder(const juce::String& folderPath)
{
    if (folderPath == "Descargas" || folderPath == "Todas las Pistas")
        return;

    allTracks.erase(std::remove_if(allTracks.begin(), allTracks.end(), [&](const TrackItem& t) {
        return t.folder == folderPath ||
               t.folder.startsWith(folderPath + " / ") ||
               t.rootFolderPath == folderPath;
    }), allTracks.end());

    if (currentSelectedFolder == folderPath || currentSelectedFolder.startsWith(folderPath + " / "))
    {
        currentSelectedFolder = "Todas las Pistas";
        folderTree.setSelectedFolder("Todas las Pistas");
    }

    folderTree.updateTree(allTracks);
    filterTracks(searchBox.getText());
}

int LibraryComponent::getNumRows()
{
    return static_cast<int>(filteredTracks.size());
}

void LibraryComponent::paintRowBackground(juce::Graphics& g, int rowNumber, int /*width*/, int /*height*/, bool rowIsSelected)
{
    if (rowIsSelected)
    {
        g.fillAll(juce::Colour::fromRGB(45, 50, 65));
    }
    else if (rowNumber == 0 || rowNumber == 1)
    {
        // Deck 1 / Deck 2 active rows have subtle highlight
        g.fillAll(juce::Colour::fromRGB(20, 24, 34).withAlpha(0.6f));
    }
    else if (rowNumber % 2 == 1)
    {
        g.fillAll(juce::Colour::fromRGB(15, 17, 23));
    }
    else
    {
        g.fillAll(juce::Colour::fromRGB(12, 13, 18));
    }
}

void LibraryComponent::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool /*rowIsSelected*/)
{
    if (rowNumber < 0 || rowNumber >= (int)filteredTracks.size())
        return;

    const auto& track = filteredTracks[static_cast<size_t>(rowNumber)];
    auto cellBounds = juce::Rectangle<int>(0, 0, width, height).reduced(4, 2);

    g.setFont(juce::FontOptions(11.5f));

    switch (columnId)
    {
        case 1: // # or Playing Speaker
        {
            if (rowNumber == 0 || rowNumber == 1)
            {
                auto iconCol = (rowNumber == 0) ? juce::Colour::fromRGB(0, 180, 216) : juce::Colour::fromRGB(0, 229, 255);
                g.setColour(iconCol);

                float cx = cellBounds.toFloat().getCentreX();
                float cy = cellBounds.toFloat().getCentreY();

                juce::Path speaker;
                speaker.startNewSubPath(cx - 5.5f, cy - 2.5f);
                speaker.lineTo(cx - 2.5f, cy - 2.5f);
                speaker.lineTo(cx + 1.5f, cy - 5.5f);
                speaker.lineTo(cx + 1.5f, cy + 5.5f);
                speaker.lineTo(cx - 2.5f, cy + 2.5f);
                speaker.lineTo(cx - 5.5f, cy + 2.5f);
                speaker.closeSubPath();
                g.fillPath(speaker);

                juce::Path wave1;
                wave1.addCentredArc(cx + 1.0f, cy, 4.5f, 4.5f, 0.0f, -0.6f, 0.6f, true);
                g.strokePath(wave1, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                juce::Path wave2;
                wave2.addCentredArc(cx + 1.0f, cy, 7.5f, 7.5f, 0.0f, -0.7f, 0.7f, true);
                g.strokePath(wave2, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            else
            {
                g.setColour(juce::Colour::fromRGB(142, 149, 165));
                g.setFont(juce::FontOptions(11.0f));
                g.drawText(juce::String(rowNumber + 1), cellBounds, juce::Justification::centred, false);
            }
            break;
        }

        case 2: // Album Cover Art Thumbnail
        {
            auto thumb = cellBounds.reduced(3, 4);
            auto cover = CoverArtGenerator::getCoverForTrack(track.title, 128);

            juce::Path p;
            p.addRoundedRectangle(thumb.toFloat(), 3.0f);
            juce::Graphics::ScopedSaveState state(g);
            g.reduceClipRegion(p);
            g.drawImage(cover, thumb.toFloat(), juce::RectanglePlacement::fillDestination);
            g.setColour(juce::Colour::fromRGB(45, 52, 68));
            g.drawRoundedRectangle(thumb.toFloat(), 3.0f, 1.0f);
            break;
        }

        case 3: // Título
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
            g.drawText(track.title, cellBounds, juce::Justification::centredLeft, true);
            break;

        case 4: // Artista
            g.setColour(juce::Colour::fromRGB(142, 149, 165));
            g.drawText(track.artist, cellBounds, juce::Justification::centredLeft, true);
            break;

        case 5: // Álbum
            g.setColour(juce::Colour::fromRGB(142, 149, 165));
            g.drawText(track.album, cellBounds, juce::Justification::centredLeft, true);
            break;

        case 6: // Duración
            g.setColour(juce::Colour::fromRGB(142, 149, 165));
            g.setFont(juce::FontOptions(11.5f));
            g.drawText(formatDuration(track.duration), cellBounds, juce::Justification::centred, false);
            break;

        case 7: // BPM
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
            g.drawText(formatBpm(track.bpm), cellBounds, juce::Justification::centred, false);
            break;

        case 8: // Key
        {
            auto pill = cellBounds.reduced(3, 6).toFloat();
            auto keyCol = KeyDetector::getCamelotColour(track.key);
            g.setColour(keyCol.withAlpha(0.16f));
            g.fillRoundedRectangle(pill, 4.0f);
            g.setColour(keyCol.withAlpha(0.40f));
            g.drawRoundedRectangle(pill, 4.0f, 1.0f);

            g.setColour(keyCol.brighter(0.25f));
            g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
            juce::String dispKey = KeyDetector::formatKeyString(track.key);
            g.drawText(dispKey, pill, juce::Justification::centred, false);
            break;
        }

        default:
            break;
    }
}

juce::Component* LibraryComponent::refreshComponentForCell(int rowNumber, int columnId, bool /*isRowSelected*/, juce::Component* existingComponentToUpdate)
{
    if (columnId == 9) // Column 9: CARGAR
    {
        auto* comp = static_cast<LoadButtonsComponent*>(existingComponentToUpdate);
        if (comp == nullptr)
        {
            comp = new LoadButtonsComponent([this, rowNumber](int deckIndex) {
                if (rowNumber >= 0 && rowNumber < (int)filteredTracks.size() && onLoadTrack)
                {
                    onLoadTrack(deckIndex, filteredTracks[static_cast<size_t>(rowNumber)].file);
                }
            });
        }
        return comp;
    }

    return nullptr;
}

void LibraryComponent::cellDoubleClicked(int rowNumber, int /*columnId*/, const juce::MouseEvent&)
{
    if (rowNumber >= 0 && rowNumber < (int)filteredTracks.size() && onLoadTrack)
    {
        onLoadTrack(0, filteredTracks[static_cast<size_t>(rowNumber)].file);
    }
}

juce::var LibraryComponent::getDragSourceDescription(const juce::SparseSet<int>& currentlySelectedRows)
{
    juce::StringArray paths;
    for (int i = 0; i < currentlySelectedRows.size(); ++i)
    {
        int row = currentlySelectedRows[i];
        if (row >= 0 && row < (int)filteredTracks.size())
            paths.add(filteredTracks[static_cast<size_t>(row)].file.getFullPathName());
    }
    if (paths.isEmpty()) return {};
    return paths.joinIntoString("\n");
}

void LibraryComponent::sortOrderChanged(int newSortColumnId, bool isForwards)
{
    currentSortColumnId = newSortColumnId;
    currentSortForwards = isForwards;
    applySorting();
    table.updateContent();
    table.repaint();
}

static int parseCamelotOrder(const juce::String& key)
{
    auto k = key.trim().toUpperCase();
    if (k.isEmpty() || k == "--") return 999;
    int num = k.getIntValue();
    bool isB = k.endsWith("B");
    return (num * 2) + (isB ? 1 : 0);
}

void LibraryComponent::applySorting()
{
    if (currentSortColumnId <= 0 || filteredTracks.empty())
        return;

    std::stable_sort(filteredTracks.begin(), filteredTracks.end(), [this](const TrackItem& a, const TrackItem& b) {
        int res = 0;
        switch (currentSortColumnId)
        {
            case 3: // TÍTULO
                res = a.title.compareNatural(b.title);
                break;
            case 4: // ARTISTA
                res = a.artist.compareNatural(b.artist);
                break;
            case 5: // ÁLBUM
                res = a.album.compareNatural(b.album);
                break;
            case 6: // DURACIÓN
                if (a.duration < b.duration) res = -1;
                else if (a.duration > b.duration) res = 1;
                break;
            case 7: // BPM
                if (a.bpm < b.bpm) res = -1;
                else if (a.bpm > b.bpm) res = 1;
                break;
            case 8: // KEY
            {
                int orderA = parseCamelotOrder(a.key);
                int orderB = parseCamelotOrder(b.key);
                if (orderA < orderB) res = -1;
                else if (orderA > orderB) res = 1;
                else res = a.key.compareNatural(b.key);
                break;
            }
            default:
                break;
        }
        return currentSortForwards ? (res < 0) : (res > 0);
    });
}

void LibraryComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Dark Library base background #0c0d12
    g.fillAll(juce::Colour::fromRGB(12, 13, 18));

    // Top border separating Decks/Mixer from Library
    g.setColour(juce::Colour::fromRGB(28, 32, 42));
    g.drawHorizontalLine(0, 0.0f, bounds.getWidth());

    // Panel vertical dividers
    g.drawVerticalLine(42, 0.0f, bounds.getHeight());  // after icon rail

    if (currentMode != LibraryMode::Local)
    {
        return;
    }

    g.drawVerticalLine(258, 0.0f, bounds.getHeight()); // after "Mis Archivos" sidebar (42 + 216)
    g.drawVerticalLine(getWidth() - 250, 0.0f, bounds.getHeight()); // before Automix panel
}

void LibraryComponent::resized()
{
    auto area = getLocalBounds();

    // 1. Far-Left Vertical Icon Rail (42px)
    auto railArea = area.removeFromLeft(42).reduced(4, 6);
    railLocalBtn.setBounds(railArea.removeFromTop(32).reduced(1, 1));
    railArea.removeFromTop(4);
    railUsbBtn.setBounds(railArea.removeFromTop(32).reduced(1, 1));
    railArea.removeFromTop(4);
    railAiBtn.setBounds(railArea.removeFromTop(32).reduced(1, 1));
    railArea.removeFromTop(4);
    railYtBtn.setBounds(railArea.removeFromTop(32).reduced(1, 1));
    railArea.removeFromTop(4);
    railScBtn.setBounds(railArea.removeFromTop(32).reduced(1, 1));

    if (currentMode == LibraryMode::USB)
    {
        if (usbView != nullptr)
            usbView->setBounds(area);
        return;
    }

    if (currentMode == LibraryMode::YouTube)
    {
        if (youtubeView != nullptr)
            youtubeView->setBounds(area);
        return;
    }

    if (currentMode == LibraryMode::AIMatch)
    {
        if (aiMatchView != nullptr)
            aiMatchView->setBounds(area);
        return;
    }

    if (currentMode == LibraryMode::SoundCloud)
    {
        if (soundcloudView != nullptr)
            soundcloudView->setBounds(area);
        return;
    }

    // 2. "Mis Archivos" Folder Tree Sidebar (216px)
    auto sidebarArea = area.removeFromLeft(216);
    folderTree.setBounds(sidebarArea);

    // 3. Right Automix Panel (250px)
    auto automixArea = area.removeFromRight(250).reduced(8, 8);
    automixHeader.setBounds(automixArea.removeFromTop(22));
    automixArea.removeFromTop(4);
    startAutomixBtn.setBounds(automixArea.removeFromTop(26).reduced(2, 0));
    automixArea.removeFromTop(6);
    smartAutomixBtn.setBounds(automixArea.removeFromTop(26).reduced(2, 0));
    automixArea.removeFromTop(8);
    auto queueRow = automixArea.removeFromTop(18);
    if (clearQueueBtn.isVisible())
    {
        queueTitle.setBounds(queueRow.removeFromLeft(queueRow.getWidth() - 22));
        clearQueueBtn.setBounds(queueRow.reduced(2, 0));
    }
    else
    {
        queueTitle.setBounds(queueRow);
    }
    automixArea.removeFromTop(6);
    automixDropArea = automixArea.toFloat();
    if (automixQueueView != nullptr)
        automixQueueView->setBounds(automixArea);

    // 4. Center Track Table Area (remaining width)
    auto centerArea = area.reduced(8, 6);
    auto searchBarRow = centerArea.removeFromTop(28);

    tableHeaderTitle.setBounds(searchBarRow.removeFromLeft(180));
    addBtn.setBounds(searchBarRow.removeFromRight(76));
    searchBarRow.removeFromRight(6);
    searchBox.setBounds(searchBarRow.reduced(2, 0));

    centerArea.removeFromTop(6);
    table.setBounds(centerArea);

    if (downloadsEmptyOverlay != nullptr)
    {
        auto emptyArea = centerArea;
        int headerH = table.getHeader().getHeight();
        if (headerH > 0)
            emptyArea.removeFromTop(headerH);
        downloadsEmptyOverlay->setBounds(emptyArea);
    }
}

bool LibraryComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress('a', juce::ModifierKeys::commandModifier, 0))
    {
        int numRows = getNumRows();
        if (numRows > 0)
        {
            table.selectRangeOfRows(0, numRows - 1);
        }
        return true;
    }
    return false;
}

bool LibraryComponent::isInterestedInDragSource(const SourceDetails& dragSourceDetails)
{
    // In JUCE, localPosition is (0,0) during this initial inquiry,
    // so we accept any non-empty drag description here.
    return dragSourceDetails.description.toString().isNotEmpty();
}

void LibraryComponent::itemDragEnter(const SourceDetails& dragSourceDetails)
{
    bool hover = automixDropArea.contains(dragSourceDetails.localPosition.toFloat());
    if (hover != automixDropHover)
    {
        automixDropHover = hover;
        repaint();
    }
}

void LibraryComponent::itemDragMove(const SourceDetails& dragSourceDetails)
{
    bool hover = automixDropArea.contains(dragSourceDetails.localPosition.toFloat());
    if (hover != automixDropHover)
    {
        automixDropHover = hover;
        repaint();
    }
}

void LibraryComponent::itemDragExit(const SourceDetails&)
{
    if (automixDropHover)
    {
        automixDropHover = false;
        repaint();
    }
}

void LibraryComponent::itemDropped(const SourceDetails& dragSourceDetails)
{
    automixDropHover = false;

    // Check if the drop happened over the Automix drop zone or panel
    if (automixDropArea.contains(dragSourceDetails.localPosition.toFloat()) || dragSourceDetails.localPosition.x >= getWidth() - 250)
    {
        handleAutomixDropTracks(dragSourceDetails.description.toString());
    }

    repaint();
}

void LibraryComponent::handleAutomixDropTracks(const juce::String& description)
{
    juce::StringArray paths;
    paths.addTokens(description, "\n", "");

    std::vector<TrackItem> tracksToAdd;
    for (const auto& path : paths)
    {
        auto trimmed = path.trim();
        if (trimmed.isEmpty()) continue;

        bool found = false;
        // 1. Search in allTracks
        for (const auto& track : allTracks)
        {
            if (track.file.getFullPathName() == trimmed || track.file == juce::File(trimmed))
            {
                // Excluir audios de menos de 60s (samples / jingles)
                if (track.duration <= 0.0 || track.duration >= 60.0)
                    tracksToAdd.push_back(track);
                found = true;
                break;
            }
        }
        // 2. Search in filteredTracks
        if (!found)
        {
            for (const auto& track : filteredTracks)
            {
                if (track.file.getFullPathName() == trimmed || track.file == juce::File(trimmed))
                {
                    if (track.duration <= 0.0 || track.duration >= 60.0)
                        tracksToAdd.push_back(track);
                    found = true;
                    break;
                }
            }
        }
        // 3. Fallback: if it's a valid local file dropped
        if (!found)
        {
            juce::File f(trimmed);
            if (f.existsAsFile())
            {
                TrackItem ti;
                ti.id = juce::String(juce::Random::getSystemRandom().nextInt());
                ti.file = f;
                ti.title = f.getFileNameWithoutExtension().replaceCharacter('_', ' ');
                ti.artist = "Artista Local";
                ti.album = "Local Audio";
                ti.bpm = 124.0;
                ti.key = "8A / Am";
                ti.duration = 180.0;

                std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(f));
                if (reader != nullptr)
                {
                    ti.duration = (double)reader->lengthInSamples / reader->sampleRate;
                    auto metaTitle = reader->metadataValues.getValue("title", "");
                    auto metaArtist = reader->metadataValues.getValue("artist", "");
                    auto metaAlbum = reader->metadataValues.getValue("album", "");
                    if (metaTitle.isNotEmpty()) ti.title = metaTitle;
                    if (metaArtist.isNotEmpty()) ti.artist = metaArtist;
                    if (metaAlbum.isNotEmpty()) ti.album = metaAlbum;

                    auto bpmStr = reader->metadataValues.getValue("bpm", "");
                    if (bpmStr.isEmpty()) bpmStr = reader->metadataValues.getValue("TBPM", "");
                    if (bpmStr.isEmpty()) bpmStr = reader->metadataValues.getValue("tempo", "");
                    if (bpmStr.isNotEmpty())
                    {
                        double val = bpmStr.getDoubleValue();
                        if (val >= 40.0 && val <= 250.0)
                            ti.bpm = val;
                    }
                    else
                    {
                        auto beatRes = BeatDetector::analyze(reader.get());
                        if (beatRes.bpm > 0.0)
                            ti.bpm = beatRes.bpm;
                    }

                    auto keyStr = reader->metadataValues.getValue("initialkey", "");
                    if (keyStr.isEmpty()) keyStr = reader->metadataValues.getValue("TKEY", "");
                    if (keyStr.isEmpty()) keyStr = reader->metadataValues.getValue("key", "");
                    if (keyStr.isNotEmpty()) ti.key = KeyDetector::formatKeyString(keyStr);
                }

                if (ti.duration <= 0.0 || ti.duration >= 60.0)
                    tracksToAdd.push_back(ti);
            }
        }
    }

    if (!tracksToAdd.empty())
        addToAutomixQueue(tracksToAdd);
}

void LibraryComponent::addToAutomixQueue(const std::vector<TrackItem>& tracks)
{
    for (const auto& t : tracks)
        automixQueue.push_back(t);

    clearQueueBtn.setVisible(true);

    if (!automixRunning)
    {
        startAutomixBtn.setCustomColours(juce::Colour::fromRGB(37, 99, 235).withAlpha(0.25f),
                                         juce::Colour::fromRGB(147, 197, 253),
                                         juce::Colour::fromRGB(147, 197, 253),
                                         juce::Colour::fromRGB(59, 130, 246).withAlpha(0.4f));
    }

    queueTitle.setText(LanguageManager::trUtf8("COLA · ", "QUEUE · ") + juce::String((int)automixQueue.size()) + LanguageManager::trUtf8(" pistas", " tracks"),
                       juce::dontSendNotification);

    if (automixQueueView != nullptr)
        automixQueueView->updateContent();

    repaint();
}

void LibraryComponent::clearAutomixQueue()
{
    automixQueue.clear();
    automixPlayingIndex = -1;
    clearQueueBtn.setVisible(false);

    startAutomixBtn.setCustomColours(juce::Colour::fromRGB(22, 25, 34),
                                     juce::Colour::fromRGB(142, 149, 165),
                                     juce::Colour::fromRGB(142, 149, 165),
                                     juce::Colour::fromRGB(38, 43, 56));

    queueTitle.setText(LanguageManager::trUtf8("COLA DE REPRODUCCIÓN", "PLAYBACK QUEUE"), juce::dontSendNotification);
    queueTitle.setTooltip("");

    if (automixQueueView != nullptr)
        automixQueueView->updateContent();

    repaint();
}

void LibraryComponent::setAutomixQueue(const std::vector<TrackItem>& tracks)
{
    // Reemplazo total (compone clear + add para reutilizar sus estados UI)
    automixQueue.clear();
    clearQueueBtn.setVisible(false);
    addToAutomixQueue(tracks);
}

void LibraryComponent::setAutomixPlayingIndex(int index)
{
    automixPlayingIndex = index;
    if (automixQueueView != nullptr)
        automixQueueView->updateContent();
    repaint();
}

void LibraryComponent::deleteTrackFromAutomixQueue(int index)
{
    if (index < 0 || index >= (int)automixQueue.size())
        return;

    automixQueue.erase(automixQueue.begin() + index);

    if (onAutomixQueueChanged)
        onAutomixQueueChanged(automixQueue);

    clearQueueBtn.setVisible(!automixQueue.empty());
    if (automixQueue.empty())
    {
        queueTitle.setText(LanguageManager::trUtf8("COLA DE REPRODUCCIÓN", "PLAYBACK QUEUE"), juce::dontSendNotification);
        setAutomixRunning(false);
        if (onStopAutomix)
            onStopAutomix();
    }
    else
    {
        queueTitle.setText(LanguageManager::trUtf8("COLA · ", "QUEUE · ") + juce::String((int)automixQueue.size()) + LanguageManager::trUtf8(" pistas", " tracks"),
                           juce::dontSendNotification);
    }

    if (automixQueueView != nullptr)
        automixQueueView->updateContent();

    repaint();
}

void LibraryComponent::reorderAutomixQueue(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= (int)automixQueue.size() ||
        toIndex < 0 || toIndex >= (int)automixQueue.size() || fromIndex == toIndex)
        return;

    auto item = automixQueue[(size_t)fromIndex];
    automixQueue.erase(automixQueue.begin() + fromIndex);
    automixQueue.insert(automixQueue.begin() + toIndex, item);

    if (onAutomixQueueChanged)
        onAutomixQueueChanged(automixQueue);

    if (automixQueueView != nullptr)
        automixQueueView->updateContent();

    repaint();
}

void LibraryComponent::setQueueMessage(const juce::String& message)
{
    queueTitle.setText(message, juce::dontSendNotification);
    queueTitle.setTooltip(message);
    repaint();
}

void LibraryComponent::setSmartBusy(bool busy)
{
    smartBusy = busy;
    startAutomixBtn.setEnabled(!busy);
    smartAutomixBtn.setEnabled(!busy);
    refreshLanguage();
    repaint();
}

std::vector<TrackItem> LibraryComponent::allLocalTrackItems() const
{
    std::vector<TrackItem> items = allTracks;
    if (usbView != nullptr)
    {
        const auto& usbTracks = usbView->getAllTracks();
        items.insert(items.end(), usbTracks.begin(), usbTracks.end());
    }
    return items;
}

void LibraryComponent::setAutomixRunning(bool running)
{
    automixRunning = running;
    if (running)
    {
        startAutomixBtn.setText(LanguageManager::trUtf8("Detener Automix", "Stop Automix"));
        startAutomixBtn.setIcon(LucideIcons::IconType::Pause, 12.0f);
        startAutomixBtn.setCustomColours(juce::Colour::fromRGB(220, 38, 38).withAlpha(0.2f),
                                         juce::Colour::fromRGB(252, 165, 165),
                                         juce::Colour::fromRGB(252, 165, 165),
                                         juce::Colour::fromRGB(220, 38, 38).withAlpha(0.4f));
        clearQueueBtn.setVisible(false);
    }
    else
    {
        startAutomixBtn.setText(LanguageManager::trUtf8("Comenzar Automix", "Start Automix"));
        startAutomixBtn.setIcon(LucideIcons::IconType::Sparkles, 12.0f);
        clearQueueBtn.setVisible(!automixQueue.empty());

        if (automixQueue.empty())
        {
            startAutomixBtn.setCustomColours(juce::Colour::fromRGB(22, 25, 34),
                                             juce::Colour::fromRGB(142, 149, 165),
                                             juce::Colour::fromRGB(142, 149, 165),
                                             juce::Colour::fromRGB(38, 43, 56));
            queueTitle.setText(LanguageManager::trUtf8("COLA DE REPRODUCCIÓN", "PLAYBACK QUEUE"), juce::dontSendNotification);
        }
        else
        {
            startAutomixBtn.setCustomColours(juce::Colour::fromRGB(37, 99, 235).withAlpha(0.25f),
                                             juce::Colour::fromRGB(147, 197, 253),
                                             juce::Colour::fromRGB(147, 197, 253),
                                             juce::Colour::fromRGB(59, 130, 246).withAlpha(0.4f));
        }
    }

    if (automixQueueView != nullptr)
        automixQueueView->updateContent();

    repaint();
}

void LibraryComponent::navigateBrowser(int delta)
{
    if (isBrowsingFolders)
    {
        folderTree.navigateFolders(delta);
    }
    else
    {
        int numRows = getNumRows();
        if (numRows <= 0)
            return;

        int curRow = table.getSelectedRow();
        if (curRow < 0)
            curRow = 0;
        else
            curRow = juce::jlimit(0, numRows - 1, curRow + delta);

        table.selectRow(curRow, false, true);
        table.scrollToEnsureRowIsOnscreen(curRow);
    }
    repaint();
}

void LibraryComponent::handleBrowseClick()
{
    if (isBrowsingFolders)
    {
        // If current folder has subfolders: expand/collapse it
        bool expandedOrCollapsed = folderTree.toggleExpandCurrentFolder();
        if (!expandedOrCollapsed)
        {
            // Leaf folder: switch focus directly to track table
            isBrowsingFolders = false;
            if (table.getSelectedRow() < 0 && getNumRows() > 0)
            {
                table.selectRow(0, false, true);
                table.scrollToEnsureRowIsOnscreen(0);
            }
        }
    }
    else
    {
        // In song list: toggle back to browsing folder tree
        isBrowsingFolders = true;
    }
    repaint();
}

juce::File LibraryComponent::getSelectedTrackFile() const
{
    int row = table.getSelectedRow();
    if (row >= 0 && row < static_cast<int>(filteredTracks.size()))
    {
        return filteredTracks[static_cast<size_t>(row)].file;
    }
    if (!filteredTracks.empty())
    {
        return filteredTracks[0].file;
    }
    return {};
}

void LibraryComponent::loadSelectedTrack(int deckIndex)
{
    int row = table.getSelectedRow();
    if (row < 0 && !filteredTracks.empty())
    {
        row = 0;
        table.selectRow(0, false, true);
    }

    if (row >= 0 && row < static_cast<int>(filteredTracks.size()) && onLoadTrack)
    {
        onLoadTrack(deckIndex, filteredTracks[static_cast<size_t>(row)].file);
    }
}

void LibraryComponent::updateTrackBpmAndKey(const juce::File& file, double bpm, const juce::String& key)
{
    if (bpm <= 0.0 && (key.isEmpty() || key == "--"))
        return;

    LibraryCacheManager::getInstance().updateBpmAndKey(file, bpm, key);

    bool updated = false;
    for (auto& t : allTracks)
    {
        if (t.file == file)
        {
            if (bpm > 0.0) t.bpm = bpm;
            if (key.isNotEmpty() && key != "--") t.key = key;
            updated = true;
            break;
        }
    }

    for (auto& t : filteredTracks)
    {
        if (t.file == file)
        {
            if (bpm > 0.0) t.bpm = bpm;
            if (key.isNotEmpty() && key != "--") t.key = key;
            updated = true;
            break;
        }
    }

    for (auto& t : automixQueue)
    {
        if (t.file == file)
        {
            if (bpm > 0.0) t.bpm = bpm;
            if (key.isNotEmpty() && key != "--") t.key = key;
            updated = true;
            break;
        }
    }

    if (updated)
    {
        tableNeedsRepaint = true;
        if (!isTimerRunning())
            startTimer(500);
    }
}

void LibraryComponent::updateTrackBpm(const juce::File& file, double bpm)
{
    if (bpm <= 0.0)
        return;
    updateTrackBpmAndKey(file, bpm, {});
}

void LibraryComponent::updateTrackKey(const juce::File& file, const juce::String& key)
{
    if (key.isEmpty() || key == "--")
        return;
    updateTrackBpmAndKey(file, 0.0, key);
}

