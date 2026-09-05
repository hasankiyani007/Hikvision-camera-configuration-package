/*
 * SimpleSoT - Bulletproof Source of Truth System
 * Bulletproof 12-field camera configuration system with atomic operations
 *
 * Key Features:
 * - 12 essential fields (removed overallPhase for individual status tracking)
 * - Atomic save operations with backup rotation
 * - No mutexes - single-threaded simplicity
 * - Crash-resistant with immediate password saving
 * - Resume capability from any interruption point
 */

#ifndef SIMPLE_SOT_H
#define SIMPLE_SOT_H

#include <windows.h>  // Include Windows headers first to define byte before std::byte
#include <string>
#include <vector>
#include <chrono>
#include <map>
#include "../jsoncpp/include/json/json.h"

using namespace std;

// Network configuration detected during startup
struct NetworkConfiguration {
    string gateway;              // Detected gateway IP
    string subnetMask;           // Detected subnet mask
    string networkRange;         // Base network range (e.g., "192.168.1")
    string baseStaticIP;         // Base IP for static assignment
    vector<string> availableIPs; // Pool of available IPs for assignment
    string detectedAt;           // Timestamp when detected
    bool isValid;                // Whether detection was successful

    NetworkConfiguration() {
        gateway = "";
        subnetMask = "";
        networkRange = "";
        baseStaticIP = "";
        availableIPs.clear();
        detectedAt = "";
        isValid = false;
    }

    void UpdateTimestamp() {
        auto now = chrono::system_clock::now();
        auto time_t = chrono::system_clock::to_time_t(now);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", gmtime(&time_t));
        detectedAt = string(timestamp) + "Z";
    }
};

// Simple camera record with only essential 13 fields
struct SimpleCameraRecord {
    // Core identifiers (2 fields)
    string serialNumber;        // Primary key - globally unique
    string macAddress;          // Secondary identifier for matching

    // Device info (1 field)
    string deviceModel;         // Device model string

    // Authentication (2 fields)
    string username;            // Always "admin" for HikVision
    string password;            // Generated or loaded password

    // Network config (1 field)
    string currentIP;           // Current working IP address

    // Streaming config (2 fields)
    string rtspMainStreamURL;   // RTSP main stream URL with credentials
    string rtspSubStreamURL;    // RTSP sub stream URL with credentials

    // Status tracking (5 fields)
    string activationStatus;    // not_started → password_generated → device_activated → login_verified
    string networkStatus;       // not_started → ip_assigned
    string dhcpStatus;          // enabled → disabled (separate from network config)
    string ntpStatus;           // not_set → set (NTP time synchronization)
    string streamingStatus;     // not_started → rtsp_configured → video_setup → stream_verified

    // Constructor with defaults
    SimpleCameraRecord() {
        username = "admin";
        password = "";
        currentIP = "";
        rtspMainStreamURL = "";
        rtspSubStreamURL = "";
        activationStatus = "NotActivated";
        networkStatus = "not_started";
        dhcpStatus = "enabled";  // Default: cameras start with DHCP enabled
        ntpStatus = "not_set";   // Default: NTP not configured
        streamingStatus = "not_started";
    }
};

// SimpleSoT metadata
struct SimpleSoTMetadata {
    string version;
    string lastUpdated;
    int totalCameras;
    string siteID;  // Backend site UUID (generated once, reused)

    SimpleSoTMetadata() {
        version = "1.0";
        totalCameras = 0;
        siteID = "";
        UpdateTimestamp();
    }

    void UpdateTimestamp() {
        auto now = chrono::system_clock::now();
        auto time_t = chrono::system_clock::to_time_t(now);
        auto ms = chrono::duration_cast<chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", gmtime(&time_t));
        lastUpdated = string(timestamp) + "." +
                     (ms.count() < 100 ? "0" : "") +
                     (ms.count() < 10 ? "0" : "") +
                     to_string(ms.count()) + "Z";
    }
};

