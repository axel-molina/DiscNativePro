#include "MidiModalComponent.h"
#include "../services/MixxxPresetCatalog.h"
#include "../services/MixxxMappingImporter.h"

namespace
{
void showMixxxImportResult(MidiManager& midi, const MixxxImportReport& report, const juce::String& extraError)
{
    juce::String title;
    juce::String msg;

    if (!report.ok)
    {
        title = LanguageManager::trUtf8("Error al importar preset", "Preset import error");
        msg = report.error.isNotEmpty() ? report.error : extraError;
        if (msg.isEmpty())
            msg = LanguageManager::trUtf8("No se pudo importar el preset.", "Could not import the preset.");
    }
    else
    {
        midi.getMappingManager().applyImportedBindings(report.bindings);
        if (report.outputsImported > 0)
            midi.getMappingManager().applyImportedOutputBindings(report.outputBindings);

        title = LanguageManager::trUtf8("Preset Mixxx importado", "Mixxx preset imported");
        msg = juce::String(LanguageManager::trUtf8("Preset: ", "Preset: ")) + report.presetName + "\n"
            + LanguageManager::trUtf8("Autor: ", "Author: ") + (report.author.isNotEmpty() ? report.author : "--")
            + LanguageManager::trUtf8("  (comunidad Mixxx)", "  (Mixxx community)") + "\n\n"
            + juce::String(report.imported)
            + LanguageManager::trUtf8(" controles importados y activos.", " controls imported and active.") + "\n";

        if (report.outputsImported > 0)
            msg += juce::String(report.outputsImported)
                 + LanguageManager::trUtf8(" salidas (LED) importadas.", " outputs (LED) imported.") + "\n";

        if (report.skippedScriptBinding > 0)
            msg += juce::String(report.skippedScriptBinding)
                 + LanguageManager::trUtf8(" controles script-binding omitidos (requieren scripts JS de Mixxx).",
                                           " script-binding controls skipped (require Mixxx JS scripts).") + "\n";

        if (report.unknownKeys.size() > 0)
        {
            msg += "\n" + LanguageManager::trUtf8("Controles sin equivalente (no importados):", "Controls without equivalent (not imported):") + "\n";
            const int shown = juce::jmin(8, report.unknownKeys.size());
            for (int i = 0; i < shown; ++i)
                msg += "  • " + report.unknownKeys[i] + "\n";
            if (report.unknownKeys.size() > shown)
                msg += "  … +" + juce::String(report.unknownKeys.size() - shown) + "\n";
        }
    }

    auto opts = juce::MessageBoxOptions()
                    .withTitle(title)
                    .withMessage(msg)
                    .withButton("OK")
                    .withIconType(juce::MessageBoxIconType::InfoIcon);
    juce::AlertWindow::showAsync(opts, nullptr);
}
}

MidiModalComponent::MidiModalComponent(MidiManager& midiMgr)
    : midi(midiMgr)
{
    headerTitle.setFont(juce::FontOptions(13.5f, juce::Font::bold));
    headerTitle.setColour(juce::Label::textColourId, juce::Colour::fromRGB(46, 196, 182));
    addAndMakeVisible(headerTitle);

    descLabel.setFont(juce::FontOptions(11.0f));
    descLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(142, 149, 165));
    addAndMakeVisible(descLabel);

    devicesHeader.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    devicesHeader.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(devicesHeader);

    scanBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(24, 28, 36));
    scanBtn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(0, 180, 216));
    scanBtn.onClick = [this]() {
        midi.rescanDevices();
        refreshDeviceList();
    };
    addAndMakeVisible(scanBtn);

    // P26: importar preset de la comunidad Mixxx (.midi.xml)
    mixxxBtn.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(24, 28, 36));
    mixxxBtn.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(255, 159, 28));
    mixxxBtn.onClick = [this]() { importMixxxPreset(); };
    addAndMakeVisible(mixxxBtn);

    monitorHeader.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    monitorHeader.setColour(juce::Label::textColourId, juce::Colour::fromRGB(100, 108, 125));
    addAndMakeVisible(monitorHeader);

    monitorText.setFont(juce::FontOptions("Menlo", 11.5f, juce::Font::plain));
    monitorText.setColour(juce::Label::textColourId, juce::Colour::fromRGB(0, 229, 255));
    addAndMakeVisible(monitorText);

    // Live listener for incoming MIDI messages
    midi.onMidiActivity = [this](const juce::String& desc) {
        monitorText.setText(desc, juce::dontSendNotification);
    };

    LanguageManager::getInstance().addListener(this);
    refreshLanguage();
    refreshDeviceList();
    setSize(480, 326);
}

MidiModalComponent::~MidiModalComponent()
{
    LanguageManager::getInstance().removeListener(this);
    midi.onMidiActivity = nullptr;
}

