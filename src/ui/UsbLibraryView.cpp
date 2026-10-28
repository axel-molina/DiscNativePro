#include "UsbLibraryView.h"
#include "../services/AsyncFolderScanner.h"
#include <algorithm>
#include <set>

namespace
{
    class UsbLoadButtonsComponent : public juce::Component
    {
    public:
        UsbLoadButtonsComponent(std::function<void(int)> cb) : onDeckLoad(std::move(cb))
        {
            btnDeck1.setText("1");
            btnDeck1.setFontSize(10.5f);
            btnDeck1.setCornerRadius(4.0f);
            btnDeck1.setCustomColours(juce::Colour::fromRGB(0, 180, 216).withAlpha(0.25f),
                                     juce::Colour::fromRGB(0, 229, 255),
                                     juce::Colour::fromRGB(0, 229, 255),
                                     juce::Colour::fromRGB(0, 180, 216).withAlpha(0.45f));
            btnDeck1.onClick = [this]() { if (onDeckLoad) onDeckLoad(0); };
            addAndMakeVisible(btnDeck1);

            btnDeck2.setText("2");
            btnDeck2.setFontSize(10.5f);
            btnDeck2.setCornerRadius(4.0f);
            btnDeck2.setCustomColours(juce::Colour::fromRGB(0, 180, 216).withAlpha(0.25f),
                                     juce::Colour::fromRGB(0, 229, 255),
                                     juce::Colour::fromRGB(0, 229, 255),
                                     juce::Colour::fromRGB(0, 180, 216).withAlpha(0.45f));
            btnDeck2.onClick = [this]() { if (onDeckLoad) onDeckLoad(1); };
            addAndMakeVisible(btnDeck2);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(2, 4);
            int btnW = 32;
            btnDeck1.setBounds(area.removeFromLeft(btnW));
            area.removeFromLeft(4);
            btnDeck2.setBounds(area.removeFromLeft(btnW));
        }

    private:
        DjButton btnDeck1;
        DjButton btnDeck2;
        std::function<void(int)> onDeckLoad;
    };

    juce::String formatDuration(double seconds)
    {
        if (seconds <= 0.0) return "--:--";
        int totalSec = static_cast<int>(seconds);
        int mins = totalSec / 60;
        int secs = totalSec % 60;
        return juce::String::formatted("%02d:%02d", mins, secs);
    }
}

