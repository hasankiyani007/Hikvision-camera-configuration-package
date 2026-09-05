/*
 * SimpleSoT - Bulletproof Source of Truth Implementation
 * Crash-resistant atomic save operations with backup rotation
 */

#include <windows.h>  // Include Windows headers first to define byte before std::byte
#include "SimpleSoT.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <ctime>
#include <iterator>
#include "CameraSharedLib_Simplified.h"

using namespace std;
namespace fs = std::filesystem;

// Global instance
SimpleSoT* g_SimpleSoT = nullptr;

SimpleSoT::SimpleSoT() : m_initialized(false) {
    m_lastError = "";
}

SimpleSoT::~SimpleSoT() {
    if (m_initialized) {
        Shutdown();
    }
}

bool SimpleSoT::Initialize(const string& filePath) {
    if (m_initialized) {
        SetError("SimpleSoT already initialized");
        return false;
    }

    m_filePath = filePath;

    // Ensure directory exists
    try {
        fs::path dir = fs::path(filePath).parent_path();
        LogMessage(LogLevel::INFO_LEVEL, "SoT Init: Directory path: " + dir.string());

        if (!dir.empty()) {
            if (!fs::exists(dir)) {
                LogMessage(LogLevel::INFO_LEVEL, "SoT Init: Creating directory: " + dir.string());
                fs::create_directories(dir);
                LogMessage(LogLevel::INFO_LEVEL, "SoT Init: Directory created successfully");
            } else {
                LogMessage(LogLevel::INFO_LEVEL, "SoT Init: Directory already exists");
            }
        }
    } catch (const exception& e) {
        SetError("Failed to create directory: " + string(e.what()));
        LogMessage(LogLevel::ERROR_LEVEL, "SoT Init: Directory creation failed: " + string(e.what()));
        return false;
    }

    // Load existing data or create new file
    if (fs::exists(filePath)) {
        if (!LoadFromFile()) {
            SetError("Failed to load existing SoT file");
            return false;
        }
    } else {
        // Create empty SoT file
        m_cameras.clear();
        m_metadata = SimpleSoTMetadata();
        if (!AtomicSave()) {
            SetError("Failed to create initial SoT file");
            return false;
        }
    }

    m_initialized = true;
    return true;
}

void SimpleSoT::Shutdown() {
    if (m_initialized) {
        // Final save before shutdown
        AtomicSave();
        m_initialized = false;
        m_cameras.clear();
    }
}

bool SimpleSoT::LoadFromFile() {
    try {
        ifstream file(m_filePath);
        if (!file.is_open()) {
            SetError("Cannot open SoT file for reading");
            return false;
        }

        Json::Value root;
        Json::Reader reader;

        if (!reader.parse(file, root)) {
            SetError("JSON parse error: " + reader.getFormattedErrorMessages());
            return false;
        }

        // Load metadata
        if (root.isMember("version")) {
            m_metadata.version = root["version"].asString();
        }
        if (root.isMember("lastUpdated")) {
            m_metadata.lastUpdated = root["lastUpdated"].asString();
        }

        // Load network configuration
        m_networkConfig = NetworkConfiguration(); // Reset to defaults
        if (root.isMember("networkConfiguration")) {
            const Json::Value& netConfig = root["networkConfiguration"];
            m_networkConfig.gateway = netConfig.get("gateway", "").asString();
            m_networkConfig.subnetMask = netConfig.get("subnetMask", "").asString();
            m_networkConfig.networkRange = netConfig.get("networkRange", "").asString();
            m_networkConfig.baseStaticIP = netConfig.get("baseStaticIP", "").asString();
            m_networkConfig.detectedAt = netConfig.get("detectedAt", "").asString();
            m_networkConfig.isValid = netConfig.get("isValid", false).asBool();

            if (netConfig.isMember("availableIPs") && netConfig["availableIPs"].isArray()) {
                m_networkConfig.availableIPs.clear();
                for (const auto& ip : netConfig["availableIPs"]) {
                    m_networkConfig.availableIPs.push_back(ip.asString());
                }
            }
        }

        // Load cameras
        m_cameras.clear();
        if (root.isMember("cameras") && root["cameras"].isArray()) {
            for (const auto& cameraJson : root["cameras"]) {
                SimpleCameraRecord camera = JsonToCamera(cameraJson);
                m_cameras.push_back(camera);
            }
        }

        m_metadata.totalCameras = m_cameras.size();
        return true;

    } catch (const exception& e) {
        SetError("Exception loading SoT file: " + string(e.what()));
        return false;
    }
}

