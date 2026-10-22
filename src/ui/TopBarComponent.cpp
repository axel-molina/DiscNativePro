#include "TopBarComponent.h"
#if JUCE_MAC
#include <mach/mach.h>
#elif JUCE_WINDOWS
#include <windows.h>
#include <psapi.h>
#elif JUCE_LINUX
#include <unistd.h>
#endif
#include <cmath>

// ========================= ViewSelectorDropdown =========================
// Panel desplegable propio (estilo ComboBox): lista todas las vistas con la
// activa marcada. Se muestra como hijo del componente padre para no recortarse
// contra los 40px de la barra superior, en estado modal: cualquier clic fuera
// del panel o Esc lo cierran.

class TopBarComponent::ViewSelectorDropdown : public juce::Component
{
public:
    explicit ViewSelectorDropdown(TopBarComponent& o) : owner(o)
    {
        setWantsKeyboardFocus(true);
    }

    void setCurrentMode(ViewMode mode)
    {
        currentMode = mode;
        repaint();
    }

    void refreshLabels()
    {
        for (int i = 0; i < numOptions; ++i)
            labels[i] = owner.viewModeLabel(modes[i]);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        g.setColour(juce::Colour::fromRGB(22, 24, 32));
        g.fillRoundedRectangle(bounds.reduced(1.0f), 6.0f);
        g.setColour(juce::Colour::fromRGB(45, 50, 65));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 6.0f, 1.0f);

        for (int i = 0; i < numOptions; ++i)
        {
            auto row = rowRect(i).toFloat();
            const bool selected = (modes[i] == currentMode);

            if (i == hoveredRow)
            {
                g.setColour(juce::Colour::fromRGB(30, 41, 59));
                g.fillRoundedRectangle(row.reduced(3.0f, 1.0f), 4.0f);
            }

            if (selected)
            {
                LucideIcons::draw(g, LucideIcons::IconType::Check,
                                  juce::Rectangle<float>(row.getX() + 10.0f, row.getCentreY() - 6.0f, 12.0f, 12.0f),
                                  juce::Colour::fromRGB(96, 165, 250), 2.0f);
            }

            g.setColour(selected ? juce::Colours::white : juce::Colour::fromRGB(142, 149, 165));
            g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
            g.drawText(labels[i],
                       juce::Rectangle<float>(row.getX() + 28.0f, row.getY(), row.getWidth() - 34.0f, row.getHeight()),
                       juce::Justification::centredLeft, false);
        }
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        int newHover = -1;
        for (int i = 0; i < numOptions; ++i)
            if (rowRect(i).contains(e.getPosition()))
            {
                newHover = i;
                break;
            }

        if (newHover != hoveredRow)
        {
            hoveredRow = newHover;
            repaint();
        }
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        if (hoveredRow != -1)
        {
            hoveredRow = -1;
            repaint();
        }
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        for (int i = 0; i < numOptions; ++i)
            if (rowRect(i).contains(e.getPosition()))
            {
                owner.selectViewMode(modes[i]);
                return;
            }
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            owner.hideViewSelector();
            return true;
        }
        return false;
    }

    void inputAttemptWhenModal() override
    {
        owner.hideViewSelector();
    }

private:
    static constexpr int numOptions = 2;
    static constexpr int rowHeight = 26;
    static constexpr ViewMode modes[numOptions] = { ViewMode::TwoDecks, ViewMode::Waves };

    juce::Rectangle<int> rowRect(int index) const
    {
        return juce::Rectangle<int>(0, 4 + index * rowHeight, getWidth(), rowHeight);
    }

    TopBarComponent& owner;
    ViewMode currentMode { ViewMode::TwoDecks };
    int hoveredRow { -1 };
    juce::String labels[numOptions];
};

