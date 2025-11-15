#include "VideoEncoder.h"
#include <juce_audio_formats/juce_audio_formats.h>

//==============================================================================
VideoEncoder::VideoEncoder(const juce::File& audioFile,
                           const juce::Image& thumbnail,
                           const juce::File& outputFile,
                           const EncodingSettings& settings)
    : juce::Thread("VideoEncoder"),
      audioFile_(audioFile),
      outputFile_(outputFile),
      thumbnail_(thumbnail),
      settings_(settings)
{
}

VideoEncoder::~VideoEncoder()
{
    stopThread(5000);  // Wait up to 5 seconds for thread to finish
}

//==============================================================================
void VideoEncoder::startEncoding()
{
#ifndef VSTUPLOADER_FFMPEG_ENABLED
    setError("FFmpeg not available. Video encoding is disabled.");
    if (onComplete)
        onComplete(false, "FFmpeg not available");
    return;
#else
    if (isEncoding_.load())
    {
        jassertfalse;  // Already encoding
        return;
    }

    // Reset state
    progress_.store(0.0f);
    shouldCancel_.store(false);
    isEncoding_.store(true);
    errorMessage_.clear();
    statusMessage_.clear();

    // Start background thread
    startThread();
#endif
}

void VideoEncoder::cancelEncoding()
{
    shouldCancel_.store(true);
    signalThreadShouldExit();
}

bool VideoEncoder::isEncoding() const
{
    return isEncoding_.load();
}

juce::String VideoEncoder::getStatusMessage() const
{
    juce::ScopedLock lock(messageMutex_);
    return statusMessage_;
}

juce::String VideoEncoder::getLastError() const
{
    juce::ScopedLock lock(messageMutex_);
    return errorMessage_;
}

//==============================================================================
void VideoEncoder::run()
{
#ifdef VSTUPLOADER_FFMPEG_ENABLED
    bool success = false;
    juce::String error;

    try
    {
        updateStatus("Initializing FFmpeg...");
        ffmpeg::initializeLogging();

        updateStatus("Loading audio file...");
        loadInputAudio();

        if (threadShouldExit())
            throw std::runtime_error("Cancelled by user");

        updateStatus("Preparing thumbnail...");
        prepareThumbnail();

        updateStatus("Initializing output container...");
        initializeOutput();

        updateStatus("Encoding video frames...");
        encodeVideoFrames();

        if (threadShouldExit())
            throw std::runtime_error("Cancelled by user");

        updateStatus("Encoding audio frames...");
        encodeAudioFrames();

        updateStatus("Finalizing output file...");
        finalizeOutput();

        updateProgress(1.0f);
        updateStatus("Encoding complete!");
        success = true;
    }
    catch (const std::exception& e)
    {
        error = juce::String("Encoding failed: ") + e.what();
        setError(error);
        success = false;
    }

    // Cleanup
    formatCtx_.reset();
    videoCodecCtx_.reset();
    audioCodecCtx_.reset();
    videoStream_ = nullptr;
    audioStream_ = nullptr;

    isEncoding_.store(false);

    if (onComplete)
        onComplete(success, error);
#endif
}

//==============================================================================
#ifdef VSTUPLOADER_FFMPEG_ENABLED

void VideoEncoder::initializeOutput()
{
    // Allocate output format context
    const char* filename = outputFile_.getFullPathName().toRawUTF8();
    AVFormatContext* rawCtx = nullptr;

    int ret = avformat_alloc_output_context2(&rawCtx, nullptr, "mp4", filename);
    ffmpeg::checkFFmpegResult(ret, "Failed to create output context");

    formatCtx_ = ffmpeg::AVFormatContextPtr(rawCtx);

    // Add video and audio streams
    videoStream_ = addVideoStream();
    audioStream_ = addAudioStream(audioSampleRate_, audioNumChannels_);

    // Open output file
    if (!(formatCtx_->oformat->flags & AVFMT_NOFILE))
    {
        ret = avio_open(&formatCtx_->pb, filename, AVIO_FLAG_WRITE);
        ffmpeg::checkFFmpegResult(ret, "Could not open output file");
    }

    // Write container header
    ffmpeg::AVDictionaryWrapper opts;
    if (settings_.enableFastStart)
    {
        opts.set("movflags", "+faststart");  // Enable progressive download
    }

    ret = avformat_write_header(formatCtx_.get(), opts.get());
    ffmpeg::checkFFmpegResult(ret, "Error writing container header");

    // Dump format info for debugging
    av_dump_format(formatCtx_.get(), 0, filename, 1);
}

