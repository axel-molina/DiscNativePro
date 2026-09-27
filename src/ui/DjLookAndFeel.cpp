#include "DjLookAndFeel.h"
#include <cmath>

DjLookAndFeel::DjLookAndFeel()
{
    setColour(juce::Slider::backgroundColourId, juce::Colour::fromRGB(12, 13, 16));
    setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(220, 225, 235));
    setColour(juce::Slider::trackColourId, juce::Colour::fromRGB(30, 34, 44));
}

DjLookAndFeel::~DjLookAndFeel()
{
}

juce::Slider::SliderLayout DjLookAndFeel::getSliderLayout(juce::Slider& slider)
{
    auto style = slider.getSliderStyle();
    if (style == juce::Slider::LinearHorizontal)
    {
        juce::Slider::SliderLayout layout;
        // 16px lateral margin for 28px width knob (14px half-width + 2px margin)
        layout.sliderBounds = slider.getLocalBounds().reduced(16, 0);
        return layout;
    }
    else if (style == juce::Slider::LinearVertical)
    {
        juce::Slider::SliderLayout layout;
        // 12px vertical margin for 18px height knob (9px half-height + 3px margin)
        layout.sliderBounds = slider.getLocalBounds().reduced(0, 12);
        return layout;
    }

    return juce::LookAndFeel_V4::getSliderLayout(slider);
}

void DjLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                     float sliderPos, float /*minSliderPos*/, float /*maxSliderPos*/,
                                     juce::Slider::SliderStyle style, juce::Slider& /*slider*/)
{
    if (style == juce::Slider::LinearVertical)
    {
        float trackW = 6.0f;
        float trackX = (float)x + ((float)width - trackW) * 0.5f;
        float trackY = (float)y;
        float trackH = (float)height;

        // Recessed track
        g.setColour(juce::Colour::fromRGB(12, 14, 18));
        g.fillRoundedRectangle(trackX, trackY, trackW, trackH, 3.0f);
        g.setColour(juce::Colour::fromRGB(36, 42, 54));
        g.drawRoundedRectangle(trackX, trackY, trackW, trackH, 3.0f, 1.0f);

        // Center detent marker
        float midY = (float)y + (float)height * 0.5f;
        g.setColour(juce::Colour::fromRGB(75, 85, 105));
        g.fillRect(trackX - 4.0f, midY - 0.75f, trackW + 8.0f, 1.5f);

        // DJ Fader Handle (Knob)
        float knobW = juce::jmin((float)width - 4.0f, 32.0f);
        float knobH = 18.0f;
        float knobX = (float)x + ((float)width - knobW) * 0.5f;
        float knobY = sliderPos - knobH * 0.5f;

        // Shadow
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillRoundedRectangle(knobX, knobY + 2.0f, knobW, knobH, 3.0f);

        // Metallic / Dark Gradient
        juce::ColourGradient knobGrad(juce::Colour::fromRGB(58, 63, 78), knobX, knobY,
                                      juce::Colour::fromRGB(22, 24, 31), knobX, knobY + knobH, false);
        knobGrad.addColour(0.5, juce::Colour::fromRGB(36, 40, 50));
        g.setGradientFill(knobGrad);
        g.fillRoundedRectangle(knobX, knobY, knobW, knobH, 3.0f);

        // Border
        g.setColour(juce::Colour::fromRGB(75, 82, 100));
        g.drawRoundedRectangle(knobX, knobY, knobW, knobH, 3.0f, 1.0f);

        // White center line notch
        g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.fillRect(knobX + 4.0f, sliderPos - 0.75f, knobW - 8.0f, 1.5f);
    }
    else if (style == juce::Slider::LinearHorizontal)
    {
        float trackH = 6.0f;
        float trackX = (float)x;
        float trackY = (float)y + ((float)height - trackH) * 0.5f;
        float trackW = (float)width;

        // Recessed track
        g.setColour(juce::Colour::fromRGB(9, 10, 13));
        g.fillRoundedRectangle(trackX, trackY, trackW, trackH, 3.0f);
        g.setColour(juce::Colour::fromRGB(38, 42, 54));
        g.drawRoundedRectangle(trackX, trackY, trackW, trackH, 3.0f, 1.0f);

        // Center detent marker
        float midX = (float)x + (float)width * 0.5f;
        g.setColour(juce::Colour::fromRGB(58, 64, 80));
        g.fillRect(midX - 0.75f, trackY - 4.0f, 1.5f, trackH + 8.0f);

        // DJ Crossfader Handle (Knob)
        float knobW = 28.0f;
        float knobH = juce::jmin((float)height - 4.0f, 22.0f);
        float knobX = sliderPos - knobW * 0.5f;
        float knobY = (float)y + ((float)height - knobH) * 0.5f;

        // Shadow
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillRoundedRectangle(knobX, knobY + 2.0f, knobW, knobH, 3.0f);

        // Gradient
        juce::ColourGradient knobGrad(juce::Colour::fromRGB(70, 76, 93), knobX, knobY,
                                      juce::Colour::fromRGB(19, 21, 28), knobX, knobY + knobH, false);
        knobGrad.addColour(0.5, juce::Colour::fromRGB(36, 39, 50));
        g.setGradientFill(knobGrad);
        g.fillRoundedRectangle(knobX, knobY, knobW, knobH, 3.0f);

        // Border
        g.setColour(juce::Colour::fromRGB(90, 98, 119));
        g.drawRoundedRectangle(knobX, knobY, knobW, knobH, 3.0f, 1.0f);

        // White vertical center notch
        g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.fillRect(sliderPos - 0.75f, knobY + 3.0f, 1.5f, knobH - 6.0f);
    }
}

void DjLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                     float sliderPosProportional, float rotaryStartAngle,
                                     float rotaryEndAngle, juce::Slider& /*slider*/)
{
    float radius = (float)juce::jmin(width, height) * 0.42f;
    float centreX = (float)x + (float)width * 0.5f;
    float centreY = (float)y + (float)height * 0.5f;
    float rx = centreX - radius;
    float ry = centreY - radius;
    float rw = radius * 2.0f;

    // Dark dial face
    juce::ColourGradient dialGrad(juce::Colour::fromRGB(35, 38, 48), centreX, ry,
                                  juce::Colour::fromRGB(14, 15, 19), centreX, ry + rw, false);
    g.setGradientFill(dialGrad);
    g.fillEllipse(rx, ry, rw, rw);

    g.setColour(juce::Colour::fromRGB(50, 56, 70));
    g.drawEllipse(rx, ry, rw, rw, 1.5f);

    // Indicator needle
    float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    float needleLen = radius * 0.75f;
    float nx = centreX + std::sin(angle) * needleLen;
    float ny = centreY - std::cos(angle) * needleLen;

    g.setColour(juce::Colours::white.withAlpha(0.95f));
    g.drawLine(centreX, centreY, nx, ny, 2.0f);

    // Center pivot cap
    float capR = radius * 0.28f;
    g.setColour(juce::Colour::fromRGB(20, 22, 28));
    g.fillEllipse(centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f);
    g.setColour(juce::Colour::fromRGB(65, 72, 88));
    g.drawEllipse(centreX - capR, centreY - capR, capR * 2.0f, capR * 2.0f, 1.0f);
}

void DjLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                        const juce::Colour& backgroundColour,
                                        bool shouldDrawButtonAsHighlighted,
                                        bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat();
    float corner = 5.0f;

    auto baseCol = backgroundColour;
    if (shouldDrawButtonAsDown)
        baseCol = baseCol.darker(0.2f);
    else if (shouldDrawButtonAsHighlighted)
        baseCol = baseCol.brighter(0.15f);

    g.setColour(baseCol);
    g.fillRoundedRectangle(bounds.reduced(1.0f), corner);

    g.setColour(baseCol.brighter(0.25f));
    g.drawRoundedRectangle(bounds.reduced(1.0f), corner, 1.0f);
}