class SimpleSoT {
private:
    string m_filePath;
    SimpleSoTMetadata m_metadata;
    vector<SimpleCameraRecord> m_cameras;
    NetworkConfiguration m_networkConfig;
    bool m_initialized;

    // Internal helper methods
    bool LoadFromFile();
    bool WriteToTempFile(const string& tempPath);
    bool ValidateFileContents(const string& filePath);
    void CreateBackupRotation();
    Json::Value CameraToJson(const SimpleCameraRecord& camera);
    SimpleCameraRecord JsonToCamera(const Json::Value& json);

public:
    SimpleSoT();
    ~SimpleSoT();

    // Core operations
    bool Initialize(const string& filePath);
    void Shutdown();

    // Camera management
    bool AddOrUpdateCamera(const SimpleCameraRecord& camera);
    bool FindCameraBySerial(const string& serialNumber, SimpleCameraRecord& camera);
    bool FindCameraByMac(const string& macAddress, SimpleCameraRecord& camera);
    bool UpdateCameraField(const string& serialNumber, const string& field, const string& value);
    bool RemoveCamera(const string& serialNumber);
    vector<SimpleCameraRecord> GetAllCameras();

    // Status updates
    bool UpdateActivationStatus(const string& serialNumber, const string& status);
    bool UpdateNetworkStatus(const string& serialNumber, const string& status);
    bool UpdateStreamingStatus(const string& serialNumber, const string& status);

    // Critical atomic operations
    bool AtomicSave();
    bool AtomicSaveAfterPasswordGeneration(const string& serialNumber, const string& password);
    bool AtomicSaveAfterActivation(const string& serialNumber);
    bool AtomicSaveAfterNetworkConfig(const string& serialNumber, const string& assignedIP);
    bool AtomicSaveAfterReconnection(const string& serialNumber, const string& currentIP);
    bool AtomicSaveAfterStreamingConfig(const string& serialNumber);
    bool AtomicSaveAfterCompletion(const string& serialNumber);

    // Discovery integration (MergeDiscoveredCameras removed - unified in CameraSharedLib)
    bool LoadExistingPassword(const string& serialNumber, string& password);
    bool LoadExistingConfig(const string& serialNumber, SimpleCameraRecord& camera);

    // Resume capability
    vector<SimpleCameraRecord> GetCamerasWithStatus(const string& statusField, const string& statusValue);

    // Network configuration management
    bool SaveNetworkConfiguration(const NetworkConfiguration& config);
    bool LoadNetworkConfiguration(NetworkConfiguration& config);
    bool HasValidNetworkConfiguration();
    NetworkConfiguration GetNetworkConfiguration() const;

    // Backup and recovery
    bool CreateBackup(const string& backupPath = "");
    bool RestoreFromBackup(const string& backupPath);
    vector<string> GetAvailableBackups();

    // Utility
    bool IsInitialized() const { return m_initialized; }
    string GetFilePath() const { return m_filePath; }
    string GetLastError() const { return m_lastError; }
    SimpleSoTMetadata GetMetadata() const { return m_metadata; }

    // Site ID management
    string GetSiteID() const { return m_metadata.siteID; }
    bool SetSiteID(const string& siteID);

private:
    string m_lastError;
    void SetError(const string& error);
};

// Global SimpleSoT instance for application-wide camera data management
extern SimpleSoT* g_SimpleSoT;

// C-style wrapper functions for external integration only
extern "C" {
    bool InitializeSimpleSoT(const char* filePath);
    void ShutdownSimpleSoT();
    bool UpdateCameraSimpleSoT(const char* serialNumber, const char* field, const char* value);
    bool LoadPasswordFromSimpleSoT(const char* serialNumber, char* passwordOut, int bufferSize);
    bool AtomicSaveSimpleSoT();
    const char* GetSimpleSoTError();
}

#endif // SIMPLE_SOT_H