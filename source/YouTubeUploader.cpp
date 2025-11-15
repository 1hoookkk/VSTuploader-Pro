#include "YouTubeUploader.h"

YouTubeUploader::YouTubeUploader()
{
    // OAuth credentials (from Google Cloud Console)
    clientId = "1050057892387-dt2slvn63qa3mj0ipgrmhd7phfgr40rv.apps.googleusercontent.com";
    clientSecret = "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";
    redirectUri = "http://127.0.0.1:" + juce::String(callbackPort) + "/callback";

    // Try to load saved tokens
    loadTokens();
}

YouTubeUploader::~YouTubeUploader()
{
    if (callbackServer)
    {
        callbackServer->stop();
        callbackServer.reset();
    }
}

//==============================================================================
// OAuth Flow
//==============================================================================
void YouTubeUploader::startAuth()
{
    // Start the callback server first
    if (!callbackServer)
    {
        callbackServer = std::make_unique<OAuthCallbackServer>(*this);
        callbackServer->startThread();
    }

    // Wait a moment for server to start
    juce::Thread::sleep(100);

    // Build the OAuth consent URL
    auto authUrl = buildAuthUrl();

    // Open system browser to the consent URL
    juce::URL(authUrl).launchInDefaultBrowser();

    if (onError)
        onError("OAuth flow started. Please complete authorization in your browser.");
}

juce::String YouTubeUploader::buildAuthUrl() const
{
    juce::String baseUrl = "https://accounts.google.com/o/oauth2/v2/auth";

    juce::URL url(baseUrl);
    url = url.withParameter("client_id", clientId);
    url = url.withParameter("redirect_uri", redirectUri);
    url = url.withParameter("response_type", "code");
    url = url.withParameter("scope", "https://www.googleapis.com/auth/youtube.upload");
    url = url.withParameter("access_type", "offline");
    url = url.withParameter("prompt", "consent");

    return url.toString(true);
}

bool YouTubeUploader::exchangeCodeForToken(const juce::String& code)
{
    juce::URL tokenUrl("https://oauth2.googleapis.com/token");

    // Build POST data
    juce::StringPairArray postData;
    postData.set("code", code);
    postData.set("client_id", clientId);
    postData.set("client_secret", clientSecret);
    postData.set("redirect_uri", redirectUri);
    postData.set("grant_type", "authorization_code");

    // Make POST request
    auto stream = tokenUrl.createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withExtraHeaders("Content-Type: application/x-www-form-urlencoded")
            .withConnectionTimeoutMs(10000)
            .withPostData(postData.getDescription()));

    if (stream == nullptr)
    {
        if (onError)
            onError("Failed to connect to Google OAuth");
        return false;
    }

    auto response = stream->readEntireStreamAsString();
    auto json = juce::JSON::parse(response);

    if (!json.isObject())
    {
        if (onError)
            onError("Invalid response from Google OAuth");
        return false;
    }

    // Parse token response
    authToken.accessToken = json["access_token"].toString();
    authToken.refreshToken = json["refresh_token"].toString();
    authToken.tokenType = json["token_type"].toString();
    authToken.expiresIn = json["expires_in"];
    authToken.issuedAt = juce::Time::currentTimeMillis();

    if (authToken.accessToken.isEmpty())
    {
        if (onError)
            onError("Failed to obtain access token");
        return false;
    }

    // Save tokens
    saveTokens();

    // Fetch channel info
    if (!fetchChannelInfo())
    {
        if (onError)
            onError("Failed to fetch channel info");
        return false;
    }

    if (onAuthComplete)
        onAuthComplete(true, "Successfully connected to YouTube!");

    return true;
}

//==============================================================================
// OAuth Callback Server
//==============================================================================
YouTubeUploader::OAuthCallbackServer::OAuthCallbackServer(YouTubeUploader& uploader)
    : juce::Thread("OAuth Callback Server"),
      uploaderRef(uploader)
{
}

YouTubeUploader::OAuthCallbackServer::~OAuthCallbackServer()
{
    stop();
}

