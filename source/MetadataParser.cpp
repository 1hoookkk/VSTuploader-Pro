#include "MetadataParser.h"

MetadataParser::MetadataParser()
{
}

//==============================================================================
ParsedMetadata MetadataParser::parse(const juce::String& rawText)
{
    ParsedMetadata metadata;
    metadata.rawInput = rawText;

    // Extract BPM
    metadata.bpm = extractBPM(rawText, metadata.bpmSource);

    // Extract social handles
    metadata.instagramHandle = extractInstagramHandle(rawText);
    metadata.twitterHandle = extractTwitterHandle(rawText);

    // Extract email
    metadata.email = extractEmail(rawText);

    // Extract usage/license terms
    extractUsageTerms(rawText, metadata);

    // Extract and clean tags
    auto rawTags = extractTags(rawText);
    metadata.originalTagCount = rawTags.size();
    metadata.tags = cleanAndPackTags(rawTags);
    metadata.tagsDeduped = metadata.originalTagCount - metadata.tags.size();

    // Clean description (remove separator lines and content after IGNORE)
    metadata.cleanDescription = cleanDescription(rawText, metadata.linesRemoved);

    return metadata;
}

//==============================================================================
// BPM Extraction
//==============================================================================
int MetadataParser::extractBPM(const juce::String& text, juce::String& source)
{
    // Pattern 1: "BPM: 148" or "• BPM: 148"
    auto lines = juce::StringArray::fromLines(text);
    for (const auto& line : lines)
    {
        auto trimmed = line.trim();

        // Remove bullet points
        if (trimmed.startsWith("•"))
            trimmed = trimmed.substring(1).trim();

        // Check for "BPM: XXX" pattern
        if (trimmed.toLowerCase().startsWith("bpm:") ||
            trimmed.toLowerCase().startsWith("bpm :"))
        {
            auto colonPos = trimmed.indexOf(":");
            if (colonPos > 0)
            {
                auto numberPart = trimmed.substring(colonPos + 1).trim();
                auto bpm = numberPart.getIntValue();
                if (bpm > 0 && bpm < 300)
                {
                    source = "Found in line: " + line.trim();
                    return bpm;
                }
            }
        }

        // Pattern 2: "148 BPM" or "148bpm"
        if (trimmed.toLowerCase().contains("bpm"))
        {
            auto bpmPos = trimmed.toLowerCase().indexOf("bpm");
            auto beforeBpm = trimmed.substring(0, bpmPos).trim();

            // Get the last word before "bpm"
            auto words = juce::StringArray::fromTokens(beforeBpm, " ", "");
            if (words.size() > 0)
            {
                auto bpm = words[words.size() - 1].getIntValue();
                if (bpm > 0 && bpm < 300)
                {
                    source = "Found in line: " + line.trim();
                    return bpm;
                }
            }
        }
    }

    return 0; // Not found
}

//==============================================================================
// Social Handle Extraction
//==============================================================================
juce::String MetadataParser::extractInstagramHandle(const juce::String& text)
{
    auto lines = juce::StringArray::fromLines(text);

    for (const auto& line : lines)
    {
        auto lower = line.toLowerCase();

        // Pattern 1: "Instagram: @handle" or "IG: @handle"
        if (lower.contains("instagram:") || lower.contains("ig:"))
        {
            auto handle = extractHandleFromLine(line, "instagram");
            if (handle.isNotEmpty())
                return handle;
        }

        // Pattern 2: "/ handle" or "/handle" (common format)
        if (line.trim().startsWith("/") && !line.contains("http"))
        {
            auto handle = line.trim().substring(1).trim();
            // Remove @ if present
            if (handle.startsWith("@"))
                handle = handle.substring(1);

            // Basic validation (no spaces, reasonable length)
            if (!handle.containsChar(' ') && handle.length() > 2 && handle.length() < 30)
            {
                return handle;
            }
        }

        // Pattern 3: "@handle" standalone
        if (line.trim().startsWith("@") && line.length() < 40)
        {
            auto handle = line.trim().substring(1).trim();
            if (!handle.containsChar(' ') && handle.length() > 2)
                return handle;
        }
    }

    return {};
}