UsbLibraryView::UsbLibraryView(juce::AudioFormatManager& fm)
    : formatManager(fm)
{
    // 1. Sidebar Header
    sidebarTitle.setText(LanguageManager::trUtf8("Biblioteca USB", "USB Library"), juce::dontSendNotification);
    sidebarTitle.setFont(juce::FontOptions(13.5f, juce::Font::bold));
    sidebarTitle.setColour(juce::Label::textColourId, juce::Colour::fromRGB(220, 225, 238));
    addAndMakeVisible(sidebarTitle);

    refreshBtn.setIcon(LucideIcons::IconType::Repeat, 13.0f);
    refreshBtn.setTooltip(LanguageManager::trUtf8("Refrescar unidades USB", "Refresh USB drives"));
    refreshBtn.setCornerRadius(4.0f);
    refreshBtn.setCustomColours(juce::Colour::fromRGB(24, 27, 36),
                                juce::Colour::fromRGB(160, 165, 180),
                                juce::Colour::fromRGB(0, 229, 255),
                                juce::Colour::fromRGB(36, 40, 52));
    refreshBtn.onClick = [this]() { refreshDrives(); };
    addAndMakeVisible(refreshBtn);

    addFolderBtn.setIcon(LucideIcons::IconType::FolderPlus, 13.0f);
    addFolderBtn.setTooltip(LanguageManager::trUtf8("Abrir unidad o carpeta...", "Open drive or folder..."));
    addFolderBtn.setCornerRadius(4.0f);
    addFolderBtn.setCustomColours(juce::Colour::fromRGB(24, 27, 36),
                                 juce::Colour::fromRGB(160, 165, 180),
                                 juce::Colour::fromRGB(0, 229, 255),
                                 juce::Colour::fromRGB(36, 40, 52));
    addFolderBtn.onClick = [this]() { openManualFolder(); };
    addAndMakeVisible(addFolderBtn);

    // Sidebar Viewport
    sidebarViewport.setViewedComponent(&sidebarContent, false);
    sidebarViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(sidebarViewport);

    // 2. Main Center Header
    headerTitle.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    headerTitle.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(headerTitle);

    searchBox.setTextToShowWhenEmpty(LanguageManager::trUtf8("Buscar en USB...", "Search USB tracks..."),
                                     juce::Colour::fromRGB(100, 108, 125));
    searchBox.setFont(juce::FontOptions(12.0f));
    searchBox.setColour(juce::TextEditor::backgroundColourId, juce::Colour::fromRGB(18, 20, 28));
    searchBox.setColour(juce::TextEditor::textColourId, juce::Colour::fromRGB(220, 225, 238));
    searchBox.setColour(juce::TextEditor::outlineColourId, juce::Colour::fromRGB(36, 40, 54));
    searchBox.onTextChange = [this]() { filterTracks(searchBox.getText()); };
    addAndMakeVisible(searchBox);

    trackCountLabel.setFont(juce::FontOptions(11.0f));
    trackCountLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(140, 146, 160));
    trackCountLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(trackCountLabel);

    // 3. Table List
    table.setModel(this);
    table.setColour(juce::ListBox::backgroundColourId, juce::Colour::fromRGB(12, 13, 18));
    table.setColour(juce::ListBox::outlineColourId, juce::Colour::fromRGB(24, 28, 38));
    // Mismos colores de header que la biblioteca local y YouTube. Sin esto manda
    // LookAndFeel_V4 (hereda de V3): fondo white.withAlpha(0.6f) y texto negro,
    // o sea una franja gris claro sobre el fondo oscuro.
    table.getHeader().setColour(juce::TableHeaderComponent::backgroundColourId, juce::Colour::fromRGB(16, 18, 24));
    table.getHeader().setColour(juce::TableHeaderComponent::textColourId, juce::Colour::fromRGB(142, 149, 165));
    table.setRowHeight(34);
    table.setMultipleSelectionEnabled(true);

    auto& hdr = table.getHeader();
    hdr.addColumn("#", 1, 34, 30, 40);
    hdr.addColumn(LanguageManager::trUtf8("Título", "Title"), 2, 260, 160, 450);
    hdr.addColumn(LanguageManager::trUtf8("Artista", "Artist"), 3, 200, 120, 350);
    hdr.addColumn("BPM", 4, 55, 45, 75);
    hdr.addColumn("Key", 5, 50, 40, 65);
    hdr.addColumn(LanguageManager::trUtf8("Duración", "Time"), 6, 65, 50, 80);
    hdr.addColumn(LanguageManager::trUtf8("Cargar", "Load"), 7, 85, 75, 95);
    addAndMakeVisible(table);

    // 4. Center Empty State Action Button
    manualExploreBtn.setText(LanguageManager::trUtf8("Explorar Unidad o Carpeta Manualmente", "Browse Drive or Folder Manually"));
    manualExploreBtn.setIcon(LucideIcons::IconType::Folder, 14.0f);
    manualExploreBtn.setFontSize(12.5f);
    manualExploreBtn.setCornerRadius(6.0f);
    manualExploreBtn.setCustomColours(juce::Colour::fromRGB(0, 180, 216).withAlpha(0.22f),
                                      juce::Colour::fromRGB(0, 229, 255),
                                      juce::Colour::fromRGB(0, 229, 255),
                                      juce::Colour::fromRGB(0, 180, 216).withAlpha(0.4f));
    manualExploreBtn.onClick = [this]() { openManualFolder(); };
    addChildComponent(manualExploreBtn);

    LanguageManager::getInstance().addListener(this);

    // Asynchronous scanner initialization
    asyncScanner = std::make_unique<AsyncFolderScanner>(formatManager);
    asyncScanner->onTracksDiscovered = [this](const std::vector<TrackItem>& batch, uint64_t /*scanId*/) {
        for (const auto& item : batch)
        {
            auto path = item.file.getFullPathName();
            if (knownUsbPaths.find(path) == knownUsbPaths.end())
            {
                knownUsbPaths.insert(path);
                allUsbTracks.push_back(item);
                if (discoveredFolderSet.insert(item.folder).second)
                {
                    availableFolders.push_back(item.folder);
                }
            }
        }
        filterTracks(searchBox.getText());
        updateSyncStatusLabel((int)filteredUsbTracks.size(), true);
    };

    asyncScanner->onProgress = [this](int totalFound, const juce::String& currentFolderName, uint64_t /*scanId*/) {
        updateSyncStatusLabel(totalFound, true, currentFolderName);
    };

    asyncScanner->onScanFinished = [this](int totalFound, uint64_t /*scanId*/) {
        isSyncing = false;
        filterTracks(searchBox.getText());
        updateSyncStatusLabel((int)filteredUsbTracks.size(), false);
        refreshDrives();
    };

    // Initial drive scan & periodic timer
    refreshDrives();
    startTimer(3000);
}