void YouTubeUploader::OAuthCallbackServer::run()
{
    // Create server socket
    serverSocket = std::make_unique<juce::StreamingSocket>();

    if (!serverSocket->createListener(callbackPort, "127.0.0.1"))
    {
        if (uploaderRef.onError)
            uploaderRef.onError("Failed to start OAuth callback server on port " + juce::String(callbackPort));
        return;
    }

    serverRunning = true;

    // Accept connections (timeout after 5 minutes)
    auto startTime = juce::Time::getMillisecondCounter();
    auto timeout = 5 * 60 * 1000; // 5 minutes

    while (!threadShouldExit() && serverRunning)
    {
        // Check timeout
        if (juce::Time::getMillisecondCounter() - startTime > timeout)
        {
            if (uploaderRef.onError)
                uploaderRef.onError("OAuth timeout - no response received within 5 minutes");
            break;
        }

        // Wait for connection (with short timeout to check threadShouldExit)
        auto clientSocket = serverSocket->waitForNextConnection();

        if (clientSocket != nullptr)
        {
            handleConnection(*clientSocket);
            delete clientSocket;

            // Stop server after handling one successful connection
            break;
        }

        // Small delay to avoid busy waiting
        juce::Thread::sleep(100);
    }

    serverRunning = false;
    serverSocket.reset();
}

void YouTubeUploader::OAuthCallbackServer::handleConnection(juce::StreamingSocket& clientSocket)
{
    // Read HTTP request
    juce::MemoryBlock buffer(1024);
    auto bytesRead = clientSocket.read(buffer.getData(), (int)buffer.getSize(), false);

    if (bytesRead <= 0)
    {
        sendResponse(clientSocket, "<html><body><h1>Error</h1><p>Invalid request</p></body></html>", 400);
        return;
    }

    juce::String request(static_cast<const char*>(buffer.getData()), (size_t)bytesRead);

    // Extract authorization code
    auto code = extractCodeFromRequest(request);

    if (code.isEmpty())
    {
        sendResponse(clientSocket,
            "<html><body style='font-family: sans-serif; text-align: center; padding: 50px;'>"
            "<h1>❌ Authorization Failed</h1>"
            "<p>No authorization code received.</p>"
            "<p>You can close this window.</p>"
            "</body></html>", 400);

        if (uploaderRef.onError)
            uploaderRef.onError("OAuth failed: No authorization code received");
        return;
    }

    // Exchange code for token (this is synchronous)
    bool success = uploaderRef.exchangeCodeForToken(code);

    if (success)
    {
        sendResponse(clientSocket,
            "<html><body style='font-family: sans-serif; text-align: center; padding: 50px;'>"
            "<h1>✅ Success!</h1>"
            "<p>VSTuploader Pro is now connected to YouTube.</p>"
            "<p>You can close this window and return to your DAW.</p>"
            "</body></html>");
    }
    else
    {
        sendResponse(clientSocket,
            "<html><body style='font-family: sans-serif; text-align: center; padding: 50px;'>"
            "<h1>❌ Authorization Failed</h1>"
            "<p>Failed to exchange authorization code for access token.</p>"
            "<p>Please try again in the plugin.</p>"
            "</body></html>", 400);
    }
}

juce::String YouTubeUploader::OAuthCallbackServer::extractCodeFromRequest(const juce::String& request)
{
    // Parse HTTP GET request
    // Format: "GET /callback?code=XXXXX&scope=... HTTP/1.1"

    auto lines = juce::StringArray::fromLines(request);
    if (lines.size() == 0)
        return {};

    auto firstLine = lines[0];

    // Find the query string
    auto queryStart = firstLine.indexOf("?");
    auto queryEnd = firstLine.indexOf(" HTTP");

    if (queryStart < 0 || queryEnd < 0 || queryEnd <= queryStart)
        return {};

    auto queryString = firstLine.substring(queryStart + 1, queryEnd);

    // Parse query parameters
    auto params = juce::StringArray::fromTokens(queryString, "&", "");

    for (const auto& param : params)
    {
        if (param.startsWith("code="))
        {
            return juce::URL::removeEscapeChars(param.substring(5));
        }
    }

    return {};
}