juce::String MetadataParser::extractTwitterHandle(const juce::String& text)
{
    auto lines = juce::StringArray::fromLines(text);

    for (const auto& line : lines)
    {
        auto lower = line.toLowerCase();

        // Pattern: "Twitter: handle" or "Twitter:handle"
        if (lower.contains("twitter:") || lower.contains("twitter :"))
        {
            auto colonPos = lower.indexOf("twitter");
            auto afterTwitter = line.substring(colonPos + 7).trim();

            if (afterTwitter.startsWith(":"))
                afterTwitter = afterTwitter.substring(1).trim();

            // Remove @ if present
            if (afterTwitter.startsWith("@"))
                afterTwitter = afterTwitter.substring(1);

            // Extract first word
            auto words = juce::StringArray::fromTokens(afterTwitter, " \t\n", "");
            if (words.size() > 0)
            {
                auto handle = words[0];
                if (handle.length() > 2 && handle.length() < 30)
                    return handle;
            }
        }
    }

    return {};
}

//==============================================================================
// Email Extraction
//==============================================================================
juce::String MetadataParser::extractEmail(const juce::String& text)
{
    auto lines = juce::StringArray::fromLines(text);

    for (const auto& line : lines)
    {
        // Simple email pattern: word@word.word
        if (line.contains("@") && line.contains("."))
        {
            auto words = juce::StringArray::fromTokens(line, " \t:,", "");
            for (const auto& word : words)
            {
                if (word.contains("@") && word.contains("."))
                {
                    auto atPos = word.indexOf("@");
                    auto dotPos = word.lastIndexOf(".");

                    if (atPos > 0 && dotPos > atPos && dotPos < word.length() - 1)
                    {
                        // Basic validation
                        if (word.length() < 100 && !word.containsChar(' '))
                            return word;
                    }
                }
            }
        }
    }

    return {};
}

//==============================================================================
// Usage Terms Extraction
//==============================================================================
void MetadataParser::extractUsageTerms(const juce::String& text, ParsedMetadata& metadata)
{
    auto lower = text.toLowerCase();

    // Detect free non-profit usage
    if (lower.contains("free") && (lower.contains("non-profit") || lower.contains("nonprofit")))
    {
        metadata.isFreeNonProfit = true;
    }

    // Detect credit requirement
    if (lower.contains("credit") && (lower.contains("prod.") || lower.contains("prod ")))
    {
        metadata.requiresCredit = true;

        // Try to extract the producer tag
        if (lower.contains("prod."))
        {
            auto prodPos = text.indexOf("prod.");
            auto afterProd = text.substring(prodPos + 5);

            // Look for content within brackets or quotes
            if (afterProd.contains("]"))
            {
                auto endPos = afterProd.indexOf("]");
                auto tag = afterProd.substring(0, endPos).trim();
                metadata.usageTerms = "Credit: prod." + tag;
            }
        }
    }

    // Build usage summary
    if (metadata.isFreeNonProfit)
    {
        metadata.usageTerms = "[FREE] Non-profit use";
        if (metadata.requiresCredit)
            metadata.usageTerms += " with credit required";
    }
}

//==============================================================================
// Tag Extraction
//==============================================================================
juce::StringArray MetadataParser::extractTags(const juce::String& text)
{
    juce::StringArray tags;
    auto lines = juce::StringArray::fromLines(text);

    for (const auto& line : lines)
    {
        auto trimmed = line.trim();

        // Skip short lines
        if (trimmed.length() < 3)
            continue;

        // Look for hashtags
        if (trimmed.contains("#"))
        {
            // Split by spaces and extract hashtags
            auto words = juce::StringArray::fromTokens(trimmed, " \t\n", "");
            for (auto word : words)
            {
                if (word.startsWith("#"))
                {
                    word = word.substring(1); // Remove #
                    if (word.isNotEmpty())
                        tags.add(word.toLowerCase());
                }
            }
        }

        // Look for comma-separated tags
        if (trimmed.contains(","))
        {
            auto parts = juce::StringArray::fromTokens(trimmed, ",", "");
            for (auto part : parts)
            {
                part = part.trim().toLowerCase();

                // Skip if it looks like a sentence (contains multiple spaces)
                if (part.contains("  "))
                    continue;

                // Skip if it's too long (probably not a tag)
                if (part.length() > 50)
                    continue;

                if (part.length() > 2 && !part.startsWith("http"))
                    tags.add(part);
            }
        }
    }

    return tags;
}

