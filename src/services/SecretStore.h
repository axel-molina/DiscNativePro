#pragma once

#include <juce_core/juce_core.h>

namespace SecretStore
{
juce::String read(const juce::String& account);
bool write(const juce::String& account, const juce::String& value);
bool remove(const juce::String& account);
bool isAvailable();
}
