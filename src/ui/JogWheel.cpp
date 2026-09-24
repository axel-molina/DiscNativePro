#include "JogWheel.h"
#include "CoverArtGenerator.h"
#include <cmath>

JogWheel::JogWheel(DeckPlayer& player, juce::Colour accentColour)
    : deck(player), accent(accentColour)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

JogWheel::~JogWheel()
{
}

void JogWheel::setTrackInfo(const juce::String& title, double bpm, juce::Colour deckColor)
{
    trackTitle = title;
    trackBpm = bpm;
    accent = deckColor;
    coverArtImage = CoverArtGenerator::getCoverForTrack(title, 256);
    repaint();
}

void JogWheel::updateAngle()
{
    if (deck.isPlaying())
    {
        double pos = deck.getPosition();
        // 33 1/3 RPM = 200 degrees / sec = 0.5555 revs / sec
        double revs = pos * (33.3333 / 60.0);
        currentAngleRadians = (float)std::fmod(revs * juce::MathConstants<double>::twoPi, juce::MathConstants<double>::twoPi);
        repaint();
    }
}

float JogWheel::getAngleFromPoint(juce::Point<float> pt) const
{
    auto center = getLocalBounds().toFloat().getCentre();
    return std::atan2(pt.y - center.y, pt.x - center.x);
}

void JogWheel::mouseDown(const juce::MouseEvent& e)
{
    wasPlayingBeforeDrag = deck.isPlaying();
    if (wasPlayingBeforeDrag)
    {
        deck.pause();
    }
    lastMouseAngle = getAngleFromPoint(e.position);
}

void JogWheel::mouseDrag(const juce::MouseEvent& e)
{
    float newAngle = getAngleFromPoint(e.position);
    float delta = newAngle - lastMouseAngle;

    // Handle circular wrap-around
    if (delta > juce::MathConstants<float>::pi)
        delta -= juce::MathConstants<float>::twoPi;
    else if (delta < -juce::MathConstants<float>::pi)
        delta += juce::MathConstants<float>::twoPi;

    // 1 full turn (2*pi) = 1.8 seconds of audio
    double timeDelta = (delta / juce::MathConstants<float>::twoPi) * 1.8;
    double newPos = juce::jmax(0.0, deck.getPosition() + timeDelta);
    deck.setPosition(newPos);

    currentAngleRadians += delta;
    lastMouseAngle = newAngle;
    repaint();
}

void JogWheel::mouseUp(const juce::MouseEvent& /*e*/)
{
    if (wasPlayingBeforeDrag)
    {
        deck.play();
    }
}

