#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <atomic>
#include <functional>

#ifdef VSTUPLOADER_FFMPEG_ENABLED
#include "FFmpegWrappers.h"
#include "FFmpegUtils.h"
#endif

//==============================================================================
/**
 * VideoEncoder - Encodes audio file + static image to MP4 for YouTube upload
 *
 * This class runs encoding on a background thread to avoid blocking the UI.
 * It uses FFmpeg to create YouTube-optimized MP4 files with H.264 video and AAC audio.
 *
 * Usage:
 *   VideoEncoder encoder(audioFile, thumbnailImage, outputFile);
 *   encoder.onProgressChanged = [](float progress) { /* update UI */ };
 *   encoder.onComplete = [](bool success, juce::String error) { /* handle result */ };
 *   encoder.startEncoding();
 */
class VideoEncoder : public juce::Thread
{
public:
    //==============================================================================
    // Encoding parameters optimized for YouTube uploads
    struct EncodingSettings {
        // Video settings
        int videoWidth = 1920;
        int videoHeight = 1080;
        int frameRate = 30;              // 30 fps (YouTube standard)
        int videoBitrate = 8000000;      // 8 Mbps for 1080p

        // Audio settings
        int audioSampleRate = 48000;     // YouTube prefers 48kHz
        int audioChannels = 2;           // Stereo
        int audioBitrate = 192000;       // 192 kbps

        // H.264 settings
        juce::String h264Preset = "slow";  // slow/medium/fast (slow = better quality)
        int crf = 18;                    // Constant Rate Factor: 18 = visually lossless

        // MP4 settings
        bool enableFastStart = true;     // Enable progressive download
    };

    //==============================================================================
    /**
     * Constructor
     * @param audioFile Input audio file (WAV, MP3, FLAC, etc.)
     * @param thumbnail Static image for video (will be resized if needed)
     * @param outputFile Output MP4 file path
     * @param settings Encoding settings (uses YouTube defaults if not specified)
     */
    VideoEncoder(const juce::File& audioFile,
                 const juce::Image& thumbnail,
                 const juce::File& outputFile,
                 const EncodingSettings& settings = EncodingSettings());

    ~VideoEncoder() override;

    //==============================================================================
    // Control methods

    /** Start encoding on background thread */
    void startEncoding();

    /** Cancel encoding operation */
    void cancelEncoding();

    /** Check if encoding is currently running */
    bool isEncoding() const;

    //==============================================================================
    // Progress and status

    /** Get current encoding progress (0.0 to 1.0) */
    float getProgress() const { return progress_.load(); }

    /** Get current status message */
    juce::String getStatusMessage() const;

    /** Get last error message (if encoding failed) */
    juce::String getLastError() const;

    //==============================================================================
    // Callbacks (set these before calling startEncoding)

    /** Called when progress changes (thread-safe, called from background thread) */
    std::function<void(float progress)> onProgressChanged;

    /** Called when encoding completes or fails (thread-safe) */
    std::function<void(bool success, juce::String errorMessage)> onComplete;

    /** Called with status message updates */
    std::function<void(juce::String statusMessage)> onStatusChanged;

private:
    //==============================================================================
    // Thread implementation
    void run() override;

    //==============================================================================
    // Encoding pipeline stages

#ifdef VSTUPLOADER_FFMPEG_ENABLED
    /** Initialize output container and streams */
    void initializeOutput();

    /** Add H.264 video stream */
    AVStream* addVideoStream();

    /** Add AAC audio stream */
    AVStream* addAudioStream(int sampleRate, int channels);

    /** Load and decode input audio file */
    void loadInputAudio();

    /** Prepare thumbnail image for encoding */
    void prepareThumbnail();

    /** Encode video frames (static image repeated for audio duration) */
    void encodeVideoFrames();

    /** Encode audio frames (AAC) */
    void encodeAudioFrames();

    /** Finalize and close output file */
    void finalizeOutput();

    /** Flush encoder buffers */
    void flushEncoder(AVCodecContext* codecCtx, AVStream* stream);

    /** Write packet to output file */
    void writePacket(AVPacket* packet, AVStream* stream);

    /** Convert JUCE Image to AVFrame in YUV420P format */
    ffmpeg::AVFramePtr imageToAVFrame(const juce::Image& image);

    /** Resample audio if needed (convert sample rate, channels) */
    void resampleAudio();
#endif

    //==============================================================================
    // Helper methods

    /** Update progress (thread-safe) */
    void updateProgress(float newProgress);

    /** Update status message (thread-safe) */
    void updateStatus(const juce::String& message);

    /** Set error message (thread-safe) */
    void setError(const juce::String& error);

    /** Calculate total duration from audio file */
    double getAudioDuration() const;

    //==============================================================================
    // Member variables

    juce::File audioFile_;
    juce::File outputFile_;
    juce::Image thumbnail_;
    EncodingSettings settings_;

    // Thread-safe state
    std::atomic<float> progress_{0.0f};
    std::atomic<bool> shouldCancel_{false};
    std::atomic<bool> isEncoding_{false};

    juce::String statusMessage_;
    juce::String errorMessage_;
    mutable juce::CriticalSection messageMutex_;

#ifdef VSTUPLOADER_FFMPEG_ENABLED
    // FFmpeg contexts (used only on encoding thread)
    ffmpeg::AVFormatContextPtr formatCtx_;
    ffmpeg::AVCodecContextPtr videoCodecCtx_;
    ffmpeg::AVCodecContextPtr audioCodecCtx_;

    AVStream* videoStream_ = nullptr;
    AVStream* audioStream_ = nullptr;

    // Audio data from input file
    juce::AudioBuffer<float> audioBuffer_;
    double audioDuration_ = 0.0;
    int audioSampleRate_ = 48000;
    int audioNumChannels_ = 2;

    // Encoding state
    int64_t nextVideoPts_ = 0;
    int64_t nextAudioPts_ = 0;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VideoEncoder)
};
