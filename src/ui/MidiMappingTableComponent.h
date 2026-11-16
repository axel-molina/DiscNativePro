#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <unordered_map>
#include "../midi/MidiManager.h"
#include "../services/LanguageManager.h"
#include "DjButton.h"
#include "LucideIcons.h"

class MidiMappingTableComponent : public juce::Component,
                                  public juce::TableListBoxModel,
                                  public LanguageManager::Listener
{
public:
    MidiMappingTableComponent(MidiManager& midiMgr);
    ~MidiMappingTableComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void languageChanged() override;
    void refreshLanguage();

    // TableListBoxModel
    int getNumRows() override;
    void paintRowBackground(juce::Graphics& g, int rowNumber, int width, int height, bool rowIsSelected) override;
    void paintCell(juce::Graphics& g, int rowNumber, int columnId, int width, int height, bool rowIsSelected) override;
    juce::Component* refreshComponentForCell(int rowNumber, int columnId, bool isRowSelected, juce::Component* existingComponentToUpdate) override;

    void refreshData();

private:
    void updateFilteredActions();
    void startLearningForAction(const juce::String& actionId);
    void cancelLearning();
    void exportMapping();
    void importMapping();
    void resetToFactoryDefaults();
    void updateConflictButton();
    void showConflictDetails();
    void handleLearningFinished(const juce::String& actionId);

    MidiMappingManager& mappingMgr;

    // Categoría activa (0 = Todas, 1 = Transporte, 2 = Mezclador, 3 = EQ, 4 = Pads, 5 = Biblioteca)
    int selectedCategoryIndex { 0 };

    juce::ComboBox categoryFilterCombo;
    DjButton resetDefaultsBtn;
    DjButton exportBtn;
    DjButton importBtn;
    DjButton conflictBtn;

    juce::TableListBox table;
    std::vector<MidiActionDefinition> filteredActions;

    // P34: acciones en conflicto (actionId → nº de acciones que comparten su control)
    std::unordered_map<juce::String, int> conflictCounts;

    // P34: control previo de la acción en modo Learn (para revertir si el usuario cancela)
    juce::String learningActionId;
    MidiBinding previousBinding;

    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MidiMappingTableComponent)
};
