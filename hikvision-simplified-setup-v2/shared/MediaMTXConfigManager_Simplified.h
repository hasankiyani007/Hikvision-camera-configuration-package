/*
 * Simplified MediaMTX Configuration Manager
 *
 * Handles targeted YAML configuration updates for camera streaming:
 * 1. EnsureGlobalMediaMTXSettings() - One-time global config setup
 * 2. AddCameraToMediaMTXConfig() - Per-camera path configuration with SRT streaming
 *
 * Key Features:
 * - Targeted YAML updates (preserves existing settings)
 * - Single file operation (load → modify → save)
 * - Duplicate prevention for camera paths
 * - FFmpeg SRT streaming with exact flags from plan
 */

#ifndef MEDIAMTX_CONFIG_MANAGER_SIMPLIFIED_H
#define MEDIAMTX_CONFIG_MANAGER_SIMPLIFIED_H

#include <string>
#include <vector>
#include <map>

using namespace std;

// Forward declaration for SimpleCameraRecord
struct SimpleCameraRecord;

class MediaMTXConfigManager_Simplified {
private:
    string m_configPath;
    string m_backendIP;
    int m_srtPort;
    int m_rtspPort;
    bool m_initialized;

    // Internal helper methods
    bool LoadYAMLFile(const string& path, string& content);
    bool SaveYAMLFile(const string& path, const string& content);
    bool UpdateYAMLValue(string& yamlContent, const string& key, const string& value);
    bool AddCameraPath(string& yamlContent, const string& pathName, const SimpleCameraRecord& camera);
    bool CheckDuplicatePath(const string& yamlContent, const string& pathName);
    bool CheckDuplicateRTSPSource(const string& yamlContent, const string& rtspURL);
    string GenerateFFmpegSRTCommand(const string& cameraPath, const string& streamId);
    string SanitizePathName(const string& input);

public:
    MediaMTXConfigManager_Simplified();
    ~MediaMTXConfigManager_Simplified();

    // Initialization
    bool Initialize(const string& configPath, const string& backendIP = "127.0.0.1",
                   int srtPort = 8890, int rtspPort = 8554);
    void Shutdown();

    // Single atomic operation: Load → Apply all changes → Save once
    bool ConfigureAllCameras(const vector<SimpleCameraRecord>& cameras);

    // Utility functions (public)
    bool IsInitialized() const { return m_initialized; }
    string GetLastError() const { return m_lastError; }

    // Internal functions (now private for single file operation)
private:
    bool ApplyGlobalSettings(string& yamlContent);
    bool ApplyAllCameraPaths(string& yamlContent, const vector<SimpleCameraRecord>& cameras);

    string m_lastError;
    void SetError(const string& error);
};

#endif // MEDIAMTX_CONFIG_MANAGER_SIMPLIFIED_H