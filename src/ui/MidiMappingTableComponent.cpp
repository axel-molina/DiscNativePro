#include "MidiMappingTableComponent.h"
#include "UiTypeScale.h"
#include "UiPalette.h"

namespace
{
class ActionButtonsCell : public juce::Component
{
public:
    ActionButtonsCell(std::function<void()> onLearn, std::function<void()> onClear)
        : learnCallback(std::move(onLearn)), clearCallback(std::move(onClear))
    {
        learnBtn.setFontSize(UiType::caption);
        learnBtn.setCornerRadius(3.0f);
        learnBtn.onClick = [this]() { if (learnCallback) learnCallback(); };
        addAndMakeVisible(learnBtn);

        clearBtn.setText(juce::String::fromUTF8("✕"));
        clearBtn.setFontSize(UiType::micro);
        clearBtn.setCornerRadius(3.0f);
        clearBtn.setCustomColours(juce::Colour::fromRGB(24, 27, 36),
                                 juce::Colour::fromRGB(239, 68, 68),
                                 juce::Colour::fromRGB(239, 68, 68),
                                 juce::Colour::fromRGB(36, 40, 54));
        clearBtn.onClick = [this]() { if (clearCallback) clearCallback(); };
        addAndMakeVisible(clearBtn);
    }

    void updateState(bool isLearning)
    {
        if (isLearning)
        {
            learnBtn.setText(LanguageManager::trUtf8("Cancelar", "Cancel"));
            learnBtn.setCustomColours(juce::Colour::fromRGB(239, 68, 68).withAlpha(0.25f),
                                     juce::Colour::fromRGB(248, 113, 113),
                                     juce::Colour::fromRGB(248, 113, 113),
                                     juce::Colour::fromRGB(239, 68, 68).withAlpha(0.5f));
            clearBtn.setVisible(false);
        }
        else
        {
            learnBtn.setText(LanguageManager::trUtf8("Aprender", "Learn"));
            learnBtn.setCustomColours(UiPalette::accent.withAlpha(0.18f),
                                     UiPalette::accentBright,
                                     UiPalette::accentBright,
                                     UiPalette::accent.withAlpha(0.4f));
            clearBtn.setVisible(true);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(2, 4);
        if (clearBtn.isVisible())
        {
            clearBtn.setBounds(area.removeFromRight(24));
            area.removeFromRight(4);
        }
        learnBtn.setBounds(area);
    }

private:
    DjButton learnBtn;
    DjButton clearBtn;
    std::function<void()> learnCallback;
    std::function<void()> clearCallback;
};
} // namespace

MidiMappingTableComponent::MidiMappingTableComponent(MidiManager& midiMgr)
    : mappingMgr(midiMgr.getMappingManager())
{
    // 1. Selector de categoría
    categoryFilterCombo.addItem(LanguageManager::trUtf8("Todas las Categorías", "All Categories"), 1);
    categoryFilterCombo.addItem(LanguageManager::trUtf8("Transporte (Deck 1 / 2)", "Transport (Deck 1 / 2)"), 2);
    categoryFilterCombo.addItem(LanguageManager::trUtf8("Mezclador y Faders", "Mixer & Faders"), 3);
    categoryFilterCombo.addItem(LanguageManager::trUtf8("Ecualizador y Filtros", "EQ & Filters"), 4);
    categoryFilterCombo.addItem(LanguageManager::trUtf8("Pads y Hot Cues", "Pads & Hot Cues"), 5);
    categoryFilterCombo.addItem(LanguageManager::trUtf8("Biblioteca y Navegación", "Library & Navigation"), 6);
    categoryFilterCombo.setSelectedId(1, juce::dontSendNotification);
    categoryFilterCombo.setColour(juce::ComboBox::backgroundColourId, juce::Colour::fromRGB(18, 20, 28));
    categoryFilterCombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    categoryFilterCombo.setColour(juce::ComboBox::outlineColourId, juce::Colour::fromRGB(38, 43, 58));
    categoryFilterCombo.onChange = [this]() {
        selectedCategoryIndex = categoryFilterCombo.getSelectedId() - 1;
        updateFilteredActions();
    };
    addAndMakeVisible(categoryFilterCombo);

    // 2. Botón Restablecer
    resetDefaultsBtn.setText(LanguageManager::trUtf8("Restablecer Fábrica", "Reset to Default"));
    resetDefaultsBtn.setIcon(LucideIcons::IconType::Repeat, 11.0f);
    resetDefaultsBtn.setFontSize(UiType::caption);
    resetDefaultsBtn.setTooltip(LanguageManager::trUtf8("Restaura la tabla de mapeo MIDI de fábrica", "Restore the factory MIDI mapping table"));
    resetDefaultsBtn.setCornerRadius(3.0f);
    resetDefaultsBtn.setCustomColours(juce::Colour::fromRGB(24, 27, 36),
                                     juce::Colour::fromRGB(148, 163, 184),
                                     juce::Colour::fromRGB(148, 163, 184),
                                     juce::Colour::fromRGB(36, 40, 54));
    resetDefaultsBtn.onClick = [this]() { resetToFactoryDefaults(); };
    addAndMakeVisible(resetDefaultsBtn);

    // 3. Botón Exportar
    exportBtn.setText(LanguageManager::trUtf8("Exportar...", "Export..."));
    exportBtn.setIcon(LucideIcons::IconType::Download, 11.0f);
    exportBtn.setFontSize(UiType::caption);
    exportBtn.setCornerRadius(3.0f);
    exportBtn.setCustomColours(juce::Colour::fromRGB(24, 27, 36),
                              UiPalette::accentBright,
                              UiPalette::accentBright,
                              juce::Colour::fromRGB(36, 40, 54));
    exportBtn.onClick = [this]() { exportMapping(); };
    exportBtn.setTooltip(LanguageManager::trUtf8("Guarda la tabla de mapeo MIDI en un archivo", "Save the MIDI mapping table to a file"));
    addAndMakeVisible(exportBtn);

    // 4. Botón Importar
    importBtn.setText(LanguageManager::trUtf8("Importar...", "Import..."));
    importBtn.setIcon(LucideIcons::IconType::Folder, 11.0f);
    importBtn.setFontSize(UiType::caption);
    importBtn.setCornerRadius(3.0f);
    importBtn.setCustomColours(juce::Colour::fromRGB(24, 27, 36),
                              juce::Colour::fromRGB(52, 211, 153),
                              juce::Colour::fromRGB(52, 211, 153),
                              juce::Colour::fromRGB(36, 40, 54));
    importBtn.onClick = [this]() { importMapping(); };
    importBtn.setTooltip(LanguageManager::trUtf8("Carga una tabla de mapeo MIDI desde un archivo", "Load a MIDI mapping table from a file"));
    addAndMakeVisible(importBtn);

    // 5. Contador de conflictos de mapeo
    conflictBtn.setFontSize(UiType::micro);
    conflictBtn.setCornerRadius(3.0f);
    conflictBtn.setCustomColours(juce::Colour::fromRGB(24, 27, 36),
                                 juce::Colour::fromRGB(248, 113, 113),
                                 juce::Colour::fromRGB(248, 113, 113),
                                 juce::Colour::fromRGB(36, 40, 54));
    conflictBtn.onClick = [this]() { showConflictDetails(); };
    addAndMakeVisible(conflictBtn);

    // 6. Configurar TableListBox
    table.setModel(this);
    table.setColour(juce::TableListBox::backgroundColourId, juce::Colour::fromRGB(14, 16, 22));
    table.setColour(juce::TableListBox::outlineColourId, juce::Colour::fromRGB(32, 36, 48));
    table.getHeader().setColour(juce::TableHeaderComponent::backgroundColourId, juce::Colour::fromRGB(20, 23, 32));
    table.getHeader().setColour(juce::TableHeaderComponent::textColourId, juce::Colour::fromRGB(148, 163, 184));
    table.getHeader().setColour(juce::TableHeaderComponent::outlineColourId, juce::Colour::fromRGB(36, 40, 54));

    table.getHeader().addColumn(LanguageManager::trUtf8("GRUPO", "GROUP"), 1, 85, 70, 100, juce::TableHeaderComponent::notSortable);
    table.getHeader().addColumn(LanguageManager::trUtf8("FUNCIÓN", "FUNCTION"), 2, 180, 140, 260, juce::TableHeaderComponent::notSortable);
    table.getHeader().addColumn(LanguageManager::trUtf8("CONTROL ASIGNADO", "MAPPED CONTROL"), 3, 180, 140, 260, juce::TableHeaderComponent::notSortable);
    table.getHeader().addColumn(LanguageManager::trUtf8("ACCIONES", "ACTIONS"), 4, 110, 90, 130, juce::TableHeaderComponent::notSortable);

    table.setRowHeight(32);
    addAndMakeVisible(table);

    // Listeners del MappingManager. El aprendizaje se dispara desde el hilo
    // MIDI, así que las actualizaciones de UI se postergan al hilo de mensajes.
    mappingMgr.onMappingChanged = [this]() {
        juce::MessageManager::callAsync([this]() { refreshData(); });
    };
    mappingMgr.onLearningStarted = [this](const juce::String& actionId) {
        if (const auto* def = mappingMgr.findAction(actionId))
            previousBinding = def->binding;
        learningActionId = actionId;
        table.updateContent();
        table.repaint();
    };
    mappingMgr.onLearningFinished = [this](const juce::String& actionId, const MidiBinding&) {
        table.updateContent();
        table.repaint();
        juce::MessageManager::callAsync([this, actionId]() { handleLearningFinished(actionId); });
    };

    LanguageManager::getInstance().addListener(this);
    updateFilteredActions();
}

MidiMappingTableComponent::~MidiMappingTableComponent()
{
    LanguageManager::getInstance().removeListener(this);
    mappingMgr.onMappingChanged = nullptr;
    mappingMgr.onLearningStarted = nullptr;
    mappingMgr.onLearningFinished = nullptr;
}

void MidiMappingTableComponent::languageChanged()
{
    refreshLanguage();
}

void MidiMappingTableComponent::refreshLanguage()
{
    int curr = categoryFilterCombo.getSelectedId();
    categoryFilterCombo.clear(juce::dontSendNotification);
    categoryFilterCombo.addItem(LanguageManager::trUtf8("Todas las Categorías", "All Categories"), 1);
    categoryFilterCombo.addItem(LanguageManager::trUtf8("Transporte (Deck 1 / 2)", "Transport (Deck 1 / 2)"), 2);
    categoryFilterCombo.addItem(LanguageManager::trUtf8("Mezclador y Faders", "Mixer & Faders"), 3);
    categoryFilterCombo.addItem(LanguageManager::trUtf8("Ecualizador y Filtros", "EQ & Filters"), 4);
    categoryFilterCombo.addItem(LanguageManager::trUtf8("Pads y Hot Cues", "Pads & Hot Cues"), 5);
    categoryFilterCombo.addItem(LanguageManager::trUtf8("Biblioteca y Navegación", "Library & Navigation"), 6);
    categoryFilterCombo.setSelectedId(curr > 0 ? curr : 1, juce::dontSendNotification);

    resetDefaultsBtn.setText(LanguageManager::trUtf8("Restablecer Fábrica", "Reset to Default"));
    exportBtn.setText(LanguageManager::trUtf8("Exportar...", "Export..."));
    importBtn.setText(LanguageManager::trUtf8("Importar...", "Import..."));

    table.getHeader().setColumnName(1, LanguageManager::trUtf8("GRUPO", "GROUP"));
    table.getHeader().setColumnName(2, LanguageManager::trUtf8("FUNCIÓN", "FUNCTION"));
    table.getHeader().setColumnName(3, LanguageManager::trUtf8("CONTROL ASIGNADO", "MAPPED CONTROL"));
    table.getHeader().setColumnName(4, LanguageManager::trUtf8("ACCIONES", "ACTIONS"));

    updateFilteredActions();
}

void MidiMappingTableComponent::updateFilteredActions()
{
    filteredActions.clear();
    const auto& all = mappingMgr.getActions();

    for (const auto& a : all)
    {
        if (selectedCategoryIndex == 0)
        {
            filteredActions.push_back(a);
        }
        else if (selectedCategoryIndex == 1 && a.category == MidiActionCategory::Transport)
        {
            filteredActions.push_back(a);
        }
        else if (selectedCategoryIndex == 2 && a.category == MidiActionCategory::Mixer)
        {
            filteredActions.push_back(a);
        }
        else if (selectedCategoryIndex == 3 && a.category == MidiActionCategory::EQ)
        {
            filteredActions.push_back(a);
        }
        else if (selectedCategoryIndex == 4 && a.category == MidiActionCategory::Pads)
        {
            filteredActions.push_back(a);
        }
        else if (selectedCategoryIndex == 5 && a.category == MidiActionCategory::Browser)
        {
            filteredActions.push_back(a);
        }
    }

    conflictCounts.clear();
    for (const auto& c : mappingMgr.findConflicts())
    {
        ++conflictCounts[c.actionId];
        ++conflictCounts[c.otherActionId];
    }
    updateConflictButton();

    table.updateContent();
    table.repaint();
}

void MidiMappingTableComponent::refreshData()
{
    updateFilteredActions();
}

int MidiMappingTableComponent::getNumRows()
{
    return static_cast<int>(filteredActions.size());
}

void MidiMappingTableComponent::paintRowBackground(juce::Graphics& g, int rowNumber, int /*width*/, int /*height*/, bool rowIsSelected)
{
    if (rowIsSelected)
    {
        g.setColour(UiPalette::accent.withAlpha(0.12f));
        g.fillAll();
    }
    else if (rowNumber % 2 == 1)
    {
        g.setColour(juce::Colour::fromRGB(18, 20, 28));
        g.fillAll();
    }
    else
    {
        g.setColour(juce::Colour::fromRGB(14, 16, 22));
        g.fillAll();
    }
}

void MidiMappingTableComponent::paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool /*rowIsSelected*/)
{
    if (rowNumber < 0 || rowNumber >= static_cast<int>(filteredActions.size()))
        return;

    const auto& action = filteredActions[static_cast<size_t>(rowNumber)];
    auto cellBounds = juce::Rectangle<int>(0, 0, width, height).reduced(6, 4);

    bool isLearningThis = mappingMgr.isLearning() && (mappingMgr.getLearningActionId() == action.id);

    switch (columnId)
    {
        case 1: // Grupo / Deck
        {
            juce::String groupName;
            juce::Colour groupCol;

            if (action.deckIndex == 0)
            {
                groupName = "DECK 1";
                groupCol = UiPalette::accentBright; // Cyan
            }
            else if (action.deckIndex == 1)
            {
                groupName = "DECK 2";
                groupCol = juce::Colour::fromRGB(52, 211, 153); // Emerald
            }
            else if (action.category == MidiActionCategory::Mixer)
            {
                groupName = "MIXER";
                groupCol = juce::Colour::fromRGB(168, 85, 247); // Purple
            }
            else
            {
                groupName = "GLOBAL";
                groupCol = juce::Colour::fromRGB(245, 158, 11); // Amber
            }

            auto pill = cellBounds.toFloat().reduced(0, 2);
            g.setColour(groupCol.withAlpha(0.15f));
            g.fillRoundedRectangle(pill, 3.0f);
            g.setColour(groupCol.withAlpha(0.35f));
            g.drawRoundedRectangle(pill, 3.0f, 1.0f);

            g.setColour(groupCol);
            g.setFont(juce::FontOptions(UiType::micro, juce::Font::bold));
            g.drawText(groupName, pill, juce::Justification::centred, false);
            break;
        }

        case 2: // Función
        {
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(UiType::body, juce::Font::plain));
            juce::String name = LanguageManager::getInstance().isEnglish()
                                ? action.nameEn : action.nameEs;
            g.drawText(name, cellBounds, juce::Justification::centredLeft, true);
            break;
        }

        case 3: // Control asignado
        {
            auto pill = cellBounds.toFloat().reduced(2, 2);
            const auto conflictIt = conflictCounts.find(action.id);
            const int conflicts = conflictIt != conflictCounts.end() ? conflictIt->second : 0;

            if (isLearningThis)
            {
                g.setColour(juce::Colour::fromRGB(245, 158, 11).withAlpha(0.25f));
                g.fillRoundedRectangle(pill, 3.0f);
                g.setColour(juce::Colour::fromRGB(245, 158, 11));
                g.drawRoundedRectangle(pill, 3.0f, 1.2f);

                g.setFont(juce::FontOptions(UiType::micro, juce::Font::bold));
                g.drawText(LanguageManager::trUtf8("Mueve un botón o fader...", "Move a knob or fader..."), pill, juce::Justification::centred, false);
            }
            else
            {
                bool hasBinding = action.binding.isValid();
                juce::Colour bgCol = hasBinding ? juce::Colour::fromRGB(24, 28, 40) : juce::Colour::fromRGB(18, 20, 26);
                juce::Colour textCol = hasBinding ? UiPalette::accentBright : juce::Colour::fromRGB(100, 108, 125);

                g.setColour(bgCol);
                g.fillRoundedRectangle(pill, 3.0f);
                g.setColour(juce::Colour::fromRGB(36, 40, 56));
                g.drawRoundedRectangle(pill, 3.0f, 1.0f);

                auto textArea = pill;
                if (conflicts > 0)
                    textArea.removeFromRight(18.0f);

                g.setColour(textCol);
                g.setFont(juce::FontOptions("Menlo", UiType::caption, juce::Font::bold));
                g.drawText(action.binding.getDisplayText(), textArea, juce::Justification::centred, false);

                if (conflicts > 0)
                {
                    g.setColour(juce::Colour::fromRGB(239, 68, 68));
                    g.setFont(juce::FontOptions(UiType::body, juce::Font::bold));
                    g.drawText(juce::String::fromUTF8("\xE2\x9A\xA0"),
                               juce::Rectangle<float>(pill.getRight() - 17.0f, pill.getY(), 16.0f, pill.getHeight()),
                               juce::Justification::centred, false);
                }
            }
            break;
        }

        default:
            break;
    }
}

