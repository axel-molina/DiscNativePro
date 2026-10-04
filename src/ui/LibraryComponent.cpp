#include "LibraryComponent.h"
#include "YouTubeLibraryView.h"
#include "../audio/SampleTrackGenerator.h"
#include "../audio/BeatDetector.h"
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

LibraryComponent::LibraryComponent(juce::AudioFormatManager& fm)
    : formatManager(fm)
{
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
    setupRailIcon(railIcon1, LucideIcons::IconType::Music, juce::Colour::fromRGB(0, 229, 255), true); // Active music
    setupRailIcon(railIcon2, LucideIcons::IconType::Layers, juce::Colour::fromRGB(244, 63, 94), false); // Coral / Playlists
    setupRailIcon(railIcon3, LucideIcons::IconType::Disc, juce::Colour::fromRGB(46, 196, 182), false); // Vinyl
    setupRailIcon(railIcon4, LucideIcons::IconType::Tv, juce::Colour::fromRGB(148, 163, 184), false); // TV
    setupRailIcon(railIcon5, LucideIcons::IconType::MoreHorizontal, juce::Colour::fromRGB(148, 163, 184), false); // ...

    railIcon1.onClick = [this]() { setLibraryMode(LibraryMode::Local); };
    railIcon4.onClick = [this]() { setLibraryMode(LibraryMode::YouTube); };

    // YouTube View
    youtubeView = std::make_unique<YouTubeLibraryView>();
    youtubeView->onLoadTrack = [this](int deckIndex, const YouTubeSearchResult& video) {
        if (onLoadYouTubeTrack)
            onLoadYouTubeTrack(deckIndex, video);
    };
    addChildComponent(*youtubeView);

    // 2. "Mis Archivos" Hierarchical Folder Tree
    folderTree.onSelectFolder = [this](const juce::String& folderPath) {
        currentSelectedFolder = folderPath;
        filterTracks(searchBox.getText());
    };
    folderTree.onAddFolder = [this]() {
        importFolder();
    };
    folderTree.onUnsyncFolder = [this](const juce::String& folderPath) {
        unsyncFolder(folderPath);
    };
    addAndMakeVisible(folderTree);

    // 3. Center Table Area Header
    tableHeaderTitle.setText(juce::String::fromUTF8("Todas las Pistas  5 Canciones"), juce::dontSendNotification);
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
    table.getHeader().addColumn("KEY", 8, 52, 42, 70);
    table.getHeader().addColumn("CARGAR", 9, 74, 68, 85, juce::TableHeaderComponent::notSortable);
    table.getHeader().setColour(juce::TableHeaderComponent::backgroundColourId, juce::Colour::fromRGB(16, 18, 24));
    table.getHeader().setColour(juce::TableHeaderComponent::textColourId, juce::Colour::fromRGB(142, 149, 165));
    table.setRowHeight(34);
    table.setMultipleSelectionEnabled(true);
    addAndMakeVisible(table);

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

    setWantsKeyboardFocus(true);

    // Ensure default DiscPro synthesized demo tracks are present on startup
    auto samplesDir = juce::File::getSpecialLocation(juce::File::userMusicDirectory).getChildFile("DiscPro_Samples");
    std::vector<juce::File> demoFiles;
    SampleTrackGenerator::ensureDefaultDemoTracks(samplesDir, demoFiles);
    for (const auto& df : demoFiles)
    {
        addTrack(df, "Demos DiscPro", {}, true);
    }

    // Add extra initial tracks matching DiscPro INITIAL_DEMO_TRACKS list
    if (allTracks.size() < 5)
    {
        TrackItem t3;
        t3.id = "3";
        t3.title = "El Murguero (Club Edit)";
        t3.artist = juce::String::fromUTF8("Los Auténticos Rmx");
        t3.album = "Carioca Fiesta";
        t3.bpm = 115.0;
        t3.key = "11A";
        t3.duration = 180.0;
        t3.file = samplesDir.getChildFile("DiscPro_Latin_Tech_Groove.wav");
        t3.folder = "Demos DiscPro";
        t3.isDemo = true;
        allTracks.push_back(t3);

        TrackItem t4;
        t4.id = "4";
        t4.title = "Los Piratas (Original Remaster)";
        t4.artist = juce::String::fromUTF8("Auténticos Sonideros");
        t4.album = "Carioca Fiesta";
        t4.bpm = 140.0;
        t4.key = "11A";
        t4.duration = 165.0;
        t4.file = samplesDir.getChildFile("DiscPro_Latin_Tech_Groove.wav");
        t4.folder = "Demos DiscPro";
        t4.isDemo = true;
        allTracks.push_back(t4);

        TrackItem t5;
        t5.id = "5";
        t5.title = "Cyberfunk Peak Time";
        t5.artist = "Trax Modular";
        t5.album = "Berlin Under";
        t5.bpm = 128.0;
        t5.key = "4A";
        t5.duration = 75.0;
        t5.file = samplesDir.getChildFile("Sunset_Beach_House_Mix.wav");
        t5.folder = "Demos DiscPro";
        t5.isDemo = true;
        allTracks.push_back(t5);
    }

    filteredTracks = allTracks;
    folderTree.updateTree(allTracks);
    table.updateContent();
}