AVStream* VideoEncoder::addVideoStream()
{
    // Find H.264 encoder
    const AVCodec* codec = ffmpeg::findEncoder(AV_CODEC_ID_H264);
    if (!codec)
        throw std::runtime_error("H.264 encoder not found");

    AVStream* stream = avformat_new_stream(formatCtx_.get(), nullptr);
    if (!stream)
        throw std::runtime_error("Failed to create video stream");

    stream->id = formatCtx_->nb_streams - 1;

    // Create codec context
    videoCodecCtx_ = ffmpeg::makeCodecContext(codec);
    AVCodecContext* ctx = videoCodecCtx_.get();

    // YouTube-optimized H.264 settings
    ctx->codec_id = AV_CODEC_ID_H264;
    ctx->width = settings_.videoWidth;
    ctx->height = settings_.videoHeight;
    ctx->time_base = AVRational{1, settings_.frameRate};
    ctx->framerate = AVRational{settings_.frameRate, 1};
    ctx->gop_size = settings_.frameRate / 2;  // Keyframe every 0.5 seconds
    ctx->max_b_frames = 2;
    ctx->pix_fmt = AV_PIX_FMT_YUV420P;

    // H.264 profile and level
    ctx->profile = FF_PROFILE_H264_HIGH;
    ctx->level = 40;  // Level 4.0

    // Rate control
    ctx->bit_rate = settings_.videoBitrate;
    ctx->rc_buffer_size = settings_.videoBitrate;
    ctx->rc_max_rate = settings_.videoBitrate;

    // Container-specific flags
    if (formatCtx_->oformat->flags & AVFMT_GLOBALHEADER)
        ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    // Open codec with preset and CRF
    ffmpeg::AVDictionaryWrapper opts;
    opts.set("preset", settings_.h264Preset.toRawUTF8());
    opts.set("crf", juce::String(settings_.crf).toRawUTF8());
    opts.set("tune", "stillimage");  // Optimize for static image

    int ret = avcodec_open2(ctx, codec, opts.get());
    ffmpeg::checkFFmpegResult(ret, "Could not open video codec");

    // Copy codec parameters to stream
    ret = avcodec_parameters_from_context(stream->codecpar, ctx);
    ffmpeg::checkFFmpegResult(ret, "Could not copy codec parameters");

    stream->time_base = ctx->time_base;

    return stream;
}

AVStream* VideoEncoder::addAudioStream(int sampleRate, int channels)
{
    // Find AAC encoder
    const AVCodec* codec = ffmpeg::findEncoder(AV_CODEC_ID_AAC);
    if (!codec)
        throw std::runtime_error("AAC encoder not found");

    AVStream* stream = avformat_new_stream(formatCtx_.get(), nullptr);
    if (!stream)
        throw std::runtime_error("Failed to create audio stream");

    stream->id = formatCtx_->nb_streams - 1;

    // Create codec context
    audioCodecCtx_ = ffmpeg::makeCodecContext(codec);
    AVCodecContext* ctx = audioCodecCtx_.get();

    // AAC settings for YouTube
    ctx->sample_fmt = AV_SAMPLE_FMT_FLTP;  // Planar float
    ctx->sample_rate = sampleRate;
    ctx->channel_layout = channels == 2 ? AV_CH_LAYOUT_STEREO : AV_CH_LAYOUT_MONO;
    ctx->channels = channels;
    ctx->bit_rate = settings_.audioBitrate;

    // Container-specific flags
    if (formatCtx_->oformat->flags & AVFMT_GLOBALHEADER)
        ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    int ret = avcodec_open2(ctx, codec, nullptr);
    ffmpeg::checkFFmpegResult(ret, "Could not open audio codec");

    // Copy codec parameters to stream
    ret = avcodec_parameters_from_context(stream->codecpar, ctx);
    ffmpeg::checkFFmpegResult(ret, "Could not copy codec parameters");

    stream->time_base = AVRational{1, ctx->sample_rate};

    return stream;
}

void VideoEncoder::loadInputAudio()
{
    if (!audioFile_.existsAsFile())
        throw std::runtime_error("Audio file does not exist: " + audioFile_.getFullPathName().toStdString());

    // Use JUCE to load audio file
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(audioFile_));
    if (!reader)
        throw std::runtime_error("Could not read audio file: " + audioFile_.getFullPathName().toStdString());

    // Store audio info
    audioSampleRate_ = static_cast<int>(reader->sampleRate);
    audioNumChannels_ = static_cast<int>(reader->numChannels);
    audioDuration_ = reader->lengthInSamples / reader->sampleRate;

    // Read entire audio file into buffer
    audioBuffer_.setSize(audioNumChannels_, static_cast<int>(reader->lengthInSamples));
    reader->read(&audioBuffer_, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
}

