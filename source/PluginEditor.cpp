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
    // Setup title label
    titleLabel.setText ("VSTuploader Pro", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (28.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    // Setup BPM label
    bpmLabel.setText ("BPM: --", juce::dontSendNotification);
    bpmLabel.setFont (juce::Font (20.0f, juce::Font::bold));
    bpmLabel.setColour (juce::Label::textColourId, juce::Colours::lightblue);
    bpmLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (bpmLabel);

    // Setup file name label
    fileNameLabel.setText ("", juce::dontSendNotification);
    fileNameLabel.setFont (juce::Font (14.0f));
    fileNameLabel.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    fileNameLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (fileNameLabel);

    // Setup status label
    statusLabel.setText ("Ready to upload beats to YouTube", juce::dontSendNotification);
    statusLabel.setFont (juce::Font (12.0f));
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    statusLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (statusLabel);

    // Setup drag-drop zone
    dragDropZone.onFileDropped = [this](juce::File file) { handleFileDropped(file); };
    addAndMakeVisible (dragDropZone);

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

    setSize (600, 500);
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

    // Title at top
    titleLabel.setBounds (area.removeFromTop(60).reduced(20, 15));

    // BPM display
    bpmLabel.setBounds (area.removeFromTop(40).reduced(20, 5));

    area.removeFromTop(10); // Spacing

    // Drag-drop zone in center
    auto dropZoneHeight = 200;
    dragDropZone.setBounds (area.removeFromTop(dropZoneHeight).reduced(40, 20));

    area.removeFromTop(10); // Spacing

    // File name label
    fileNameLabel.setBounds (area.removeFromTop(30).reduced(20, 5));

    area.removeFromTop(20); // Spacing

    // Status label
    statusLabel.setBounds (area.removeFromTop(25).reduced(20, 5));

    // Inspector button at bottom (for debugging)
    inspectButton.setBounds (area.removeFromBottom(30).withSizeKeepingCentre(120, 25));
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

    // TODO: In future, this will trigger metadata parsing and upload prep
}
