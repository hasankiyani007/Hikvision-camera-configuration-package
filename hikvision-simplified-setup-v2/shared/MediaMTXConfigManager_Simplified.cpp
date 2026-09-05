/*
 * Simplified MediaMTX Configuration Manager Implementation
 *
 * Implements targeted YAML updates for MediaMTX configuration according to your exact plan:
 * - Global settings: SRT yes, RTSP yes, forced TCP, default ports
 * - Per-camera paths with SRT streaming via FFmpeg runOnReady
 * - Exact SRT flags: streamid, latency=500, sndbuf=2097152, rcvbuf=2097152
 */

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define byte win_byte_override

#include <windows.h>

#undef byte

#include "MediaMTXConfigManager_Simplified.h"
#include "SimpleSoT.h"
#include "CameraSharedLib_Simplified.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>

using namespace std;

MediaMTXConfigManager_Simplified::MediaMTXConfigManager_Simplified()
    : m_initialized(false), m_srtPort(8890), m_rtspPort(8554), m_backendIP("127.0.0.1") {
}

MediaMTXConfigManager_Simplified::~MediaMTXConfigManager_Simplified() {
    Shutdown();
}

bool MediaMTXConfigManager_Simplified::Initialize(const string& configPath, const string& backendIP,
                                                int srtPort, int rtspPort) {
    m_configPath = configPath;
    m_backendIP = backendIP;
    m_srtPort = srtPort;
    m_rtspPort = rtspPort;

    // Verify config file exists
    ifstream file(m_configPath);
    if (!file.is_open()) {
        SetError("MediaMTX config file not found: " + m_configPath);
        return false;
    }
    file.close();

    m_initialized = true;
    LogMessage(LogLevel::INFO_LEVEL, "MediaMTX Config Manager initialized with config: " + m_configPath);
    return true;
}

void MediaMTXConfigManager_Simplified::Shutdown() {
    m_initialized = false;
}

// Single atomic operation: Load → Apply all changes → Save once
bool MediaMTXConfigManager_Simplified::ConfigureAllCameras(const vector<SimpleCameraRecord>& cameras) {
    if (!m_initialized) {
        SetError("MediaMTX Config Manager not initialized");
        return false;
    }

    LogMessage(LogLevel::INFO_LEVEL, "Configuring " + to_string(cameras.size()) + " cameras with single file operation...");

    // SINGLE FILE OPERATION: Load once
    string yamlContent;
    if (!LoadYAMLFile(m_configPath, yamlContent)) {
        return false;
    }

    bool modified = false;

    // Apply global settings
    if (ApplyGlobalSettings(yamlContent)) {
        modified = true;
        LogMessage(LogLevel::INFO_LEVEL, "Applied global MediaMTX settings");
    }

    // Apply all camera paths
    if (ApplyAllCameraPaths(yamlContent, cameras)) {
        modified = true;
        LogMessage(LogLevel::INFO_LEVEL, "Applied camera path configurations");
    }

    // SINGLE FILE OPERATION: Save once (only if modified)
    if (modified) {
        if (!SaveYAMLFile(m_configPath, yamlContent)) {
            return false;
        }
        LogMessage(LogLevel::INFO_LEVEL, "MediaMTX configuration updated successfully with single atomic operation");
    } else {
        LogMessage(LogLevel::INFO_LEVEL, "MediaMTX configuration already up to date");
    }

    return true;
}

// Internal: Apply global settings to YAML content (no file I/O)
bool MediaMTXConfigManager_Simplified::ApplyGlobalSettings(string& yamlContent) {
    bool modified = false;

    // Ensure SRT is enabled with default port
    if (UpdateYAMLValue(yamlContent, "srt", "yes")) {
        modified = true;
        LogMessage(LogLevel::INFO_LEVEL, "Set srt: yes");
    }
    if (UpdateYAMLValue(yamlContent, "srtAddress", ":8890")) {
        modified = true;
        LogMessage(LogLevel::INFO_LEVEL, "Set srtAddress: :8890");
    }

    // Ensure RTSP is enabled with default port and forced TCP
    if (UpdateYAMLValue(yamlContent, "rtsp", "yes")) {
        modified = true;
        LogMessage(LogLevel::INFO_LEVEL, "Set rtsp: yes");
    }
    if (UpdateYAMLValue(yamlContent, "rtspAddress", ":8554")) {
        modified = true;
        LogMessage(LogLevel::INFO_LEVEL, "Set rtspAddress: :8554");
    }
    if (UpdateYAMLValue(yamlContent, "rtspTransports", "[tcp]")) {
        modified = true;
        LogMessage(LogLevel::INFO_LEVEL, "Set rtspTransports: [tcp] (forced TCP locally)");
    }

    // Set global defaults for sourceOnDemand
    if (UpdateYAMLValue(yamlContent, "sourceOnDemand", "no")) {
        modified = true;
        LogMessage(LogLevel::INFO_LEVEL, "Set sourceOnDemand: no");
    }

    // Enable API for service monitoring
    if (UpdateYAMLValue(yamlContent, "api", "yes")) {
        modified = true;
        LogMessage(LogLevel::INFO_LEVEL, "Set api: yes (required for service monitoring)");
    }

    return modified;
}