UsbLibraryView::~UsbLibraryView()
{
    stopTimer();
    if (asyncScanner)
        asyncScanner->cancelScan();
    LanguageManager::getInstance().removeListener(this);
}

void UsbLibraryView::languageChanged()
{
    refreshLanguage();
}

void UsbLibraryView::refreshLanguage()
{
    sidebarTitle.setText(LanguageManager::trUtf8("Biblioteca USB", "USB Library"), juce::dontSendNotification);
    refreshBtn.setTooltip(LanguageManager::trUtf8("Refrescar unidades USB", "Refresh USB drives"));
    addFolderBtn.setTooltip(LanguageManager::trUtf8("Abrir unidad o carpeta...", "Open drive or folder..."));
    searchBox.setTextToShowWhenEmpty(LanguageManager::trUtf8("Buscar en USB...", "Search USB tracks..."),
                                     juce::Colour::fromRGB(100, 108, 125));
    manualExploreBtn.setText(LanguageManager::trUtf8("Explorar Unidad o Carpeta Manualmente", "Browse Drive or Folder Manually"));

    auto& hdr = table.getHeader();
    hdr.setColumnName(2, LanguageManager::trUtf8("Título", "Title"));
    hdr.setColumnName(3, LanguageManager::trUtf8("Artista", "Artist"));
    hdr.setColumnName(6, LanguageManager::trUtf8("Duración", "Time"));
    hdr.setColumnName(7, LanguageManager::trUtf8("Cargar", "Load"));

    filterTracks(searchBox.getText());
    repaint();
}

void UsbLibraryView::timerCallback()
{
    auto currentDetected = UsbDriveScanner::getConnectedDrives();

    // Check if detected drives count or paths changed
    bool changed = (currentDetected.size() != drives.size());
    if (!changed)
    {
        for (size_t i = 0; i < currentDetected.size(); ++i)
        {
            if (currentDetected[i].id != drives[i].id)
            {
                changed = true;
                break;
            }
        }
    }

    if (changed)
    {
        refreshDrives();
    }
}

