# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

VSTuploader Pro is a JUCE-based audio plugin (VST3/AU/Standalone/CLAP) that enables beat producers to upload music directly to YouTube from within their DAW. Built on the Pamplejuce template, it combines JUCE's audio framework with modern C++20 development practices.

**Current Status**: Week 1 MVP (v0.2.0) - Basic plugin with drag-drop support, BPM reading, and intelligent metadata parsing from YouTube descriptions.

## Build System

### Initial Setup

**1. Clone with submodules:**
```bash
# Required for JUCE, melatonin_inspector, clap-juce-extensions
git submodule update --init --recursive
```

**2. Install vcpkg (for FFmpeg dependency):**
```bash
# Clone vcpkg to C:\vcpkg (Windows) or ~/vcpkg (macOS/Linux)
git clone https://github.com/microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg

# Bootstrap vcpkg
bootstrap-vcpkg.bat  # Windows
./bootstrap-vcpkg.sh # macOS/Linux
```

**3. Install FFmpeg via vcpkg:**
```bash
# Windows
C:\vcpkg\vcpkg install ffmpeg:x64-windows

# macOS
~/vcpkg/vcpkg install ffmpeg:x64-osx

# Linux
~/vcpkg/vcpkg install ffmpeg:x64-linux
```

### Build Commands

**Configure with vcpkg toolchain:**
```bash
# Windows
cmake -B build -G "Visual Studio 17 2022" ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release

# macOS
cmake -B build -G Xcode \
  -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release

# Linux
cmake -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
```

**Run tests:**
```bash
cd build
ctest -C Release --verbose --output-on-failure
```

**Run benchmarks:**
```bash
# Benchmarks are separate from tests
cd build
./Benchmarks_Release  # or platform-specific executable
```

### Build Output
- Plugins are built to `build/VSTuploaderPro_artefacts/Release/`
- On macOS, plugins are automatically copied to `~/Library/Audio/Plug-Ins/` after build
- Supported formats: VST3, AU, AUv3, Standalone, CLAP

## Architecture

### Component Hierarchy

**PluginProcessor** (`source/PluginProcessor.cpp/h`)
- JUCE AudioProcessor subclass
- Provides DAW integration: audio processing, state management, MIDI
- Exposes `getCurrentBPM()` and `isPlaying()` for reading host transport state
- Does NOT process audio (passthrough) - this is a utility plugin, not an effect

**PluginEditor** (`source/PluginEditor.cpp/h`)
- Main UI component, owns all child components
- Timer-based updates for BPM display (polls processor every 100ms)
- Manages workflow: file drop → metadata paste → parsing → display
- Coordinates between DragDropZone and MetadataParser

**DragDropZone** (`source/PluginEditor.h`)
- Custom Component implementing FileDragAndDropTarget
- Visual feedback for drag states (isDragging flag)
- Accepts audio files: WAV, MP3, FLAC, AIFF, OGG
- Callback pattern: `onFileDropped` function called when file is dropped

**MetadataParser** (`source/MetadataParser.cpp/h`)
- Core parsing engine for YouTube description text
- Extracts: BPM, social handles (Instagram/Twitter), email, usage terms, tags
- **Clean & Pack**: Deduplicates tags and enforces YouTube's 500-char limit
- Separator detection: removes content after "IGNORE !" or similar markers
- Returns structured `ParsedMetadata` object

**FFmpegWrappers** (`source/FFmpegWrappers.h`)
- RAII wrapper classes for FFmpeg C API
- Smart pointers with custom deleters: AVFormatContextPtr, AVCodecContextPtr, AVFramePtr, AVPacketPtr
- Factory functions for safe object creation
- Handles JUCE/FFmpeg preprocessor conflicts (DEBUG macro)

**FFmpegUtils** (`source/FFmpegUtils.h`)
- Error handling utilities: ffmpegErrorString, checkFFmpegResult
- Result<T> type for non-exception error handling
- Timestamp conversion utilities
- Codec and format validation helpers

### Data Flow

1. User drops audio file → DragDropZone captures → `onFileDropped` callback → PluginEditor stores file
2. User pastes description → TextEditor stores text
3. User clicks "Paste & Parse" → MetadataParser.parse() → ParsedMetadata returned
4. PluginEditor updates UI labels with parsed data
5. Timer polls processor for BPM updates from DAW host

