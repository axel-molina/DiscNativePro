#include "CoverArtGenerator.h"

static std::map<juce::String, juce::Image>& getCache()
{
    static std::map<juce::String, juce::Image> cache;
    return cache;
}

void CoverArtGenerator::clearCache()
{
    getCache().clear();
}

juce::Image CoverArtGenerator::getCoverForTrack(const juce::String& title, int size)
{
    auto& cache = getCache();
    auto key = title.toLowerCase().trim();

    if (cache.find(key) != cache.end())
        return cache[key];

    juce::Image img;
    if (key.contains("latin") || key.contains("groove") || key.contains("discpro"))
    {
        img = createOceanTechCover(size);
    }
    else if (key.contains("sunset") || key.contains("beach") || key.contains("house"))
    {
        img = createSunsetHouseCover(size);
    }
    else if (key.contains("murguero") || key.contains("carioca"))
    {
        img = createCarnivalCover(size);
    }
    else if (key.contains("piratas"))
    {
        img = createPiratasCover(size);
    }
    else if (key.contains("cyberfunk") || key.contains("berlin"))
    {
        img = createCyberfunkCover(size);
    }
    else
    {
        img = createDefaultCover(title, size);
    }

    cache[key] = img;
    return img;
}

juce::Image CoverArtGenerator::createOceanTechCover(int size)
{
    float sz = (float)size;
    juce::Image img(juce::Image::ARGB, size, size, true);
    juce::Graphics g(img);

    // Deep ocean teal and cyan background gradient
    juce::ColourGradient bg(juce::Colour::fromRGB(4, 30, 48), 0.0f, 0.0f,
                            juce::Colour::fromRGB(1, 10, 20), sz, sz, false);
    bg.addColour(0.4, juce::Colour::fromRGB(0, 110, 140));
    bg.addColour(0.8, juce::Colour::fromRGB(2, 60, 90));
    g.setGradientFill(bg);
    g.fillAll();

    // Cosmic teal swirls and wave ribbons
    juce::Path wave1;
    wave1.startNewSubPath(0.0f, sz * 0.4f);
    wave1.cubicTo(sz * 0.3f, sz * 0.2f, sz * 0.7f, sz * 0.6f, sz, sz * 0.35f);
    wave1.lineTo(sz, sz);
    wave1.lineTo(0.0f, sz);
    wave1.closeSubPath();
    g.setColour(juce::Colour::fromRGB(0, 210, 255).withAlpha(0.35f));
    g.fillPath(wave1);

    juce::Path wave2;
    wave2.startNewSubPath(0.0f, sz * 0.6f);
    wave2.cubicTo(sz * 0.4f, sz * 0.8f, sz * 0.6f, sz * 0.4f, sz, sz * 0.7f);
    wave2.lineTo(sz, sz);
    wave2.lineTo(0.0f, sz);
    wave2.closeSubPath();
    g.setColour(juce::Colour::fromRGB(34, 211, 238).withAlpha(0.25f));
    g.fillPath(wave2);

    // Glowing cyan radial core
    juce::ColourGradient core(juce::Colour::fromRGB(56, 189, 248).withAlpha(0.6f), sz * 0.45f, sz * 0.45f,
                              juce::Colours::transparentBlack, sz * 0.45f + sz * 0.4f, sz * 0.45f, true);
    g.setGradientFill(core);
    g.fillEllipse(sz * 0.1f, sz * 0.1f, sz * 0.7f, sz * 0.7f);

    return img;
}

juce::Image CoverArtGenerator::createSunsetHouseCover(int size)
{
    float sz = (float)size;
    juce::Image img(juce::Image::ARGB, size, size, true);
    juce::Graphics g(img);

    // Deep purple / indigo to neon magenta gradient
    juce::ColourGradient bg(juce::Colour::fromRGB(15, 6, 35), 0.0f, 0.0f,
                            juce::Colour::fromRGB(50, 10, 70), 0.0f, sz, false);
    bg.addColour(0.5, juce::Colour::fromRGB(88, 28, 135));
    g.setGradientFill(bg);
    g.fillAll();

    // Glowing neon sun / horizon
    juce::ColourGradient sun(juce::Colour::fromRGB(244, 63, 94).withAlpha(0.85f), sz * 0.5f, sz * 0.55f,
                             juce::Colours::transparentBlack, sz * 0.5f, sz * 0.2f, true);
    sun.addColour(0.5, juce::Colour::fromRGB(236, 72, 153).withAlpha(0.5f));
    g.setGradientFill(sun);
    g.fillEllipse(sz * 0.2f, sz * 0.25f, sz * 0.6f, sz * 0.6f);

    // Mountain silhouettes in foreground
    juce::Path mountain;
    mountain.startNewSubPath(0.0f, sz * 0.8f);
    mountain.lineTo(sz * 0.35f, sz * 0.55f);
    mountain.lineTo(sz * 0.65f, sz * 0.7f);
    mountain.lineTo(sz * 0.85f, sz * 0.58f);
    mountain.lineTo(sz, sz * 0.75f);
    mountain.lineTo(sz, sz);
    mountain.lineTo(0.0f, sz);
    mountain.closeSubPath();
    g.setColour(juce::Colour::fromRGB(10, 4, 22));
    g.fillPath(mountain);

    return img;
}