void UsbLibraryView::refreshDrives()
{
    auto detected = UsbDriveScanner::getConnectedDrives();

    // Preserve any custom manually opened drives
    for (const auto& d : drives)
    {
        if (d.isCustom && d.root.isDirectory())
        {
            bool exists = false;
            for (const auto& cur : detected)
            {
                if (cur.id == d.id) { exists = true; break; }
            }
            if (!exists)
                detected.push_back(d);
        }
    }

    drives = std::move(detected);

    if (drives.empty())
    {
        selectedDriveIndex = -1;
        allUsbTracks.clear();
        filteredUsbTracks.clear();
        availableFolders.clear();
        table.updateContent();
    }
    else
    {
        if (selectedDriveIndex < 0 || selectedDriveIndex >= (int)drives.size())
        {
            selectDrive(0);
        }
    }

    // Rebuild sidebar controls
    sidebarContent.removeAllChildren();
    driveButtons.clear();
    folderButtons.clear();

    int y = 4;
    for (int i = 0; i < (int)drives.size(); ++i)
    {
        const auto& d = drives[static_cast<size_t>(i)];
        auto btn = std::make_unique<DjButton>();
        btn->setIcon(LucideIcons::IconType::Usb, 13.0f);
        btn->setText(d.name);
        btn->setFontSize(11.5f);
        btn->setCornerRadius(4.0f);

        bool isSel = (i == selectedDriveIndex);
        btn->setCustomColours(isSel ? juce::Colour::fromRGB(0, 180, 216).withAlpha(0.2f) : juce::Colour::fromRGB(18, 20, 28),
                              isSel ? juce::Colour::fromRGB(0, 229, 255) : juce::Colour::fromRGB(200, 205, 220),
                              isSel ? juce::Colour::fromRGB(0, 229, 255) : juce::Colour::fromRGB(200, 205, 220),
                              isSel ? juce::Colour::fromRGB(0, 180, 216).withAlpha(0.35f) : juce::Colour::fromRGB(28, 32, 44));
        btn->onClick = [this, i]() { selectDrive(i); };

        btn->setBounds(4, y, 196, 28);
        sidebarContent.addAndMakeVisible(btn.get());
        driveButtons.push_back(std::move(btn));
        y += 32;

        // If selected drive, show subfolder hierarchy
        if (isSel)
        {
            // "Todas las pistas" option
            auto allBtn = std::make_unique<DjButton>();
            allBtn->setIcon(LucideIcons::IconType::Music, 12.0f);
            allBtn->setText(LanguageManager::trUtf8("Todas las pistas", "All Tracks"));
            allBtn->setFontSize(10.5f);
            allBtn->setCornerRadius(3.0f);

            bool isAllSel = (selectedFolderName == "Todas las pistas");
            allBtn->setCustomColours(isAllSel ? juce::Colour::fromRGB(0, 180, 216).withAlpha(0.18f) : juce::Colour::fromRGB(14, 16, 22),
                                     isAllSel ? juce::Colour::fromRGB(0, 229, 255) : juce::Colour::fromRGB(160, 165, 180),
                                     isAllSel ? juce::Colour::fromRGB(0, 229, 255) : juce::Colour::fromRGB(160, 165, 180),
                                     isAllSel ? juce::Colour::fromRGB(0, 180, 216).withAlpha(0.3f) : juce::Colour::fromRGB(22, 25, 34));
            allBtn->onClick = [this]() { selectFolder("Todas las pistas"); };
            allBtn->setBounds(18, y, 182, 24);
            sidebarContent.addAndMakeVisible(allBtn.get());
            folderButtons.push_back(std::move(allBtn));
            y += 28;

            for (const auto& fName : availableFolders)
            {
                auto fBtn = std::make_unique<DjButton>();
                fBtn->setIcon(LucideIcons::IconType::Folder, 12.0f);
                fBtn->setText(fName);
                fBtn->setFontSize(10.5f);
                fBtn->setCornerRadius(3.0f);

                bool isFolderSel = (selectedFolderName == fName);
                fBtn->setCustomColours(isFolderSel ? juce::Colour::fromRGB(0, 180, 216).withAlpha(0.18f) : juce::Colour::fromRGB(14, 16, 22),
                                       isFolderSel ? juce::Colour::fromRGB(0, 229, 255) : juce::Colour::fromRGB(160, 165, 180),
                                       isFolderSel ? juce::Colour::fromRGB(0, 229, 255) : juce::Colour::fromRGB(160, 165, 180),
                                       isFolderSel ? juce::Colour::fromRGB(0, 180, 216).withAlpha(0.3f) : juce::Colour::fromRGB(22, 25, 34));
                fBtn->onClick = [this, fName]() { selectFolder(fName); };
                fBtn->setBounds(18, y, 182, 24);
                sidebarContent.addAndMakeVisible(fBtn.get());
                folderButtons.push_back(std::move(fBtn));
                y += 28;
            }
            y += 4;
        }
    }

    sidebarContent.setSize(204, juce::jmax(y + 10, sidebarViewport.getHeight()));

    bool hasDrives = !drives.empty();
    headerTitle.setVisible(hasDrives);
    searchBox.setVisible(hasDrives);
    trackCountLabel.setVisible(hasDrives);
    table.setVisible(hasDrives);
    manualExploreBtn.setVisible(!hasDrives);

    resized();
    repaint();
}

void UsbLibraryView::updateSyncStatusLabel(int count, bool isScanningNow, const juce::String& currentFolder)
{
    isSyncing = isScanningNow;
    if (isScanningNow)
    {
        trackCountLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 229, 255));
        juce::String text = LanguageManager::trUtf8("Sincronizando: ", "Syncing: ") + juce::String(count) + " " + LanguageManager::trUtf8("pistas...", "tracks...");
        if (currentFolder.isNotEmpty())
            text += " (" + currentFolder + ")";
        trackCountLabel.setText(text, juce::dontSendNotification);
    }
    else
    {
        trackCountLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(140, 146, 160));
        trackCountLabel.setText(juce::String(count) + " " + LanguageManager::trUtf8("pistas", "tracks"), juce::dontSendNotification);
    }
}