LibraryComponent::~LibraryComponent()
{
}

void LibraryComponent::importFiles()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Seleccionar archivos de musica...",
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
        "Seleccionar carpeta con musica...",
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
    juce::Array<juce::File> files;
    folder.findChildFiles(files, juce::File::findFiles, true, "*.wav;*.mp3;*.flac;*.aiff;*.aif;*.m4a;*.ogg;*.aac");

    auto rootName = folder.getFileName();
    auto rootPath = folder.getFullPathName();

    for (const auto& f : files)
    {
        auto parentDir = f.getParentDirectory();
        juce::String folderTag = rootName;

        if (parentDir != folder)
        {
            auto relDirPath = parentDir.getRelativePathFrom(folder);
            juce::String cleanRel = relDirPath.replaceCharacter('\\', '/');
            juce::StringArray subParts;
            subParts.addTokens(cleanRel, "/", "\"");
            subParts.removeEmptyStrings();

            if (!subParts.isEmpty())
            {
                folderTag = rootName + " / " + subParts.joinIntoString(" / ");
            }
        }

        addTrack(f, folderTag, rootPath, false);
    }
}

void LibraryComponent::addTrack(const juce::File& file, const juce::String& folderTag, const juce::String& rootFolder, bool isDemo)
{
    // Check if already in library
    for (const auto& t : allTracks)
    {
        if (t.file == file) return;
    }

    TrackItem item;
    item.file = file;
    item.folder = folderTag;
    item.rootFolderPath = rootFolder;
    item.isDemo = isDemo;
    item.id = juce::String(allTracks.size() + 1);

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
        auto keyStr = reader->metadataValues.getValue("initialkey", "");
        if (keyStr.isEmpty()) keyStr = reader->metadataValues.getValue("TKEY", "");
        if (keyStr.isEmpty()) keyStr = reader->metadataValues.getValue("key", "");
        if (keyStr.isNotEmpty())
        {
            item.key = keyStr;
        }

        // 3. If BPM not in metadata, analyze audio content with BeatDetector
        if (!bpmFound)
        {
            auto beatRes = BeatDetector::analyze(reader.get());
            if (beatRes.bpm > 0.0)
            {
                item.bpm = beatRes.bpm;
                bpmFound = true;
            }
        }
    }
    else
    {
        item.title = file.getFileNameWithoutExtension().replaceCharacter('_', ' ');
        item.artist = "Artista Local";
        item.album = "Local Audio";
        item.duration = 0.0;
    }

    if (!bpmFound)
    {
        if (item.title.containsIgnoreCase("Latin") || item.title.containsIgnoreCase("Groove"))
        {
            item.bpm = 124.0;
            item.key = "8A";
            item.album = "Club Sessions Vol. 1";
        }
        else if (item.title.containsIgnoreCase("Sunset") || item.title.containsIgnoreCase("Beach"))
        {
            item.bpm = 126.0;
            item.key = "11A";
            item.album = "Ibiza Opening 2026";
        }
        else
        {
            item.bpm = 124.0;
            item.key = "8A";
        }
    }

    allTracks.push_back(item);
}