TopBarComponent::TopBarComponent()
{
    // 1. Left Action Buttons
    recButton.setIcon(LucideIcons::IconType::CircleDot, 12.0f);
    recButton.setText("REC");
    recButton.setCustomColours(juce::Colour::fromRGB(24, 26, 32),
                               juce::Colour::fromRGB(142, 149, 165),
                               juce::Colour::fromRGB(239, 68, 68),
                               juce::Colour::fromRGB(45, 50, 65));
    recButton.onClick = [this]() { if (onRecClicked) onRecClicked(); };
    addAndMakeVisible(recButton);

    fxButton.setText("FX");
    fxButton.setCustomColours(juce::Colour::fromRGB(24, 26, 32),
                              juce::Colour::fromRGB(96, 165, 250),
                              juce::Colour::fromRGB(96, 165, 250),
                              juce::Colour::fromRGB(45, 50, 65));
    fxButton.onClick = [this]() { if (onFxClicked) onFxClicked(); };
    addAndMakeVisible(fxButton);

    smpButton.setText("SMP");
    smpButton.setCustomColours(juce::Colour::fromRGB(24, 26, 32),
                               juce::Colour::fromRGB(96, 165, 250),
                               juce::Colour::fromRGB(96, 165, 250),
                               juce::Colour::fromRGB(45, 50, 65));
    smpButton.onClick = [this]() { if (onSmpClicked) onSmpClicked(); };
    addAndMakeVisible(smpButton);

    // 2. Center Brand
    brandLabel.setText("DiscNativePro", juce::dontSendNotification);
    brandLabel.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    brandLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    brandLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(brandLabel);

    // 3. Right Status & Actions
    updateClock();
    clockLabel.setFont(juce::FontOptions("Menlo", 11.0f, juce::Font::plain));
    clockLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(142, 149, 165));
    clockLabel.setColour(juce::Label::backgroundColourId, juce::Colour::fromRGB(22, 24, 32));
    clockLabel.setColour(juce::Label::outlineColourId, juce::Colour::fromRGB(37, 40, 51));
    clockLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(clockLabel);

    updateMemoryUsage();
    ramLabel.setFont(juce::FontOptions("Menlo", 11.0f, juce::Font::plain));
    ramLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(142, 149, 165));
    ramLabel.setColour(juce::Label::backgroundColourId, juce::Colour::fromRGB(22, 24, 32));
    ramLabel.setColour(juce::Label::outlineColourId, juce::Colour::fromRGB(37, 40, 51));
    ramLabel.setJustificationType(juce::Justification::centred);
    ramLabel.setTooltip("Consumo de memoria RAM de DiscNativePro en tiempo real");
    addAndMakeVisible(ramLabel);

    // Selector de vista desplegable: muestra la vista activa y despliega la lista de opciones
    viewSelectorButton.setIcon(LucideIcons::IconType::ChevronDown, 12.0f);
    viewSelectorButton.setFontSize(10.5f);
    viewSelectorButton.setCornerRadius(6.0f);
    viewSelectorButton.onClick = [this]() { toggleViewSelector(); };
    addAndMakeVisible(viewSelectorButton);
    updateViewSelectorAppearance(false);

    settingsButton.setIcon(LucideIcons::IconType::Settings, 15.0f);
    settingsButton.setCustomColours(juce::Colour::fromRGB(24, 26, 32),
                                   juce::Colours::white,
                                   juce::Colour::fromRGB(142, 149, 165),
                                   juce::Colour::fromRGB(45, 50, 65));
    settingsButton.onClick = [this]() { if (onSettingsClicked) onSettingsClicked(); };
    addAndMakeVisible(settingsButton);

    fullscreenButton.setIcon(LucideIcons::IconType::Maximize, 14.0f);
    fullscreenButton.setCustomColours(juce::Colour::fromRGB(24, 26, 32),
                                     juce::Colours::white,
                                     juce::Colour::fromRGB(142, 149, 165),
                                     juce::Colour::fromRGB(45, 50, 65));
    fullscreenButton.onClick = [this]() { if (onFullscreenClicked) onFullscreenClicked(); };
    addAndMakeVisible(fullscreenButton);

    updateButton.setIcon(LucideIcons::IconType::ChevronDown, 12.0f);
    updateButton.setText("UPDATE");
    updateButton.setFontSize(10.5f);
    updateButton.setCornerRadius(6.0f);
    updateButton.setCustomColours(juce::Colour::fromRGB(245, 158, 11),
                                 juce::Colours::black,
                                 juce::Colours::black,
                                 juce::Colour::fromRGB(251, 191, 36));
    updateButton.onClick = [this]() { if (onUpdateClicked) onUpdateClicked(); };
    updateButton.setVisible(false);
    addAndMakeVisible(updateButton);

    LanguageManager::getInstance().addListener(this);
    refreshLanguage();

    startTimer(1000); // 1-second clock timer
}

TopBarComponent::~TopBarComponent()
{
    LanguageManager::getInstance().removeListener(this);
    stopTimer();
}

void TopBarComponent::languageChanged()
{
    refreshLanguage();
}

void TopBarComponent::refreshLanguage()
{
    ramLabel.setTooltip(LanguageManager::trUtf8("Consumo de memoria RAM de DiscNativePro en tiempo real", "Real-time RAM usage of DiscNativePro"));
    settingsButton.setTooltip(LanguageManager::trUtf8("Preferencias", "Preferences"));
    fullscreenButton.setTooltip(LanguageManager::trUtf8("Pantalla Completa", "Fullscreen"));
    recButton.setTooltip(LanguageManager::trUtf8("Grabar Sesión (⌘R / Ctrl+R)", "Record Session (⌘R / Ctrl+R)"));
    fxButton.setTooltip(LanguageManager::trUtf8("Panel de Efectos (FX)", "Effects Panel (FX)"));
    viewSelectorButton.setText(viewModeLabel(currentViewMode));
    viewSelectorButton.setTooltip(LanguageManager::trUtf8("Cambiar la vista de la interfaz", "Switch interface view"));
    if (viewDropdown != nullptr)
        viewDropdown->refreshLabels();
    updateButton.setTooltip(LanguageManager::trUtf8("Nueva versión disponible — clic para ver", "New version available — click to view"));
}

