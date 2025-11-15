#include "PluginEditor.h"

//==============================================================================
// DragDropZone Implementation
//==============================================================================
DragDropZone::DragDropZone()
{
}

void DragDropZone::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced(10);

    // Draw dashed border
    g.setColour (isDragging ? juce::Colours::lightblue : juce::Colours::grey);

    const float dashLengths[] = { 10.0f, 5.0f };
    g.drawDashedLine (juce::Line<float>(bounds.getTopLeft().toFloat(),
                                        bounds.getTopRight().toFloat()),
                     dashLengths, 2, 2.0f);
    g.drawDashedLine (juce::Line<float>(bounds.getTopRight().toFloat(),
                                        bounds.getBottomRight().toFloat()),
                     dashLengths, 2, 2.0f);
    g.drawDashedLine (juce::Line<float>(bounds.getBottomRight().toFloat(),
                                        bounds.getBottomLeft().toFloat()),
                     dashLengths, 2, 2.0f);
    g.drawDashedLine (juce::Line<float>(bounds.getBottomLeft().toFloat(),
                                        bounds.getTopLeft().toFloat()),
                     dashLengths, 2, 2.0f);

    // Background fill
    if (isDragging)
    {
        g.setColour (juce::Colours::lightblue.withAlpha(0.1f));
        g.fillRect (bounds);
    }

    // Draw icon or text
    g.setColour (isDragging ? juce::Colours::white : juce::Colours::lightgrey);
    g.setFont (18.0f);

    if (droppedFileName.isEmpty())
    {
        g.drawText ("Drop Audio File Here\n(WAV, MP3, FLAC, AIFF)",
                   bounds, juce::Justification::centred);
    }
    else
    {
        g.setFont (16.0f);
        g.setColour (juce::Colours::lightgreen);
        g.drawText ("✓ File Loaded", bounds.removeFromTop(30),
                   juce::Justification::centred);

        g.setFont (14.0f);
        g.setColour (juce::Colours::white);
        g.drawText (droppedFileName, bounds,
                   juce::Justification::centred);
    }
}

bool DragDropZone::isInterestedInFileDrag (const juce::StringArray& files)
{
    // Check if any of the files are audio files
    for (const auto& filepath : files)
    {
        juce::File file (filepath);
        juce::String extension = file.getFileExtension().toLowerCase();

        if (extension == ".wav" || extension == ".mp3" ||
            extension == ".flac" || extension == ".aiff" ||
            extension == ".aif" || extension == ".ogg")
        {
            return true;
        }
    }
    return false;
}

void DragDropZone::filesDropped (const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused (x, y);

    isDragging = false;

    if (files.size() > 0)
    {
        juce::File file (files[0]);
        droppedFileName = file.getFileName();

        if (onFileDropped)
            onFileDropped (file);

        repaint();
    }
}

void DragDropZone::fileDragEnter (const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused (files, x, y);
    isDragging = true;
    repaint();
}

void DragDropZone::fileDragExit (const juce::StringArray& files)
{
    juce::ignoreUnused (files);
    isDragging = false;
    repaint();
}

void DragDropZone::setDroppedFileName (const juce::String& name)
{
    droppedFileName = name;
    repaint();
}

