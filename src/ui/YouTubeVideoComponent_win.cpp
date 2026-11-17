#include "YouTubeVideoComponent.h"
#include "YouTubeEmbedHtml.h"
#include "LucideIcons.h"
#include <juce_gui_extra/juce_gui_extra.h>

#if JUCE_WINDOWS

struct YouTubeVideoComponent::Pimpl
{
    std::unique_ptr<juce::WebBrowserComponent> browser;
    juce::String currentVideoId;
    bool currentAutoplay { false };

    Pimpl()
    {
        auto dataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                           .getChildFile("DiscNativePro")
                           .getChildFile("WebView2Profile");
        dataDir.createDirectory();

        auto options = juce::WebBrowserComponent::Options{}
                           .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
                           .withWinWebView2Options(
                               juce::WebBrowserComponent::Options::WinWebView2{}
                                   .withUserDataFolder(dataDir)
                                   .withStatusBarDisabled()
                           )
                           .withResourceProvider([this](const juce::String& path) -> std::optional<juce::WebBrowserComponent::Resource> {
                               juce::String vid = currentVideoId;
                               bool autoPl = currentAutoplay;

                               if (path.contains("v="))
                               {
                                   auto query = path.fromFirstOccurrenceOf("?", false, false);
                                   auto params = juce::StringArray::fromTokens(query, "&", "");
                                   for (auto& p : params)
                                   {
                                       if (p.startsWith("v="))
                                           vid = p.substring(2);
                                       else if (p.startsWith("a="))
                                           autoPl = (p.substring(2) == "1");
                                   }
                               }

                               juce::String html = buildYouTubeEmbedHtml(vid, autoPl, "https://juce.backend");
                               auto rawUtf8 = html.toRawUTF8();
                               auto len = strlen(rawUtf8);

                               std::vector<std::byte> byteVec(len);
                               std::memcpy(byteVec.data(), rawUtf8, len);

                               return juce::WebBrowserComponent::Resource { std::move(byteVec), "text/html" };
                           });

        browser = std::make_unique<juce::WebBrowserComponent>(options);
    }

    ~Pimpl() = default;

    void loadVideo(const juce::String& videoId, bool autoplay)
    {
        if (!browser) return;

        currentVideoId = videoId;
        currentAutoplay = autoplay;

        // Carga bajo origen HTTPS seguro https://juce.backend/player.html
        browser->goToURL(juce::WebBrowserComponent::getResourceProviderRoot() + "player.html?v=" + videoId + "&a=" + (autoplay ? "1" : "0"));
    }

    void setVolume(float volume0to1)
    {
        if (!browser) return;
        int vol = (int)(juce::jlimit(0.0f, 1.0f, volume0to1) * 100.0f);
        browser->evaluateJavascript("try { var msg = '{\"event\":\"command\",\"func\":\"setVolume\",\"args\":[" + juce::String(vol) + "]}'; window.postMessage(msg, '*'); var f = document.querySelector('iframe'); if(f) f.contentWindow.postMessage(msg, '*'); } catch(e){}");
    }

    void play()
    {
        if (!browser) return;
        browser->evaluateJavascript("try { var msg = '{\"event\":\"command\",\"func\":\"playVideo\",\"args\":\"\"}'; window.postMessage(msg, '*'); var f = document.querySelector('iframe'); if(f) f.contentWindow.postMessage(msg, '*'); } catch(e){}");
    }

    void pause()
    {
        if (!browser) return;
        browser->evaluateJavascript("try { var msg = '{\"event\":\"command\",\"func\":\"pauseVideo\",\"args\":\"\"}'; window.postMessage(msg, '*'); var f = document.querySelector('iframe'); if(f) f.contentWindow.postMessage(msg, '*'); } catch(e){}");
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
        browser->evaluateJavascript("try { var msg = '{\"event\":\"command\",\"func\":\"seekTo\",\"args\":[" + juce::String(seconds) + ", true]}'; window.postMessage(msg, '*'); var f = document.querySelector('iframe'); if(f) f.contentWindow.postMessage(msg, '*'); } catch(e){}");
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
    g.fillRoundedRectangle(bounds, 3.0f);

    // Border
    g.setColour(juce::Colour::fromRGB(36, 39, 48));
    g.drawRoundedRectangle(bounds, 3.0f, 1.5f);

    // Header bar (24px)
    auto headerRect = bounds.removeFromTop(24.0f).reduced(6.0f, 2.0f);

    // Left badge: RED YOUTUBE BADGE
    auto badgeRect = headerRect.removeFromLeft(96.0f);
    g.setColour(juce::Colour::fromRGB(220, 38, 38).withAlpha(0.2f));
    g.fillRoundedRectangle(badgeRect, 3.0f);
    g.setColour(juce::Colour::fromRGB(220, 38, 38));
    g.drawRoundedRectangle(badgeRect, 3.0f, 1.0f);

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
    g.fillRoundedRectangle(volRect, 3.0f);
    g.setColour(volPercent > 0 ? juce::Colour::fromRGB(74, 222, 128) : juce::Colour::fromRGB(239, 68, 68));
    g.setFont(juce::FontOptions(9.5f, juce::Font::plain));
    g.drawText(volPercent > 0 ? juce::String::formatted("VOL %d%%", volPercent) : "MUTED",
               volRect, juce::Justification::centred, false);
}

void YouTubeVideoComponent::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(24);
    if (pimpl->browser)
        pimpl->browser->setBounds(area.reduced(2));
}

#endif // JUCE_WINDOWS