juce::Component* MidiMappingTableComponent::refreshComponentForCell(int rowNumber, int columnId, bool /*isRowSelected*/, juce::Component* existingComponentToUpdate)
{
    if (columnId == 4) // Acciones (Aprender / Borrar)
    {
        if (rowNumber < 0 || rowNumber >= static_cast<int>(filteredActions.size()))
            return nullptr;

        const auto& action = filteredActions[static_cast<size_t>(rowNumber)];
        bool isLearningThis = mappingMgr.isLearning() && (mappingMgr.getLearningActionId() == action.id);
        juce::String actId = action.id;

        auto* cell = dynamic_cast<ActionButtonsCell*>(existingComponentToUpdate);
        if (cell == nullptr)
        {
            cell = new ActionButtonsCell(
                [this, actId]() {
                    if (mappingMgr.isLearning() && mappingMgr.getLearningActionId() == actId)
                        cancelLearning();
                    else
                        startLearningForAction(actId);
                },
                [this, actId]() {
                    mappingMgr.clearBinding(actId);
                }
            );
        }

        cell->updateState(isLearningThis);
        return cell;
    }

    delete existingComponentToUpdate;
    return nullptr;
}

void MidiMappingTableComponent::startLearningForAction(const juce::String& actionId)
{
    mappingMgr.startLearning(actionId);
}