//==============================================================================
// PluginEditor Implementation
//==============================================================================
PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    // Setup title label (compact)
    titleLabel.setText ("VSTuploader Pro", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (13.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    // Setup BPM label (compact, inline)
    bpmLabel.setText ("BPM: --", juce::dontSendNotification);
    bpmLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    bpmLabel.setColour (juce::Label::textColourId, juce::Colours::lightblue);
    bpmLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (bpmLabel);

    // Setup file name label (compact)
    fileNameLabel.setText ("", juce::dontSendNotification);
    fileNameLabel.setFont (juce::Font (9.0f));
    fileNameLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    fileNameLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (fileNameLabel);

    // Setup status label (compact)
    statusLabel.setText ("Ready", juce::dontSendNotification);
    statusLabel.setFont (juce::Font (9.0f));
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    statusLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (statusLabel);

    // Setup drag-drop zone
    dragDropZone.onFileDropped = [this](juce::File file) { handleFileDropped(file); };
    addAndMakeVisible (dragDropZone);

    // Setup Paste & Parse section
    pasteLabel.setText ("Paste YouTube Description:", juce::dontSendNotification);
    pasteLabel.setFont (juce::Font (14.0f, juce::Font::bold));
    pasteLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (pasteLabel);

    pasteTextEditor.setMultiLine (true);
    pasteTextEditor.setReturnKeyStartsNewLine (true);
    pasteTextEditor.setScrollbarsShown (true);
    pasteTextEditor.setCaretVisible (true);
    pasteTextEditor.setPopupMenuEnabled (true);
    pasteTextEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff2a2a2a));
    pasteTextEditor.setColour (juce::TextEditor::textColourId, juce::Colours::white);
    pasteTextEditor.setColour (juce::TextEditor::outlineColourId, juce::Colours::grey);
    pasteTextEditor.setFont (juce::Font (9.0f));
    addAndMakeVisible (pasteTextEditor);

    parseButton.onClick = [this]() { handlePasteAndParse(); };
    parseButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff4a9eff));
    addAndMakeVisible (parseButton);

    // Setup parsed metadata display
    parsedSectionLabel.setText ("Parsed Metadata:", juce::dontSendNotification);
    parsedSectionLabel.setFont (juce::Font (14.0f, juce::Font::bold));
    parsedSectionLabel.setColour (juce::Label::textColourId, juce::Colours::lightblue);
    addAndMakeVisible (parsedSectionLabel);

    keyLabel.setFont (juce::Font (9.0f));
    keyLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (keyLabel);

    instagramLabel.setFont (juce::Font (9.0f));
    instagramLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (instagramLabel);

    twitterLabel.setFont (juce::Font (9.0f));
    twitterLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (twitterLabel);

    emailLabel.setFont (juce::Font (9.0f));
    emailLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (emailLabel);

    usageLabel.setFont (juce::Font (9.0f));
    usageLabel.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
    addAndMakeVisible (usageLabel);

    tagsLabel.setFont (juce::Font (8.0f));
    tagsLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (tagsLabel);

    // Setup YouTube section
    youtubeLabel.setText ("YouTube Upload:", juce::dontSendNotification);
    youtubeLabel.setFont (juce::Font (14.0f, juce::Font::bold));
    youtubeLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (youtubeLabel);

    connectYouTubeButton.onClick = [this]() { handleConnectYouTube(); };
    connectYouTubeButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffff0000)); // YouTube red
    addAndMakeVisible (connectYouTubeButton);

    youtubeStatusLabel.setFont (juce::Font (9.0f));
    youtubeStatusLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (youtubeStatusLabel);

    uploadButton.onClick = [this]() { handleUploadToYouTube(); };
    uploadButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff4a9eff));
    uploadButton.setEnabled (false); // Disabled until connected
    addAndMakeVisible (uploadButton);

    // Setup YouTube callbacks
    youtubeUploader.onAuthComplete = [this](bool success, juce::String message)
    {
        juce::MessageManager::callAsync([this, success, message]()
        {
            if (success)
            {
                updateYouTubeStatus();
                uploadButton.setEnabled(true);
            }
            statusLabel.setText(message, juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId,
                                 success ? juce::Colours::lightgreen : juce::Colours::orange);
        });
    };

    youtubeUploader.onError = [this](juce::String error)
    {
        juce::MessageManager::callAsync([this, error]()
        {
            statusLabel.setText(error, juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
        });
    };

    // Update YouTube status on startup
    updateYouTubeStatus();

    // Setup inspector button (for debugging)
    addAndMakeVisible (inspectButton);
    inspectButton.onClick = [&] {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }
        inspector->setVisible (true);
    };

    // Start timer to update BPM display (30 fps)
    startTimerHz (30);

    // Initialize metadata display
    updateMetadataDisplay();

    setSize (380, 650);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
}

void PluginEditor::paint (juce::Graphics& g)
{
    // Modern gradient background
    g.fillAll (juce::Colour (0xff1a1a1a));

    auto bounds = getLocalBounds();
    juce::ColourGradient gradient (juce::Colour (0xff2a2a2a),
                                   bounds.getTopLeft().toFloat(),
                                   juce::Colour (0xff1a1a1a),
                                   bounds.getBottomRight().toFloat(),
                                   false);
    g.setGradientFill (gradient);
    g.fillAll();
}

