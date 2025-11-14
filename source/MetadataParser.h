#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
// Data structure to hold parsed metadata from description text
struct ParsedMetadata
{
    int bpm = 0;
    juce::String bpmSource;  // Where we found it (for debugging)

    juce::String key;  // Musical key (e.g., "C min", "C#", "Bb")
    juce::String keySource;

    juce::String instagramHandle;
    juce::String twitterHandle;
    juce::StringArray emails;  // Can have multiple emails

    juce::String usageTerms;
    bool isFreeNonProfit = false;
    bool requiresCredit = false;

    juce::StringArray tags;
    int originalTagCount = 0;

    juce::String cleanDescription;
    juce::String rawInput;

    // Stats for the UI
    int linesRemoved = 0;
    int tagsDeduped = 0;

    void clear()
    {
        bpm = 0;
        bpmSource.clear();
        key.clear();
        keySource.clear();
        instagramHandle.clear();
        twitterHandle.clear();
        emails.clear();
        usageTerms.clear();
        isFreeNonProfit = false;
        requiresCredit = false;
        tags.clear();
        originalTagCount = 0;
        cleanDescription.clear();
        rawInput.clear();
        linesRemoved = 0;
        tagsDeduped = 0;
    }
};

//==============================================================================
// Metadata parser that extracts structured data from pasted description text
class MetadataParser
{
public:
    MetadataParser();

    // Main parsing method
    ParsedMetadata parse(const juce::String& rawText);

    // Individual extraction methods
    int extractBPM(const juce::String& text, juce::String& source);
    juce::String extractKey(const juce::String& text, juce::String& source);
    juce::String extractInstagramHandle(const juce::String& text);
    juce::String extractTwitterHandle(const juce::String& text);
    juce::StringArray extractEmails(const juce::String& text);

    void extractUsageTerms(const juce::String& text, ParsedMetadata& metadata);
    juce::StringArray extractTags(const juce::String& text);

    // Clean & Pack functionality
    juce::StringArray cleanAndPackTags(const juce::StringArray& inputTags, int maxChars = 500);

    // Description cleaning
    juce::String cleanDescription(const juce::String& text, int& linesRemoved);

private:
    // Helper methods
    bool containsIgnoreSeparator(const juce::String& line);
    juce::String normalizeWhitespace(const juce::String& text);
    juce::String extractHandleFromLine(const juce::String& line, const juce::String& platform);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MetadataParser)
};