// Internal: Apply all camera paths to YAML content (no file I/O)
bool MediaMTXConfigManager_Simplified::ApplyAllCameraPaths(string& yamlContent, const vector<SimpleCameraRecord>& cameras) {
    bool modified = false;
    int addedCount = 0;
    int skippedCount = 0;

    for (const auto& camera : cameras) {
        if (camera.serialNumber.empty() || camera.currentIP.empty()) {
            LogMessage(LogLevel::WARN_LEVEL, "Skipping camera with empty serial or IP");
            continue;
        }

        // Generate camera path names
        string mainPathName = "camera_" + SanitizePathName(camera.serialNumber) + "_main";
        string subPathName = "camera_" + SanitizePathName(camera.serialNumber) + "_sub";

        // Check for duplicates and add main stream (check RTSP URL, not path name)
        if (!CheckDuplicateRTSPSource(yamlContent, camera.rtspMainStreamURL)) {
            if (AddCameraPath(yamlContent, mainPathName, camera)) {
                modified = true;
                addedCount++;
                LogMessage(LogLevel::INFO_LEVEL, "Added main stream path: " + mainPathName + " [" + camera.rtspMainStreamURL + "]");
            }
        } else {
            skippedCount++;
            LogMessage(LogLevel::INFO_LEVEL, "Main stream RTSP URL already exists: " + camera.rtspMainStreamURL);
        }

        // Check for duplicates and add sub stream (check RTSP URL, not path name)
        if (!CheckDuplicateRTSPSource(yamlContent, camera.rtspSubStreamURL)) {
            SimpleCameraRecord subCamera = camera;
            subCamera.rtspMainStreamURL = camera.rtspSubStreamURL; // Use sub stream URL
            if (AddCameraPath(yamlContent, subPathName, subCamera)) {
                modified = true;
                addedCount++;
                LogMessage(LogLevel::INFO_LEVEL, "Added sub stream path: " + subPathName + " [" + camera.rtspSubStreamURL + "]");
            }
        } else {
            skippedCount++;
            LogMessage(LogLevel::INFO_LEVEL, "Sub stream RTSP URL already exists: " + camera.rtspSubStreamURL);
        }
    }

    LogMessage(LogLevel::INFO_LEVEL, "Camera paths: " + to_string(addedCount) + " added, " +
              to_string(skippedCount) + " skipped (duplicates)");

    return modified;
}

// Helper Methods

bool MediaMTXConfigManager_Simplified::LoadYAMLFile(const string& path, string& content) {
    ifstream file(path);
    if (!file.is_open()) {
        SetError("Failed to open YAML file: " + path);
        return false;
    }

    stringstream buffer;
    buffer << file.rdbuf();
    content = buffer.str();
    file.close();

    return true;
}

bool MediaMTXConfigManager_Simplified::SaveYAMLFile(const string& path, const string& content) {
    ofstream file(path);
    if (!file.is_open()) {
        SetError("Failed to save YAML file: " + path);
        return false;
    }

    file << content;
    file.close();

    return true;
}

bool MediaMTXConfigManager_Simplified::UpdateYAMLValue(string& yamlContent, const string& key, const string& value) {
    // Create regex pattern to find the key
    regex keyPattern("^(\\s*)" + key + "\\s*:\\s*(.*)$", regex::multiline);
    smatch matches;

    if (regex_search(yamlContent, matches, keyPattern)) {
        // Key exists, check if value needs updating
        string currentValue = matches[2].str();
        // Remove quotes and trim whitespace
        currentValue = regex_replace(currentValue, regex("^\\s*['\"]?|['\"]?\\s*$"), "");

        if (currentValue != value) {
            // Update the value
            string replacement = matches[1].str() + key + ": " + value;
            yamlContent = regex_replace(yamlContent, keyPattern, replacement);
            return true;
        }
    } else {
        // Key doesn't exist in global settings area, try to add it
        // Find a good location (after similar settings)
        string insertLocation;
        if (key == "srt" || key == "srtAddress") {
            insertLocation = "srt.*\n";
        } else if (key == "rtsp" || key == "rtspAddress" || key == "rtspTransports") {
            insertLocation = "rtsp.*\n";
        }

        if (!insertLocation.empty()) {
            regex insertPattern(insertLocation);
            if (regex_search(yamlContent, insertPattern)) {
                string newLine = key + ": " + value + "\n";
                yamlContent = regex_replace(yamlContent, insertPattern, "$&" + newLine);
                return true;
            }
        }
    }

    return false;
}

