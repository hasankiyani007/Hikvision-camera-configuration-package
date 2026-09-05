/*
 * Simplified HikVision Camera Setup Tool
 *
 * Streamlined workflow that integrates with HiTools delivery software:
 * 1. User pre-configures all cameras using HiTools (activation, IP assignment, NTP, etc.)
 * 2. User runs this tool which performs discovery and generates MediaMTX configuration
 * 3. User enters the universal password they set for all cameras
 * 4. Tool generates RTSP URLs and configures MediaMTX YAML file
 *
 * Uses EXACT same discovery and merge functions from the original CameraSharedLib
 */

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define byte win_byte_override

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <winhttp.h>

#undef byte
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <regex>
#include <fstream>
#include <limits>
#include <sstream>

// Include our simplified shared libraries (no SDK dependencies)
#include "shared/CameraSharedLib_Simplified.h"
#include "shared/SimpleSoT.h"
#include "shared/MediaMTXConfigManager_Simplified.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "winhttp.lib")

using namespace std;

class SimplifiedCameraSetup {
private:
    SimpleSoT* m_simpleSoT;
    MediaMTXConfigManager_Simplified* m_mediaMTXManager;
    string m_universalPassword;
    string m_dataDirectory;
    bool m_initialized;

public:
    SimplifiedCameraSetup()
        : m_simpleSoT(nullptr), m_mediaMTXManager(nullptr), m_initialized(false) {
        m_dataDirectory = GetExecutableDirectory() + "data";
    }

    ~SimplifiedCameraSetup() {
        Cleanup();
    }

    // Get directory where executable is located
    string GetExecutableDirectory() {
        char path[MAX_PATH];
        DWORD size = GetModuleFileName(nullptr, path, MAX_PATH);
        if (size == 0) {
            return "./"; // Fallback to current directory
        }

        string fullPath(path);
        size_t lastSlash = fullPath.find_last_of("\\/");
        if (lastSlash != string::npos) {
            return fullPath.substr(0, lastSlash + 1);
        }
        return "./";
    }

    bool Initialize() {
        cout << "========================================" << endl;
        cout << "   Hikvision Camera Setup Tool   " << endl;
        cout << "========================================" << endl;
        cout << endl;
        cout << "This tool works with cameras pre-configured using HiTools." << endl;
        cout << "Ensure all cameras are:" << endl;
        cout << "  - Activated with the same password" << endl;
        cout << "  - Assigned static IP addresses" << endl;
        cout << "  - NTP configured" << endl;
        cout << "  - Connected to the network" << endl;
        cout << endl;

        // Configure Windows Firewall for UDP multicast discovery
        

        // Create data directory
        CreateDirectoryA(m_dataDirectory.c_str(), NULL);

        // Initialize SimpleSoT
        m_simpleSoT = new SimpleSoT();
        string sotPath = m_dataDirectory + "/camera_discovery.json";
        if (!m_simpleSoT->Initialize(sotPath)) {
            cerr << "Failed to initialize SimpleSoT: " << m_simpleSoT->GetLastError() << endl;
            return false;
        }

        // Initialize global SimpleSoT for merge operations
        g_SimpleSoT = m_simpleSoT;

        // Initialize MediaMTX Config Manager
        m_mediaMTXManager = new MediaMTXConfigManager_Simplified();

        // Get executable directory to find mediamtx.yml
        string mediaMTXConfigPath = GetExecutableDirectory() + "mediamtx.yml";
        if (!m_mediaMTXManager->Initialize(mediaMTXConfigPath)) {
            cerr << "Failed to initialize MediaMTX config manager: " << m_mediaMTXManager->GetLastError() << endl;
            return false;
        }

        m_initialized = true;
        return true;
    }

    // Set password directly (for GUI wrapper or command-line arguments)
    bool SetPasswordDirectly(const string& password) {
        if (password.empty()) {
            cerr << "Password cannot be empty." << endl;
            return false;
        }
        m_universalPassword = password;
        cout << "Password configured." << endl;
        cout << endl;
        return true;
    }