void TopBarComponent::setViewMode(ViewMode mode, bool sendCallback)
{
    currentViewMode = mode;
    viewSelectorButton.setText(viewModeLabel(mode));

    if (sendCallback && onViewModeChanged)
        onViewModeChanged(mode);
}

void TopBarComponent::selectViewMode(ViewMode mode)
{
    hideViewSelector();

    if (mode != currentViewMode)
        setViewMode(mode, true);
}

void TopBarComponent::toggleViewSelector()
{
    if (viewDropdown != nullptr && viewDropdown->isVisible())
    {
        hideViewSelector();
        return;
    }

    if (viewDropdown == nullptr)
        viewDropdown = std::make_unique<ViewSelectorDropdown>(*this);

    if (auto* parent = getParentComponent())
    {
        const auto buttonPos = viewSelectorButton.getScreenPosition() - parent->getScreenPosition();
        viewDropdown->setBounds(buttonPos.x, buttonPos.y + viewSelectorButton.getHeight() + 2,
                                viewSelectorButton.getWidth(), 8 + 2 * 26);
        parent->addAndMakeVisible(viewDropdown.get());
        viewDropdown->setCurrentMode(currentViewMode);
        viewDropdown->refreshLabels();
        viewDropdown->toFront(true);
        viewDropdown->enterModalState(true);
        updateViewSelectorAppearance(true);
    }
}

void TopBarComponent::hideViewSelector()
{
    if (viewDropdown != nullptr && viewDropdown->isVisible())
    {
        viewDropdown->exitModalState(0);
        viewDropdown->setVisible(false);
        updateViewSelectorAppearance(false);
    }
}

void TopBarComponent::updateViewSelectorAppearance(bool open)
{
    viewSelectorButton.setCustomColours(juce::Colour::fromRGB(22, 24, 32),
                                        juce::Colours::white,
                                        juce::Colour::fromRGB(142, 149, 165),
                                        open ? juce::Colour::fromRGB(96, 165, 250)
                                             : juce::Colour::fromRGB(37, 40, 51));
}

juce::String TopBarComponent::viewModeLabel(ViewMode mode) const
{
    return mode == ViewMode::Waves ? LanguageManager::trUtf8("Ondas", "Waves")
                                   : LanguageManager::trUtf8("2 Bandejas", "2 Decks");
}

void TopBarComponent::setUpdateAvailable(bool available)
{
    updateButton.setVisible(available);
    resized();
}

void TopBarComponent::setFxActive(bool active)
{
    if (active)
    {
        fxButton.setCustomColours(juce::Colour::fromRGB(30, 41, 59),
                                  juce::Colour::fromRGB(56, 189, 248),
                                  juce::Colour::fromRGB(56, 189, 248),
                                  juce::Colour::fromRGB(56, 189, 248).withAlpha(0.6f));
    }
    else
    {
        fxButton.setCustomColours(juce::Colour::fromRGB(24, 26, 32),
                                  juce::Colour::fromRGB(96, 165, 250),
                                  juce::Colour::fromRGB(96, 165, 250),
                                  juce::Colour::fromRGB(45, 50, 65));
    }
}

void TopBarComponent::setSmpActive(bool active)
{
    if (active)
    {
        smpButton.setCustomColours(juce::Colour::fromRGB(30, 41, 59),
                                   juce::Colour::fromRGB(56, 189, 248),
                                   juce::Colour::fromRGB(56, 189, 248),
                                   juce::Colour::fromRGB(56, 189, 248).withAlpha(0.6f));
    }
    else
    {
        smpButton.setCustomColours(juce::Colour::fromRGB(24, 26, 32),
                                   juce::Colour::fromRGB(96, 165, 250),
                                   juce::Colour::fromRGB(96, 165, 250),
                                   juce::Colour::fromRGB(45, 50, 65));
    }
}