void UsbLibraryView::selectDrive(int index)
{
    if (index < 0 || index >= (int)drives.size())
        return;

    const auto& drive = drives[static_cast<size_t>(index)];

    // If already on this drive and tracks exist, avoid unnecessary rescans
    if (selectedDriveIndex == index && currentlyScannedDriveId == drive.id && !allUsbTracks.empty())
        return;

    selectedDriveIndex = index;
    selectedFolderName = "Todas las pistas";
    currentlyScannedDriveId = drive.id;

    if (asyncScanner)
        asyncScanner->cancelScan();

    allUsbTracks.clear();
    filteredUsbTracks.clear();
    knownUsbPaths.clear();
    discoveredFolderSet.clear();
    availableFolders.clear();

    table.updateContent();
    table.repaint();

    updateSyncStatusLabel(0, true);

    if (asyncScanner)
        asyncScanner->startScan(drive.root);

    filterTracks(searchBox.getText());
    refreshDrives();
}

void UsbLibraryView::selectFolder(const juce::String& folderName)
{
    selectedFolderName = folderName;
    filterTracks(searchBox.getText());
    refreshDrives();
}

void UsbLibraryView::openManualFolder()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        LanguageManager::trUtf8("Seleccionar Memoria USB o Carpeta de Música", "Select USB Drive or Music Folder"),
        juce::File::getSpecialLocation(juce::File::userHomeDirectory),
        "");

    auto folderFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
    fileChooser->launchAsync(folderFlags, [this](const juce::FileChooser& fc) {
        auto result = fc.getResult();
        if (result.isDirectory())
        {
            UsbDrive d;
            d.id = result.getFullPathName();
            d.root = result;
            d.name = result.getFileName();
            auto freeBytes = result.getBytesFreeOnVolume();
            if (freeBytes > 0)
                d.sizeDesc = UsbDriveScanner::formatBytes(freeBytes) + " libre";
            d.isRemovable = true;
            d.isCustom = true;

            drives.push_back(d);
            selectDrive((int)drives.size() - 1);
        }
    });
}

void UsbLibraryView::filterTracks(const juce::String& query)
{
    filteredUsbTracks.clear();
    auto q = query.toLowerCase().trim();

    for (const auto& t : allUsbTracks)
    {
        if (selectedFolderName != "Todas las pistas" && t.folder != selectedFolderName)
            continue;

        if (q.isNotEmpty())
        {
            if (!t.title.toLowerCase().contains(q) &&
                !t.artist.toLowerCase().contains(q) &&
                !t.folder.toLowerCase().contains(q))
            {
                continue;
            }
        }

        filteredUsbTracks.push_back(t);
    }

    // Update header
    juce::String driveName = (selectedDriveIndex >= 0 && selectedDriveIndex < (int)drives.size())
                                ? drives[static_cast<size_t>(selectedDriveIndex)].name
                                : "USB";
    headerTitle.setText(driveName + " > " + selectedFolderName, juce::dontSendNotification);

    trackCountLabel.setText(juce::String((int)filteredUsbTracks.size()) + " " +
                            LanguageManager::trUtf8("pistas", "tracks"),
                            juce::dontSendNotification);

    table.updateContent();
    table.repaint();
}

int UsbLibraryView::getNumRows()
{
    return static_cast<int>(filteredUsbTracks.size());
}

void UsbLibraryView::paintRowBackground(juce::Graphics& g, int rowNumber, int /*width*/, int /*height*/, bool rowIsSelected)
{
    if (rowIsSelected)
        g.fillAll(juce::Colour::fromRGB(45, 50, 65));
    else if (rowNumber % 2 == 1)
        g.fillAll(juce::Colour::fromRGB(15, 17, 23));
    else
        g.fillAll(juce::Colour::fromRGB(12, 13, 18));
}