    bool CollectUniversalPassword() {
        cout << "========================================" << endl;
        cout << "        Password Configuration          " << endl;
        cout << "========================================" << endl;
        cout << endl;
        cout << "Enter the universal password you set for all cameras using HiTools:" << endl;

        // Clear any leftover input buffer from previous operations
        cin.clear();
        cin.ignore(numeric_limits<streamsize>::max(), '\n');

        while (true) {
            cout << "Password: ";
            string password1;
            getline(cin, password1);

            if (password1.empty()) {
                cout << "Password cannot be empty. Please try again." << endl;
                continue;
            }

            cout << "Confirm password: ";
            string password2;
            getline(cin, password2);

            if (password1 != password2) {
                cout << "Passwords do not match. Please try again." << endl;
                continue;
            }

            m_universalPassword = password1;
            cout << "Password confirmed and saved." << endl;
            cout << endl;
            return true;
        }
    }

    bool PerformCameraDiscovery() {
        cout << "========================================" << endl;
        cout << "         Camera Discovery               " << endl;
        cout << "========================================" << endl;
        cout << endl;

        // Use EXACT same discovery function as original
        vector<CameraInfo> discoveredCameras;

        cout << "Starting camera discovery using SADP protocol..." << endl;
        if (!DiscoverCameras(discoveredCameras)) {
            cerr << "Failed to discover cameras using SADP protocol." << endl;
            return false;
        }

        cout << "Discovery completed. Found " << discoveredCameras.size() << " cameras." << endl;
        cout << endl;

        if (discoveredCameras.empty()) {
            cout << "No cameras found. Please ensure:" << endl;
            cout << "  - Cameras are powered on and connected to network" << endl;
            cout << "  - Cameras are on the same network segment" << endl;
            cout << "  - Firewall is not blocking multicast traffic" << endl;
            return false;
        }

        // Display discovered cameras
        DisplayDiscoveredCameras(discoveredCameras);

        // Use EXACT same merge function as original
        cout << "Merging discovered cameras with SimpleSoT database..." << endl;
        if (!MergeSimpleSoTWithDiscoveredCameras(discoveredCameras)) {
            cerr << "Failed to merge discovered cameras with database." << endl;
            return false;
        }

        cout << "Camera discovery and merge completed successfully." << endl;
        cout << endl;

        return true;
    }

    bool GenerateRTSPURLsForAllCameras() {
        cout << "========================================" << endl;
        cout << "       Generating RTSP URLs             " << endl;
        cout << "========================================" << endl;
        cout << endl;

        // Get all discovered cameras from SimpleSoT
        vector<SimpleCameraRecord> cameras = m_simpleSoT->GetAllCameras();
        if (cameras.empty()) {
            cerr << "No cameras found in database. Run discovery first." << endl;
            return false;
        }

        cout << "Generating RTSP URLs for " << cameras.size() << " cameras..." << endl;
        cout << endl;

        int processedCount = 0;
        for (auto& camera : cameras) {
            cout << "Processing camera " << (processedCount + 1) << "/" << cameras.size()
                 << " [" << camera.currentIP << "]..." << endl;

            // Set universal password
            camera.password = m_universalPassword;
            camera.username = "admin";

            // Generate RTSP URLs using standard HikVision format
            string baseURL = "rtsp://" + camera.username + ":" + camera.password + "@" + camera.currentIP + ":554";
            camera.rtspMainStreamURL = baseURL + "/Streaming/Channels/101";
            camera.rtspSubStreamURL = baseURL + "/Streaming/Channels/102";

            // Update camera in SimpleSoT
            if (!m_simpleSoT->AddOrUpdateCamera(camera)) {
                cerr << "Failed to update camera " << camera.currentIP << " in database." << endl;
                continue;
            }

            processedCount++;
            cout << "  ✓ RTSP URLs generated: " << camera.currentIP << endl;
            cout << "    Main: " << camera.rtspMainStreamURL << endl;
            cout << "    Sub:  " << camera.rtspSubStreamURL << endl;
        }

        // Save all changes to database
        if (!m_simpleSoT->AtomicSave()) {
            cerr << "Failed to save camera database." << endl;
            return false;
        }

        cout << endl;
        cout << "Successfully generated RTSP URLs for " << processedCount << "/" << cameras.size() << " cameras." << endl;
        cout << endl;

        return true;
    }