### Key Dependencies

**Submodules** (must be initialized):
- `JUCE/` - JUCE framework (develop branch)
- `modules/melatonin_inspector/` - UI debugging tool
- `modules/clap-juce-extensions/` - CLAP format support
- `cmake/` - Pamplejuce CMake includes

**CMake Modules** (from `cmake/` submodule):
- `PamplejuceVersion.cmake` - Reads VERSION file
- `CPM.cmake` - Dependency management
- `JUCEDefaults.cmake` - Standard JUCE settings
- `Tests.cmake` - Catch2 test setup
- `Benchmarks.cmake` - Performance testing

## Development Patterns

### Source File Organization
- All plugin code in `source/` directory (lowercase convention)
- Files use CONFIGURE_DEPENDS glob in CMakeLists.txt (auto-detects new files)
- Tests in `tests/` use Catch2 framework
- Benchmarks in `benchmarks/` (separate target from tests)

### Preprocessor Definitions
- `CMAKE_BUILD_TYPE` - Debug or Release
- `VERSION` - From VERSION file (currently "0.0.1")
- `PRODUCT_NAME_WITHOUT_VERSION` - "VSTuploader Pro"
- `JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0` - Disabled by default

### JUCE Module Configuration
Linked modules (via SharedCode INTERFACE target):
- `juce_audio_utils` - Audio utilities and player
- `juce_audio_processors` - Plugin hosting
- `juce_dsp` - DSP utilities
- `juce_gui_basics` - Basic GUI components
- `juce_gui_extra` - Advanced GUI (FileDragAndDropTarget, etc.)

### Testing
- Tests use Catch2 (v3.x)
- Test helper in `tests/helpers/test_helpers.h`
- Pattern: Create PluginProcessor instance, test methods
- IPP tests conditionally compiled with `PAMPLEJUCE_IPP`

### Plugin Identification
- **Project Name**: VSTuploaderPro (no spaces, for CMake)
- **Product Name**: "VSTuploader Pro" (displayed in DAWs)
- **Bundle ID**: com.vstuploader.pro
- **Manufacturer Code**: Vstu
- **Plugin Code**: Vup1

## CI/CD

GitHub Actions workflow (`.github/workflows/build_and_test.yml`):
- Builds on Windows, macOS (universal binary), Linux
- Runs tests and benchmarks via ctest
- Runs pluginval validation (strictness level 10)
- macOS: Code signing with certificates, notarization
- Windows: Azure Trusted Signing
- Artifacts: .exe (Windows), .pkg (macOS), .zip (Linux)

## Common Issues

### Submodules Not Initialized
If build fails with missing JUCE:
```bash
git submodule update --init --recursive
```

### macOS Universal Binary
Uses `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` to build for both Apple Silicon and Intel.

### Windows IPP Support
Intel Performance Primitives can be installed via NuGet:
```bash
nuget install intelipp.static.win-x64 -Version 2022.2.0.575
```

### Plugin Not Showing in DAW
- Check that plugin was copied to correct location
- On macOS, verify `~/Library/Audio/Plug-Ins/VST3/` or `AU/`
- Check DAW's plugin scan settings

## MetadataParser Implementation Details

The MetadataParser is designed to handle messy, inconsistent YouTube description formats. Key algorithms:

**BPM Extraction**: Searches for patterns like "BPM: 148", "148 BPM", "148bpm" (case-insensitive)

**Social Handle Extraction**:
- Instagram: Looks for patterns like "IG:", "@username", "instagram.com/username"
- Twitter: Similar patterns, handles multiple variations
- Normalizes to @username format

**Tag Cleaning**:
- Deduplicates case-insensitively
- Removes '#' prefix from hashtags
- Packs into YouTube's 500-char limit
- Prioritizes by original order

**Separator Detection**: Stops parsing at lines containing "IGNORE !" or similar markers

## Future Development Notes

The codebase is structured to add these features next:
1. YouTube OAuth integration (will need juce_gui_extra web browser component)
2. MP4 encoding (audio + static image from Resources/)
3. YouTube Data API v3 upload endpoint
4. Upload queue management (likely in PluginProcessor for persistence)