void YouTubeUploader::OAuthCallbackServer::sendResponse(juce::StreamingSocket& socket,
                                                        const juce::String& html,
                                                        int statusCode)
{
    juce::String statusText = (statusCode == 200) ? "OK" : "Bad Request";

    juce::String response =
        "HTTP/1.1 " + juce::String(statusCode) + " " + statusText + "\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Content-Length: " + juce::String(html.length()) + "\r\n"
        "Connection: close\r\n"
        "\r\n" +
        html;

    socket.write(response.toRawUTF8(), (int)response.getNumBytesAsUTF8());
}

void YouTubeUploader::OAuthCallbackServer::stop()
{
    serverRunning = false;
    signalThreadShouldExit();
    stopThread(2000);
}

//==============================================================================
// Token Refresh
//==============================================================================
bool YouTubeUploader::refreshAccessToken()
{
    if (authToken.refreshToken.isEmpty())
    {
        if (onError)
            onError("No refresh token available");
        return false;
    }

    juce::URL tokenUrl("https://oauth2.googleapis.com/token");

    juce::StringPairArray postData;
    postData.set("refresh_token", authToken.refreshToken);
    postData.set("client_id", clientId);
    postData.set("client_secret", clientSecret);
    postData.set("grant_type", "refresh_token");

    auto stream = tokenUrl.createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withExtraHeaders("Content-Type: application/x-www-form-urlencoded")
            .withConnectionTimeoutMs(10000)
            .withPostData(postData.getDescription()));

    if (stream == nullptr)
        return false;

    auto response = stream->readEntireStreamAsString();
    auto json = juce::JSON::parse(response);

    if (!json.isObject())
        return false;

    authToken.accessToken = json["access_token"].toString();
    authToken.expiresIn = json["expires_in"];
    authToken.issuedAt = juce::Time::currentTimeMillis();

    saveTokens();

    return authToken.accessToken.isNotEmpty();
}

//==============================================================================
// Channel Info
//==============================================================================
bool YouTubeUploader::fetchChannelInfo()
{
    if (!authToken.isValid() && !refreshAccessToken())
        return false;

    juce::URL channelUrl("https://www.googleapis.com/youtube/v3/channels");
    channelUrl = channelUrl.withParameter("part", "snippet")
                           .withParameter("mine", "true");

    auto response = makeAuthenticatedRequest(channelUrl, "GET");

    if (!response.isObject())
        return false;

    auto items = response["items"];
    if (!items.isArray() || items.size() == 0)
        return false;

    auto channel = items[0];
    channelInfo.channelId = channel["id"].toString();
    channelInfo.channelName = channel["snippet"]["title"].toString();

    auto thumbnails = channel["snippet"]["thumbnails"];
    if (thumbnails.isObject() && thumbnails["default"].isObject())
        channelInfo.thumbnailUrl = thumbnails["default"]["url"].toString();

    return true;
}

//==============================================================================
// Upload
//==============================================================================
bool YouTubeUploader::uploadVideo(const juce::File& audioFile,
                                  const juce::File& thumbnailFile,
                                  const YouTubeUploadMetadata& metadata,
                                  std::function<void(float)> progressCallback)
{
    if (!authToken.isValid() && !refreshAccessToken())
    {
        if (onError)
            onError("Not authenticated. Please connect to YouTube first.");
        return false;
    }

    // TODO: Implement video upload
    // This requires:
    // 1. Encode audio + thumbnail to MP4
    // 2. Upload video file to YouTube
    // 3. Set metadata (title, description, tags, etc.)

    if (onError)
        onError("Video upload not yet implemented. Coming soon!");

    return false;
}

