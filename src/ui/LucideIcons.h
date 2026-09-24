#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class LucideIcons
{
public:
    enum class IconType
    {
        Play,
        Pause,
        Disc,
        Headphones,
        Repeat,
        ChevronLeft,
        ChevronRight,
        ChevronDown,
        Folder,
        FolderPlus,
        FolderX,
        Sparkles,
        Music,
        Volume2,
        Settings,
        Maximize,
        Sliders,
        Zap,
        Tv,
        MoreHorizontal,
        Plus,
        Minus,
        Search,
        CircleDot,
        Layers,
        Keyboard
    };

    // Draw the icon vector scaled into the destination rectangle
    static void draw(juce::Graphics& g,
                     IconType icon,
                     juce::Rectangle<float> bounds,
                     juce::Colour colour,
                     float strokeWidth = 1.75f,
                     bool fill = false);

    // Get a juce::Path of the icon in 24x24 coordinate space
    static juce::Path getPath(IconType icon);
};