void PluginEditor::resized()
{
    auto area = getLocalBounds();
    int padding = 8;

    // Compact header: Channel + BPM inline
    auto headerArea = area.removeFromTop(30).reduced(padding, 4);
    titleLabel.setBounds (headerArea.removeFromLeft(180));
    bpmLabel.setBounds (headerArea);

    area.removeFromTop(2); // Tight spacing

    // Compact drag-drop zone
    auto dropZoneHeight = 70;
    dragDropZone.setBounds (area.removeFromTop(dropZoneHeight).reduced(padding, 4));

    // File name (compact)
    fileNameLabel.setBounds (area.removeFromTop(20).reduced(padding, 2));

    area.removeFromTop(4); // Tight spacing

    // Paste & Parse button (compact, no label)
    parseButton.setBounds (area.removeFromTop(28).reduced(padding, 2));

    // Description editor (hidden by default, will add toggle later)
    // For now, keep it small
    auto descHeight = 80;
    pasteTextEditor.setBounds (area.removeFromTop(descHeight).reduced(padding, 2));

    area.removeFromTop(4);

    // Metadata chips (compact, single line each)
    auto metadataArea = area.removeFromTop(120).reduced(padding, 2);
    int chipHeight = 18;

    keyLabel.setBounds (metadataArea.removeFromTop(chipHeight));
    instagramLabel.setBounds (metadataArea.removeFromTop(chipHeight));
    twitterLabel.setBounds (metadataArea.removeFromTop(chipHeight));
    emailLabel.setBounds (metadataArea.removeFromTop(chipHeight));
    usageLabel.setBounds (metadataArea.removeFromTop(chipHeight));
    metadataArea.removeFromTop(2);
    tagsLabel.setBounds (metadataArea.removeFromTop(chipHeight * 2));

    area.removeFromTop(4);

    // YouTube section (compact)
    auto ytArea = area.removeFromTop(70).reduced(padding, 2);

    connectYouTubeButton.setBounds (ytArea.removeFromTop(28));
    youtubeStatusLabel.setBounds (ytArea.removeFromTop(16));
    ytArea.removeFromTop(2);
    uploadButton.setBounds (ytArea.removeFromTop(28));

    area.removeFromTop(4);

    // Status at bottom
    statusLabel.setBounds (area.removeFromTop(20).reduced(padding, 2));

    // Hide labels initially
    pasteLabel.setVisible(false);
    parsedSectionLabel.setVisible(false);
    youtubeLabel.setVisible(false);
    inspectButton.setVisible(false); // Hide in minimal mode
}

void PluginEditor::timerCallback()
{
    // Update BPM display from host
    double currentBPM = processorRef.getCurrentBPM();
    bpmLabel.setText ("BPM: " + juce::String (currentBPM, 1),
                     juce::dontSendNotification);
}