void MidiModalComponent::languageChanged()
{
    refreshLanguage();
}

void MidiModalComponent::refreshLanguage()
{
    headerTitle.setText(LanguageManager::trUtf8("Controladores MIDI USB", "USB MIDI Controllers"), juce::dontSendNotification);
    descLabel.setText(LanguageManager::trUtf8("DiscNativePro detecta automáticamente cualquier controladora DJ física conectada por USB (Pioneer DDJ, Hercules, Numark, Native Instruments). Mapea jog wheels, perillas de EQ, faders y pads.",
                                             "DiscNativePro automatically detects any physical DJ controller connected via USB (Pioneer DDJ, Hercules, Numark, Native Instruments). Maps jog wheels, EQ knobs, faders and pads."), juce::dontSendNotification);
    scanBtn.setButtonText(LanguageManager::trUtf8("Escanear", "Scan"));
    mixxxBtn.setButtonText(LanguageManager::trUtf8("Importar Mixxx", "Import Mixxx"));
    monitorHeader.setText(LanguageManager::trUtf8("MONITOR DE SEÑALES MIDI EN VIVO", "LIVE MIDI SIGNAL MONITOR"), juce::dontSendNotification);
    monitorText.setText(LanguageManager::trUtf8("Esperando movimiento de fader, jog o pad...", "Waiting for fader, jog or pad movement..."), juce::dontSendNotification);
    refreshDeviceList();
}

void MidiModalComponent::refreshDeviceList()
{
    currentDevices = midi.getConnectedDeviceNames();
    devicesHeader.setText(LanguageManager::trUtf8("DISPOSITIVOS CONECTADOS (", "CONNECTED DEVICES (") + juce::String(currentDevices.size()) + ")",
                          juce::dontSendNotification);
    repaint();
}

void MidiModalComponent::importMixxxPreset()
{
    if (importing.load())
        return;

    // 1. Detectar controladora conectada con preset en el catálogo de Mixxx
    MixxxPresetEntry entry;
    juce::String matchedDevice;
    for (const auto& d : midi.getConnectedDeviceNames())
    {
        if (MixxxPresetCatalog::findForDevice(d, entry))
        {
            matchedDevice = d;
            break;
        }
    }

    if (matchedDevice.isEmpty())
    {
        // Sin match: FileChooser manual para importar cualquier .midi.xml
        auto chooser = std::make_shared<juce::FileChooser>(
            LanguageManager::trUtf8("Seleccionar preset .midi.xml de Mixxx", "Select Mixxx .midi.xml preset"),
            MixxxPresetCatalog::presetsFolder(), "*.midi.xml;*.xml");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc) {
                auto f = fc.getResult();
                if (f.existsAsFile())
                    showMixxxImportResult(midi, MixxxMappingImporter::importFile(f), {});
            });
        return;
    }

    // 2. Descarga del preset oficial en thread de fondo (HTTP, no toca audio)
    importing.store(true);
    mixxxBtn.setEnabled(false);
    mixxxBtn.setButtonText(LanguageManager::trUtf8("Descargando…", "Downloading…"));

    const auto presetFile = entry.presetFile;
    juce::Thread::launch([safe = juce::Component::SafePointer<MidiModalComponent>(this), presetFile]() {
        juce::String error;
        const bool ok = MixxxPresetCatalog::downloadPreset(presetFile, error);

        juce::MessageManager::callAsync([safe, presetFile, ok, error]() {
            if (safe == nullptr)
                return;
            safe->mixxxBtn.setEnabled(true);
            safe->importing.store(false);
            safe->mixxxBtn.setButtonText(LanguageManager::trUtf8("Importar Mixxx", "Import Mixxx"));

            if (!ok)
            {
                MixxxImportReport fail;
                fail.error = error;
                showMixxxImportResult(safe->midi, fail, {});
                return;
            }

            auto report = MixxxMappingImporter::importFile(MixxxPresetCatalog::localPresetFile(presetFile));
            showMixxxImportResult(safe->midi, report, {});
        });
    });
}

void MidiModalComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Modal Background #161822 with border #2c3142
    g.setColour(juce::Colour::fromRGB(22, 24, 34));
    g.fillRoundedRectangle(bounds, 10.0f);

    g.setColour(juce::Colour::fromRGB(44, 49, 66));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.2f);

    // Header bar #12131c
    auto headerBounds = bounds.removeFromTop(42.0f);
    g.setColour(juce::Colour::fromRGB(18, 19, 28));
    g.fillRoundedRectangle(headerBounds, 10.0f);
    g.fillRect(headerBounds.removeFromBottom(10.0f)); // straighten bottom corners of header
    g.setColour(juce::Colour::fromRGB(36, 40, 56));
    g.drawHorizontalLine(42, 0.0f, (float)getWidth());

    LucideIcons::draw(g, LucideIcons::IconType::Zap,
                      juce::Rectangle<float>(14.0f, 13.0f, 16.0f, 16.0f),
                      juce::Colour::fromRGB(46, 196, 182), 1.8f);

    // Devices List Card area
    juce::Rectangle<float> devBox(16.0f, 128.0f, (float)getWidth() - 32.0f, 110.0f);
    if (currentDevices.isEmpty())
    {
        g.setColour(juce::Colour::fromRGB(17, 18, 25));
        g.fillRoundedRectangle(devBox, 8.0f);

        // Dashed border
        g.setColour(juce::Colour::fromRGB(40, 44, 60));
        g.drawRoundedRectangle(devBox, 8.0f, 1.0f);

        g.setColour(juce::Colour::fromRGB(255, 159, 28));
        g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        g.drawText(LanguageManager::trUtf8("No se detectó ningún controlador", "No controller detected"), devBox.removeFromTop(60.0f), juce::Justification::centred, false);

        g.setColour(juce::Colour::fromRGB(142, 149, 165));
        g.setFont(juce::FontOptions(10.5f));
        g.drawText(LanguageManager::trUtf8("Conecta tu controladora DJ por cable USB y presiona 'Escanear'.",
                                           "Connect your DJ controller via USB and click 'Scan'."),
                   devBox.removeFromTop(35.0f), juce::Justification::centred, false);
    }
    else
    {
        float cardY = 128.0f;
        for (int i = 0; i < currentDevices.size() && i < 2; ++i)
        {
            juce::Rectangle<float> card(16.0f, cardY, (float)getWidth() - 32.0f, 48.0f);
            g.setColour(juce::Colour::fromRGB(26, 29, 41));
            g.fillRoundedRectangle(card, 6.0f);

            g.setColour(juce::Colour::fromRGB(42, 47, 64));
            g.drawRoundedRectangle(card, 6.0f, 1.0f);

            // Green connected dot
            g.setColour(juce::Colour::fromRGB(46, 196, 182));
            g.fillEllipse(card.getX() + 12.0f, card.getCentreY() - 4.0f, 8.0f, 8.0f);

            // Name
            g.setColour(juce::Colours::white);
            g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
            juce::Rectangle<int> nameBox((int)(card.getX() + 30.0f), (int)(card.getY() + 8.0f), (int)(card.getWidth() - 120.0f), 16);
            g.drawText(currentDevices[i], nameBox, juce::Justification::centredLeft, true);

            g.setColour(juce::Colour::fromRGB(142, 149, 165));
            g.setFont(juce::FontOptions(10.0f));
            juce::Rectangle<int> subBox((int)(card.getX() + 30.0f), (int)(card.getY() + 26.0f), (int)(card.getWidth() - 120.0f), 14);
            g.drawText(LanguageManager::trUtf8("Fabricante: Dispositivo MIDI USB", "Manufacturer: USB MIDI Device"), subBox, juce::Justification::centredLeft, true);

            // "Conectado" badge
            juce::Rectangle<float> badge(card.getRight() - 76.0f, card.getCentreY() - 9.0f, 66.0f, 18.0f);
            g.setColour(juce::Colour::fromRGB(46, 196, 182).withAlpha(0.18f));
            g.fillRoundedRectangle(badge, 4.0f);
            g.setColour(juce::Colour::fromRGB(46, 196, 182));
            g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
            g.drawText(LanguageManager::trUtf8("Conectado", "Connected"), badge, juce::Justification::centred, false);

            cardY += 54.0f;
        }
    }

    // Monitor background box #0d0e14
    juce::Rectangle<float> monBox(16.0f, 248.0f, (float)getWidth() - 32.0f, 60.0f);
    g.setColour(juce::Colour::fromRGB(13, 14, 20));
    g.fillRoundedRectangle(monBox, 6.0f);
    g.setColour(juce::Colour::fromRGB(33, 37, 52));
    g.drawRoundedRectangle(monBox, 6.0f, 1.0f);
}

void MidiModalComponent::resized()
{
    auto area = getLocalBounds();

    // Header
    auto header = area.removeFromTop(42).reduced(12, 6);
    header.removeFromLeft(22); // spacing for Zap icon
    headerTitle.setBounds(header);

    // Description text
    descLabel.setBounds(16, 48, getWidth() - 32, 42);

    // Devices bar
    devicesHeader.setBounds(16, 98, 236, 22);
    scanBtn.setBounds(getWidth() - 96, 98, 80, 22);
    mixxxBtn.setBounds(getWidth() - 210, 98, 106, 22);

    // Monitor text
    monitorHeader.setBounds(24, 252, getWidth() - 48, 16);
    monitorText.setBounds(24, 274, getWidth() - 48, 24);
}
