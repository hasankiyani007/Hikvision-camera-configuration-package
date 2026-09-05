/*
 * Simplified HikVision Camera Shared Library Header
 *
 * Simplified version that removes SDK dependencies but keeps:
 * - XMLSadpDiscovery function (pure UDP multicast, no SDK needed)
 * - CameraInfo structure (without SDK-specific fields)
 * - Discovery and merge functions
 */

#ifndef CAMERA_SHARED_LIB_SIMPLIFIED_H
#define CAMERA_SHARED_LIB_SIMPLIFIED_H

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <chrono>
#include "SimpleSoT.h"

using namespace std;

// No extern "C" needed for C++ only code

// Log levels (from original implementation)
enum class LogLevel {
    INFO_LEVEL = 0,
    WARN_LEVEL = 1,
    ERROR_LEVEL = 2,
    CRITICAL_LEVEL = 3
};

// Simplified Camera information structure (without SDK dependencies)
struct CameraInfo {
    string currentIP;
    WORD port;
    string serialNumber;
    string macAddress;
    string softwareVersion;
    string deviceModel;
    bool isActivated;
    bool isConnected;
    string username;
    string adminPassword;
    LogLevel lastSeverity;
    string lastError;

    // RTSP streaming URLs
    string rtspMainStreamURL;
    string rtspSubStreamURL;
    string onvifURL;

    // Status flags
    bool isStreaming;
    bool isConfigured;
    chrono::system_clock::time_point lastVerificationTime;

    // Constructor with defaults
    CameraInfo() {
        port = 8000;
        isActivated = false;
        isConnected = false;
        username = "admin";
        adminPassword = "";
        lastSeverity = LogLevel::INFO_LEVEL;
        isStreaming = false;
        isConfigured = false;
        lastVerificationTime = chrono::system_clock::now();
    }
};

// Core discovery functions (no SDK required)
bool DiscoverCameras(vector<CameraInfo>& cameras);
bool XMLSadpDiscovery(vector<CameraInfo>& cameras);
string GetLocalNetworkRange();
string ParseProbeMatchField(const string& xml, const string& fieldName);

// Merge function
bool MergeSimpleSoTWithDiscoveredCameras(vector<CameraInfo>& discoveredCameras);

// Logging and display functions
void LogMessage(LogLevel level, const string& message);
void DisplayCameraInfo(const CameraInfo& camera);
void DisplayDiscoveredCameras(const vector<CameraInfo>& cameras);

// Utility functions
string NormalizeMac(const string& input);
void GenerateRTSPURLs(CameraInfo& camera);
SimpleCameraRecord ConvertToSimpleCameraRecord(const CameraInfo& camera);

// End of header

#endif // CAMERA_SHARED_LIB_SIMPLIFIED_H