juce::Image CoverArtGenerator::createCarnivalCover(int size)
{
    float sz = (float)size;
    juce::Image img(juce::Image::ARGB, size, size, true);
    juce::Graphics g(img);

    juce::ColourGradient bg(juce::Colour::fromRGB(30, 12, 4), 0.0f, 0.0f,
                            juce::Colour::fromRGB(80, 30, 5), sz, sz, false);
    g.setGradientFill(bg);
    g.fillAll();

    juce::ColourGradient glow(juce::Colour::fromRGB(245, 158, 11).withAlpha(0.7f), sz * 0.5f, sz * 0.5f,
                              juce::Colours::transparentBlack, sz * 0.5f, sz * 0.1f, true);
    glow.addColour(0.4, juce::Colour::fromRGB(239, 68, 68).withAlpha(0.5f));
    g.setGradientFill(glow);
    g.fillEllipse(sz * 0.15f, sz * 0.15f, sz * 0.7f, sz * 0.7f);

    return img;
}

juce::Image CoverArtGenerator::createPiratasCover(int size)
{
    float sz = (float)size;
    juce::Image img(juce::Image::ARGB, size, size, true);
    juce::Graphics g(img);

    juce::ColourGradient bg(juce::Colour::fromRGB(8, 14, 30), 0.0f, 0.0f,
                            juce::Colour::fromRGB(2, 6, 18), 0.0f, sz, false);
    bg.addColour(0.5, juce::Colour::fromRGB(30, 58, 138));
    g.setGradientFill(bg);
    g.fillAll();

    juce::ColourGradient star(juce::Colour::fromRGB(96, 165, 250).withAlpha(0.6f), sz * 0.4f, sz * 0.4f,
                              juce::Colours::transparentBlack, sz * 0.4f, 0.0f, true);
    g.setGradientFill(star);
    g.fillEllipse(sz * 0.1f, sz * 0.1f, sz * 0.6f, sz * 0.6f);

    return img;
}

juce::Image CoverArtGenerator::createCyberfunkCover(int size)
{
    float sz = (float)size;
    juce::Image img(juce::Image::ARGB, size, size, true);
    juce::Graphics g(img);

    juce::ColourGradient bg(juce::Colour::fromRGB(24, 10, 6), 0.0f, 0.0f,
                            juce::Colour::fromRGB(10, 4, 3), 0.0f, sz, false);
    bg.addColour(0.6, juce::Colour::fromRGB(67, 20, 7));
    g.setGradientFill(bg);
    g.fillAll();

    juce::ColourGradient neon(juce::Colour::fromRGB(249, 115, 22).withAlpha(0.7f), sz * 0.5f, sz * 0.6f,
                              juce::Colours::transparentBlack, sz * 0.5f, sz * 0.2f, true);
    g.setGradientFill(neon);
    g.fillEllipse(sz * 0.2f, sz * 0.3f, sz * 0.6f, sz * 0.6f);

    return img;
}

juce::Image CoverArtGenerator::createDefaultCover(const juce::String& title, int size)
{
    float sz = (float)size;
    juce::Image img(juce::Image::ARGB, size, size, true);
    juce::Graphics g(img);

    juce::ColourGradient bg(juce::Colour::fromRGB(26, 30, 40), 0.0f, 0.0f,
                            juce::Colour::fromRGB(12, 14, 18), sz, sz, false);
    g.setGradientFill(bg);
    g.fillAll();

    g.setColour(juce::Colour::fromRGB(56, 189, 248).withAlpha(0.3f));
    g.drawEllipse(sz * 0.2f, sz * 0.2f, sz * 0.6f, sz * 0.6f, 2.0f);

    g.setColour(juce::Colours::white.withAlpha(0.8f));
    g.setFont(juce::FontOptions(sz * 0.35f, juce::Font::bold));
    juce::String initial = title.isNotEmpty() ? title.substring(0, 1).toUpperCase() : "D";
    g.drawText(initial, 0, 0, size, size, juce::Justification::centred, false);

    return img;
}