bool MediaMTXConfigManager_Simplified::AddCameraPath(string& yamlContent, const string& pathName,
                                                   const SimpleCameraRecord& camera) {
    string cameraConfig = "  " + pathName + ":\n";
    cameraConfig += "    source: " + camera.rtspMainStreamURL + "\n";
    cameraConfig += "    sourceOnDemand: no\n";

    // Generate FFmpeg command for SRT streaming
    // Backend requires: publish:srt_<name> format
    string streamId = "publish:srt_" + pathName;
    string ffmpegCmd = GenerateFFmpegSRTCommand(pathName, streamId);
    cameraConfig += "    runOnReady: " + ffmpegCmd + "\n";
    cameraConfig += "    runOnReadyRestart: yes\n";
    cameraConfig += "\n";

    // Find "paths:" line and insert camera config after it
    size_t pathsPos = yamlContent.find("\npaths:");
    if (pathsPos == string::npos) {
        pathsPos = yamlContent.find("paths:");
        if (pathsPos != 0 && pathsPos != string::npos) {
            pathsPos = string::npos; // paths: not at start of line
        }
    }

    if (pathsPos != string::npos) {
        // Find the end of the "paths:" line
        size_t pathsLineEnd = yamlContent.find('\n', pathsPos + 1);
        if (pathsLineEnd == string::npos) {
            pathsLineEnd = yamlContent.length();
        } else {
            pathsLineEnd++; // Include the newline
        }

        // Insert camera config right after the "paths:" line
        yamlContent.insert(pathsLineEnd, cameraConfig);
        return true;
    } else {
        // If paths section not found, add it at the end
        yamlContent += "\npaths:\n" + cameraConfig;
        return true;
    }
}

bool MediaMTXConfigManager_Simplified::CheckDuplicatePath(const string& yamlContent, const string& pathName) {
    regex pathPattern("^\\s*" + pathName + "\\s*:", regex::multiline);
    return regex_search(yamlContent, pathPattern);
}

bool MediaMTXConfigManager_Simplified::CheckDuplicateRTSPSource(const string& yamlContent, const string& rtspURL) {
    // Escape special regex characters in the RTSP URL
    string escapedURL = rtspURL;
    const string specialChars = "\\^$.|?*+()[]{}";
    for (char c : specialChars) {
        string charStr(1, c);
        string escapedChar = "\\" + charStr;
        size_t pos = 0;
        while ((pos = escapedURL.find(charStr, pos)) != string::npos) {
            escapedURL.replace(pos, 1, escapedChar);
            pos += 2;
        }
    }

    // Look for "source: rtspURL" pattern
    regex sourcePattern("^\\s*source:\\s*" + escapedURL + "\\s*$", regex::multiline);
    return regex_search(yamlContent, sourcePattern);
}

string MediaMTXConfigManager_Simplified::GenerateFFmpegSRTCommand(const string& cameraPath, const string& streamId) {
    // FFmpeg command with timeout flag and SRT configuration
    // IMPORTANT: Wrap SRT URL in double quotes to prevent shell interpretation of & characters
    stringstream cmd;
    cmd << "ffmpeg -rtsp_transport tcp";
    cmd << " -timeout 5000000";           // I/O timeout (5 seconds)
    cmd << " -i rtsp://127.0.0.1:" << m_rtspPort << "/" << cameraPath;
    cmd << " -c copy -f mpegts";
    cmd << " \"srt://" << m_backendIP << ":" << m_srtPort;
    cmd << "?streamid=" << streamId;
    cmd << "&latency=500";
    cmd << "&sndbuf=2097152";
    cmd << "&rcvbuf=2097152";
    cmd << "\"";  // Close quotes

    return cmd.str();
}

string MediaMTXConfigManager_Simplified::SanitizePathName(const string& input) {
    string result = input;
    // Replace non-alphanumeric characters with underscores
    result = regex_replace(result, regex("[^a-zA-Z0-9]"), "_");
    // Remove multiple consecutive underscores
    result = regex_replace(result, regex("_+"), "_");
    // Remove leading/trailing underscores
    result = regex_replace(result, regex("^_+|_+$"), "");
    return result;
}

void MediaMTXConfigManager_Simplified::SetError(const string& error) {
    m_lastError = error;
    LogMessage(LogLevel::ERROR_LEVEL, "MediaMTX Config Error: " + error);
}