void VideoEncoder::prepareThumbnail()
{
    // Ensure thumbnail is correct size for video
    if (thumbnail_.getWidth() != settings_.videoWidth ||
        thumbnail_.getHeight() != settings_.videoHeight)
    {
        // Resize thumbnail to match video dimensions
        juce::Image resized(juce::Image::RGB, settings_.videoWidth, settings_.videoHeight, true);
        juce::Graphics g(resized);
        g.drawImageWithin(thumbnail_, 0, 0, settings_.videoWidth, settings_.videoHeight,
                          juce::RectanglePlacement::centred);
        thumbnail_ = resized;
    }

    // Ensure RGB format
    if (thumbnail_.getFormat() != juce::Image::RGB)
    {
        thumbnail_ = thumbnail_.convertedToFormat(juce::Image::RGB);
    }
}

void VideoEncoder::encodeVideoFrames()
{
    // Calculate total number of video frames
    int totalFrames = static_cast<int>(audioDuration_ * settings_.frameRate);

    // Convert thumbnail to AVFrame once (static image)
    auto videoFrame = imageToAVFrame(thumbnail_);
    if (!videoFrame)
        throw std::runtime_error("Failed to convert image to AVFrame");

    auto packet = ffmpeg::makePacket();
    AVCodecContext* ctx = videoCodecCtx_.get();

    // Encode frames
    for (int frameNum = 0; frameNum < totalFrames && !threadShouldExit(); ++frameNum)
    {
        // Set frame PTS
        videoFrame->pts = nextVideoPts_++;

        // Send frame to encoder
        int ret = avcodec_send_frame(ctx, videoFrame.get());
        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
            ffmpeg::checkFFmpegResult(ret, "Error sending frame to video encoder");

        // Receive encoded packets
        while (ret >= 0)
        {
            ret = avcodec_receive_packet(ctx, packet.get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                break;
            }
            else if (ret < 0)
            {
                ffmpeg::checkFFmpegResult(ret, "Error receiving packet from video encoder");
            }

            writePacket(packet.get(), videoStream_);
            av_packet_unref(packet.get());
        }

        // Update progress (video encoding is ~50% of total work)
        float progress = (frameNum + 1.0f) / totalFrames * 0.5f;
        updateProgress(progress);

        if (onProgressChanged)
            onProgressChanged(progress);
    }

    // Flush video encoder
    flushEncoder(ctx, videoStream_);
}

