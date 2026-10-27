#include "YouTubeVideoComponent.h"
#include "YouTubeEmbedHtml.h"
#include "LucideIcons.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <dlfcn.h>

#if JUCE_LINUX

namespace
{
    // JUCE 8.0.6 abre las librerías de WebKit con el nombre SIN versión
    // ("libwebkit2gtk-4.1.so", "libgtk-3.so"). Ese symlink solo lo instala el
    // paquete -dev: en una máquina con solo el runtime (libwebkit2gtk-4.1-0)
    // todos los dlopen fallan, WebBrowserComponent se queda mudo y el área de
    // vídeo aparece blanca sin ningún error. (juce-framework/JUCE#1730.)
    //
    // Reproducimos la misma comprobación para poder decírselo al usuario en vez
    // de dejarle un rectángulo blanco.
    //
    // ponytail: no hacemos dlclose; los handles quedan abiertos hasta salir.
    // Es una comprobación única y, si WebKit está, JUCE lo carga igualmente.
    bool webKitRuntimeAvailable()
    {
        auto canOpen = [](const char* name)
        {
            return ::dlopen(name, RTLD_LAZY | RTLD_LOCAL) != nullptr;
        };

        const bool gtk = canOpen("libgtk-3.so") && canOpen("libglib-2.0.so");

        const bool webkit41 = canOpen("libwebkit2gtk-4.1.so")
                           && canOpen("libjavascriptcoregtk-4.1.so")
                           && canOpen("libsoup-3.0.so");

        const bool webkit40 = canOpen("libwebkit2gtk-4.0.so")
                           && canOpen("libjavascriptcoregtk-4.0.so")
                           && canOpen("libsoup-2.4.so");

        return gtk && (webkit41 || webkit40);
    }
}

struct YouTubeVideoComponent::Pimpl
{
    std::unique_ptr<juce::WebBrowserComponent> browser;
    const bool webKitAvailable = webKitRuntimeAvailable();

    Pimpl()
    {
#if JUCE_WEB_BROWSER
        if (webKitAvailable)
            browser = std::make_unique<juce::WebBrowserComponent>();
#endif
    }

    ~Pimpl() = default;

    void loadVideo(const juce::String& videoId, bool autoplay)
    {
        if (!browser) return;

        // Wrapper con blocker anti-clics; data: URL porque WebBrowserComponent no expone loadHTMLString
        juce::String html = buildYouTubeEmbedHtml(videoId, autoplay, "http://localhost:3000");
        browser->goToURL("data:text/html;charset=utf-8," + juce::URL::addEscapeChars(html, true));
    }

    void setVolume(float volume0to1)
    {
        if (!browser) return;
        int vol = (int)(juce::jlimit(0.0f, 1.0f, volume0to1) * 100.0f);
        browser->evaluateJavascript("try { var f = document.querySelector('iframe'); if(f) f.contentWindow.postMessage('{\"event\":\"command\",\"func\":\"setVolume\",\"args\":[" + juce::String(vol) + "]}', '*'); } catch(e){}");
    }

    void play()
    {
        if (!browser) return;
        browser->evaluateJavascript("try { var f = document.querySelector('iframe'); if(f) f.contentWindow.postMessage('{\"event\":\"command\",\"func\":\"playVideo\",\"args\":\"\"}', '*'); } catch(e){}");
    }

    void pause()
    {
        if (!browser) return;
        browser->evaluateJavascript("try { var f = document.querySelector('iframe'); if(f) f.contentWindow.postMessage('{\"event\":\"command\",\"func\":\"pauseVideo\",\"args\":\"\"}', '*'); } catch(e){}");
    }

    void stop()
    {
        if (!browser) return;
        pause();
        browser->goToURL("about:blank");
    }

    void seek(double seconds)
    {
        if (!browser) return;
        browser->evaluateJavascript("try { var f = document.querySelector('iframe'); if(f) f.contentWindow.postMessage('{\"event\":\"command\",\"func\":\"seekTo\",\"args\":[" + juce::String(seconds) + ", true]}', '*'); } catch(e){}");
    }
};

YouTubeVideoComponent::YouTubeVideoComponent(int idx)
    : pimpl(std::make_unique<Pimpl>())
{
    juce::ignoreUnused(idx);
    if (pimpl->browser)
        addAndMakeVisible(*pimpl->browser);
}

