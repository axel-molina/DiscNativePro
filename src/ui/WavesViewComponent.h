#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "OverviewWaveform.h"
#include "../audio/DeckPlayer.h"
#include "../services/LanguageManager.h"

class DeckComponent;

// Snapshot de los datos de pista que el deck muestra en su cabecera
// (título, artista, BPM, Key, tiempo actual/total y pitch/offset).
// MainComponent lo rellena desde el timer de UI (30 Hz) en modo Ondas.
struct WavesTrackInfo
{
    juce::String title;
    juce::String artist;
    juce::String bpmText;   // "128.0" o "--"
    juce::String keyText;   // "8A" o "--"
    juce::String timeText;  // "01:23.4 / 05:12.0"
    juce::String pitchText; // "+0.0%"
    bool loaded { false };
};

// Tira horizontal a todo el ancho: datos del deck + OverviewWaveform ligado
// al mismo DeckPlayer (mismos picos, seek y marcadores que la onda del deck).
class WavesTrackStrip : public juce::Component
{
public:
    WavesTrackStrip(int deckNumber, DeckPlayer& player, juce::Colour deckAccent);
    ~WavesTrackStrip() override;

    void setTrackInfo(const WavesTrackInfo& info);
    void refreshLanguage();

    void paint(juce::Graphics& g) override;
    void resized() override;

    OverviewWaveform& getWaveform() { return waveform; }

private:
    int deckIndex;
    juce::Colour accent;

    juce::Label deckBadge;
    juce::Label titleLabel;
    juce::Label artistLabel;
    juce::Label keyBadge;
    juce::Label bpmLabel;
    juce::Label timeLabel;
    juce::Label pitchLabel;
    OverviewWaveform waveform;
    juce::String lastKeyText;
    juce::Colour lastKeyColour { juce::Colour::fromRGB(142, 149, 165) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WavesTrackStrip)
};

// Vista de ondas: las dos OverviewWaveform apiladas una debajo de otra,
// cada una ocupando todo el ancho de la app, con los datos de su deck.
class WavesViewComponent : public juce::Component,
                           public LanguageManager::Listener
{
public:
    WavesViewComponent(DeckPlayer& player1, juce::Colour accent1,
                       DeckPlayer& player2, juce::Colour accent2);
    ~WavesViewComponent() override;

    // Refleja los datos de cada deck en su tira y refresca ambas ondas.
    void updateFromDecks(DeckComponent& deck1, DeckComponent& deck2);

    void languageChanged() override;
    void resized() override;

private:
    WavesTrackStrip strip1;
    WavesTrackStrip strip2;
    bool seekHandlersWired { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WavesViewComponent)
};
