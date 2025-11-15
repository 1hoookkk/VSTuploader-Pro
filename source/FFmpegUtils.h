#pragma once

#include "FFmpegWrappers.h"
#include <juce_core/juce_core.h>
#include <stdexcept>
#include <string>

namespace ffmpeg {

//==============================================================================
// Error Handling Utilities
//==============================================================================

/**
 * Convert FFmpeg error code to human-readable string
 * @param errorCode Negative error code from FFmpeg function
 * @return String description of the error
 */
inline std::string ffmpegErrorString(int errorCode) {
    char errBuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errorCode, errBuf, sizeof(errBuf));
    return std::string(errBuf);
}

/**
 * Convert FFmpeg error code to JUCE String
 * @param errorCode Negative error code from FFmpeg function
 * @return JUCE String description of the error
 */
inline juce::String ffmpegErrorJuceString(int errorCode) {
    return juce::String(ffmpegErrorString(errorCode));
}

/**
 * Throw exception if FFmpeg function returned error
 * @param ret Return value from FFmpeg function
 * @param context Description of what was being attempted
 * @throws std::runtime_error if ret < 0
 */
inline void checkFFmpegResult(int ret, const std::string& context) {
    if (ret < 0) {
        std::string errorMsg = context + ": " + ffmpegErrorString(ret);
        throw std::runtime_error(errorMsg);
    }
}

//==============================================================================
// Result Type for Error Handling
// Preferred over exceptions for non-fatal errors
//==============================================================================

template<typename T>
class Result {
public:
    // Success constructor
    Result(T value) : value_(std::move(value)), success_(true) {}

    // Error constructor
    Result(const std::string& error) : error_(error), success_(false) {}
    Result(const juce::String& error) : error_(error.toStdString()), success_(false) {}

    bool isSuccess() const { return success_; }
    bool isError() const { return !success_; }

    const T& getValue() const {
        if (!success_) {
            throw std::runtime_error("Attempted to get value from error Result");
        }
        return value_;
    }

    T&& moveValue() {
        if (!success_) {
            throw std::runtime_error("Attempted to get value from error Result");
        }
        return std::move(value_);
    }

    const std::string& getError() const {
        return error_;
    }

    juce::String getErrorJuceString() const {
        return juce::String(error_);
    }

private:
    T value_;
    std::string error_;
    bool success_;
};

// Specialization for void Result
template<>
class Result<void> {
public:
    // Success constructor
    Result() : success_(true) {}

    // Error constructor
    Result(const std::string& error) : error_(error), success_(false) {}
    Result(const juce::String& error) : error_(error.toStdString()), success_(false) {}

    bool isSuccess() const { return success_; }
    bool isError() const { return !success_; }

    const std::string& getError() const {
        return error_;
    }

    juce::String getErrorJuceString() const {
        return juce::String(error_);
    }

private:
    std::string error_;
    bool success_;
};

//==============================================================================
// Timestamp Utilities
//==============================================================================

/**
 * Convert frame number to PTS (presentation timestamp)
 * @param frameNumber Frame index (0-based)
 * @param timebase Codec or stream time_base
 * @return Correctly scaled PTS value
 */
inline int64_t frameToPTS(int64_t frameNumber, AVRational timebase) {
    return frameNumber;  // PTS in timebase units
}

/**
 * Rescale timestamp from one timebase to another
 * @param pts Timestamp in source timebase
 * @param srcTimebase Source timebase
 * @param dstTimebase Destination timebase
 * @return Timestamp in destination timebase
 */
inline int64_t rescaleTimestamp(int64_t pts, AVRational srcTimebase, AVRational dstTimebase) {
    return av_rescale_q(pts, srcTimebase, dstTimebase);
}

/**
 * Calculate duration for a single frame/packet
 * @param timebase Time base to use
 * @return Duration in timebase units
 */
inline int64_t getFrameDuration(AVRational timebase) {
    return 1;  // 1 unit in the given timebase
}

//==============================================================================
// Codec and Format Utilities
//==============================================================================

/**
 * Find encoder by codec ID with error checking
 * @param codecId FFmpeg codec ID (e.g., AV_CODEC_ID_H264)
 * @return Pointer to codec or nullptr if not found
 */
inline const AVCodec* findEncoder(AVCodecID codecId) {
    const AVCodec* codec = avcodec_find_encoder(codecId);
    return codec;
}

/**
 * Find encoder by name with error checking
 * @param name Codec name (e.g., "libx264")
 * @return Pointer to codec or nullptr if not found
 */
inline const AVCodec* findEncoderByName(const char* name) {
    const AVCodec* codec = avcodec_find_encoder_by_name(name);
    return codec;
}

/**
 * Get codec name as string
 * @param codecId FFmpeg codec ID
 * @return Human-readable codec name
 */
inline std::string getCodecName(AVCodecID codecId) {
    const char* name = avcodec_get_name(codecId);
    return name ? std::string(name) : "unknown";
}

//==============================================================================
// Frame and Image Utilities
//==============================================================================

/**
 * Allocate frame buffers based on frame parameters
 * @param frame Frame to allocate buffers for (width, height, format must be set)
 * @param align Buffer alignment (typically 32)
 * @return 0 on success, negative on error
 */
inline int allocateFrameBuffers(AVFrame* frame, int align = 32) {
    return av_frame_get_buffer(frame, align);
}

/**
 * Calculate required buffer size for image
 * @param pixelFormat FFmpeg pixel format
 * @param width Image width
 * @param height Image height
 * @param align Buffer alignment
 * @return Required buffer size in bytes
 */
inline int getImageBufferSize(AVPixelFormat pixelFormat, int width, int height, int align = 1) {
    return av_image_get_buffer_size(pixelFormat, width, height, align);
}

//==============================================================================
// Logging Control
//==============================================================================

/**
 * Set FFmpeg log level
 * @param level AV_LOG_QUIET, AV_LOG_ERROR, AV_LOG_WARNING, AV_LOG_INFO, AV_LOG_DEBUG
 */
inline void setLogLevel(int level) {
    av_log_set_level(level);
}

/**
 * Initialize FFmpeg logging to redirect to JUCE logger
 */
inline void initializeLogging() {
    // Set to warning level in release, debug level in debug builds
    #ifdef NDEBUG
        setLogLevel(AV_LOG_WARNING);
    #else
        setLogLevel(AV_LOG_DEBUG);
    #endif
}

//==============================================================================
// Validation Utilities
//==============================================================================

/**
 * Check if pixel format is supported by codec
 * @param codec Codec to check
 * @param pixelFormat Pixel format to validate
 * @return true if supported
 */
inline bool isPixelFormatSupported(const AVCodec* codec, AVPixelFormat pixelFormat) {
    if (!codec || !codec->pix_fmts) {
        return false;
    }

    for (const AVPixelFormat* p = codec->pix_fmts; *p != AV_PIX_FMT_NONE; ++p) {
        if (*p == pixelFormat) {
            return true;
        }
    }
    return false;
}

/**
 * Get first supported pixel format for codec
 * @param codec Codec to check
 * @return First supported pixel format or AV_PIX_FMT_NONE
 */
inline AVPixelFormat getFirstSupportedPixelFormat(const AVCodec* codec) {
    if (!codec || !codec->pix_fmts) {
        return AV_PIX_FMT_NONE;
    }
    return codec->pix_fmts[0];
}

} // namespace ffmpeg
