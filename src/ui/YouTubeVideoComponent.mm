#include "YouTubeVideoComponent.h"
#include <juce_gui_extra/juce_gui_extra.h>
#import <WebKit/WebKit.h>
#import <Cocoa/Cocoa.h>

struct YouTubeVideoComponent::Pimpl
{
    juce::NSViewComponent nsViewComp;
    WKWebView* webView { nil };

    Pimpl()
    {
        WKWebViewConfiguration* config = [[WKWebViewConfiguration alloc] init];
        config.allowsAirPlayForMediaPlayback = YES;
        config.mediaTypesRequiringUserActionForPlayback = WKAudiovisualMediaTypeNone;
        if (@available(macOS 10.15, *))
        {
            config.defaultWebpagePreferences.allowsContentJavaScript = YES;
        }

        webView = [[WKWebView alloc] initWithFrame:NSZeroRect configuration:config];
        webView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        webView.customUserAgent = @"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Safari/605.1.15";
        nsViewComp.setView(webView);
    }

    ~Pimpl()
    {
        if (webView != nil)
        {
            [webView stopLoading];
            nsViewComp.setView(nullptr);
            [webView release];
            webView = nil;
        }
    }

    void loadVideo(const juce::String& videoId, float initialVol)
    {
        if (webView == nil) return;

        int vol = (int)(juce::jlimit(0.0f, 1.0f, initialVol) * 100.0f);

        juce::String html =
            "<!DOCTYPE html>"
            "<html>"
            "<head>"
            "<meta name='viewport' content='width=device-width, initial-scale=1'>"
            "<style>"
            "* { margin: 0; padding: 0; box-sizing: border-box; }"
            "body, html { width: 100%; height: 100%; overflow: hidden; background: #000; }"
            "#player { width: 100%; height: 100%; position: absolute; top: 0; left: 0; border: none; }"
            "</style>"
            "</head>"
            "<body>"
            "<iframe id='player' "
            "  src='https://www.youtube-nocookie.com/embed/" + videoId + "?autoplay=1&controls=1&enablejsapi=1&origin=http://localhost:3000' "
            "  allow='autoplay; encrypted-media; picture-in-picture' "
            "  allowfullscreen></iframe>"
            "<script src='https://www.youtube.com/iframe_api'></script>"
            "<script>"
            "var player;"
            "function onYouTubeIframeAPIReady() {"
            "  try {"
            "    player = new YT.Player('player', {"
            "      events: { 'onReady': onPlayerReady }"
            "    });"
            "  } catch(e) {}"
            "}"
            "function onPlayerReady(event) {"
            "  try {"
            "    event.target.unMute();"
            "    event.target.setVolume(" + juce::String(vol) + ");"
            "    event.target.playVideo();"
            "  } catch(e) {}"
            "}"
            "function play() {"
            "  try {"
            "    if (player && player.playVideo) {"
            "      player.unMute();"
            "      player.playVideo();"
            "    }"
            "  } catch(e) {}"
            "  var f = document.getElementById('player');"
            "  if (f && f.contentWindow) {"
            "    f.contentWindow.postMessage('{\"event\":\"command\",\"func\":\"unMute\",\"args\":\"\"}', '*');"
            "    f.contentWindow.postMessage('{\"event\":\"command\",\"func\":\"playVideo\",\"args\":\"\"}', '*');"
            "  }"
            "}"
            "function pause() {"
            "  try {"
            "    if (player && player.pauseVideo) player.pauseVideo();"
            "  } catch(e) {}"
            "  var f = document.getElementById('player');"
            "  if (f && f.contentWindow) {"
            "    f.contentWindow.postMessage('{\"event\":\"command\",\"func\":\"pauseVideo\",\"args\":\"\"}', '*');"
            "  }"
            "}"
            "function setVolume(v) {"
            "  try {"
            "    if (player && player.setVolume) {"
            "      player.setVolume(v);"
            "      if (v <= 0) player.mute(); else player.unMute();"
            "    }"
            "  } catch(e) {}"
            "  var f = document.getElementById('player');"
            "  if (f && f.contentWindow) {"
            "    if (v <= 0) {"
            "      f.contentWindow.postMessage('{\"event\":\"command\",\"func\":\"mute\",\"args\":\"\"}', '*');"
            "    } else {"
            "      f.contentWindow.postMessage('{\"event\":\"command\",\"func\":\"unMute\",\"args\":\"\"}', '*');"
            "      f.contentWindow.postMessage('{\"event\":\"command\",\"func\":\"setVolume\",\"args\":[' + v + ']}', '*');"
            "    }"
            "  }"
            "}"
            "function seek(s) {"
            "  try {"
            "    if (player && player.seekTo) player.seekTo(s, true);"
            "  } catch(e) {}"
            "  var f = document.getElementById('player');"
            "  if (f && f.contentWindow) {"
            "    f.contentWindow.postMessage('{\"event\":\"command\",\"func\":\"seekTo\",\"args\":[' + s + ', true]}', '*');"
            "  }"
            "}"
            "</script>"
            "</body>"
            "</html>";

        NSString* nsHtml = [NSString stringWithUTF8String:html.toRawUTF8()];
        NSURL* baseUrl = [NSURL URLWithString:@"http://localhost:3000"];
        [webView loadHTMLString:nsHtml baseURL:baseUrl];
    }

    void setVolume(float volume0to1)
    {
        if (webView == nil) return;
        int vol = (int)(juce::jlimit(0.0f, 1.0f, volume0to1) * 100.0f);
        NSString* js = [NSString stringWithFormat:@"setVolume(%d);", vol];
        [webView evaluateJavaScript:js completionHandler:nil];
    }

    void play()
    {
        if (webView == nil) return;
        [webView evaluateJavaScript:@"play();" completionHandler:nil];
    }

    void pause()
    {
        if (webView == nil) return;
        [webView evaluateJavaScript:@"pause();" completionHandler:nil];
    }

    void seek(double seconds)
    {
        if (webView == nil) return;
        NSString* js = [NSString stringWithFormat:@"seek(%f);", seconds];
        [webView evaluateJavaScript:js completionHandler:nil];
    }
};

YouTubeVideoComponent::YouTubeVideoComponent(int idx)
    : deckIndex(idx),
      pimpl(std::make_unique<Pimpl>())
{
    addAndMakeVisible(pimpl->nsViewComp);
}

YouTubeVideoComponent::~YouTubeVideoComponent()
{
}

void YouTubeVideoComponent::loadVideo(const juce::String& videoId, const juce::String& title)
{
    currentVideoId = videoId;
    currentVideoTitle = title;
    pimpl->loadVideo(videoId, currentVolume);
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
    g.setFont(juce::FontOptions("Menlo", 9.5f, juce::Font::plain));
    g.drawText(volPercent > 0 ? juce::String::formatted("VOL %d%%", volPercent) : "MUTED",
               volRect, juce::Justification::centred, false);
}

void YouTubeVideoComponent::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop(24);
    pimpl->nsViewComp.setBounds(area.reduced(2));
}
