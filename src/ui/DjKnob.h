#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class DjKnob : public juce::Slider
{
public:
    DjKnob(const juce::String& labelText, juce::Colour accentColour = juce::Colour::fromRGB(0, 210, 255))
        : label(labelText), accent(accentColour)
    {
        setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                            juce::MathConstants<float>::pi * 2.75f,
                            true);
    }

    void setAccentColour(juce::Colour c) { accent = c; repaint(); }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto textHeight = 14.0f;
        auto knobArea = bounds.removeFromTop(bounds.getHeight() - textHeight);

        float size = juce::jmin(knobArea.getWidth(), knobArea.getHeight()) - 6.0f;
        auto center = knobArea.getCentre();
        auto radius = size * 0.5f;

        // Outer dark ring / bezel
        g.setColour(juce::Colour::fromRGB(23, 25, 30));
        g.fillEllipse(center.x - radius, center.y - radius, size, size);

        g.setColour(juce::Colour::fromRGB(42, 45, 54));
        g.drawEllipse(center.x - radius, center.y - radius, size, size, 1.5f);

        // Value arc
        float rotaryStartAngle = juce::MathConstants<float>::pi * 1.25f;
        float rotaryEndAngle   = juce::MathConstants<float>::pi * 2.75f;
        float sliderPosProportion = (float)valueToProportionOfLength(getValue());
        float currentAngle = rotaryStartAngle + sliderPosProportion * (rotaryEndAngle - rotaryStartAngle);

        juce::Path backgroundArc;
        backgroundArc.addCentredArc(center.x, center.y, radius - 3.0f, radius - 3.0f, 0.0f,
                                    rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour::fromRGB(32, 36, 44));
        g.strokePath(backgroundArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc(center.x, center.y, radius - 3.0f, radius - 3.0f, 0.0f,
                               rotaryStartAngle, currentAngle, true);
        g.setColour(accent);
        g.strokePath(valueArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Inner cap (metal gradient)
        float innerSize = size * 0.72f;
        float innerRadius = innerSize * 0.5f;
        juce::ColourGradient capGrad(juce::Colour::fromRGB(28, 30, 36), center.x, center.y - innerRadius,
                                     juce::Colour::fromRGB(15, 16, 20), center.x, center.y + innerRadius, false);
        g.setGradientFill(capGrad);
        g.fillEllipse(center.x - innerRadius, center.y - innerRadius, innerSize, innerSize);

        g.setColour(juce::Colour::fromRGB(50, 54, 65));
        g.drawEllipse(center.x - innerRadius, center.y - innerRadius, innerSize, innerSize, 1.0f);

        // Pointer indicator line
        juce::Path pointer;
        pointer.startNewSubPath(center.x, center.y - innerRadius * 0.25f);
        pointer.lineTo(center.x, center.y - innerRadius + 2.0f);

        g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.strokePath(pointer, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                     juce::AffineTransform::rotation(currentAngle, center.x, center.y));

        // Label below knob
        g.setColour(juce::Colour::fromRGB(142, 149, 165));
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText(label, bounds, juce::Justification::centred, false);
    }

private:
    juce::String label;
    juce::Colour accent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DjKnob)
};