void TopBarComponent::setRecordingState(bool recording, double elapsedSeconds){
    isRecording = recording;
    currentRecSeconds = elapsedSeconds;

    if (isRecording)
    {
        int mins = (int)elapsedSeconds / 60;
        int secs = (int)elapsedSeconds % 60;
        recButton.setIcon(LucideIcons::IconType::CircleDot, 12.0f);
        recButton.setText(juce::String::formatted("%02d:%02d", mins, secs));
        recButton.setCustomColours(juce::Colour::fromRGB(220, 38, 38),
                                   juce::Colours::white,
                                   juce::Colours::white,
                                   juce::Colour::fromRGB(239, 68, 68));
    }
    else
    {
        recButton.setIcon(LucideIcons::IconType::CircleDot, 12.0f);
        recButton.setText("REC");
        recButton.setCustomColours(juce::Colour::fromRGB(24, 26, 32),
                                   juce::Colour::fromRGB(142, 149, 165),
                                   juce::Colour::fromRGB(239, 68, 68),
                                   juce::Colour::fromRGB(45, 50, 65));
    }
}

void TopBarComponent::updateClock()
{
    auto now = juce::Time::getCurrentTime();
    int h = now.getHours();
    int m = now.getMinutes();
    bool pm = h >= 12;
    if (h == 0) h = 12;
    else if (h > 12) h -= 12;

    timeString = juce::String::formatted("%d:%02d %s", h, m, pm ? "PM" : "AM");
    clockLabel.setText(timeString, juce::dontSendNotification);
}

double TopBarComponent::getProcessMemoryMB()
{
#if JUCE_MAC
    mach_task_basic_info info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS)
    {
        return static_cast<double>(info.resident_size) / (1024.0 * 1024.0);
    }
#elif JUCE_WINDOWS
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    {
        return static_cast<double>(pmc.WorkingSetSize) / (1024.0 * 1024.0);
    }
#elif JUCE_LINUX
    FILE* file = std::fopen("/proc/self/statm", "r");
    if (file != nullptr)
    {
        long pages = 0;
        long resident = 0;
        if (std::fscanf(file, "%ld %ld", &pages, &resident) == 2)
        {
            std::fclose(file);
            long pageSize = sysconf(_SC_PAGESIZE);
            return static_cast<double>(resident * pageSize) / (1024.0 * 1024.0);
        }
        std::fclose(file);
    }
#endif
    return 0.0;
}

void TopBarComponent::updateMemoryUsage()
{
    double memMb = getProcessMemoryMB();
    if (memMb >= 1024.0)
    {
        memoryString = juce::String::formatted("RAM: %.1f GB", memMb / 1024.0);
    }
    else if (memMb > 0.0)
    {
        memoryString = juce::String::formatted("RAM: %d MB", static_cast<int>(std::round(memMb)));
    }
    else
    {
        memoryString = "RAM: --";
    }

    ramLabel.setText(memoryString, juce::dontSendNotification);
}

void TopBarComponent::timerCallback()
{
    updateClock();
    updateMemoryUsage();
}

void TopBarComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Dark TopBar background #0f1115
    g.fillAll(juce::Colour::fromRGB(15, 17, 21));

    // Bottom border #20232c
    g.setColour(juce::Colour::fromRGB(32, 35, 44));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, bounds.getWidth());

    // Center Disc Logo icon (pure vector LucideIcons::Disc)
    float centerX = bounds.getCentreX() - 48.0f;
    float centerY = bounds.getCentreY();
    LucideIcons::draw(g, LucideIcons::IconType::Disc,
                      juce::Rectangle<float>(centerX - 8.0f, centerY - 8.0f, 16.0f, 16.0f),
                      juce::Colour::fromRGB(200, 205, 215), 1.8f);
}

void TopBarComponent::resized()
{
    auto area = getLocalBounds().reduced(8, 4);

    // Left Buttons
    recButton.setBounds(area.removeFromLeft(78).reduced(0, 2));
    area.removeFromLeft(6);
    fxButton.setBounds(area.removeFromLeft(58).reduced(0, 2));
    area.removeFromLeft(6);
    smpButton.setBounds(area.removeFromLeft(58).reduced(0, 2));

    // Right Buttons & Indicators
    fullscreenButton.setBounds(area.removeFromRight(32).reduced(0, 2));
    area.removeFromRight(4);
    settingsButton.setBounds(area.removeFromRight(32).reduced(0, 2));
    area.removeFromRight(6);
    if (updateButton.isVisible())
    {
        updateButton.setBounds(area.removeFromRight(92).reduced(0, 2));
        area.removeFromRight(6);
    }
    // Selector de vista desplegable (vista activa + lista de opciones)
    hideViewSelector();
    auto viewSelector = area.removeFromRight(132);
    viewSelectorButton.setBounds(viewSelector.reduced(1, 3));
    area.removeFromRight(6);
    clockLabel.setBounds(area.removeFromRight(80).reduced(0, 3));
    area.removeFromRight(6);
    ramLabel.setBounds(area.removeFromRight(95).reduced(0, 3));

    // Center Brand Title
    int centerWidth = 220;
    brandLabel.setBounds((getWidth() - centerWidth) / 2 + 10, area.getY(), centerWidth, area.getHeight());
}
