#pragma once

#include "PluginProcessor.h"
#include "BinaryData.h"
#include "melatonin_inspector/melatonin_inspector.h"

//==============================================================================
// Custom component that handles drag and drop of audio files
class DragDropZone : public juce::Component,
                     public juce::FileDragAndDropTarget
{
public:
    DragDropZone();
    void paint (juce::Graphics& g) override;

    // FileDragAndDropTarget implementation
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;
    void fileDragEnter (const juce::StringArray& files, int x, int y) override;
    void fileDragExit (const juce::StringArray& files) override;

    void setDroppedFileName (const juce::String& name);
    juce::String getDroppedFileName() const { return droppedFileName; }

    std::function<void(juce::File)> onFileDropped;

private:
    bool isDragging = false;
    juce::String droppedFileName;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DragDropZone)
};

//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    void handleFileDropped (juce::File file);

    PluginProcessor& processorRef;
    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect the UI" };

    // VSTuploader Pro UI components
    DragDropZone dragDropZone;
    juce::Label titleLabel;
    juce::Label bpmLabel;
    juce::Label fileNameLabel;
    juce::Label statusLabel;

    juce::File droppedAudioFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