bool SimpleSoT::WriteToTempFile(const string& tempPath) {
    try {
        Json::Value root;

        // Write metadata
        m_metadata.UpdateTimestamp();
        m_metadata.totalCameras = m_cameras.size();

        root["version"] = m_metadata.version;
        root["lastUpdated"] = m_metadata.lastUpdated;

        // Write network configuration
        Json::Value networkConfig;
        networkConfig["gateway"] = m_networkConfig.gateway;
        networkConfig["subnetMask"] = m_networkConfig.subnetMask;
        networkConfig["networkRange"] = m_networkConfig.networkRange;
        networkConfig["baseStaticIP"] = m_networkConfig.baseStaticIP;
        networkConfig["detectedAt"] = m_networkConfig.detectedAt;
        networkConfig["isValid"] = m_networkConfig.isValid;

        Json::Value availableIPs(Json::arrayValue);
        for (const auto& ip : m_networkConfig.availableIPs) {
            availableIPs.append(ip);
        }
        networkConfig["availableIPs"] = availableIPs;
        root["networkConfiguration"] = networkConfig;

        // Write cameras
        Json::Value camerasArray(Json::arrayValue);
        for (const auto& camera : m_cameras) {
            camerasArray.append(CameraToJson(camera));
        }
        root["cameras"] = camerasArray;

        // Write to temp file
        LogMessage(LogLevel::INFO_LEVEL, "SoT WriteTemp: Opening file: " + tempPath);
        ofstream file(tempPath);
        if (!file.is_open()) {
            SetError("Cannot create temp file: " + tempPath);
            LogMessage(LogLevel::ERROR_LEVEL, "SoT WriteTemp: Failed to open file: " + tempPath);
            return false;
        }
        LogMessage(LogLevel::INFO_LEVEL, "SoT WriteTemp: File opened successfully");

        Json::StreamWriterBuilder builder;
        builder["indentation"] = "  ";
        unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
        LogMessage(LogLevel::INFO_LEVEL, "SoT WriteTemp: Writing JSON content");
        writer->write(root, &file);

        LogMessage(LogLevel::INFO_LEVEL, "SoT WriteTemp: Closing file");
        file.close();

        if (file.fail()) {
            SetError("Failed to write or close temp file: " + tempPath);
            LogMessage(LogLevel::ERROR_LEVEL, "SoT WriteTemp: File write/close failed");
            return false;
        }

        LogMessage(LogLevel::INFO_LEVEL, "SoT WriteTemp: File written and closed successfully");
        return true;

    } catch (const exception& e) {
        SetError("Exception writing temp file: " + string(e.what()));
        return false;
    }
}

bool SimpleSoT::ValidateFileContents(const string& filePath) {
    try {
        ifstream file(filePath);
        if (!file.is_open()) {
            return false;
        }

        Json::Value root;
        Json::Reader reader;
        bool parseSuccess = reader.parse(file, root);
        file.close();

        return parseSuccess && root.isMember("cameras") && root.isMember("version");
    } catch (...) {
        return false;
    }
}

void SimpleSoT::CreateBackupRotation() {
    try {
        if (!fs::exists(m_filePath)) {
            return;
        }

        string backupDir = fs::path(m_filePath).parent_path().string() + "/backups";
        if (!fs::exists(backupDir)) {
            fs::create_directory(backupDir);
        }

        // Generate backup filename with timestamp
        auto now = chrono::system_clock::now();
        auto time_t = chrono::system_clock::to_time_t(now);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", localtime(&time_t));

        string backupPath = backupDir + "/sot_backup_" + string(timestamp) + ".json";

        // Copy current file to backup
        fs::copy_file(m_filePath, backupPath);

        // Keep only last 5 backups
        vector<fs::path> backups;
        for (const auto& entry : fs::directory_iterator(backupDir)) {
            if (entry.path().extension() == ".json" &&
                entry.path().filename().string().find("sot_backup_") == 0) {
                backups.push_back(entry.path());
            }
        }

        if (backups.size() > 5) {
            sort(backups.begin(), backups.end());
            for (size_t i = 0; i < backups.size() - 5; i++) {
                fs::remove(backups[i]);
            }
        }

    } catch (const exception& e) {
        // Backup failure shouldn't prevent save operation
        SetError("Backup warning: " + string(e.what()));
    }
}