void MidiMappingTableComponent::cancelLearning()
{
    mappingMgr.cancelLearning();
}

void MidiMappingTableComponent::resetToFactoryDefaults()
{
    mappingMgr.resetToDefaults();
}

void MidiMappingTableComponent::exportMapping()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        LanguageManager::trUtf8("Exportar Mapeo MIDI", "Export MIDI Mapping"),
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("discnativepro_midi.json"),
        "*.json");

    fileChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
        [this](const juce::FileChooser& fc) {
            auto result = fc.getResult();
            if (result != juce::File())
            {
                mappingMgr.exportToFile(result);
            }
        });
}

void MidiMappingTableComponent::importMapping()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        LanguageManager::trUtf8("Importar Mapeo MIDI", "Import MIDI Mapping"),
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
        "*.json");

    fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& fc) {
            auto result = fc.getResult();
            if (result != juce::File() && result.existsAsFile())
            {
                mappingMgr.importFromFile(result);
                refreshData();
            }
        });
}

void MidiMappingTableComponent::updateConflictButton()
{
    const int total = static_cast<int>(conflictCounts.size());
    const bool hasConflicts = total > 0;

    conflictBtn.setVisible(hasConflicts);
    conflictBtn.setText(hasConflicts ? juce::String::fromUTF8("\xE2\x9A\xA0") + juce::String(total) : juce::String());
    conflictBtn.setTooltip(hasConflicts
                               ? LanguageManager::trUtf8("Controles MIDI duplicados: pulsa para ver el detalle",
                                                         "Duplicate MIDI controls: click for details")
                               : juce::String());

    resized();
}

