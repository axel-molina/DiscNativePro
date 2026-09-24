#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class VuMeter : public juce::Component
{
public:
    VuMeter() {}

    void setLevels(float leftPeak, float rightPeak)
    {
        currentLeft = juce::jlimit(0.0f, 1.2f, leftPeak);
        currentRight = juce::jlimit(0.0f, 1.2f, rightPeak);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.fillAll(juce::Colour::fromRGB(16, 17, 21)); // Dark trough

        float chWidth = (bounds.getWidth() - 3.0f) * 0.5f;

        drawChannel(g, bounds.removeFromLeft(chWidth), currentLeft);
        drawChannel(g, bounds.removeFromRight(chWidth), currentRight);
    }

private:
    void drawChannel(juce::Graphics& g, juce::Rectangle<float> area, float level)
    {
        g.setColour(juce::Colour::fromRGB(24, 27, 34));
        g.fillRect(area);

        int numLeds = 14;
        float numLedsF = static_cast<float>(numLeds);
        float ledSpacing = 2.0f;
        float totalHeight = area.getHeight();
        float ledHeight = (totalHeight - (numLedsF - 1.0f) * ledSpacing) / numLedsF;

        // Draw segmented LEDs from bottom to top
        for (int i = 0; i < numLeds; ++i)
        {
            float fi = static_cast<float>(i + 1);
            float normPos = fi / numLedsF;
            float y = area.getY() + totalHeight - fi * (ledHeight + ledSpacing) + ledSpacing;

            juce::Rectangle<float> ledRect(area.getX(), y, area.getWidth(), ledHeight);

            bool lit = (level >= (normPos * 0.95f));

            juce::Colour ledColour;
            if (i >= 12)
                ledColour = lit ? juce::Colour::fromRGB(255, 51, 75) : juce::Colour::fromRGB(60, 20, 25); // Red clip
            else if (i >= 9)
                ledColour = lit ? juce::Colour::fromRGB(255, 159, 28) : juce::Colour::fromRGB(65, 45, 15); // Amber
            else
                ledColour = lit ? juce::Colour::fromRGB(46, 196, 182) : juce::Colour::fromRGB(15, 45, 40); // Teal / Green

            g.setColour(ledColour);
            g.fillRoundedRectangle(ledRect, 1.0f);
        }
    }

    float currentLeft { 0.0f };
    float currentRight { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VuMeter)
};