    bool GenerateMediaMTXConfiguration() {
        cout << "========================================" << endl;
        cout << "      MediaMTX Configuration           " << endl;
        cout << "========================================" << endl;
        cout << endl;

        // Get all discovered cameras from SimpleSoT
        vector<SimpleCameraRecord> cameras = m_simpleSoT->GetAllCameras();
        if (cameras.empty()) {
            cerr << "No cameras found in database. Run discovery first." << endl;
            return false;
        }

        cout << "Configuring MediaMTX for " << cameras.size() << " cameras..." << endl;
        cout << "Using exact SRT streaming configuration from plan:" << endl;
        cout << "  - Backend IP: 127.0.0.1 (localhost for testing)" << endl;
        cout << "  - SRT Port: 8890" << endl;
        cout << "  - RTSP Port: 8554" << endl;
        cout << "  - SRT Flags: streamid, latency=500, sndbuf=2097152, rcvbuf=2097152" << endl;
        cout << endl;

        // Configure all cameras using our simplified manager
        if (!m_mediaMTXManager->ConfigureAllCameras(cameras)) {
            cerr << "Failed to configure MediaMTX: " << m_mediaMTXManager->GetLastError() << endl;
            return false;
        }

        cout << "MediaMTX configuration completed successfully!" << endl;
        cout << "Configuration saved to: " << GetExecutableDirectory() << "mediamtx.yml" << endl;
        cout << endl;

        return true;
    }