Json::Value SimpleSoT::CameraToJson(const SimpleCameraRecord& camera) {
    Json::Value json;
    json["serialNumber"] = camera.serialNumber;
    json["macAddress"] = camera.macAddress;
    // originalIP field removed - using only currentIP
    json["deviceModel"] = camera.deviceModel;
    json["username"] = camera.username;
    json["password"] = camera.password;
    // assignedStaticIP field removed - using only currentIP
    json["currentIP"] = camera.currentIP;
    json["rtspMainStreamURL"] = camera.rtspMainStreamURL;
    json["rtspSubStreamURL"] = camera.rtspSubStreamURL;
    json["activationStatus"] = camera.activationStatus;
    json["networkStatus"] = camera.networkStatus;
    json["dhcpStatus"] = camera.dhcpStatus;
    json["ntpStatus"] = camera.ntpStatus;
    json["streamingStatus"] = camera.streamingStatus;
    return json;
}

SimpleCameraRecord SimpleSoT::JsonToCamera(const Json::Value& json) {
    SimpleCameraRecord camera;
    camera.serialNumber = json.get("serialNumber", "").asString();
    camera.macAddress = json.get("macAddress", "").asString();
    // originalIP field removed - using only currentIP
    camera.deviceModel = json.get("deviceModel", "").asString();
    camera.username = json.get("username", "admin").asString();
    camera.password = json.get("password", "").asString();
    // assignedStaticIP field removed - using only currentIP
    camera.currentIP = json.get("currentIP", "").asString();
    camera.rtspMainStreamURL = json.get("rtspMainStreamURL", "").asString();
    camera.rtspSubStreamURL = json.get("rtspSubStreamURL", "").asString();
    camera.activationStatus = json.get("activationStatus", "NotActivated").asString();
    camera.networkStatus = json.get("networkStatus", "not_started").asString();
    camera.dhcpStatus = json.get("dhcpStatus", "enabled").asString();
    camera.ntpStatus = json.get("ntpStatus", "not_set").asString();
    camera.streamingStatus = json.get("streamingStatus", "not_started").asString();
    return camera;
}

// ═══════════════════════════════════════════════════════════════
// CRITICAL ATOMIC SAVE OPERATIONS
// ═══════════════════════════════════════════════════════════════

bool SimpleSoT::AtomicSave() {
    // Allow AtomicSave during initialization (when m_filePath is set but m_initialized is false)
    if (!m_initialized && m_filePath.empty()) {
        SetError("SimpleSoT not initialized");
        return false;
    }

    try {
        // Step 1: Create backup of current file
        CreateBackupRotation();

        // Step 2: Write to temporary file
        string tempFile = m_filePath + ".tmp";
        LogMessage(LogLevel::INFO_LEVEL, "SoT Save: Writing to temp file: " + tempFile);
        if (!WriteToTempFile(tempFile)) {
            LogMessage(LogLevel::ERROR_LEVEL, "SoT Save: WriteToTempFile failed: " + m_lastError);
            return false;
        }
        LogMessage(LogLevel::INFO_LEVEL, "SoT Save: Temp file written successfully");

        // Step 3: Validate temporary file
        if (!ValidateFileContents(tempFile)) {
            fs::remove(tempFile);
            SetError("Temporary file validation failed");
            return false;
        }

        // Step 4: Atomic rename (transaction commit)
        fs::rename(tempFile, m_filePath);

        // Step 5: Final verification
        if (!ValidateFileContents(m_filePath)) {
            SetError("Final file validation failed");
            return false;
        }

        return true;

    } catch (const exception& e) {
        SetError("Atomic save failed: " + string(e.what()));
        return false;
    }
}

bool SimpleSoT::AtomicSaveAfterPasswordGeneration(const string& serialNumber, const string& password) {
    // CRITICAL: Save password BEFORE device activation attempt
    // Camera remains NotActivated until actual device activation
    if (UpdateCameraField(serialNumber, "password", password)) {
        return AtomicSave();
    }
    return false;
}

bool SimpleSoT::AtomicSaveAfterActivation(const string& serialNumber) {
    if (UpdateActivationStatus(serialNumber, "Activated")) {
        return AtomicSave();
    }
    return false;
}

