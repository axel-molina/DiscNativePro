#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "LucideIcons.h"
#include <optional>

class DjButton : public juce::Button
{
public:
    DjButton(const juce::String& buttonName = {})
        : Button(buttonName)
    {
    }

    void setIcon(LucideIcons::IconType type, float size = 15.0f)
    {
        icon = type;
        iconSize = size;
        repaint();
    }

    void clearIcon()
    {
        icon.reset();
        repaint();
    }

    void setText(const juce::String& newText)
    {
        buttonText = newText;
        repaint();
    }

    const juce::String& getText() const { return buttonText; }

    void setCustomColours(juce::Colour bg,
                          juce::Colour textCol,
                          juce::Colour iconCol,
                          juce::Colour border = juce::Colours::transparentBlack)
    {
        bgColour = bg;
        textColour = textCol;
        iconColour = iconCol;
        borderColour = border;
        repaint();
    }

    void setCornerRadius(float r) { cornerRadius = r; repaint(); }
    void setFontSize(float fs) { fontSize = fs; repaint(); }

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = getLocalBounds().toFloat();

        auto bg = bgColour;
        if (shouldDrawButtonAsDown)
            bg = bg.darker(0.18f);
        else if (shouldDrawButtonAsHighlighted)
            bg = bg.brighter(0.12f);

        g.setColour(bg);
        g.fillRoundedRectangle(bounds.reduced(1.0f), cornerRadius);

        auto border = borderColour.isTransparent() ? bg.brighter(0.2f) : borderColour;
        if (shouldDrawButtonAsHighlighted)
            border = border.brighter(0.25f);

        g.setColour(border);
        g.drawRoundedRectangle(bounds.reduced(1.0f), cornerRadius, 1.0f);

        if (icon.has_value() && buttonText.isNotEmpty())
        {
            // Measure total width to centre both icon and text
            auto font = juce::FontOptions(fontSize, juce::Font::bold);
            juce::GlyphArrangement ga;
            ga.addLineOfText(juce::Font(font), buttonText, 0.0f, 0.0f);
            float textWidth = ga.getBoundingBox(0, -1, false).getWidth();

            float gap = 5.0f;
            float totalWidth = iconSize + gap + textWidth;
            float startX = (bounds.getWidth() - totalWidth) * 0.5f;
            if (startX < 4.0f) startX = 4.0f;

            auto iconArea = juce::Rectangle<float>(startX, (bounds.getHeight() - iconSize) * 0.5f, iconSize, iconSize);
            LucideIcons::draw(g, *icon, iconArea, iconColour, 1.8f);

            auto textArea = juce::Rectangle<float>(startX + iconSize + gap, 0.0f, textWidth + 8.0f, bounds.getHeight());
            g.setColour(textColour);
            g.setFont(font);
            g.drawText(buttonText, textArea, juce::Justification::centredLeft, false);
        }
        else if (icon.has_value())
        {
            auto iconArea = bounds.withSizeKeepingCentre(iconSize, iconSize);
            LucideIcons::draw(g, *icon, iconArea, iconColour, 1.8f);
        }
        else
        {
            g.setColour(textColour);
            g.setFont(juce::FontOptions(fontSize, juce::Font::bold));
            g.drawText(buttonText, bounds, juce::Justification::centred, false);
        }
    }

private:
    std::optional<LucideIcons::IconType> icon;
    juce::String buttonText;
    float iconSize { 15.0f };
    float cornerRadius { 6.0f };
    float fontSize { 11.5f };

    juce::Colour bgColour { juce::Colour::fromRGB(24, 26, 34) };
    juce::Colour textColour { juce::Colours::white };
    juce::Colour iconColour { juce::Colour::fromRGB(156, 163, 175) };
    juce::Colour borderColour { juce::Colour::fromRGB(38, 43, 56) };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DjButton)
};