void UsbLibraryView::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool /*rowIsSelected*/)
{
    if (rowNumber < 0 || rowNumber >= (int)filteredUsbTracks.size())
        return;

    const auto& track = filteredUsbTracks[static_cast<size_t>(rowNumber)];
    auto bounds = juce::Rectangle<int>(0, 0, width, height);

    switch (columnId)
    {
        case 1: // #
            g.setColour(juce::Colour::fromRGB(110, 118, 135));
            g.setFont(juce::FontOptions(11.0f));
            g.drawText(juce::String(rowNumber + 1), bounds, juce::Justification::centred, false);
            break;

        case 2: // TÍTULO
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
            g.drawText(track.title, bounds.reduced(6, 0), juce::Justification::centredLeft, true);
            break;

        case 3: // ARTISTA
            g.setColour(juce::Colour::fromRGB(150, 158, 175));
            g.setFont(juce::FontOptions(11.5f));
            g.drawText(track.artist, bounds.reduced(6, 0), juce::Justification::centredLeft, true);
            break;

        case 4: // BPM
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
            g.drawText(juce::String(track.bpm, 1), bounds, juce::Justification::centred, false);
            break;

        case 5: // KEY
        {
            auto pill = bounds.reduced(4, 7).toFloat();
            g.setColour(juce::Colour::fromRGB(0, 180, 216).withAlpha(0.15f));
            g.fillRoundedRectangle(pill, 3.0f);
            g.setColour(juce::Colour::fromRGB(0, 180, 216).withAlpha(0.35f));
            g.drawRoundedRectangle(pill, 3.0f, 1.0f);
            g.setColour(juce::Colour::fromRGB(0, 229, 255));
            g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
            g.drawText(track.key, pill, juce::Justification::centred, false);
            break;
        }

        case 6: // DURACIÓN
            g.setColour(juce::Colour::fromRGB(142, 149, 165));
            g.setFont(juce::FontOptions(11.0f));
            g.drawText(formatDuration(track.duration), bounds, juce::Justification::centred, false);
            break;

        default:
            break;
    }
}

juce::Component* UsbLibraryView::refreshComponentForCell(int rowNumber, int columnId, bool /*isRowSelected*/, juce::Component* existingComponentToUpdate)
{
    if (columnId == 7) // CARGAR 1 / 2
    {
        auto* comp = dynamic_cast<UsbLoadButtonsComponent*>(existingComponentToUpdate);
        if (comp == nullptr)
        {
            comp = new UsbLoadButtonsComponent([this, rowNumber](int deckIndex) {
                if (rowNumber >= 0 && rowNumber < (int)filteredUsbTracks.size() && onLoadTrack)
                {
                    onLoadTrack(deckIndex, filteredUsbTracks[static_cast<size_t>(rowNumber)].file);
                }
            });
        }
        return comp;
    }

    delete existingComponentToUpdate;
    return nullptr;
}

void UsbLibraryView::cellDoubleClicked(int rowNumber, int /*columnId*/, const juce::MouseEvent&)
{
    if (rowNumber >= 0 && rowNumber < (int)filteredUsbTracks.size() && onLoadTrack)
    {
        onLoadTrack(0, filteredUsbTracks[static_cast<size_t>(rowNumber)].file);
    }
}

juce::var UsbLibraryView::getDragSourceDescription(const juce::SparseSet<int>& currentlySelectedRows)
{
    juce::StringArray paths;
    for (int i = 0; i < currentlySelectedRows.size(); ++i)
    {
        int row = currentlySelectedRows[i];
        if (row >= 0 && row < (int)filteredUsbTracks.size())
            paths.add(filteredUsbTracks[static_cast<size_t>(row)].file.getFullPathName());
    }
    if (paths.isEmpty()) return {};
    return paths.joinIntoString("\n");
}