void PluginEditor::handleFileDropped (juce::File file)
{
    droppedAudioFile = file;

    // Update file name label
    fileNameLabel.setText ("File: " + file.getFileName(),
                          juce::dontSendNotification);

    // Update status
    statusLabel.setText ("File loaded! Ready for YouTube upload",
                        juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
}

void PluginEditor::handlePasteAndParse()
{
    auto inputText = pasteTextEditor.getText();

    if (inputText.isEmpty())
    {
        statusLabel.setText ("Paste some text first!", juce::dontSendNotification);
        statusLabel.setColour (juce::Label::textColourId, juce::Colours::orange);
        return;
    }

    // Parse the metadata
    parsedMetadata = metadataParser.parse(inputText);

    // Update the display
    updateMetadataDisplay();

    // Update status
    juce::String statusMessage = "Parsed! ";
    if (parsedMetadata.bpm > 0)
        statusMessage += "BPM: " + juce::String(parsedMetadata.bpm) + " | ";
    if (parsedMetadata.key.isNotEmpty())
        statusMessage += "Key: " + parsedMetadata.key + " | ";
    if (parsedMetadata.tags.size() > 0)
        statusMessage += juce::String(parsedMetadata.tags.size()) + " tags";
    if (parsedMetadata.tagsDeduped > 0)
        statusMessage += " (" + juce::String(parsedMetadata.tagsDeduped) + " dupes removed)";

    statusLabel.setText (statusMessage, juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
}

void PluginEditor::updateMetadataDisplay()
{
    // Key
    if (parsedMetadata.key.isNotEmpty())
        keyLabel.setText ("Key: " + parsedMetadata.key, juce::dontSendNotification);
    else
        keyLabel.setText ("Key: (not found)", juce::dontSendNotification);

    // Instagram
    if (parsedMetadata.instagramHandle.isNotEmpty())
        instagramLabel.setText ("Instagram: @" + parsedMetadata.instagramHandle, juce::dontSendNotification);
    else
        instagramLabel.setText ("Instagram: (not found)", juce::dontSendNotification);

    // Twitter
    if (parsedMetadata.twitterHandle.isNotEmpty())
        twitterLabel.setText ("Twitter: @" + parsedMetadata.twitterHandle, juce::dontSendNotification);
    else
        twitterLabel.setText ("Twitter: (not found)", juce::dontSendNotification);

    // Emails (can be multiple)
    if (parsedMetadata.emails.size() > 0)
    {
        juce::String emailText = "Email: ";
        for (int i = 0; i < parsedMetadata.emails.size(); ++i)
        {
            emailText += parsedMetadata.emails[i];
            if (i < parsedMetadata.emails.size() - 1)
                emailText += ", ";
        }
        emailLabel.setText (emailText, juce::dontSendNotification);
    }
    else
    {
        emailLabel.setText ("Email: (not found)", juce::dontSendNotification);
    }

    // Usage terms
    if (parsedMetadata.usageTerms.isNotEmpty())
        usageLabel.setText ("Usage: " + parsedMetadata.usageTerms, juce::dontSendNotification);
    else
        usageLabel.setText ("Usage: (not detected)", juce::dontSendNotification);

    // Tags
    if (parsedMetadata.tags.size() > 0)
    {
        juce::String tagString = "Tags (" + juce::String(parsedMetadata.tags.size()) + "): ";
        for (int i = 0; i < juce::jmin(10, parsedMetadata.tags.size()); ++i)
        {
            tagString += parsedMetadata.tags[i];
            if (i < juce::jmin(9, parsedMetadata.tags.size() - 1))
                tagString += ", ";
        }
        if (parsedMetadata.tags.size() > 10)
            tagString += "... +" + juce::String(parsedMetadata.tags.size() - 10) + " more";

        tagsLabel.setText (tagString, juce::dontSendNotification);
    }
    else
    {
        tagsLabel.setText ("Tags: (none found)", juce::dontSendNotification);
    }
}

void PluginEditor::handleConnectYouTube()
{
    if (youtubeUploader.isAuthenticated())
    {
        // Disconnect
        youtubeUploader.disconnect();
        updateYouTubeStatus();
        uploadButton.setEnabled(false);
        statusLabel.setText("Disconnected from YouTube", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
    }
    else
    {
        // Start OAuth flow
        youtubeUploader.startAuth();
        statusLabel.setText("Opening browser for YouTube authorization...", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightblue);
    }
}

void PluginEditor::handleUploadToYouTube()
{
    if (!youtubeUploader.isAuthenticated())
    {
        statusLabel.setText("Please connect to YouTube first", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
        return;
    }

    if (!droppedAudioFile.existsAsFile())
    {
        statusLabel.setText("Please drop an audio file first", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
        return;
    }

    // Build metadata from parsed data
    YouTubeUploadMetadata metadata;
    metadata.title = droppedAudioFile.getFileNameWithoutExtension();
    metadata.description = parsedMetadata.cleanDescription;
    metadata.tags = parsedMetadata.tags;
    metadata.privacy = "unlisted"; // Default to unlisted
    metadata.categoryId = "10"; // Music category

    // TODO: Create thumbnail (for now, use default)
    juce::File thumbnailFile;

    // Start upload
    statusLabel.setText("Uploading to YouTube...", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightblue);

    bool success = youtubeUploader.uploadVideo(
        droppedAudioFile,
        thumbnailFile,
        metadata,
        [this](float progress)
        {
            juce::MessageManager::callAsync([this, progress]()
            {
                statusLabel.setText("Uploading: " + juce::String(int(progress * 100)) + "%",
                                   juce::dontSendNotification);
            });
        });

    if (!success)
    {
        statusLabel.setText("Upload failed. See console for details.", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
    }
}

void PluginEditor::updateYouTubeStatus()
{
    if (youtubeUploader.isAuthenticated())
    {
        auto channelInfo = youtubeUploader.getChannelInfo();
        if (channelInfo.channelName.isNotEmpty())
        {
            youtubeStatusLabel.setText("Connected: " + channelInfo.channelName,
                                      juce::dontSendNotification);
            youtubeStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
            connectYouTubeButton.setButtonText("Disconnect");
        }
        else
        {
            youtubeStatusLabel.setText("Connected (loading channel...)",
                                      juce::dontSendNotification);
            youtubeStatusLabel.setColour(juce::Label::textColourId, juce::Colours::lightblue);
            connectYouTubeButton.setButtonText("Disconnect");
        }
    }
    else
    {
        youtubeStatusLabel.setText("Not connected", juce::dontSendNotification);
        youtubeStatusLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
        connectYouTubeButton.setButtonText("Connect YouTube");
    }
}