bool SimpleSoT::AtomicSaveAfterNetworkConfig(const string& serialNumber, const string& assignedIP) {
    if (UpdateCameraField(serialNumber, "assignedStaticIP", assignedIP) &&
        UpdateNetworkStatus(serialNumber, "ip_assigned")) {
        return AtomicSave();
    }
    return false;
}

bool SimpleSoT::AtomicSaveAfterReconnection(const string& serialNumber, const string& currentIP) {
    if (UpdateCameraField(serialNumber, "currentIP", currentIP) &&
        UpdateNetworkStatus(serialNumber, "connection_verified")) {
        return AtomicSave();
    }
    return false;
}

bool SimpleSoT::AtomicSaveAfterStreamingConfig(const string& serialNumber) {
    if (UpdateStreamingStatus(serialNumber, "stream_verified")) {
        return AtomicSave();
    }
    return false;
}

bool SimpleSoT::AtomicSaveAfterCompletion(const string& serialNumber) {
    // No specific completion status needed - individual phases track themselves
    return AtomicSave();
}

// ═══════════════════════════════════════════════════════════════
// CAMERA MANAGEMENT OPERATIONS
// ═══════════════════════════════════════════════════════════════

bool SimpleSoT::AddOrUpdateCamera(const SimpleCameraRecord& camera) {
    if (!m_initialized) {
        SetError("SimpleSoT not initialized");
        return false;
    }

    // Find existing camera by serial number
    auto it = find_if(m_cameras.begin(), m_cameras.end(),
        [&camera](const SimpleCameraRecord& existing) {
            return existing.serialNumber == camera.serialNumber;
        });

    if (it != m_cameras.end()) {
        // Update existing camera
        *it = camera;
    } else {
        // Add new camera
        m_cameras.push_back(camera);
    }

    return true;
}

bool SimpleSoT::FindCameraBySerial(const string& serialNumber, SimpleCameraRecord& camera) {
    auto it = find_if(m_cameras.begin(), m_cameras.end(),
        [&serialNumber](const SimpleCameraRecord& cam) {
            return cam.serialNumber == serialNumber;
        });

    if (it != m_cameras.end()) {
        camera = *it;
        return true;
    }
    return false;
}

bool SimpleSoT::FindCameraByMac(const string& macAddress, SimpleCameraRecord& camera) {
    auto it = find_if(m_cameras.begin(), m_cameras.end(),
        [&macAddress](const SimpleCameraRecord& cam) {
            return cam.macAddress == macAddress;
        });

    if (it != m_cameras.end()) {
        camera = *it;
        return true;
    }
    return false;
}

bool SimpleSoT::UpdateCameraField(const string& serialNumber, const string& field, const string& value) {
    auto it = find_if(m_cameras.begin(), m_cameras.end(),
        [&serialNumber](const SimpleCameraRecord& cam) {
            return cam.serialNumber == serialNumber;
        });

    if (it == m_cameras.end()) {
        SetError("Camera not found: " + serialNumber);
        return false;
    }

    // Update the specified field
    if (field == "password") it->password = value;
    else if (field == "currentIP") it->currentIP = value;
    else if (field == "deviceModel") it->deviceModel = value;
    else if (field == "activationStatus") it->activationStatus = value;
    else if (field == "networkStatus") it->networkStatus = value;
    else if (field == "dhcpStatus") it->dhcpStatus = value;
    else if (field == "streamingStatus") it->streamingStatus = value;
    else if (field == "username") it->username = value;
    else if (field == "rtspMainStreamURL") it->rtspMainStreamURL = value;
    else if (field == "rtspSubStreamURL") it->rtspSubStreamURL = value;
    else {
        SetError("Unknown field: " + field);
        return false;
    }

    return true;
}

bool SimpleSoT::UpdateActivationStatus(const string& serialNumber, const string& status) {
    return UpdateCameraField(serialNumber, "activationStatus", status);
}

bool SimpleSoT::UpdateNetworkStatus(const string& serialNumber, const string& status) {
    return UpdateCameraField(serialNumber, "networkStatus", status);
}

bool SimpleSoT::UpdateStreamingStatus(const string& serialNumber, const string& status) {
    return UpdateCameraField(serialNumber, "streamingStatus", status);
}