//==============================================================================
// HTTP Helpers
//==============================================================================
juce::var YouTubeUploader::makeAuthenticatedRequest(const juce::URL& url,
                                                    const juce::String& method,
                                                    const juce::var& payload)
{
    if (!authToken.isValid() && !refreshAccessToken())
        return juce::var();

    juce::String authHeader = "Authorization: Bearer " + authToken.accessToken;

    auto stream = url.createInputStream(
        juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
            .withExtraHeaders(authHeader)
            .withConnectionTimeoutMs(10000));

    if (stream == nullptr)
        return juce::var();

    auto response = stream->readEntireStreamAsString();
    return juce::JSON::parse(response);
}

//==============================================================================
// Authentication State
//==============================================================================
bool YouTubeUploader::isAuthenticated() const
{
    return authToken.accessToken.isNotEmpty() || authToken.refreshToken.isNotEmpty();
}

void YouTubeUploader::disconnect()
{
    authToken = YouTubeAuthToken();
    channelInfo = YouTubeChannelInfo();
    deleteStoredRefreshToken();

    // Stop callback server if running
    if (callbackServer)
    {
        callbackServer->stop();
        callbackServer.reset();
    }
}

//==============================================================================
// Token Storage
//==============================================================================
void YouTubeUploader::saveTokens()
{
    // Save refresh token to secure storage
    if (authToken.refreshToken.isNotEmpty())
        storeRefreshToken(authToken.refreshToken);

    // Save other token data to app data
    juce::File tokenFile = juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory)
        .getChildFile("VSTuploaderPro")
        .getChildFile("youtube_token.json");

    tokenFile.getParentDirectory().createDirectory();

    juce::var tokenData(new juce::DynamicObject());
    tokenData.getDynamicObject()->setProperty("access_token", authToken.accessToken);
    tokenData.getDynamicObject()->setProperty("token_type", authToken.tokenType);
    tokenData.getDynamicObject()->setProperty("expires_in", authToken.expiresIn);
    tokenData.getDynamicObject()->setProperty("issued_at", (int64)authToken.issuedAt);
    tokenData.getDynamicObject()->setProperty("channel_id", channelInfo.channelId);
    tokenData.getDynamicObject()->setProperty("channel_name", channelInfo.channelName);

    tokenFile.replaceWithText(juce::JSON::toString(tokenData));
}

void YouTubeUploader::loadTokens()
{
    // Load refresh token from secure storage
    authToken.refreshToken = getStoredRefreshToken();

    // Load other token data from app data
    juce::File tokenFile = juce::File::getSpecialLocation(
        juce::File::userApplicationDataDirectory)
        .getChildFile("VSTuploaderPro")
        .getChildFile("youtube_token.json");

    if (!tokenFile.existsAsFile())
        return;

    auto json = juce::JSON::parse(tokenFile);
    if (!json.isObject())
        return;

    authToken.accessToken = json["access_token"].toString();
    authToken.tokenType = json["token_type"].toString();
    authToken.expiresIn = json["expires_in"];
    authToken.issuedAt = (juce::int64)json["issued_at"];

    channelInfo.channelId = json["channel_id"].toString();
    channelInfo.channelName = json["channel_name"].toString();

    // If we have a refresh token but expired access token, refresh it
    if (authToken.refreshToken.isNotEmpty() && authToken.isExpired())
    {
        refreshAccessToken();
        if (!channelInfo.channelId.isEmpty())
            fetchChannelInfo(); // Re-fetch if we had channel info before
    }
}

//==============================================================================
// Platform-specific token storage
//==============================================================================
juce::String YouTubeUploader::getStoredRefreshToken() const
{
    // TODO: Implement platform-specific secure storage
    // Windows: Use Credential Manager (CredRead/CredWrite)
    // macOS: Use Keychain Services
    // Linux: Use Secret Service API

    // For now, return empty (tokens will only persist in JSON file)
    return juce::String();
}

void YouTubeUploader::storeRefreshToken(const juce::String& token)
{
    // TODO: Implement platform-specific secure storage
    // For now, do nothing (refresh token won't be securely stored)
    juce::ignoreUnused(token);
}

void YouTubeUploader::deleteStoredRefreshToken()
{
    // TODO: Implement platform-specific secure storage deletion
}
