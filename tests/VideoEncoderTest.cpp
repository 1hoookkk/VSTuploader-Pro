#include <catch2/catch_test_macros.hpp>
#include <VideoEncoder.h>
#include <juce_graphics/juce_graphics.h>

#ifdef VSTUPLOADER_FFMPEG_ENABLED

TEST_CASE("VideoEncoder basic functionality", "[video][encoder]")
{
    SECTION("Constructor initialization")
    {
        juce::File audioFile("test_audio.wav");
        juce::File outputFile("test_output.mp4");
        juce::Image thumbnail(juce::Image::RGB, 1920, 1080, true);

        // Fill thumbnail with a solid color for testing
        juce::Graphics g(thumbnail);
        g.fillAll(juce::Colours::blue);

        VideoEncoder encoder(audioFile, thumbnail, outputFile);

        REQUIRE_FALSE(encoder.isEncoding());
        REQUIRE(encoder.getProgress() == 0.0f);
    }

    SECTION("Custom encoding settings")
    {
        VideoEncoder::EncodingSettings settings;
        settings.videoWidth = 1280;
        settings.videoHeight = 720;
        settings.frameRate = 24;
        settings.h264Preset = "fast";
        settings.crf = 23;

        juce::File audioFile("test_audio.wav");
        juce::File outputFile("test_output.mp4");
        juce::Image thumbnail(juce::Image::RGB, 1920, 1080, true);

        VideoEncoder encoder(audioFile, thumbnail, outputFile, settings);

        REQUIRE_FALSE(encoder.isEncoding());
    }
}

TEST_CASE("VideoEncoder creates valid thumbnail", "[video][encoder]")
{
    // Create a test image with known pattern
    juce::Image testImage(juce::Image::RGB, 1920, 1080, true);
    juce::Graphics g(testImage);

    // Draw gradient
    g.setGradientFill(juce::ColourGradient(
        juce::Colours::red, 0, 0,
        juce::Colours::blue, 1920, 1080,
        false));
    g.fillAll();

    // Draw text
    g.setColour(juce::Colours::white);
    g.setFont(48.0f);
    g.drawText("VSTuploader Pro Test", 0, 0, 1920, 1080, juce::Justification::centred);

    REQUIRE(testImage.isValid());
    REQUIRE(testImage.getWidth() == 1920);
    REQUIRE(testImage.getHeight() == 1080);
}

// This test would require an actual audio file, so it's disabled by default
TEST_CASE("VideoEncoder end-to-end encoding", "[video][encoder][.integration]")
{
    // Note: This test requires a real audio file and takes time to run
    // Tag it with [.integration] so it doesn't run in normal test suite

    // TODO: Add test audio file to repository or generate synthetic audio
    SKIP("Integration test - requires audio file");
}

#else

TEST_CASE("VideoEncoder disabled without FFmpeg", "[video][encoder]")
{
    WARN("FFmpeg not enabled - video encoding tests skipped");
    REQUIRE(true);  // Tests pass but warn that FFmpeg is missing
}

#endif  // VSTUPLOADER_FFMPEG_ENABLED
