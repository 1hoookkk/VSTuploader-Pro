#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
// OAuth token data
struct YouTubeAuthToken
{
    juce::String accessToken;
    juce::String refreshToken;
    juce::String tokenType;
    int expiresIn = 0;
    juce::int64 issuedAt = 0; // Timestamp when token was issued

    bool isExpired() const
    {
        auto now = juce::Time::currentTimeMillis();
        auto expiryTime = issuedAt + (expiresIn * 1000) - 60000; // 1 min buffer
        return now >= expiryTime;
    }

    bool isValid() const
    {
        return accessToken.isNotEmpty() && !isExpired();
    }
};

//==============================================================================
// YouTube channel info
struct YouTubeChannelInfo
{
    juce::String channelId;
    juce::String channelName;
    juce::String thumbnailUrl;
};

//==============================================================================
// Upload metadata
struct YouTubeUploadMetadata
{
    juce::String title;
    juce::String description;
    juce::StringArray tags;
    juce::String privacy; // "public", "unlisted", "private"
    juce::String categoryId; // "10" for Music
};

//==============================================================================
// YouTube uploader with OAuth support
class YouTubeUploader
{
public:
    YouTubeUploader();
    ~YouTubeUploader();

    // OAuth methods
    void startAuth();
    bool isAuthenticated() const;
    void disconnect();

    // Channel info
    YouTubeChannelInfo getChannelInfo() const { return channelInfo; }

    // Upload methods
    bool uploadVideo(const juce::File& audioFile,
                    const juce::File& thumbnailFile,
                    const YouTubeUploadMetadata& metadata,
                    std::function<void(float)> progressCallback);

    // Token management
    bool refreshAccessToken();
    void saveTokens();
    void loadTokens();

    // Callbacks
    std::function<void(bool success, juce::String message)> onAuthComplete;
    std::function<void(juce::String error)> onError;

private:
    // OAuth callback server
    class OAuthCallbackServer : public juce::Thread
    {
    public:
        OAuthCallbackServer(YouTubeUploader& uploader);
        ~OAuthCallbackServer() override;

        void run() override;
        void stop();

        bool isRunning() const { return serverRunning; }

    private:
        YouTubeUploader& uploaderRef;
        std::unique_ptr<juce::StreamingSocket> serverSocket;
        bool serverRunning = false;

        void handleConnection(juce::StreamingSocket& clientSocket);
        juce::String extractCodeFromRequest(const juce::String& request);
        void sendResponse(juce::StreamingSocket& socket, const juce::String& html, int statusCode = 200);
    };

    std::unique_ptr<OAuthCallbackServer> callbackServer;

    // OAuth configuration
    juce::String clientId;
    juce::String clientSecret;
    juce::String redirectUri;
    static constexpr int callbackPort = 5173;

    // Token storage
    YouTubeAuthToken authToken;
    YouTubeChannelInfo channelInfo;

    // OAuth helpers
    juce::String buildAuthUrl() const;
    bool exchangeCodeForToken(const juce::String& code);
    bool fetchChannelInfo();

    // HTTP helpers
    juce::var makeAuthenticatedRequest(const juce::URL& url,
                                       const juce::String& method,
                                       const juce::var& payload = juce::var());

    // Token storage (Windows Credential Manager / macOS Keychain)
    juce::String getStoredRefreshToken() const;
    void storeRefreshToken(const juce::String& token);
    void deleteStoredRefreshToken();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (YouTubeUploader)
};
