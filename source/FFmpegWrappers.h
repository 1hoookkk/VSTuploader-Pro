#pragma once

// Prevent macro conflicts between JUCE and FFmpeg
#pragma push_macro("DEBUG")
#pragma push_macro("TRUE")
#pragma push_macro("FALSE")
#undef DEBUG
#undef TRUE
#undef FALSE

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/frame.h>
    #include <libavutil/imgutils.h>
    #include <libavutil/opt.h>
    #include <libswscale/swscale.h>
    #include <libswresample/swresample.h>
}

#pragma pop_macro("FALSE")
#pragma pop_macro("TRUE")
#pragma pop_macro("DEBUG")

#include <memory>
#include <string>

namespace ffmpeg {

//==============================================================================
// Custom Deleters for FFmpeg Contexts
// These ensure proper cleanup of FFmpeg resources using RAII principles
//==============================================================================

struct AVFormatContextDeleter {
    void operator()(AVFormatContext* ctx) const {
        if (ctx) {
            // Close output file if it was opened
            if (ctx->pb && !(ctx->oformat->flags & AVFMT_NOFILE)) {
                avio_closep(&ctx->pb);
            }
            avformat_free_context(ctx);
        }
    }
};

struct AVCodecContextDeleter {
    void operator()(AVCodecContext* ctx) const {
        if (ctx) {
            avcodec_free_context(&ctx);
        }
    }
};

struct AVFrameDeleter {
    void operator()(AVFrame* frame) const {
        if (frame) {
            av_frame_free(&frame);
        }
    }
};

struct AVPacketDeleter {
    void operator()(AVPacket* pkt) const {
        if (pkt) {
            av_packet_free(&pkt);
        }
    }
};

struct SwsContextDeleter {
    void operator()(SwsContext* ctx) const {
        if (ctx) {
            sws_freeContext(ctx);
        }
    }
};

struct SwrContextDeleter {
    void operator()(SwrContext* ctx) const {
        if (ctx) {
            swr_free(&ctx);
        }
    }
};

//==============================================================================
// Smart Pointer Type Aliases
// Use these instead of raw pointers for automatic memory management
//==============================================================================

using AVFormatContextPtr = std::unique_ptr<AVFormatContext, AVFormatContextDeleter>;
using AVCodecContextPtr = std::unique_ptr<AVCodecContext, AVCodecContextDeleter>;
using AVFramePtr = std::unique_ptr<AVFrame, AVFrameDeleter>;
using AVPacketPtr = std::unique_ptr<AVPacket, AVPacketDeleter>;
using SwsContextPtr = std::unique_ptr<SwsContext, SwsContextDeleter>;
using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;

//==============================================================================
// Factory Functions
// Create FFmpeg objects with proper initialization
//==============================================================================

inline AVFormatContextPtr makeFormatContext() {
    return AVFormatContextPtr(avformat_alloc_context());
}

inline AVFormatContextPtr makeOutputContext(const char* format, const char* filename) {
    AVFormatContext* ctx = nullptr;
    avformat_alloc_output_context2(&ctx, nullptr, format, filename);
    return AVFormatContextPtr(ctx);
}

inline AVCodecContextPtr makeCodecContext(const AVCodec* codec) {
    return AVCodecContextPtr(avcodec_alloc_context3(codec));
}

inline AVFramePtr makeFrame() {
    return AVFramePtr(av_frame_alloc());
}

inline AVPacketPtr makePacket() {
    return AVPacketPtr(av_packet_alloc());
}

//==============================================================================
// RAII Wrapper for AVDictionary
// Automatically frees dictionary on scope exit
//==============================================================================

class AVDictionaryWrapper {
public:
    AVDictionaryWrapper() : dict_(nullptr) {}

    ~AVDictionaryWrapper() {
        if (dict_) {
            av_dict_free(&dict_);
        }
    }

    // No copy
    AVDictionaryWrapper(const AVDictionaryWrapper&) = delete;
    AVDictionaryWrapper& operator=(const AVDictionaryWrapper&) = delete;

    // Move support
    AVDictionaryWrapper(AVDictionaryWrapper&& other) noexcept : dict_(other.dict_) {
        other.dict_ = nullptr;
    }

    AVDictionaryWrapper& operator=(AVDictionaryWrapper&& other) noexcept {
        if (this != &other) {
            if (dict_) {
                av_dict_free(&dict_);
            }
            dict_ = other.dict_;
            other.dict_ = nullptr;
        }
        return *this;
    }

    void set(const char* key, const char* value, int flags = 0) {
        av_dict_set(&dict_, key, value, flags);
    }

    AVDictionary** get() { return &dict_; }
    AVDictionary* getRaw() const { return dict_; }

private:
    AVDictionary* dict_;
};

} // namespace ffmpeg