void MidiMappingTableComponent::showConflictDetails()
{
    const auto conflicts = mappingMgr.findConflicts();
    if (conflicts.empty())
        return;

    const bool english = LanguageManager::getInstance().isEnglish();
    juce::String message;

    for (const auto& c : conflicts)
    {
        const auto* a = mappingMgr.findAction(c.actionId);
        const auto* b = mappingMgr.findAction(c.otherActionId);
        if (a == nullptr || b == nullptr)
            continue;

        const juce::String nameA = english ? a->nameEn : a->nameEs;
        const juce::String nameB = english ? b->nameEn : b->nameEs;
        message += juce::String::fromUTF8("\xE2\x9A\xA0") + " " + nameA + "  "
                   + juce::String::fromUTF8("\xE2\x86\x92") + "  " + nameB + "\n";
    }

    message += "\n" + LanguageManager::trUtf8("Ambos controles responden al mismo mensaje MIDI. En cada pareja solo se ejecutará el primero de la tabla.",
                                              "Both controls answer the same MIDI message. In each pair only the first one in the table will run.");

    juce::AlertWindow::showAsync(juce::MessageBoxOptions()
                                     .withTitle(LanguageManager::trUtf8("Controles MIDI en conflicto", "Conflicting MIDI controls"))
                                     .withMessage(message)
                                     .withButton("OK")
                                     .withIconType(juce::MessageBoxIconType::WarningIcon),
                                 nullptr);
}