void JogWheel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    // Clamped max ~226px matching Turntable.tsx max-w-[230px]
    float diameter = juce::jmin(226.0f, juce::jmin(bounds.getWidth(), bounds.getHeight()) - 6.0f);
    if (diameter <= 20.0f) return;

    auto center = bounds.getCentre();
    float radius = diameter * 0.5f;

    // 1. Turntable Platter Chassis Base (#111215 with border #242730)
    g.setColour(juce::Colour::fromRGB(17, 18, 21));
    g.fillEllipse(center.x - radius, center.y - radius, diameter, diameter);

    g.setColour(juce::Colour::fromRGB(36, 39, 48));
    g.drawEllipse(center.x - radius, center.y - radius, diameter, diameter, 2.0f);

    // Outer Strobe Dots Rim (dashed border #383d4c)
    float strobeR = radius - 3.0f;
    int numTicks = 44;
    for (int i = 0; i < numTicks; ++i)
    {
        float a = (float)i * (juce::MathConstants<float>::twoPi / (float)numTicks);
        float x1 = center.x + std::cos(a) * (strobeR - 1.0f);
        float y1 = center.y + std::sin(a) * (strobeR - 1.0f);
        float x2 = center.x + std::cos(a) * (strobeR - 3.5f);
        float y2 = center.y + std::sin(a) * (strobeR - 3.5f);

        g.setColour(juce::Colour::fromRGB(56, 61, 76).withAlpha(0.7f));
        g.drawLine(x1, y1, x2, y2, 1.2f);
    }

    // 2. Vinyl Record Body (Rotates with playback & scratch)
    float vinylRadius = radius - 6.0f;
    float vinylDia = vinylRadius * 2.0f;
    float labelRadius = vinylRadius * 0.38f;
    float labelDia = labelRadius * 2.0f;

    {
        juce::Graphics::ScopedSaveState state(g);
        // Apply rotation around center
        g.addTransform(juce::AffineTransform::rotation(currentAngleRadians, center.x, center.y));

        // Vinyl grooves background (.vinyl-grooves radial gradient)
        juce::ColourGradient vinylGrad(juce::Colour::fromRGB(27, 28, 32), center.x, center.y,
                                       juce::Colour::fromRGB(22, 23, 27), center.x, center.y - vinylRadius, true);
        vinylGrad.addColour(0.18, juce::Colour::fromRGB(15, 16, 19));
        vinylGrad.addColour(0.25, juce::Colour::fromRGB(35, 38, 44));
        vinylGrad.addColour(0.38, juce::Colour::fromRGB(19, 20, 23));
        vinylGrad.addColour(0.50, juce::Colour::fromRGB(37, 40, 48));
        vinylGrad.addColour(0.65, juce::Colour::fromRGB(18, 19, 22));
        vinylGrad.addColour(0.75, juce::Colour::fromRGB(31, 33, 39));
        vinylGrad.addColour(0.88, juce::Colour::fromRGB(14, 15, 18));
        g.setGradientFill(vinylGrad);
        g.fillEllipse(center.x - vinylRadius, center.y - vinylRadius, vinylDia, vinylDia);

        // Concentric microgroove rings
        for (int r = 1; r <= 6; ++r)
        {
            float stepR = labelRadius + (vinylRadius - labelRadius) * ((float)r / 7.0f);
            g.setColour(juce::Colour::fromRGB(45, 48, 58).withAlpha(0.35f));
            g.drawEllipse(center.x - stepR, center.y - stepR, stepR * 2.0f, stepR * 2.0f, 0.7f);
        }

        // Realistic Vinyl Sheen Overlay (conic butterfly reflections)
        juce::Path sheen;
        sheen.addPieSegment(center.x - vinylRadius, center.y - vinylRadius, vinylDia, vinylDia,
                            0.78f, 1.40f, 0.38f);
        sheen.addPieSegment(center.x - vinylRadius, center.y - vinylRadius, vinylDia, vinylDia,
                            3.92f, 4.54f, 0.38f);
        g.setColour(juce::Colours::white.withAlpha(0.065f));
        g.fillPath(sheen);

        juce::Path sheen2;
        sheen2.addPieSegment(center.x - vinylRadius, center.y - vinylRadius, vinylDia, vinylDia,
                             2.35f, 2.85f, 0.38f);
        sheen2.addPieSegment(center.x - vinylRadius, center.y - vinylRadius, vinylDia, vinylDia,
                             5.50f, 6.00f, 0.38f);
        g.setColour(juce::Colours::white.withAlpha(0.045f));
        g.fillPath(sheen2);

        // Scratch Position White Cue Tape Line (Matte hardware stripe pointing at 12 o'clock)
        // Extends from vinyl outer edge down to center label
        float tapeTop = center.y - vinylRadius + 1.0f;
        float tapeBottom = center.y - labelRadius;
        float tapeWidth = 3.5f;

        g.setColour(juce::Colours::white.withAlpha(0.92f));
        g.fillRoundedRectangle(center.x - tapeWidth * 0.5f, tapeTop, tapeWidth, tapeBottom - tapeTop, 1.5f);

        // 3. Center Label (Record Sticker)
        juce::Path labelCircle;
        labelCircle.addEllipse(center.x - labelRadius, center.y - labelRadius, labelDia, labelDia);

        {
            juce::Graphics::ScopedSaveState labelState(g);
            g.reduceClipRegion(labelCircle);

            // Album Artwork Background
            if (coverArtImage.isValid())
            {
                g.drawImage(coverArtImage,
                            juce::Rectangle<float>(center.x - labelRadius, center.y - labelRadius, labelDia, labelDia),
                            juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);
                g.setColour(juce::Colours::black.withAlpha(0.35f));
                g.fillRect(center.x - labelRadius, center.y - labelRadius, labelDia, labelDia);
            }
            else
            {
                juce::ColourGradient stickerBg(juce::Colour::fromRGB(27, 30, 39), center.x, center.y - labelRadius,
                                               juce::Colour::fromRGB(13, 14, 18), center.x, center.y + labelRadius, false);
                g.setGradientFill(stickerBg);
                g.fillAll();
            }

            // DiscPro Center Branding
            g.setColour(juce::Colours::white.withAlpha(0.95f));
            g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
            g.drawText("DiscPro",
                       juce::Rectangle<float>(center.x - labelRadius, center.y - 17.0f, labelDia, 13.0f),
                       juce::Justification::centred, false);

            // Color bar separator
            g.setColour(accent);
            g.fillRoundedRectangle(center.x - 12.0f, center.y - 2.5f, 24.0f, 2.0f, 1.0f);

            // BPM Text
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.setFont(juce::FontOptions(7.5f));
            juce::String bpmText = (trackBpm > 0.0) ? juce::String((int)std::round(trackBpm)) + " BPM" : "33 RPM";
            g.drawText(bpmText,
                       juce::Rectangle<float>(center.x - labelRadius, center.y + 1.5f, labelDia, 11.0f),
                       juce::Justification::centred, false);
        }

        // Center Label Border
        g.setColour(juce::Colour::fromRGB(51, 56, 70));
        g.drawEllipse(center.x - labelRadius, center.y - labelRadius, labelDia, labelDia, 2.0f);

        // 4. Center Spindle Hole (#0a0b0d with border #4b5263 and silver dot)
        float spindleRadius = 7.0f;
        g.setColour(juce::Colour::fromRGB(10, 11, 13));
        g.fillEllipse(center.x - spindleRadius, center.y - spindleRadius, spindleRadius * 2.0f, spindleRadius * 2.0f);
        g.setColour(juce::Colour::fromRGB(75, 82, 99));
        g.drawEllipse(center.x - spindleRadius, center.y - spindleRadius, spindleRadius * 2.0f, spindleRadius * 2.0f, 1.5f);

        // Silver center pin
        g.setColour(juce::Colour::fromRGB(190, 195, 205));
        g.fillEllipse(center.x - 1.5f, center.y - 1.5f, 3.0f, 3.0f);
    }
}