void UsbLibraryView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(12, 13, 18));

    // Left Sidebar divider line
    int sidebarW = 210;
    g.setColour(juce::Colour::fromRGB(28, 32, 44));
    g.drawLine((float)sidebarW, 0.0f, (float)sidebarW, (float)getHeight(), 1.0f);

    // Sidebar empty state if no drives
    if (drives.empty())
    {
        auto sbBounds = juce::Rectangle<float>(0.0f, 40.0f, (float)sidebarW, (float)getHeight() - 40.0f);
        auto iconRect = juce::Rectangle<float>(sbBounds.getCentreX() - 20.0f, sbBounds.getCentreY() - 44.0f, 40.0f, 40.0f);
        LucideIcons::draw(g, LucideIcons::IconType::Usb, iconRect, juce::Colour::fromRGB(70, 76, 92), 1.6f);

        g.setColour(juce::Colour::fromRGB(150, 158, 175));
        g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
        g.drawText(LanguageManager::trUtf8("Conecta la unidad USB", "Connect USB Drive"),
                   0, (int)(iconRect.getBottom() + 10.0f), sidebarW, 18, juce::Justification::centred, false);

        g.setColour(juce::Colour::fromRGB(100, 108, 125));
        g.setFont(juce::FontOptions(9.5f));
        g.drawText(LanguageManager::trUtf8("Detección automática", "Auto-detection enabled"),
                   10, (int)(iconRect.getBottom() + 28.0f), sidebarW - 20, 16, juce::Justification::centred, false);
    }

    // Main Center Empty State (matching reference image)
    if (drives.empty())
    {
        auto centerArea = getLocalBounds().withTrimmedLeft(sidebarW);
        float cardW = juce::jmin(440.0f, (float)centerArea.getWidth() - 40.0f);
        float cardH = 210.0f;
        auto cardRect = juce::Rectangle<float>((float)centerArea.getX() + ((float)centerArea.getWidth() - cardW) * 0.5f,
                                               (float)centerArea.getY() + ((float)centerArea.getHeight() - cardH) * 0.5f - 10.0f,
                                               cardW, cardH);

        // Center card background
        g.setColour(juce::Colour::fromRGB(16, 18, 25));
        g.fillRoundedRectangle(cardRect, 12.0f);
        g.setColour(juce::Colour::fromRGB(32, 36, 48));
        g.drawRoundedRectangle(cardRect, 12.0f, 1.0f);

        // Large USB Icon inside pill
        float iconBoxW = 56.0f;
        float iconBoxH = 56.0f;
        auto iconBox = juce::Rectangle<float>(cardRect.getCentreX() - iconBoxW * 0.5f, cardRect.getY() + 20.0f, iconBoxW, iconBoxH);
        g.setColour(juce::Colour::fromRGB(24, 28, 38));
        g.fillRoundedRectangle(iconBox, 14.0f);

        LucideIcons::draw(g, LucideIcons::IconType::Usb, iconBox.reduced(10.0f), juce::Colours::white, 2.0f);

        // Title
        g.setColour(juce::Colours::white);
        g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
        g.drawText(LanguageManager::trUtf8("Biblioteca USB", "USB Library"),
                   (int)cardRect.getX(), (int)(iconBox.getBottom() + 10.0f), (int)cardRect.getWidth(), 22,
                   juce::Justification::centred, false);

        // Description
        g.setColour(juce::Colour::fromRGB(140, 146, 162));
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(LanguageManager::trUtf8("Conecta una memoria USB o disco externo para acceder y mezclar tus pistas.",
                                           "Connect a USB drive or external disk to access and mix your tracks."),
                   (int)cardRect.getX() + 16, (int)(iconBox.getBottom() + 32.0f), (int)cardRect.getWidth() - 32, 32,
                   juce::Justification::centred, true);
    }
}

void UsbLibraryView::resized()
{
    auto area = getLocalBounds();
    int sidebarW = 210;

    // Sidebar layout
    auto sbArea = area.removeFromLeft(sidebarW);
    auto topSbRow = sbArea.removeFromTop(36).reduced(6, 4);

    addFolderBtn.setBounds(topSbRow.removeFromRight(26));
    topSbRow.removeFromRight(4);
    refreshBtn.setBounds(topSbRow.removeFromRight(26));
    topSbRow.removeFromRight(6);
    sidebarTitle.setBounds(topSbRow);

    sidebarViewport.setBounds(sbArea);
    sidebarContent.setSize(sidebarW - 6, sidebarContent.getHeight());

    // Main Area layout
    auto mainArea = area;
    if (drives.empty())
    {
        float cardW = juce::jmin(440.0f, (float)mainArea.getWidth() - 40.0f);
        float cardH = 210.0f;
        auto cardRect = juce::Rectangle<float>((float)mainArea.getX() + ((float)mainArea.getWidth() - cardW) * 0.5f,
                                               (float)mainArea.getY() + ((float)mainArea.getHeight() - cardH) * 0.5f - 10.0f,
                                               cardW, cardH);
        manualExploreBtn.setBounds((int)(cardRect.getCentreX() - 140.0f), (int)(cardRect.getBottom() - 44.0f), 280, 30);
    }
    else
    {
        auto topRow = mainArea.removeFromTop(38).reduced(8, 5);
        trackCountLabel.setBounds(topRow.removeFromRight(240));
        topRow.removeFromRight(8);
        searchBox.setBounds(topRow.removeFromRight(180));
        topRow.removeFromRight(12);
        headerTitle.setBounds(topRow);

        table.setBounds(mainArea);
    }
}