    bool UploadSoTToBackend(const string& backendURL, const string& siteID) {
        cout << "========================================" << endl;
        cout << "   Uploading Camera Data to Backend    " << endl;
        cout << "========================================" << endl;
        cout << endl;

        // Read SoT file
        string sotFilePath = m_simpleSoT->GetFilePath();
        ifstream sotFile(sotFilePath);
        if (!sotFile.is_open()) {
            cerr << "Failed to open SoT file: " << sotFilePath << endl;
            return false;
        }

        stringstream buffer;
        buffer << sotFile.rdbuf();
        string sotContents = buffer.str();
        sotFile.close();

        if (sotContents.empty()) {
            cerr << "SoT file is empty" << endl;
            return false;
        }

        // Extract cameras array from SoT file (simple string manipulation)
        size_t camerasStart = sotContents.find("\"cameras\"");
        if (camerasStart == string::npos) {
            cerr << "Could not find cameras array in SoT file" << endl;
            return false;
        }

        size_t arrayStart = sotContents.find("[", camerasStart);
        if (arrayStart == string::npos) {
            cerr << "Malformed cameras array in SoT file" << endl;
            return false;
        }

        // Find matching closing bracket
        int bracketCount = 1;
        size_t arrayEnd = arrayStart + 1;
        while (arrayEnd < sotContents.length() && bracketCount > 0) {
            if (sotContents[arrayEnd] == '[') bracketCount++;
            else if (sotContents[arrayEnd] == ']') bracketCount--;
            arrayEnd++;
        }

        if (bracketCount != 0) {
            cerr << "Malformed cameras array - unmatched brackets" << endl;
            return false;
        }

        string camerasArray = sotContents.substr(arrayStart, arrayEnd - arrayStart);

        // Build JSON payload with site_id
        stringstream json;
        json << "{\"site_id\":\"" << siteID << "\",\"cameras\":" << camerasArray << "}";
        string jsonPayload = json.str();

        cout << "Uploading " << m_simpleSoT->GetAllCameras().size() << " cameras to backend..." << endl;
        cout << "Backend URL: " << backendURL << endl;
        cout << "Site ID: " << siteID << endl;
        cout << "Payload size: " << jsonPayload.length() << " bytes" << endl;
        cout << endl;

        // Parse URL (format: http://192.168.1.100:8000)
        wstring wBackendURL(backendURL.begin(), backendURL.end());

        URL_COMPONENTS urlComp;
        ZeroMemory(&urlComp, sizeof(urlComp));
        urlComp.dwStructSize = sizeof(urlComp);

        wchar_t szHostName[256];
        wchar_t szUrlPath[2048];

        urlComp.lpszHostName = szHostName;
        urlComp.dwHostNameLength = sizeof(szHostName) / sizeof(wchar_t);
        urlComp.lpszUrlPath = szUrlPath;
        urlComp.dwUrlPathLength = sizeof(szUrlPath) / sizeof(wchar_t);

        if (!WinHttpCrackUrl(wBackendURL.c_str(), 0, 0, &urlComp)) {
            cerr << "Failed to parse backend URL. Format: http://IP:PORT" << endl;
            return false;
        }

        // Open HTTP session
        HINTERNET hSession = WinHttpOpen(L"HikvisionCameraSetup/1.0",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS, 0);
        if (!hSession) {
            cerr << "Failed to initialize HTTP session" << endl;
            return false;
        }

        // Connect to server
        HINTERNET hConnect = WinHttpConnect(hSession, urlComp.lpszHostName,
                                            urlComp.nPort, 0);
        if (!hConnect) {
            cerr << "Failed to connect to " << backendURL << endl;
            WinHttpCloseHandle(hSession);
            return false;
        }

        // Open request
        wstring apiPath = L"/api/v1/cameras/import-sot";
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", apiPath.c_str(),
                                                nullptr, WINHTTP_NO_REFERER,
                                                WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
        if (!hRequest) {
            cerr << "Failed to create HTTP request" << endl;
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        // Add headers
        wstring headers = L"Content-Type: application/json\r\n";
        WinHttpAddRequestHeaders(hRequest, headers.c_str(), -1L, WINHTTP_ADDREQ_FLAG_ADD);

        // Send request
        BOOL bResults = WinHttpSendRequest(hRequest,
                                          WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                          (LPVOID)jsonPayload.c_str(), jsonPayload.length(),
                                          jsonPayload.length(), 0);

        if (!bResults) {
            cerr << "Failed to send HTTP request" << endl;
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        // Receive response
        bResults = WinHttpReceiveResponse(hRequest, nullptr);
        if (!bResults) {
            cerr << "Failed to receive HTTP response" << endl;
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            return false;
        }

        // Check status code
        DWORD dwStatusCode = 0;
        DWORD dwSize = sizeof(dwStatusCode);
        WinHttpQueryHeaders(hRequest,
                           WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                           WINHTTP_HEADER_NAME_BY_INDEX,
                           &dwStatusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);

        // Read response body
        string responseBody;
        DWORD dwDownloaded = 0;
        char respBuffer[4096];
        do {
            dwSize = 0;
            if (WinHttpQueryDataAvailable(hRequest, &dwSize) && dwSize > 0) {
                if (dwSize > sizeof(respBuffer)) dwSize = sizeof(respBuffer);
                ZeroMemory(respBuffer, sizeof(respBuffer));
                if (WinHttpReadData(hRequest, respBuffer, dwSize, &dwDownloaded)) {
                    responseBody.append(respBuffer, dwDownloaded);
                }
            }
        } while (dwSize > 0);

        // Cleanup
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);

        // Check result
        if (dwStatusCode == 200) {
            cout << "✓ Successfully uploaded camera data to backend!" << endl;
            cout << "  HTTP Status: " << dwStatusCode << endl;

            // Parse summary from response if available
            if (responseBody.find("\"created\"") != string::npos) {
                size_t createdPos = responseBody.find("\"created\":");
                size_t updatedPos = responseBody.find("\"updated\":");
                if (createdPos != string::npos && updatedPos != string::npos) {
                    cout << "  Backend processed cameras successfully" << endl;
                }
            }
            cout << endl;
            return true;
        } else {
            cerr << "✗ Backend returned error: HTTP " << dwStatusCode << endl;
            if (!responseBody.empty()) {
                cerr << "Response: " << responseBody.substr(0, 500) << endl;
            }
            cerr << endl;
            return false;
        }
    }


    void DisplaySummary() {
        cout << "========================================" << endl;
        cout << "            Setup Complete              " << endl;
        cout << "========================================" << endl;
        cout << endl;

        vector<SimpleCameraRecord> cameras = m_simpleSoT->GetAllCameras();

        cout << "Camera Setup Summary:" << endl;
        cout << "  Total cameras configured: " << cameras.size() << endl;
        cout << endl;

        cout << "Camera Details:" << endl;
        for (const auto& camera : cameras) {
            cout << "  Camera: " << camera.serialNumber << endl;
            cout << "    IP: " << camera.currentIP << endl;
            cout << "    Model: " << camera.deviceModel << endl;
            cout << "    Main Stream: " << camera.rtspMainStreamURL << endl;
            cout << "    Sub Stream: " << camera.rtspSubStreamURL << endl;
            cout << endl;
        }

        cout << "Next Steps:" << endl;
        cout << "1. MediaMTX configuration has been generated automatically" << endl;
        cout << "2. MediaMTX service runs automatically (HikvisionStreamingService)" << endl;
        cout << "3. Streams will be available via SRT to backend on port 8890" << endl;
        cout << "4. Camera data saved in: " << m_simpleSoT->GetFilePath() << endl;
        cout << endl;
    }

private:
    void DisplayDiscoveredCameras(const vector<CameraInfo>& cameras) {
        cout << "Discovered Cameras:" << endl;
        for (size_t i = 0; i < cameras.size(); i++) {
            const auto& camera = cameras[i];
            cout << "  " << (i + 1) << ". " << camera.currentIP
                 << " [" << camera.serialNumber << "]" << endl;
            cout << "     Model: " << camera.deviceModel << endl;
            cout << "     MAC: " << camera.macAddress << endl;
            cout << "     Activated: " << (camera.isActivated ? "Yes" : "No") << endl;
        }
        cout << endl;
    }


    void Cleanup() {
        if (m_mediaMTXManager) {
            m_mediaMTXManager->Shutdown();
            delete m_mediaMTXManager;
            m_mediaMTXManager = nullptr;
        }

        if (m_simpleSoT) {
            m_simpleSoT->Shutdown();
            delete m_simpleSoT;
            m_simpleSoT = nullptr;
        }

        // Clear global SimpleSoT reference
        g_SimpleSoT = nullptr;
    }
};

int main(int argc, char* argv[]) {
    // Initialize Windows Sockets
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "Failed to initialize Windows Sockets." << endl;
        return 1;
    }