void LibraryComponent::setLibraryMode(LibraryMode mode)
{
    currentMode = mode;
    bool isYt = (currentMode == LibraryMode::YouTube);

    railIcon1.setCustomColours(!isYt ? juce::Colour::fromRGB(0, 180, 216).withAlpha(0.2f) : juce::Colour::fromRGB(18, 20, 26),
                              juce::Colour::fromRGB(0, 229, 255),
                              juce::Colour::fromRGB(0, 229, 255),
                              !isYt ? juce::Colour::fromRGB(0, 180, 216).withAlpha(0.4f) : juce::Colour::fromRGB(30, 34, 44));
    railIcon4.setCustomColours(isYt ? juce::Colour::fromRGB(220, 38, 38).withAlpha(0.2f) : juce::Colour::fromRGB(18, 20, 26),
                              isYt ? juce::Colour::fromRGB(239, 68, 68) : juce::Colour::fromRGB(148, 163, 184),
                              isYt ? juce::Colour::fromRGB(239, 68, 68) : juce::Colour::fromRGB(148, 163, 184),
                              isYt ? juce::Colour::fromRGB(220, 38, 38).withAlpha(0.4f) : juce::Colour::fromRGB(30, 34, 44));

    folderTree.setVisible(!isYt);
    tableHeaderTitle.setVisible(!isYt);
    searchBox.setVisible(!isYt);
    addBtn.setVisible(!isYt);
    table.setVisible(!isYt);
    automixHeader.setVisible(!isYt);
    startAutomixBtn.setVisible(!isYt);
    clearQueueBtn.setVisible(!isYt && !automixQueue.empty());
    queueTitle.setVisible(!isYt);

    if (youtubeView != nullptr)
    {
        youtubeView->setVisible(isYt);
        if (isYt)
            youtubeView->updateApiStatus();
    }

    resized();
    repaint();
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
        else if (currentSelectedFolder == "Demos DiscPro")
        {
            matchesFolder = (t.isDemo || t.folder == "Demos DiscPro");
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

    // Update table header title
    tableHeaderTitle.setText(currentSelectedFolder + "  " + juce::String(filteredTracks.size()) + " Canciones", juce::dontSendNotification);

    table.updateContent();
    table.repaint();
}

void LibraryComponent::unsyncFolder(const juce::String& folderPath)
{
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
            g.drawText(juce::String(track.bpm, 1), cellBounds, juce::Justification::centred, false);
            break;

        case 8: // Key
        {
            auto pill = cellBounds.reduced(4, 7).toFloat();
            g.setColour(juce::Colour::fromRGB(0, 180, 216).withAlpha(0.15f));
            g.fillRoundedRectangle(pill, 3.0f);
            g.setColour(juce::Colour::fromRGB(0, 180, 216).withAlpha(0.35f));
            g.drawRoundedRectangle(pill, 3.0f, 1.0f);

            g.setColour(juce::Colour::fromRGB(0, 229, 255));
            g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
            g.drawText(track.key, pill, juce::Justification::centred, false);
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

    if (currentMode == LibraryMode::YouTube)
    {
        return;
    }

    g.drawVerticalLine(258, 0.0f, bounds.getHeight()); // after "Mis Archivos" sidebar (42 + 216)
    g.drawVerticalLine(getWidth() - 250, 0.0f, bounds.getHeight()); // before Automix panel

    // Automix Dropzone
    juce::Rectangle<float> dropArea((float)getWidth() - 240.0f, 74.0f, 230.0f, (float)getHeight() - 86.0f);
    g.setColour(automixDropHover ? juce::Colour::fromRGB(37, 99, 235).withAlpha(0.1f) : juce::Colour::fromRGB(16, 18, 25));
    g.fillRoundedRectangle(dropArea, 8.0f);

    g.setColour(automixDropHover ? juce::Colour::fromRGB(96, 165, 250).withAlpha(0.6f) : juce::Colour::fromRGB(36, 42, 56));
    g.drawRoundedRectangle(dropArea, 8.0f, automixDropHover ? 2.0f : 1.0f);

    if (automixQueue.empty())
    {
        // Empty state placeholder
        float iconCentreX = dropArea.getCentreX();
        float iconCentreY = dropArea.getY() + 38.0f;
        g.setColour(juce::Colour::fromRGB(24, 28, 38));
        g.fillEllipse(iconCentreX - 16.0f, iconCentreY - 16.0f, 32.0f, 32.0f);
        LucideIcons::draw(g, LucideIcons::IconType::Music,
                          juce::Rectangle<float>(iconCentreX - 8.0f, iconCentreY - 8.0f, 16.0f, 16.0f),
                          juce::Colour::fromRGB(142, 149, 165), 1.6f);

        g.setColour(juce::Colour::fromRGB(203, 213, 225));
        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.drawText(juce::String::fromUTF8("Arrastra canciones aqu\xc3\xad"),
                   (int)dropArea.getX(), (int)(iconCentreY + 22.0f), (int)dropArea.getWidth(), 18,
                   juce::Justification::centred, false);

        g.setColour(juce::Colour::fromRGB(100, 108, 125));
        g.setFont(juce::FontOptions(9.5f));
        g.drawText(juce::String::fromUTF8("Usa Shift o Cmd+A para seleccionar\ny arrastrar varias pistas."),
                   (int)dropArea.getX() + 8, (int)(iconCentreY + 40.0f), (int)dropArea.getWidth() - 16, 30,
                   juce::Justification::centred, true);
    }
    else
    {
        // Draw queued tracks
        int itemHeight = 32;
        int y = (int)dropArea.getY() + 4;
        int maxVisible = ((int)dropArea.getHeight() - 8) / itemHeight;
        int numToDraw = juce::jmin((int)automixQueue.size(), maxVisible);

        for (int i = 0; i < numToDraw; ++i)
        {
            const auto& track = automixQueue[static_cast<size_t>(i)];
            auto itemRect = juce::Rectangle<int>((int)dropArea.getX() + 4, y, (int)dropArea.getWidth() - 8, itemHeight);

            // Alternating row background
            g.setColour((i % 2 == 0) ? juce::Colour::fromRGB(18, 20, 28) : juce::Colour::fromRGB(14, 16, 22));
            g.fillRoundedRectangle(itemRect.toFloat(), 4.0f);

            // Highlight first item when automix is running (currently playing)
            if (automixRunning && i == 0)
            {
                g.setColour(juce::Colour::fromRGB(96, 165, 250).withAlpha(0.15f));
                g.fillRoundedRectangle(itemRect.toFloat(), 4.0f);
                g.setColour(juce::Colour::fromRGB(96, 165, 250).withAlpha(0.5f));
                g.drawRoundedRectangle(itemRect.toFloat().reduced(0.5f), 4.0f, 1.0f);
            }

            // Track number
            g.setColour(juce::Colour::fromRGB(100, 108, 125));
            g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
            auto numArea = itemRect;
            g.drawText(juce::String(i + 1), numArea.removeFromLeft(20).reduced(2, 0), juce::Justification::centred, false);

            // BPM badge on right
            auto bpmArea = numArea.removeFromRight(38).reduced(2, 6);
            g.setColour(juce::Colour::fromRGB(0, 180, 216).withAlpha(0.1f));
            g.fillRoundedRectangle(bpmArea.toFloat(), 3.0f);
            g.setColour(juce::Colour::fromRGB(0, 229, 255));
            g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
            g.drawText(juce::String(track.bpm, 0), bpmArea, juce::Justification::centred, false);

            // Title + Artist
            auto textArea = numArea.reduced(4, 2);
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
            g.drawText(track.title, textArea.removeFromTop(14), juce::Justification::centredLeft, true);

            g.setColour(juce::Colour::fromRGB(142, 149, 165));
            g.setFont(juce::FontOptions(9.0f));
            g.drawText(track.artist, textArea, juce::Justification::centredLeft, true);

            y += itemHeight;
        }

        // Show overflow count if more items than visible
        if ((int)automixQueue.size() > maxVisible)
        {
            g.setColour(juce::Colour::fromRGB(100, 108, 125));
            g.setFont(juce::FontOptions(9.5f));
            g.drawText("+" + juce::String((int)automixQueue.size() - maxVisible) + juce::String::fromUTF8(" m\xc3\xa1s..."),
                       (int)dropArea.getX(), (int)(dropArea.getBottom() - 18.0f), (int)dropArea.getWidth(), 16,
                       juce::Justification::centred, false);
        }
    }
}

void LibraryComponent::resized()
{
    auto area = getLocalBounds();

    // 1. Far-Left Vertical Icon Rail (42px)
    auto railArea = area.removeFromLeft(42).reduced(4, 6);
    railIcon1.setBounds(railArea.removeFromTop(32).reduced(1, 1));
    railArea.removeFromTop(4);
    railIcon2.setBounds(railArea.removeFromTop(32).reduced(1, 1));
    railArea.removeFromTop(4);
    railIcon3.setBounds(railArea.removeFromTop(32).reduced(1, 1));
    railArea.removeFromTop(4);
    railIcon4.setBounds(railArea.removeFromTop(32).reduced(1, 1));
    railArea.removeFromTop(4);
    railIcon5.setBounds(railArea.removeFromTop(32).reduced(1, 1));

    if (currentMode == LibraryMode::YouTube)
    {
        if (youtubeView != nullptr)
            youtubeView->setBounds(area);
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
    automixArea.removeFromTop(8);
    auto queueRow = automixArea.removeFromTop(18);
    queueTitle.setBounds(queueRow.removeFromLeft(queueRow.getWidth() - 22));
    clearQueueBtn.setBounds(queueRow.reduced(2, 0));

    // 4. Center Track Table Area (remaining width)
    auto centerArea = area.reduced(8, 6);
    auto searchBarRow = centerArea.removeFromTop(28);

    tableHeaderTitle.setBounds(searchBarRow.removeFromLeft(180));
    addBtn.setBounds(searchBarRow.removeFromRight(76));
    searchBarRow.removeFromRight(6);
    searchBox.setBounds(searchBarRow.reduced(2, 0));

    centerArea.removeFromTop(6);
    table.setBounds(centerArea);
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
    bool hover = (dragSourceDetails.localPosition.x >= getWidth() - 260);
    if (hover != automixDropHover)
    {
        automixDropHover = hover;
        repaint();
    }
}

void LibraryComponent::itemDragMove(const SourceDetails& dragSourceDetails)
{
    bool hover = (dragSourceDetails.localPosition.x >= getWidth() - 260);
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

    // Check if the drop happened over the right 260px Automix panel
    if (dragSourceDetails.localPosition.x >= getWidth() - 260)
    {
        juce::StringArray paths;
        paths.addTokens(dragSourceDetails.description.toString(), "\n", "");

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
                    ti.key = "8A";
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
                        if (keyStr.isNotEmpty()) ti.key = keyStr;
                    }

                    tracksToAdd.push_back(ti);
                }
            }
        }

        if (!tracksToAdd.empty())
            addToAutomixQueue(tracksToAdd);
    }

    repaint();
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

    queueTitle.setText(juce::String::fromUTF8("COLA · ") + juce::String((int)automixQueue.size()) + " pistas",
                       juce::dontSendNotification);
    repaint();
}