void VideoEncoder::encodeAudioFrames()
{
    AVCodecContext* ctx = audioCodecCtx_.get();
    const int frameSize = ctx->frame_size;  // Samples per frame for AAC
    const int totalSamples = audioBuffer_.getNumSamples();
    int samplesEncoded = 0;

    auto audioFrame = ffmpeg::makeFrame();
    audioFrame->format = ctx->sample_fmt;
    audioFrame->channel_layout = ctx->channel_layout;
    audioFrame->channels = ctx->channels;
    audioFrame->sample_rate = ctx->sample_rate;
    audioFrame->nb_samples = frameSize;

    int ret = av_frame_get_buffer(audioFrame.get(), 0);
    ffmpeg::checkFFmpegResult(ret, "Could not allocate audio frame buffers");

    auto packet = ffmpeg::makePacket();

    while (samplesEncoded < totalSamples && !threadShouldExit())
    {
        int samplesToEncode = juce::jmin(frameSize, totalSamples - samplesEncoded);

        // Copy audio data from JUCE buffer to AVFrame
        // AAC uses planar float format (separate buffers for each channel)
        for (int ch = 0; ch < ctx->channels; ++ch)
        {
            float* dst = reinterpret_cast<float*>(audioFrame->data[ch]);
            const float* src = audioBuffer_.getReadPointer(juce::jmin(ch, audioBuffer_.getNumChannels() - 1),
                                                            samplesEncoded);
            memcpy(dst, src, samplesToEncode * sizeof(float));

            // Pad with silence if needed
            if (samplesToEncode < frameSize)
            {
                memset(dst + samplesToEncode, 0, (frameSize - samplesToEncode) * sizeof(float));
            }
        }

        audioFrame->pts = nextAudioPts_;
        nextAudioPts_ += frameSize;

        // Send frame to encoder
        ret = avcodec_send_frame(ctx, audioFrame.get());
        if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
            ffmpeg::checkFFmpegResult(ret, "Error sending frame to audio encoder");

        // Receive encoded packets
        while (ret >= 0)
        {
            ret = avcodec_receive_packet(ctx, packet.get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            {
                break;
            }
            else if (ret < 0)
            {
                ffmpeg::checkFFmpegResult(ret, "Error receiving packet from audio encoder");
            }

            writePacket(packet.get(), audioStream_);
            av_packet_unref(packet.get());
        }

        samplesEncoded += samplesToEncode;

        // Update progress (audio encoding is the remaining 50%)
        float progress = 0.5f + (static_cast<float>(samplesEncoded) / totalSamples * 0.5f);
        updateProgress(progress);

        if (onProgressChanged)
            onProgressChanged(progress);
    }

    // Flush audio encoder
    flushEncoder(ctx, audioStream_);
}

void VideoEncoder::flushEncoder(AVCodecContext* codecCtx, AVStream* stream)
{
    // Send NULL frame to signal end of stream
    int ret = avcodec_send_frame(codecCtx, nullptr);
    if (ret < 0)
        return;  // Already flushed or error

    auto packet = ffmpeg::makePacket();

    while (true)
    {
        ret = avcodec_receive_packet(codecCtx, packet.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;

        if (ret < 0)
            ffmpeg::checkFFmpegResult(ret, "Error flushing encoder");

        writePacket(packet.get(), stream);
        av_packet_unref(packet.get());
    }
}

void VideoEncoder::writePacket(AVPacket* packet, AVStream* stream)
{
    // Rescale packet timestamps to stream timebase
    av_packet_rescale_ts(packet,
                         stream == videoStream_ ? videoCodecCtx_->time_base : audioCodecCtx_->time_base,
                         stream->time_base);
    packet->stream_index = stream->index;

    // Write packet to output file (av_interleaved_write_frame handles interleaving)
    int ret = av_interleaved_write_frame(formatCtx_.get(), packet);
    if (ret < 0)
        ffmpeg::checkFFmpegResult(ret, "Error writing packet");
}

void VideoEncoder::finalizeOutput()
{
    // Write container trailer
    int ret = av_write_trailer(formatCtx_.get());
    ffmpeg::checkFFmpegResult(ret, "Error writing trailer");
}

ffmpeg::AVFramePtr VideoEncoder::imageToAVFrame(const juce::Image& image)
{
    const int width = image.getWidth();
    const int height = image.getHeight();

    // Allocate frame for YUV420P
    auto frame = ffmpeg::makeFrame();
    frame->format = AV_PIX_FMT_YUV420P;
    frame->width = width;
    frame->height = height;

    int ret = av_frame_get_buffer(frame.get(), 32);
    ffmpeg::checkFFmpegResult(ret, "Could not allocate video frame buffers");

    // Convert RGB to YUV420P manually
    // This is a simplified conversion - for production, use swscale
    juce::Image::BitmapData bitmap(image, juce::Image::BitmapData::readOnly);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            juce::Colour pixel = bitmap.getPixelColour(x, y);
            uint8_t r = pixel.getRed();
            uint8_t g = pixel.getGreen();
            uint8_t b = pixel.getBlue();

            // RGB to YUV conversion (BT.709)
            int yVal = (( 66 * r + 129 * g +  25 * b + 128) >> 8) +  16;
            int uVal = ((-38 * r -  74 * g + 112 * b + 128) >> 8) + 128;
            int vVal = ((112 * r -  94 * g -  18 * b + 128) >> 8) + 128;

            // Clamp values
            yVal = juce::jlimit(16, 235, yVal);
            uVal = juce::jlimit(16, 240, uVal);
            vVal = juce::jlimit(16, 240, vVal);

            // Y plane (full resolution)
            frame->data[0][y * frame->linesize[0] + x] = static_cast<uint8_t>(yVal);

            // U and V planes (subsampled 2x2)
            if (x % 2 == 0 && y % 2 == 0)
            {
                int uvX = x / 2;
                int uvY = y / 2;
                frame->data[1][uvY * frame->linesize[1] + uvX] = static_cast<uint8_t>(uVal);
                frame->data[2][uvY * frame->linesize[2] + uvX] = static_cast<uint8_t>(vVal);
            }
        }
    }

    return frame;
}

#endif  // VSTUPLOADER_FFMPEG_ENABLED

//==============================================================================
// Helper methods

void VideoEncoder::updateProgress(float newProgress)
{
    progress_.store(newProgress);
}

void VideoEncoder::updateStatus(const juce::String& message)
{
    {
        juce::ScopedLock lock(messageMutex_);
        statusMessage_ = message;
    }

    if (onStatusChanged)
        onStatusChanged(message);
}

void VideoEncoder::setError(const juce::String& error)
{
    juce::ScopedLock lock(messageMutex_);
    errorMessage_ = error;
}

double VideoEncoder::getAudioDuration() const
{
#ifdef VSTUPLOADER_FFMPEG_ENABLED
    return audioDuration_;
#else
    return 0.0;
#endif
}
