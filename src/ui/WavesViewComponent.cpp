#include "WavesViewComponent.h"
#include "DeckComponent.h"
#include "../audio/KeyDetector.h"
#include <cstdio>

namespace
{
// Mismo formato que DeckComponent::formatTime: MM:SS.d
void formatStripTime(double seconds, char* buffer, size_t bufferSize)
{
    if (seconds < 0.0)
        seconds = 0.0;
    int totalTenths = (int)(seconds * 10.0);
    int tenths = totalTenths % 10;
    int totalSecs = totalTenths / 10;
    int mins = totalSecs / 60;
    int secs = totalSecs % 60;
    std::snprintf(buffer, bufferSize, "%02d:%02d.%1d", mins, secs, tenths);
}
} // namespace

// ============================== WavesTrackStrip ==============================

WavesTrackStrip::WavesTrackStrip(int deckNum, DeckPlayer& player, juce::Colour deckAccent)
    : deckIndex(deckNum), accent(deckAccent), waveform(player, deckAccent)
{
    deckBadge.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    deckBadge.setColour(juce::Label::textColourId, accent);
    deckBadge.setColour(juce::Label::backgroundColourId, accent.withAlpha(0.14f));
    deckBadge.setColour(juce::Label::outlineColourId, accent.withAlpha(0.35f));
    deckBadge.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(deckBadge);

    titleLabel.setText("--", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(titleLabel);

    artistLabel.setText("--", juce::dontSendNotification);
    artistLabel.setFont(juce::FontOptions(10.0f));
    artistLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(142, 149, 165));
    artistLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(artistLabel);

    keyBadge.setText("--", juce::dontSendNotification);
    keyBadge.setFont(juce::FontOptions(10.0f, juce::Font::bold));
    keyBadge.setColour(juce::Label::textColourId, juce::Colour::fromRGB(142, 149, 165));
    keyBadge.setColour(juce::Label::backgroundColourId, juce::Colour::fromRGB(142, 149, 165).withAlpha(0.12f));
    keyBadge.setColour(juce::Label::outlineColourId, juce::Colour::fromRGB(142, 149, 165).withAlpha(0.25f));
    keyBadge.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(keyBadge);

    bpmLabel.setText("--", juce::dontSendNotification);
    bpmLabel.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    bpmLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    bpmLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(bpmLabel);

    timeLabel.setText("00:00.0 / 00:00.0", juce::dontSendNotification);
    timeLabel.setFont(juce::FontOptions("Menlo", 11.5f, juce::Font::bold));
    timeLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(200, 205, 215));
    timeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(timeLabel);

    pitchLabel.setText("+0.0%", juce::dontSendNotification);
    pitchLabel.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    pitchLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(142, 149, 165));
    pitchLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(pitchLabel);

    addAndMakeVisible(waveform);

    refreshLanguage();
}

WavesTrackStrip::~WavesTrackStrip()
{
}

void WavesTrackStrip::refreshLanguage()
{
    deckBadge.setText(LanguageManager::trUtf8("BANDEJA ", "DECK ") + juce::String(deckIndex + 1),
                      juce::dontSendNotification);
    deckBadge.setTooltip(LanguageManager::trUtf8("Datos y onda completa de la bandeja ", "Full track data & waveform for deck ")
                         + juce::String(deckIndex + 1));
    timeLabel.setTooltip(LanguageManager::trUtf8("Tiempo actual / total", "Elapsed / total time"));
    pitchLabel.setTooltip(LanguageManager::trUtf8("Pitch / offset del deck", "Deck pitch / offset"));
}

void WavesTrackStrip::setTrackInfo(const WavesTrackInfo& info)
{
    titleLabel.setText(info.title.isNotEmpty() ? info.title : "--", juce::dontSendNotification);
    artistLabel.setText(info.artist.isNotEmpty() ? info.artist : "--", juce::dontSendNotification);
    bpmLabel.setText(info.bpmText.isNotEmpty() ? info.bpmText : "--", juce::dontSendNotification);
    keyBadge.setText(info.keyText.isNotEmpty() ? info.keyText : "--", juce::dontSendNotification);
    timeLabel.setText(info.timeText.isNotEmpty() ? info.timeText : "00:00.0 / 00:00.0", juce::dontSendNotification);
    pitchLabel.setText(info.pitchText.isNotEmpty() ? info.pitchText : "+0.0%", juce::dontSendNotification);

    // Key badge coloreado por rueda Camelot (mismo criterio que el deck),
    // recalculado solo cuando cambia la tonalidad.
    const auto keyText = info.keyText.isNotEmpty() ? info.keyText : "--";
    if (keyText != lastKeyText)
    {
        lastKeyText = keyText;
        lastKeyColour = info.loaded ? KeyDetector::getCamelotColour(keyText)
                                    : juce::Colour::fromRGB(142, 149, 165);
        keyBadge.setColour(juce::Label::textColourId, lastKeyColour);
        keyBadge.setColour(juce::Label::backgroundColourId, lastKeyColour.withAlpha(0.15f));
        keyBadge.setColour(juce::Label::outlineColourId, lastKeyColour.withAlpha(0.35f));
    }
}