void MidiMappingTableComponent::handleLearningFinished(const juce::String& actionId)
{
    const auto* def = mappingMgr.findAction(actionId);
    if (def == nullptr || !def->binding.isValid())
        return;

    // El aprendizaje se canceló: el control no cambió, no hay nada que preguntar.
    if (def->binding == previousBinding)
        return;

    const auto conflicts = mappingMgr.findConflictsFor(actionId);
    if (conflicts.empty())
        return;

    const bool english = LanguageManager::getInstance().isEnglish();
    juce::String message = LanguageManager::trUtf8("El control aprendido ya está asignado a:",
                                                   "The learned control is already assigned to:");

    for (const auto& c : conflicts)
    {
        const auto* other = mappingMgr.findAction(c.otherActionId);
        if (other == nullptr)
            continue;

        const juce::String name = english ? other->nameEn : other->nameEs;
        message += "\n" + juce::String::fromUTF8("\xE2\x80\xA2") + " " + name;
    }

    message += "\n\n" + LanguageManager::trUtf8("¿Liberar ese control para quedarte con el nuevo?",
                                                "Release that control so the new one takes over?");

    juce::AlertWindow::showAsync(juce::MessageBoxOptions()
                                     .withTitle(LanguageManager::trUtf8("Control MIDI duplicado", "Duplicate MIDI control"))
                                     .withMessage(message)
                                     .withButton(LanguageManager::trUtf8("Reemplazar", "Replace"))
                                     .withButton(LanguageManager::trUtf8("Cancelar", "Cancel"))
                                     .withIconType(juce::MessageBoxIconType::WarningIcon),
                                 [this, actionId, conflicts](int buttonIndex)
                                 {
                                     if (buttonIndex == 0)
                                     {
                                         for (const auto& c : conflicts)
                                             mappingMgr.clearBinding(c.otherActionId);
                                     }
                                     else
                                     {
                                         mappingMgr.setBinding(actionId, previousBinding);
                                         mappingMgr.saveToUserFile();
                                     }

                                     refreshData();
                                 });
}

void MidiMappingTableComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour::fromRGB(18, 20, 28));
    g.fillRoundedRectangle(bounds, 3.0f);
    g.setColour(juce::Colour::fromRGB(36, 40, 56));
    g.drawRoundedRectangle(bounds, 3.0f, 1.0f);
}

void MidiMappingTableComponent::resized()
{
    auto area = getLocalBounds().reduced(8);

    // Toolbar superior (30px)
    auto topBar = area.removeFromTop(30);
    categoryFilterCombo.setBounds(topBar.removeFromLeft(180));
    topBar.removeFromLeft(8);

    if (conflictBtn.isVisible())
    {
        conflictBtn.setBounds(topBar.removeFromLeft(38));
        topBar.removeFromLeft(6);
    }

    resetDefaultsBtn.setBounds(topBar.removeFromRight(140));
    topBar.removeFromRight(6);
    exportBtn.setBounds(topBar.removeFromRight(100));
    topBar.removeFromRight(6);
    importBtn.setBounds(topBar.removeFromRight(100));

    area.removeFromTop(8);

    // Tabla principal
    table.setBounds(area);
}