YouTubeVideoComponent::~YouTubeVideoComponent() = default;

void YouTubeVideoComponent::loadVideo(const juce::String& videoId, const juce::String& title, bool autoplay)
{
    currentVideoId = videoId;
    currentVideoTitle = title;
    pimpl->loadVideo(videoId, autoplay);
    repaint();
}

void YouTubeVideoComponent::play()
{
    pimpl->play();
}

void YouTubeVideoComponent::pause()
{
    pimpl->pause();
}

void YouTubeVideoComponent::stop()
{
    pimpl->stop();
    currentVideoId.clear();
    currentVideoTitle.clear();
    repaint();
}

void YouTubeVideoComponent::seekTo(double seconds)
{
    pimpl->seek(seconds);
}

void YouTubeVideoComponent::setVolume(float vol)
{
    currentVolume = juce::jlimit(0.0f, 1.0f, vol);
    pimpl->setVolume(currentVolume);
    repaint();
}

void YouTubeVideoComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Dark sleek background
    g.setColour(juce::Colour::fromRGB(11, 12, 16));
    g.fillRoundedRectangle(bounds, 10.0f);

    // Border
    g.setColour(juce::Colour::fromRGB(36, 39, 48));
    g.drawRoundedRectangle(bounds, 10.0f, 1.5f);

    // Header bar (24px)
    auto headerRect = bounds.removeFromTop(24.0f).reduced(6.0f, 2.0f);

    // Left badge: RED YOUTUBE BADGE
    auto badgeRect = headerRect.removeFromLeft(96.0f);
    g.setColour(juce::Colour::fromRGB(220, 38, 38).withAlpha(0.2f));
    g.fillRoundedRectangle(badgeRect, 4.0f);
    g.setColour(juce::Colour::fromRGB(220, 38, 38));
    g.drawRoundedRectangle(badgeRect, 4.0f, 1.0f);

    LucideIcons::draw(g, LucideIcons::IconType::Tv,
                      juce::Rectangle<float>(badgeRect.getX() + 4.0f, badgeRect.getY() + 3.0f, 13.0f, 13.0f),
                      juce::Colour::fromRGB(239, 68, 68), 1.6f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
    g.drawText("YOUTUBE", badgeRect.removeFromRight(badgeRect.getWidth() - 20.0f), juce::Justification::centredLeft, false);

    // Right volume badge
    int volPercent = (int)(currentVolume * 100.0f);
    auto volRect = headerRect.removeFromRight(64.0f);
    g.setColour(juce::Colour::fromRGB(20, 24, 32));
    g.fillRoundedRectangle(volRect, 4.0f);
    g.setColour(volPercent > 0 ? juce::Colour::fromRGB(74, 222, 128) : juce::Colour::fromRGB(239, 68, 68));
    g.setFont(juce::FontOptions(9.5f, juce::Font::plain));
    g.drawText(volPercent > 0 ? juce::String::formatted("VOL %d%%", volPercent) : "MUTED",
               volRect, juce::Justification::centred, false);

    // Sin WebKit cargado no hay webview: en vez de dejar el hueco en blanco,
    // explicamos qué falta. El audio sigue sonando porque va por el deck nativo
    // solo en modo audio; en modo vídeo aquí no habría ni imagen ni sonido.
    if (!pimpl->webKitAvailable)
    {
        auto area = bounds.reduced(14.0f, 10.0f);

        g.setColour(juce::Colour::fromRGB(239, 68, 68));
        g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
        g.drawText("Reproductor web no disponible",
                   area.removeFromTop(22.0f), juce::Justification::centred, false);

        g.setColour(juce::Colour::fromRGB(160, 166, 180));
        g.setFont(juce::FontOptions(11.0f, juce::Font::plain));
        g.drawFittedText("Falta WebKitGTK. Instala libwebkit2gtk-4.1-0 y libgtk-3-0 "
                         "(o libwebkit2gtk-4.1-dev si lo compilaste tú).",
                         area.removeFromTop(46.0f), juce::Justification::centredTop, 3);
    }
}

void YouTubeVideoComponent::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(24);
    if (pimpl->browser)
        pimpl->browser->setBounds(area);
}

#endif