void LibraryComponent::clearAutomixQueue()
{
    automixQueue.clear();
    clearQueueBtn.setVisible(false);

    startAutomixBtn.setCustomColours(juce::Colour::fromRGB(22, 25, 34),
                                     juce::Colour::fromRGB(142, 149, 165),
                                     juce::Colour::fromRGB(142, 149, 165),
                                     juce::Colour::fromRGB(38, 43, 56));

    queueTitle.setText(juce::String::fromUTF8("COLA DE REPRODUCCIÓN"), juce::dontSendNotification);
    repaint();
}

void LibraryComponent::setAutomixRunning(bool running)
{
    automixRunning = running;
    if (running)
    {
        startAutomixBtn.setText("Detener Automix");
        startAutomixBtn.setIcon(LucideIcons::IconType::Pause, 12.0f);
        startAutomixBtn.setCustomColours(juce::Colour::fromRGB(220, 38, 38).withAlpha(0.2f),
                                         juce::Colour::fromRGB(252, 165, 165),
                                         juce::Colour::fromRGB(252, 165, 165),
                                         juce::Colour::fromRGB(220, 38, 38).withAlpha(0.4f));
        clearQueueBtn.setVisible(false);
    }
    else
    {
        startAutomixBtn.setText("Comenzar Automix");
        startAutomixBtn.setIcon(LucideIcons::IconType::Sparkles, 12.0f);
        clearQueueBtn.setVisible(!automixQueue.empty());

        if (automixQueue.empty())
        {
            startAutomixBtn.setCustomColours(juce::Colour::fromRGB(22, 25, 34),
                                             juce::Colour::fromRGB(142, 149, 165),
                                             juce::Colour::fromRGB(142, 149, 165),
                                             juce::Colour::fromRGB(38, 43, 56));
            queueTitle.setText(juce::String::fromUTF8("COLA DE REPRODUCCIÓN"), juce::dontSendNotification);
        }
        else
        {
            startAutomixBtn.setCustomColours(juce::Colour::fromRGB(37, 99, 235).withAlpha(0.25f),
                                             juce::Colour::fromRGB(147, 197, 253),
                                             juce::Colour::fromRGB(147, 197, 253),
                                             juce::Colour::fromRGB(59, 130, 246).withAlpha(0.4f));
        }
    }
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

void LibraryComponent::updateTrackBpm(const juce::File& file, double bpm)
{
    if (bpm <= 0.0)
        return;

    bool updated = false;
    for (auto& t : allTracks)
    {
        if (t.file == file)
        {
            t.bpm = bpm;
            updated = true;
            break;
        }
    }

    for (auto& t : filteredTracks)
    {
        if (t.file == file)
        {
            t.bpm = bpm;
            updated = true;
            break;
        }
    }

    for (auto& t : automixQueue)
    {
        if (t.file == file)
        {
            t.bpm = bpm;
            updated = true;
        }
    }

    if (updated)
    {
        table.updateContent();
        table.repaint();
        repaint();
    }
}

