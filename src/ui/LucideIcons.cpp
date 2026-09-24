#include "LucideIcons.h"
#include <cmath>

juce::Path LucideIcons::getPath(IconType icon)
{
    juce::Path p;

    switch (icon)
    {
        case IconType::Play:
        {
            p.startNewSubPath(6.0f, 4.0f);
            p.lineTo(20.0f, 12.0f);
            p.lineTo(6.0f, 20.0f);
            p.closeSubPath();
            break;
        }

        case IconType::Pause:
        {
            p.addRoundedRectangle(6.0f, 4.0f, 3.5f, 16.0f, 1.0f);
            p.addRoundedRectangle(14.5f, 4.0f, 3.5f, 16.0f, 1.0f);
            break;
        }

        case IconType::Disc:
        {
            p.addEllipse(2.0f, 2.0f, 20.0f, 20.0f);
            p.startNewSubPath(15.0f, 12.0f);
            p.addEllipse(9.0f, 9.0f, 6.0f, 6.0f);
            p.startNewSubPath(13.0f, 12.0f);
            p.addEllipse(11.0f, 11.0f, 2.0f, 2.0f);
            break;
        }

        case IconType::Headphones:
        {
            // Headband arc
            p.startNewSubPath(3.0f, 14.0f);
            p.cubicTo(3.0f, 6.0f, 21.0f, 6.0f, 21.0f, 14.0f);
            // Left ear cup
            p.startNewSubPath(2.0f, 13.0f);
            p.addRoundedRectangle(2.0f, 13.0f, 4.0f, 8.0f, 1.5f);
            // Right ear cup
            p.startNewSubPath(18.0f, 13.0f);
            p.addRoundedRectangle(18.0f, 13.0f, 4.0f, 8.0f, 1.5f);
            break;
        }

        case IconType::Repeat:
        {
            // Top cycle
            p.startNewSubPath(17.0f, 2.0f);
            p.lineTo(21.0f, 6.0f);
            p.lineTo(17.0f, 10.0f);

            p.startNewSubPath(3.0f, 11.0f);
            p.lineTo(3.0f, 8.0f);
            p.cubicTo(3.0f, 6.0f, 5.0f, 6.0f, 7.0f, 6.0f);
            p.lineTo(21.0f, 6.0f);

            // Bottom cycle
            p.startNewSubPath(7.0f, 22.0f);
            p.lineTo(3.0f, 18.0f);
            p.lineTo(7.0f, 14.0f);

            p.startNewSubPath(21.0f, 13.0f);
            p.lineTo(21.0f, 16.0f);
            p.cubicTo(21.0f, 18.0f, 19.0f, 18.0f, 17.0f, 18.0f);
            p.lineTo(3.0f, 18.0f);
            break;
        }

        case IconType::ChevronLeft:
        {
            p.startNewSubPath(15.0f, 18.0f);
            p.lineTo(9.0f, 12.0f);
            p.lineTo(15.0f, 6.0f);
            break;
        }

        case IconType::ChevronRight:
        {
            p.startNewSubPath(9.0f, 18.0f);
            p.lineTo(15.0f, 12.0f);
            p.lineTo(9.0f, 6.0f);
            break;
        }

        case IconType::ChevronDown:
        {
            p.startNewSubPath(6.0f, 9.0f);
            p.lineTo(12.0f, 15.0f);
            p.lineTo(18.0f, 9.0f);
            break;
        }

        case IconType::Folder:
        {
            p.startNewSubPath(4.0f, 20.0f);
            p.lineTo(20.0f, 20.0f);
            p.cubicTo(21.1f, 20.0f, 22.0f, 19.1f, 22.0f, 18.0f);
            p.lineTo(22.0f, 9.0f);
            p.cubicTo(22.0f, 7.9f, 21.1f, 7.0f, 20.0f, 7.0f);
            p.lineTo(12.0f, 7.0f);
            p.lineTo(10.0f, 4.0f);
            p.lineTo(4.0f, 4.0f);
            p.cubicTo(2.9f, 4.0f, 2.0f, 4.9f, 2.0f, 6.0f);
            p.lineTo(2.0f, 18.0f);
            p.cubicTo(2.0f, 19.1f, 2.9f, 20.0f, 4.0f, 20.0f);
            p.closeSubPath();
            break;
        }

        case IconType::FolderPlus:
        {
            // Folder outline
            p.startNewSubPath(4.0f, 20.0f);
            p.lineTo(20.0f, 20.0f);
            p.cubicTo(21.1f, 20.0f, 22.0f, 19.1f, 22.0f, 18.0f);
            p.lineTo(22.0f, 9.0f);
            p.cubicTo(22.0f, 7.9f, 21.1f, 7.0f, 20.0f, 7.0f);
            p.lineTo(12.0f, 7.0f);
            p.lineTo(10.0f, 4.0f);
            p.lineTo(4.0f, 4.0f);
            p.cubicTo(2.9f, 4.0f, 2.0f, 4.9f, 2.0f, 6.0f);
            p.lineTo(2.0f, 18.0f);
            p.cubicTo(2.0f, 19.1f, 2.9f, 20.0f, 4.0f, 20.0f);
            p.closeSubPath();

            // Plus symbol in center
            p.startNewSubPath(12.0f, 10.5f);
            p.lineTo(12.0f, 16.5f);
            p.startNewSubPath(9.0f, 13.5f);
            p.lineTo(15.0f, 13.5f);
            break;
        }

        case IconType::FolderX:
        {
            // Folder outline
            p.startNewSubPath(4.0f, 20.0f);
            p.lineTo(20.0f, 20.0f);
            p.cubicTo(21.1f, 20.0f, 22.0f, 19.1f, 22.0f, 18.0f);
            p.lineTo(22.0f, 9.0f);
            p.cubicTo(22.0f, 7.9f, 21.1f, 7.0f, 20.0f, 7.0f);
            p.lineTo(12.0f, 7.0f);
            p.lineTo(10.0f, 4.0f);
            p.lineTo(4.0f, 4.0f);
            p.cubicTo(2.9f, 4.0f, 2.0f, 4.9f, 2.0f, 6.0f);
            p.lineTo(2.0f, 18.0f);
            p.cubicTo(2.0f, 19.1f, 2.9f, 20.0f, 4.0f, 20.0f);
            p.closeSubPath();

            // X symbol
            p.startNewSubPath(10.0f, 11.5f);
            p.lineTo(14.0f, 15.5f);
            p.startNewSubPath(14.0f, 11.5f);
            p.lineTo(10.0f, 15.5f);
            break;
        }

        case IconType::Sparkles:
        {
            // Main sparkle
            p.startNewSubPath(12.0f, 3.0f);
            p.cubicTo(12.0f, 8.0f, 8.0f, 12.0f, 3.0f, 12.0f);
            p.cubicTo(8.0f, 12.0f, 12.0f, 16.0f, 12.0f, 21.0f);
            p.cubicTo(12.0f, 16.0f, 16.0f, 12.0f, 21.0f, 12.0f);
            p.cubicTo(16.0f, 12.0f, 12.0f, 8.0f, 12.0f, 3.0f);
            p.closeSubPath();

            // Mini top-right sparkle
            p.startNewSubPath(19.0f, 3.0f);
            p.lineTo(19.0f, 7.0f);
            p.startNewSubPath(17.0f, 5.0f);
            p.lineTo(21.0f, 5.0f);
            break;
        }

        case IconType::Music:
        {
            p.addEllipse(3.0f, 15.0f, 6.0f, 6.0f);
            p.startNewSubPath(9.0f, 18.0f);
            p.lineTo(9.0f, 5.0f);
            p.lineTo(21.0f, 3.0f);
            p.lineTo(21.0f, 16.0f);
            p.startNewSubPath(15.0f, 13.0f);
            p.addEllipse(15.0f, 13.0f, 6.0f, 6.0f);
            break;
        }

        case IconType::Volume2:
        {
            // Speaker cone
            p.startNewSubPath(4.0f, 9.5f);
            p.lineTo(8.0f, 9.5f);
            p.lineTo(13.0f, 6.0f);
            p.lineTo(13.0f, 18.0f);
            p.lineTo(8.0f, 14.5f);
            p.lineTo(4.0f, 14.5f);
            p.closeSubPath();

            // Wave 1
            p.startNewSubPath(16.0f, 9.0f);
            p.cubicTo(18.0f, 10.5f, 18.0f, 13.5f, 16.0f, 15.0f);

            // Wave 2
            p.startNewSubPath(19.0f, 6.5f);
            p.cubicTo(22.5f, 9.5f, 22.5f, 14.5f, 19.0f, 17.5f);
            break;
        }

        case IconType::Settings:
        {
            // 1. Central bore hole
            p.addEllipse(9.0f, 9.0f, 6.0f, 6.0f);

            // 2. Continuous 6-tooth mechanical gear perimeter (tuerca/engranaje)
            constexpr int numTeeth = 6;
            constexpr float rIn = 7.0f;
            constexpr float rOut = 10.0f;
            constexpr float wBase = 0.26f;
            constexpr float wTip = 0.16f;

            for (int i = 0; i < numTeeth; ++i)
            {
                float a = (float)i * (juce::MathConstants<float>::twoPi / (float)numTeeth);

                float aRiseBase = a - wBase;
                float aRiseTip  = a - wTip;
                float aFallTip  = a + wTip;
                float aFallBase = a + wBase;

                if (i == 0)
                {
                    p.startNewSubPath(12.0f + std::cos(aRiseBase) * rIn,
                                      12.0f + std::sin(aRiseBase) * rIn);
                }
                else
                {
                    p.lineTo(12.0f + std::cos(aRiseBase) * rIn,
                             12.0f + std::sin(aRiseBase) * rIn);
                }

                p.lineTo(12.0f + std::cos(aRiseTip) * rOut,
                         12.0f + std::sin(aRiseTip) * rOut);
                p.lineTo(12.0f + std::cos(aFallTip) * rOut,
                         12.0f + std::sin(aFallTip) * rOut);
                p.lineTo(12.0f + std::cos(aFallBase) * rIn,
                         12.0f + std::sin(aFallBase) * rIn);
            }
            p.closeSubPath();
            break;
        }

        case IconType::Maximize:
        {
            // Top-right
            p.startNewSubPath(15.0f, 3.0f);
            p.lineTo(21.0f, 3.0f);
            p.lineTo(21.0f, 9.0f);
            p.startNewSubPath(21.0f, 3.0f);
            p.lineTo(14.0f, 10.0f);

            // Bottom-left
            p.startNewSubPath(9.0f, 21.0f);
            p.lineTo(3.0f, 21.0f);
            p.lineTo(3.0f, 15.0f);
            p.startNewSubPath(3.0f, 21.0f);
            p.lineTo(10.0f, 14.0f);
            break;
        }

        case IconType::Sliders:
        {
            // 3 vertical lines with knobs
            p.startNewSubPath(4.0f, 3.0f);
            p.lineTo(4.0f, 21.0f);
            p.startNewSubPath(2.0f, 8.0f);
            p.lineTo(6.0f, 8.0f);

            p.startNewSubPath(12.0f, 3.0f);
            p.lineTo(12.0f, 21.0f);
            p.startNewSubPath(10.0f, 15.0f);
            p.lineTo(14.0f, 15.0f);

            p.startNewSubPath(20.0f, 3.0f);
            p.lineTo(20.0f, 21.0f);
            p.startNewSubPath(18.0f, 9.0f);
            p.lineTo(22.0f, 9.0f);
            break;
        }

        case IconType::Zap:
        {
            p.startNewSubPath(13.0f, 2.0f);
            p.lineTo(3.0f, 14.0f);
            p.lineTo(12.0f, 14.0f);
            p.lineTo(11.0f, 22.0f);
            p.lineTo(21.0f, 10.0f);
            p.lineTo(12.0f, 10.0f);
            p.closeSubPath();
            break;
        }

        case IconType::Tv:
        {
            p.addRoundedRectangle(2.0f, 5.0f, 20.0f, 14.0f, 2.0f);
            p.startNewSubPath(12.0f, 19.0f);
            p.lineTo(12.0f, 22.0f);
            p.startNewSubPath(8.0f, 22.0f);
            p.lineTo(16.0f, 22.0f);
            break;
        }

        case IconType::MoreHorizontal:
        {
            p.addEllipse(4.0f, 10.5f, 3.0f, 3.0f);
            p.addEllipse(10.5f, 10.5f, 3.0f, 3.0f);
            p.addEllipse(17.0f, 10.5f, 3.0f, 3.0f);
            break;
        }

        case IconType::Plus:
        {
            p.startNewSubPath(12.0f, 5.0f);
            p.lineTo(12.0f, 19.0f);
            p.startNewSubPath(5.0f, 12.0f);
            p.lineTo(19.0f, 12.0f);
            break;
        }

        case IconType::Minus:
        {
            p.startNewSubPath(5.0f, 12.0f);
            p.lineTo(19.0f, 12.0f);
            break;
        }

        case IconType::Search:
        {
            p.addEllipse(4.0f, 4.0f, 13.0f, 13.0f);
            p.startNewSubPath(14.5f, 14.5f);
            p.lineTo(20.0f, 20.0f);
            break;
        }

        case IconType::CircleDot:
        {
            p.addEllipse(3.0f, 3.0f, 18.0f, 18.0f);
            p.startNewSubPath(15.0f, 12.0f);
            p.addEllipse(9.0f, 9.0f, 6.0f, 6.0f);
            break;
        }

        case IconType::Layers:
        {
            p.startNewSubPath(12.0f, 2.0f);
            p.lineTo(22.0f, 7.0f);
            p.lineTo(12.0f, 12.0f);
            p.lineTo(2.0f, 7.0f);
            p.closeSubPath();

            p.startNewSubPath(2.0f, 12.0f);
            p.lineTo(12.0f, 17.0f);
            p.lineTo(22.0f, 12.0f);

            p.startNewSubPath(2.0f, 17.0f);
            p.lineTo(12.0f, 22.0f);
            p.lineTo(22.0f, 17.0f);
            break;
        }

        case IconType::Keyboard:
        {
            p.addRoundedRectangle(2.0f, 4.0f, 20.0f, 16.0f, 2.0f);
            // Spacebar
            p.startNewSubPath(6.0f, 16.0f);
            p.lineTo(18.0f, 16.0f);
            // Row 1 keys
            p.startNewSubPath(6.0f, 8.0f); p.lineTo(7.0f, 8.0f);
            p.startNewSubPath(10.0f, 8.0f); p.lineTo(11.0f, 8.0f);
            p.startNewSubPath(14.0f, 8.0f); p.lineTo(15.0f, 8.0f);
            p.startNewSubPath(17.5f, 8.0f); p.lineTo(18.5f, 8.0f);
            // Row 2 keys
            p.startNewSubPath(6.0f, 12.0f); p.lineTo(7.0f, 12.0f);
            p.startNewSubPath(10.0f, 12.0f); p.lineTo(11.0f, 12.0f);
            p.startNewSubPath(14.0f, 12.0f); p.lineTo(15.0f, 12.0f);
            p.startNewSubPath(17.5f, 12.0f); p.lineTo(18.5f, 12.0f);
            break;
        }
    }

    return p;
}

void LucideIcons::draw(juce::Graphics& g,
                       IconType icon,
                       juce::Rectangle<float> bounds,
                       juce::Colour colour,
                       float strokeWidth,
                       bool fill)
{
    if (bounds.isEmpty())
        return;

    auto p = getPath(icon);

    // Scale 24x24 path into bounds preserving aspect ratio
    float scale = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 24.0f;
    float offsetX = bounds.getX() + (bounds.getWidth() - 24.0f * scale) * 0.5f;
    float offsetY = bounds.getY() + (bounds.getHeight() - 24.0f * scale) * 0.5f;

    auto transform = juce::AffineTransform::scale(scale).translated(offsetX, offsetY);
    p.applyTransform(transform);

    g.setColour(colour);

    if (fill || icon == IconType::Play)
    {
        g.fillPath(p);
    }
    else
    {
        juce::PathStrokeType stroke(strokeWidth * scale,
                                    juce::PathStrokeType::curved,
                                    juce::PathStrokeType::rounded);
        g.strokePath(p, stroke);
    }
}