vector<SimpleCameraRecord> SimpleSoT::GetAllCameras() {
    // Read fresh data directly from SoT file
    vector<SimpleCameraRecord> freshCameras;

    // Read entire file content to string (allows concurrent access)
    ifstream file(m_filePath, ios::in);
    if (!file.is_open()) {
        return freshCameras; // Return empty vector
    }

    // Read entire file content and close immediately
    string fileContent((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    file.close();

    Json::Value root;
    Json::Reader reader;

    if (!reader.parse(fileContent, root)) {
        return freshCameras; // Return empty vector
    }

    // Load cameras from file
    if (root.isMember("cameras") && root["cameras"].isArray()) {
        for (const auto& cameraJson : root["cameras"]) {
            SimpleCameraRecord camera = JsonToCamera(cameraJson);
            freshCameras.push_back(camera);
        }
    }

    return freshCameras;
}

// ═══════════════════════════════════════════════════════════════
// DISCOVERY INTEGRATION & RESUME CAPABILITY
// ═══════════════════════════════════════════════════════════════

// REMOVED: MergeDiscoveredCameras() - functionality moved to unified MergeSimpleSoTWithDiscoveredCameras()
// in CameraSharedLib.cpp for better efficiency and consistency

bool SimpleSoT::LoadExistingPassword(const string& serialNumber, string& password) {
    SimpleCameraRecord camera;
    if (FindCameraBySerial(serialNumber, camera) && !camera.password.empty()) {
        password = camera.password;
        return true;
    }
    return false;
}

bool SimpleSoT::LoadExistingConfig(const string& serialNumber, SimpleCameraRecord& camera) {
    return FindCameraBySerial(serialNumber, camera);
}


// ═══════════════════════════════════════════════════════════════
// NETWORK CONFIGURATION MANAGEMENT
// ═══════════════════════════════════════════════════════════════

bool SimpleSoT::SaveNetworkConfiguration(const NetworkConfiguration& config) {
    m_networkConfig = config;
    return AtomicSave();
}

bool SimpleSoT::LoadNetworkConfiguration(NetworkConfiguration& config) {
    if (!m_initialized) {
        SetError("SimpleSoT not initialized");
        return false;
    }
    config = m_networkConfig;
    return m_networkConfig.isValid;
}

bool SimpleSoT::HasValidNetworkConfiguration() {
    return m_networkConfig.isValid;
}

NetworkConfiguration SimpleSoT::GetNetworkConfiguration() const {
    return m_networkConfig;
}

// ═══════════════════════════════════════════════════════════════
// C-STYLE WRAPPER FUNCTIONS FOR COMPATIBILITY
// ═══════════════════════════════════════════════════════════════

extern "C" {

bool InitializeSimpleSoT(const char* filePath) {
    if (g_SimpleSoT) {
        delete g_SimpleSoT;
    }
    g_SimpleSoT = new SimpleSoT();
    return g_SimpleSoT->Initialize(string(filePath));
}

void ShutdownSimpleSoT() {
    if (g_SimpleSoT) {
        g_SimpleSoT->Shutdown();
        delete g_SimpleSoT;
        g_SimpleSoT = nullptr;
    }
}

// Removed SaveCameraDataToSimpleSoT - was unused dead code

bool UpdateCameraSimpleSoT(const char* serialNumber, const char* field, const char* value) {
    if (!g_SimpleSoT) return false;
    return g_SimpleSoT->UpdateCameraField(string(serialNumber), string(field), string(value));
}

bool LoadPasswordFromSimpleSoT(const char* serialNumber, char* passwordOut, int bufferSize) {
    if (!g_SimpleSoT) return false;

    string password;
    if (g_SimpleSoT->LoadExistingPassword(string(serialNumber), password)) {
        strncpy(passwordOut, password.c_str(), bufferSize - 1);
        passwordOut[bufferSize - 1] = '\0';
        return true;
    }
    return false;
}

bool AtomicSaveSimpleSoT() {
    if (!g_SimpleSoT) return false;
    return g_SimpleSoT->AtomicSave();
}

const char* GetSimpleSoTError() {
    if (!g_SimpleSoT) return "SimpleSoT not initialized";
    return g_SimpleSoT->GetLastError().c_str();
}

} // extern "C"

void SimpleSoT::SetError(const string& error) {
    m_lastError = error;
}