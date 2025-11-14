# VSTuploader Pro

**Upload beats directly to YouTube from within your DAW**

VSTuploader Pro is a VST3/AU plugin that streamlines the beat upload process for music producers. Drop your beat into the plugin, let it auto-parse metadata, and upload directly to YouTube - all without leaving your DAW.

## Target Users
Beat producers who waste 30+ minutes per upload dealing with manual YouTube processes. With VSTuploader Pro, go from beat completion to YouTube upload in under 2 minutes.

## Current Status: Week 1 MVP

### ✅ Completed Features (v0.2.0)
- ✅ Basic JUCE VST3/AU plugin shell
- ✅ Drag-drop audio file support (WAV, MP3, FLAC, AIFF)
- ✅ BPM reading from DAW host
- ✅ Modern, clean UI with visual feedback
- ✅ File validation and display
- ✅ **Paste & Parse** - Auto-extract metadata from YouTube descriptions:
  - BPM extraction (multiple formats: "BPM: 148", "148 BPM", etc.)
  - Social handles (Instagram, Twitter)
  - Email addresses
  - Usage/license terms detection
  - Hashtag and tag parsing
  - **Clean & Pack** - Automatic tag deduplication and 500-char limit enforcement
  - Separator detection (removes content after "IGNORE !")

### 🚧 Coming Soon (Week 1)
- YouTube OAuth authentication
- MP4 encoding (audio + static image)
- Basic YouTube upload functionality

### 🔮 Future Features
- Advanced metadata parsing
- Custom thumbnail support
- Upload queue management
- Analytics dashboard
- Batch upload support

## Technical Stack

- **Framework**: JUCE 8.0.1.0
- **Language**: C++20
- **Build System**: CMake 3.25+
- **Target Formats**: VST3, AU, Standalone
- **Platform Support**: Windows, macOS, Linux

## Project Structure

```
/VSTuploaderPro
  /source
    ├── PluginProcessor.cpp/h   - Audio processor with BPM reading
    ├── PluginEditor.cpp/h      - UI with drag-drop and Paste & Parse
    ├── MetadataParser.cpp/h    - Intelligent metadata extraction engine
  /Resources
    └── README.md               - Resource documentation
  /tests
    └── PluginBasics.cpp        - Unit tests
  /benchmarks
    └── Benchmarks.cpp          - Performance benchmarks
```

## Building

### Prerequisites
- CMake 3.25 or higher
- C++20 compatible compiler
- JUCE 8.0.1.0 (included as submodule)
- Git

### Initial Setup
```bash
# Clone with submodules
git clone --recursive https://github.com/yourusername/VSTuploader-Pro.git
cd VSTuploader-Pro

# If you forgot --recursive
git submodule update --init --recursive
```

### Build Commands

**Windows (Visual Studio):**
```bash
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

**macOS:**
```bash
cmake -B build -G Xcode
cmake --build build --config Release
```

**Linux:**
```bash
cmake -B build
cmake --build build --config Release
```

### Running Tests
```bash
cd build
ctest -C Release
```

## Usage

1. **Load the Plugin**: Open VSTuploader Pro in your DAW
2. **Drop Your Beat**: Drag and drop your audio file (WAV, MP3, FLAC, AIFF) into the plugin
3. **Check BPM**: The plugin automatically reads the BPM from your DAW host
4. **Paste & Parse**: Paste your YouTube description text and click "Paste & Parse"
   - Automatically extracts: BPM, Instagram, Twitter, Email, Usage terms, Tags
   - Removes duplicate tags and enforces YouTube's 500-char tag limit
   - Cleans up separator lines and content after "IGNORE !"
5. **Review Metadata**: Check the parsed metadata below the text box
6. **Ready to Upload**: (Coming soon) Click upload to send directly to YouTube

### Example: Paste & Parse

**Input:**
```
• BPM: 148
• Leave A Like If You Enjoyed 💯 🤝
• / bapebrazy
       Twitter:bapebrazyy
• Email For WAV, Exclusive, etc: Bapebrazy1@gmail.com
• [FREE] For NON-Profit Use, Credit Me! [prod.bapebrazy]

IGNORE !
_______________________________________________
hip hop beats, instrumental, ballout, #BankrollFreshTypeBeat #DrichTypeBeat
```

**Output:**
- BPM: 148
- Instagram: @bapebrazy
- Twitter: @bapebrazyy
- Email: Bapebrazy1@gmail.com
- Usage: [FREE] Non-profit use with credit required
- Tags: bankroll fresh type beat, drich type beat, hip hop beats, instrumental, ballout (cleaned & deduped)

## Development Roadmap

### Week 1 (Current)
- [x] Basic plugin shell with drag-drop
- [x] BPM reading from host
- [x] Paste & Parse metadata extraction
- [ ] YouTube OAuth integration
- [ ] MP4 encoding pipeline
- [ ] Basic YouTube upload

### Week 2-4
- [ ] Advanced metadata parsing
- [ ] Thumbnail customization
- [ ] Upload queue management
- [ ] Progress tracking
- [ ] Error handling and retry logic

### Beyond MVP
- [ ] Analytics dashboard
- [ ] Batch upload support
- [ ] Template system for metadata
- [ ] Integration with beat marketplaces

## Pricing Model

- **$4.99/month** subscription
- **First 1000 users**: Free access for life
- Additional features for premium tiers coming soon

## Built With

This project is built on [Pamplejuce](https://github.com/sudara/pamplejuce), a modern JUCE template with:
- CPM for dependency management
- Catch2 for testing
- GitHub Actions CI/CD
- Melatonin Inspector for UI debugging
- Cross-platform build support

## Contributing

This project is currently in active development. Contributions, issues, and feature requests are welcome!

## License

[Your License Here]

## Author

Built by an experienced audio plugin developer with a focus on solving real problems for beat producers.

---

**VSTuploader Pro** - Stop wasting time. Start uploading beats.