    // Parse command-line arguments (only password and GUI mode)
    string password;
    bool interactiveMode = true;

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--password" && i + 1 < argc) {
            password = argv[i + 1];
            interactiveMode = false;
            i++; // Skip next argument
        } else if (arg == "--gui") {
            interactiveMode = false; // Don't wait for keypress at end
        }
    }

    // Hardcoded backend configuration
    const string BACKEND_URL = "http://localhost:8000";  // Change to your backend IP
    const string DEFAULT_SITE_ID = "00000000-0000-0000-0000-000000000001";  // Default site ID

    SimplifiedCameraSetup setup;

    if (!setup.Initialize()) {
        cerr << "Failed to initialize camera setup tool." << endl;
        WSACleanup();
        return 1;
    }

    // Main workflow
    bool success = true;

    // Step 1: Perform camera discovery using EXACT same functions
    if (success && !setup.PerformCameraDiscovery()) {
        success = false;
    }

    // Step 2: Collect universal password (interactive or from command line)
    if (success) {
        if (!password.empty()) {
            // Password provided via command line
            if (!setup.SetPasswordDirectly(password)) {
                success = false;
            }
        } else {
            // Interactive password collection
            if (!setup.CollectUniversalPassword()) {
                success = false;
            }
        }
    }

    // Step 3: Generate RTSP URLs and update camera records
    if (success && !setup.GenerateRTSPURLsForAllCameras()) {
        success = false;
    }

    // Step 4: Generate MediaMTX configuration
    if (success && !setup.GenerateMediaMTXConfiguration()) {
        success = false;
    }

    // Step 5: Upload SoT data to backend (automatically)
    if (success) {
        if (!setup.UploadSoTToBackend(BACKEND_URL, DEFAULT_SITE_ID)) {
            cout << "Warning: Failed to upload camera data to backend." << endl;
            cout << "Backend URL: " << BACKEND_URL << endl;
            cout << "Site ID: " << DEFAULT_SITE_ID << endl;
            cout << endl;
            cout << "You can manually upload the file later:" << endl;
            cout << "  File: data/camera_discovery.json" << endl;
            cout << "  API: POST " << BACKEND_URL << "/api/v1/cameras/import-sot" << endl;
            cout << endl;
            // Don't fail the entire setup if backend upload fails
        }
    }

    // Display summary
    if (success) {
        setup.DisplaySummary();
    } else {
        cerr << "Setup failed. Please check the error messages above." << endl;
    }

    WSACleanup();

    // Only wait for keypress in interactive mode
    if (interactiveMode) {
        cout << "Press any key to exit..." << endl;
        cin.get();
    }

    return success ? 0 : 1;
}