void WavesTrackStrip::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(juce::Colour::fromRGB(15, 17, 22));
    g.fillRoundedRectangle(bounds.reduced(1.0f), 8.0f);

    g.setColour(juce::Colour::fromRGB(30, 34, 44));
    g.drawRoundedRectangle(bounds.reduced(1.0f), 8.0f, 1.2f);

    // Barra de acento del deck en el borde izquierdo
    g.setColour(accent.withAlpha(0.9f));
    g.fillRoundedRectangle(bounds.getX() + 3.0f, bounds.getY() + 10.0f, 3.0f, bounds.getHeight() - 20.0f, 1.5f);
}

void WavesTrackStrip::resized()
{
    auto area = getLocalBounds().reduced(10, 3);

    // 1. Fila de datos (24px): insignia + título/artista | key, BPM, tiempo, pitch
    auto infoRow = area.removeFromTop(24);
    deckBadge.setBounds(infoRow.removeFromLeft(66).reduced(0, 4));
    infoRow.removeFromLeft(6);

    auto rightMeta = infoRow.removeFromRight(282);
    pitchLabel.setBounds(rightMeta.removeFromRight(58));
    timeLabel.setBounds(rightMeta.removeFromRight(126));
    bpmLabel.setBounds(rightMeta.removeFromRight(54));
    keyBadge.setBounds(rightMeta.reduced(2, 4));

    titleLabel.setBounds(infoRow.removeFromTop(13));
    artistLabel.setBounds(infoRow);

    area.removeFromTop(2);

    // 2. Onda a todo el ancho de la tira
    waveform.setBounds(area);
}

// ============================= WavesViewComponent =============================

WavesViewComponent::WavesViewComponent(DeckPlayer& player1, juce::Colour accent1,
                                       DeckPlayer& player2, juce::Colour accent2)
    : strip1(0, player1, accent1),
      strip2(1, player2, accent2)
{
    addAndMakeVisible(strip1);
    addAndMakeVisible(strip2);
}

WavesViewComponent::~WavesViewComponent()
{
}

void WavesViewComponent::updateFromDecks(DeckComponent& deck1, DeckComponent& deck2)
{
    // Seek de vídeo YouTube desde la onda (misma paridad que la onda interna del deck)
    if (!seekHandlersWired)
    {
        seekHandlersWired = true;
        strip1.getWaveform().onVideoSeek = [&deck1](double seconds) { deck1.seekVideo(seconds); };
        strip2.getWaveform().onVideoSeek = [&deck2](double seconds) { deck2.seekVideo(seconds); };
    }

    auto buildInfo = [](DeckComponent& deck) {
        WavesTrackInfo info;
        info.title = deck.getTrackTitle();
        info.artist = deck.getTrackArtist();
        info.keyText = deck.getTrackKey();

        const bool hasTrack = deck.isLoaded();
        info.bpmText = hasTrack ? juce::String(deck.getBpm(), 1) : juce::String("--");

        char curBuf[24];
        char totBuf[24];
        formatStripTime(deck.getCurrentSeconds(), curBuf, sizeof(curBuf));
        formatStripTime(deck.getTotalSeconds(), totBuf, sizeof(totBuf));
        info.timeText = juce::String(curBuf) + " / " + juce::String(totBuf);

        float pitch = deck.getPitchOffsetPercent();
        info.pitchText = (pitch >= 0.0f ? "+" : "") + juce::String(pitch, 1) + "%";
        info.loaded = hasTrack;
        return info;
    };

    strip1.setTrackInfo(buildInfo(deck1));
    strip2.setTrackInfo(buildInfo(deck2));

    strip1.getWaveform().updateWaveform();
    strip2.getWaveform().updateWaveform();
}

void WavesViewComponent::languageChanged()
{
    strip1.refreshLanguage();
    strip2.refreshLanguage();
}

void WavesViewComponent::resized()
{
    auto area = getLocalBounds();
    const int gap = 4;
    const int stripHeight = juce::jmax(1, (area.getHeight() - gap) / 2);

    strip1.setBounds(area.removeFromTop(stripHeight));
    area.removeFromTop(gap);
    strip2.setBounds(area);
}
