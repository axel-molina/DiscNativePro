#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <utility>
#include <vector>
#include <functional>
#include <unordered_map>

enum class MidiMessageType
{
    None,
    NoteOn,
    CC,
    PitchBend
};

enum class MidiActionCategory
{
    Transport,
    Mixer,
    EQ,
    Pads,
    Browser
};

struct MidiBinding
{
    MidiMessageType type { MidiMessageType::None };
    int channel { 0 };  // 1..16, o 0 para cualquier canal
    int number { -1 };  // Nota (0..127), CC (0..127), o 0 para PitchBend

    bool isValid() const { return type != MidiMessageType::None; }
    void clear() { type = MidiMessageType::None; channel = 0; number = -1; }

    bool matches(const juce::MidiMessage& msg) const
    {
        if (!isValid())
            return false;

        if (channel > 0 && msg.getChannel() != channel)
            return false;

        // NoteOn también matchea releases (note-off / note-on vel 0): el
        // dispatch de cada acción decide qué hacer con él (p.ej. loop roll hold).
        if (type == MidiMessageType::NoteOn && (msg.isNoteOn() || msg.isNoteOff(true)))
            return msg.getNoteNumber() == number;

        if (type == MidiMessageType::CC && msg.isController())
            return msg.getControllerNumber() == number;

        if (type == MidiMessageType::PitchBend && msg.isPitchWheel())
            return true;

        return false;
    }

    juce::String getDisplayText() const
    {
        if (!isValid())
            return "--";

        juce::String chStr = (channel > 0) ? "CH " + juce::String(channel) + " • " : "";
        switch (type)
        {
            case MidiMessageType::NoteOn:
                return chStr + "Nota " + juce::String(number);
            case MidiMessageType::CC:
                return chStr + "CC " + juce::String(number);
            case MidiMessageType::PitchBend:
                return chStr + "PitchBend";
            case MidiMessageType::None:
            default:
                return "--";
        }
    }
};

struct MidiActionDefinition
{
    juce::String id;
    juce::String nameEs;
    juce::String nameEn;
    MidiActionCategory category { MidiActionCategory::Transport };
    int deckIndex { -1 }; // 0 = Deck 1, 1 = Deck 2, -1 = Global
    MidiBinding binding;
    MidiBinding defaultBinding;
};

class MidiMappingManager
{
public:
    MidiMappingManager();
    ~MidiMappingManager() = default;

    // Configuración y lista de acciones
    const std::vector<MidiActionDefinition>& getActions() const { return actions; }
    std::vector<MidiActionDefinition>& getActions() { return actions; }

    const MidiActionDefinition* findAction(const juce::String& id) const;
    MidiActionDefinition* findAction(const juce::String& id);

    // Búsqueda de acción vinculada a un mensaje MIDI recibido
    const MidiActionDefinition* matchMessage(const juce::MidiMessage& msg) const;

    // Modo Learn
    bool isLearning() const { return learningActive; }
    juce::String getLearningActionId() const { return learningActionId; }
    void startLearning(const juce::String& actionId);
    void cancelLearning();
    bool processLearningMessage(const juce::MidiMessage& msg);

    // Asignación manual
    void setBinding(const juce::String& actionId, const MidiBinding& binding);
    void clearBinding(const juce::String& actionId);
    void resetToDefaults();

    // P26: aplica en lote bindings importados (p. ej. presets Mixxx) y
    // persiste una sola vez. Devuelve cuántos se aplicaron.
    int applyImportedBindings(const std::vector<std::pair<juce::String, MidiBinding>>& bindings);

    // P27: salidas (LEDs) importadas (p. ej. <outputs> de Mixxx). Persisten
    // junto al mapping; el follower de LED las consulta para saber a qué
    // mensaje encender cada luz.
    int applyImportedOutputBindings(const std::vector<std::pair<juce::String, MidiBinding>>& bindings);
    const MidiBinding* findOutputBinding(const juce::String& actionId) const;

    // Persistencia JSON
    bool saveToUserFile();
    bool loadFromUserFile();
    bool exportToFile(const juce::File& file);
    bool importFromFile(const juce::File& file);

    // Callbacks de notificación a la UI
    std::function<void()> onMappingChanged;
    std::function<void(const juce::String& actionId)> onLearningStarted;
    std::function<void(const juce::String& actionId, const MidiBinding& binding)> onLearningFinished;

private:
    void initialiseDefaultDefinitions();
    juce::File getUserMappingFile() const;
    juce::var toVar() const;
    bool fromVar(const juce::var& v);

    std::vector<MidiActionDefinition> actions;
    std::unordered_map<juce::String, MidiBinding> outputBindings; // P27: acción → mensaje LED
    bool learningActive { false };
    juce::String learningActionId;
};