//==============================================================================
// Clean & Pack Tags
//==============================================================================
juce::StringArray MetadataParser::cleanAndPackTags(const juce::StringArray& inputTags, int maxChars)
{
    juce::StringArray cleaned;
    juce::StringArray seen;

    for (auto tag : inputTags)
    {
        // Normalize
        tag = tag.trim().toLowerCase();

        // Remove common prefixes that aren't useful
        tag = tag.replace("type beat", "type beat");

        // Skip empty or very short tags
        if (tag.length() < 3)
            continue;

        // Skip duplicates
        if (seen.contains(tag))
            continue;

        seen.add(tag);

        // Check if adding this tag would exceed max chars
        int currentLength = 0;
        for (const auto& existingTag : cleaned)
            currentLength += existingTag.length() + 2; // +2 for ", "

        if (currentLength + tag.length() + 2 <= maxChars)
        {
            cleaned.add(tag);
        }
        else
        {
            break; // Hit the limit
        }
    }

    return cleaned;
}

//==============================================================================
// Description Cleaning
//==============================================================================
juce::String MetadataParser::cleanDescription(const juce::String& text, int& linesRemoved)
{
    auto lines = juce::StringArray::fromLines(text);
    juce::StringArray cleanedLines;
    linesRemoved = 0;
    bool hitIgnoreSeparator = false;

    for (const auto& line : lines)
    {
        // Check for ignore separators
        if (containsIgnoreSeparator(line))
        {
            hitIgnoreSeparator = true;
            linesRemoved++;
            continue;
        }

        // Skip everything after IGNORE separator
        if (hitIgnoreSeparator)
        {
            linesRemoved++;
            continue;
        }

        // Skip lines that are just underscores or dashes
        auto trimmed = line.trim();
        if (trimmed.isEmpty())
            continue;

        bool isJustSeparators = true;
        for (int i = 0; i < trimmed.length(); ++i)
        {
            if (trimmed[i] != '_' && trimmed[i] != '-' && trimmed[i] != '=' && trimmed[i] != ' ')
            {
                isJustSeparators = false;
                break;
            }
        }

        if (isJustSeparators)
        {
            linesRemoved++;
            continue;
        }

        cleanedLines.add(line);
    }

    return cleanedLines.joinIntoString("\n");
}

//==============================================================================
// Helper Methods
//==============================================================================
bool MetadataParser::containsIgnoreSeparator(const juce::String& line)
{
    auto lower = line.toLowerCase().trim();
    return lower.contains("ignore") ||
           lower.startsWith("---") ||
           lower.startsWith("___");
}

juce::String MetadataParser::normalizeWhitespace(const juce::String& text)
{
    return text.trim().replace("\t", " ");
}

juce::String MetadataParser::extractHandleFromLine(const juce::String& line, const juce::String& platform)
{
    auto colonPos = line.toLowerCase().indexOf(platform);
    if (colonPos < 0)
        colonPos = line.toLowerCase().indexOf("ig");

    if (colonPos >= 0)
    {
        auto afterPlatform = line.substring(colonPos + platform.length()).trim();

        if (afterPlatform.startsWith(":"))
            afterPlatform = afterPlatform.substring(1).trim();

        // Remove @ if present
        if (afterPlatform.startsWith("@"))
            afterPlatform = afterPlatform.substring(1);

        // Extract first word
        auto words = juce::StringArray::fromTokens(afterPlatform, " \t\n", "");
        if (words.size() > 0)
        {
            auto handle = words[0];
            if (handle.length() > 2 && handle.length() < 30)
                return handle;
        }
    }

    return {};
}
