/*
 * Simplified Camera Shared Library - Working Functions Only
 *
 * Contains EXACT working code copied from original implementation:
 * - Discovery functions (XMLSadpDiscovery, DiscoverCameras)
 * - Deduplication logic
 * - SimpleSoT merge functions
 */

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define byte win_byte_override

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#undef byte

#include "CameraSharedLib_Simplified.h"
#include "SimpleSoT.h"
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <ctime>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

using namespace std;

// Global SimpleSoT instance (defined in SimpleSoT.cpp)

// COPIED EXACT WORKING FUNCTIONS FROM ORIGINAL

// MAC address normalization (EXACT COPY)
string NormalizeMac(const string& input) {
    string normalized;
    for (char c : input) {
        if (c != ':' && c != '-' && c != ' ') {
            normalized += tolower(c);
        }
    }
    return normalized;
}

// Logging function (simplified version)
void LogMessage(LogLevel level, const string& message) {
    cout << "[" << (level == LogLevel::INFO_LEVEL ? "INFO" :
                   level == LogLevel::WARN_LEVEL ? "WARN" :
                   level == LogLevel::ERROR_LEVEL ? "ERROR" : "DEBUG")
         << "] " << message << endl;
}

// EXACT COPY: Discovery functions from original
bool DiscoverCameras(vector<CameraInfo>& cameras) {
    LogMessage(LogLevel::INFO_LEVEL, "Starting enhanced discovery with repetition for better reliability...");

    // Method 1: XML SADP multicast (Universal Scanner approach) - Run TWICE for better results
    LogMessage(LogLevel::INFO_LEVEL, "Method 1: XML SADP multicast discovery (Universal Scanner method) - First Pass");
    vector<CameraInfo> firstPassCameras;
    bool firstPassSuccess = XMLSadpDiscovery(firstPassCameras);

    if (firstPassSuccess && !firstPassCameras.empty()) {
        LogMessage(LogLevel::INFO_LEVEL, "First pass XML SADP discovery successful - found " +
                  to_string(firstPassCameras.size()) + " cameras, running second pass...");

        // Wait a moment before second pass
        this_thread::sleep_for(chrono::milliseconds(500));

        // Second pass to catch any cameras that didn't respond initially
        LogMessage(LogLevel::INFO_LEVEL, "Method 1: XML SADP multicast discovery - Second Pass");
        vector<CameraInfo> secondPassCameras;
        if (XMLSadpDiscovery(secondPassCameras)) {
            LogMessage(LogLevel::INFO_LEVEL, "Second pass XML SADP discovery successful - found " +
                      to_string(secondPassCameras.size()) + " cameras");
        }

        // Merge results - filter duplicates BETWEEN passes
        cameras = firstPassCameras;
        int secondPassUnique = 0;

        for (const auto& secondCamera : secondPassCameras) {
            // Check if this camera already exists in first pass results
            // Use AND logic: BOTH MAC and serial must match (consistent with SoT merge)
            bool isDuplicateAcrossPasses = false;
            for (const auto& firstCamera : firstPassCameras) {
                if (!secondCamera.macAddress.empty() && !firstCamera.macAddress.empty() &&
                    !secondCamera.serialNumber.empty() && !firstCamera.serialNumber.empty() &&
                    NormalizeMac(secondCamera.macAddress) == NormalizeMac(firstCamera.macAddress) &&
                    secondCamera.serialNumber == firstCamera.serialNumber) {
                    isDuplicateAcrossPasses = true;
                    LogMessage(LogLevel::INFO_LEVEL, "Filtering duplicate between passes: " + secondCamera.currentIP +
                              " (MAC and Serial match with first pass)");
                    break;
                }
            }
            if (!isDuplicateAcrossPasses) {
                cameras.push_back(secondCamera);
                secondPassUnique++;
                LogMessage(LogLevel::INFO_LEVEL, "Added unique camera from second pass: " + secondCamera.currentIP);
            }
        }

        LogMessage(LogLevel::INFO_LEVEL, "Discovery summary: First pass=" + to_string(firstPassCameras.size()) +
                  ", Second pass=" + to_string(secondPassCameras.size()) + ", Second pass unique=" +
                  to_string(secondPassUnique) + ", Total unique=" + to_string(cameras.size()));
        return true;
    } else if (firstPassSuccess) {
        LogMessage(LogLevel::INFO_LEVEL, "First pass XML SADP discovery succeeded but found no cameras");
        cameras = firstPassCameras;
        return true;
    }

    // If XML SADP discovery failed, no fallback methods
    LogMessage(LogLevel::ERROR_LEVEL, "XML SADP multicast discovery failed - no cameras found");
    return false;
}

// EXACT COPY: XML SADP Discovery from original
bool XMLSadpDiscovery(vector<CameraInfo>& cameras) {
    LogMessage(LogLevel::INFO_LEVEL, "Starting XML SADP multicast discovery (Universal Scanner method)...");

    cameras.clear();

    // Initialize Winsock (if not already done)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // Create UDP socket for multicast
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        LogMessage(LogLevel::ERROR_LEVEL, "XMLSadpDiscovery: Failed to create UDP socket");
        WSACleanup();
        return false;
    }

    // Set socket options exactly like Universal Scanner
    BOOL reuseAddr = TRUE;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&reuseAddr, sizeof(reuseAddr)) == SOCKET_ERROR) {
        LogMessage(LogLevel::WARN_LEVEL, "XMLSadpDiscovery: Warning - Failed to set SO_REUSEADDR");
    }

    // Bind to any address on port 37020 (like Universal Scanner)
    sockaddr_in bindAddr = {};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_addr.s_addr = INADDR_ANY;  // IPAddress.Any
    bindAddr.sin_port = htons(37020);

    if (bind(sock, (sockaddr*)&bindAddr, sizeof(bindAddr)) == SOCKET_ERROR) {
        LogMessage(LogLevel::ERROR_LEVEL, "XMLSadpDiscovery: Failed to bind to port 37020");
        closesocket(sock);
        WSACleanup();
        return false;
    }

    // Enumerate network interfaces and join multicast group on each (like Universal Scanner)
    LogMessage(LogLevel::INFO_LEVEL, "XMLSadpDiscovery: Enumerating network interfaces for multicast join...");

    // Get adapter information
    ULONG bufferSize = 0;
    DWORD result = GetAdaptersInfo(nullptr, &bufferSize);
    if (result != ERROR_BUFFER_OVERFLOW) {
        LogMessage(LogLevel::ERROR_LEVEL, "XMLSadpDiscovery: Failed to get adapter info buffer size");
        closesocket(sock);
        WSACleanup();
        return false;
    }

    PIP_ADAPTER_INFO adapterInfo = (PIP_ADAPTER_INFO)malloc(bufferSize);
    if (!adapterInfo) {
        LogMessage(LogLevel::ERROR_LEVEL, "XMLSadpDiscovery: Failed to allocate adapter info buffer");
        closesocket(sock);
        WSACleanup();
        return false;
    }

    result = GetAdaptersInfo(adapterInfo, &bufferSize);
    if (result != NO_ERROR) {
        LogMessage(LogLevel::ERROR_LEVEL, "XMLSadpDiscovery: Failed to get adapter info");
        free(adapterInfo);
        closesocket(sock);
        WSACleanup();
        return false;
    }

    // Join multicast group on each active interface
    int interfacesJoined = 0;
    PIP_ADAPTER_INFO adapter = adapterInfo;
    while (adapter) {
        // Skip loopback and non-operational interfaces
        if (adapter->Type != MIB_IF_TYPE_LOOPBACK &&
            strlen(adapter->IpAddressList.IpAddress.String) > 0 &&
            strcmp(adapter->IpAddressList.IpAddress.String, "0.0.0.0") != 0) {

            ip_mreq mreq = {};
            inet_pton(AF_INET, "239.255.255.250", &mreq.imr_multiaddr);
            inet_pton(AF_INET, adapter->IpAddressList.IpAddress.String, &mreq.imr_interface);

            if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) == SOCKET_ERROR) {
                LogMessage(LogLevel::WARN_LEVEL, "XMLSadpDiscovery: Failed to join multicast on interface " +
                          string(adapter->IpAddressList.IpAddress.String));
            } else {
                interfacesJoined++;
                LogMessage(LogLevel::INFO_LEVEL, "XMLSadpDiscovery: Joined multicast group on interface " +
                          string(adapter->IpAddressList.IpAddress.String) + " (" + string(adapter->Description) + ")");
            }
        }
        adapter = adapter->Next;
    }

    free(adapterInfo);

    if (interfacesJoined == 0) {
        LogMessage(LogLevel::ERROR_LEVEL, "XMLSadpDiscovery: Failed to join multicast group on any interface");
        closesocket(sock);
        WSACleanup();
        return false;
    }

    LogMessage(LogLevel::INFO_LEVEL, "XMLSadpDiscovery: Successfully joined multicast group on " +
              to_string(interfacesJoined) + " interfaces");

    // Create sending socket for multicast
    SOCKET sendSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sendSock == INVALID_SOCKET) {
        LogMessage(LogLevel::ERROR_LEVEL, "XMLSadpDiscovery: Failed to create sending socket");
        closesocket(sock);
        WSACleanup();
        return false;
    }

    // Set TTL for multicast
    int ttl = 1;
    setsockopt(sendSock, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ttl, sizeof(ttl));

    // Prepare multicast destination
    sockaddr_in multicastAddr = {};
    multicastAddr.sin_family = AF_INET;
    multicastAddr.sin_port = htons(37020);
    inet_pton(AF_INET, "239.255.255.250", &multicastAddr.sin_addr);

    // Generate XML probe packet - EXACT format from Universal Scanner
    string xmlProbe = "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
                      "<Probe>"
                      "<Uuid>6a0eaae3-897b-4472-a692-ca0b08e09cd1</Uuid>"  // Exact GUID
                      "<Types>inquiry</Types>"
                      "</Probe>";

    LogMessage(LogLevel::INFO_LEVEL, "XMLSadpDiscovery: Sending XML probe packets to 239.255.255.250:37020");

    // Transmit 3–5 probes spaced 250–500 ms (use 4 at 300 ms)
    const int probeCount = 4;
    const int probeSpacingMs = 300;
    for (int i = 0; i < probeCount; ++i) {
        int sent = sendto(sendSock, xmlProbe.c_str(), (int)xmlProbe.length(), 0,
                          (sockaddr*)&multicastAddr, sizeof(multicastAddr));
        if (sent == SOCKET_ERROR) {
            LogMessage(LogLevel::WARN_LEVEL, "XMLSadpDiscovery: Failed to send one probe packet");
        }
        if (i < probeCount - 1) {
            this_thread::sleep_for(chrono::milliseconds(probeSpacingMs));
        }
    }

    closesocket(sendSock);  // Close sending socket

    LogMessage(LogLevel::INFO_LEVEL, "XMLSadpDiscovery: Probes sent, waiting for responses...");

    // Set socket timeout for receiving and increase buffer to handle bursts
    DWORD timeout = 10000;  // 10 seconds receive window
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    int rcvbuf = 256 * 1024;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&rcvbuf, sizeof(rcvbuf));

    // Listen for responses
    char buffer[4096];
    sockaddr_in fromAddr;
    int fromLen = sizeof(fromAddr);
    int devicesFound = 0;

    time_t startTime = time(nullptr); // start window after successful IP_ADD_MEMBERSHIP and probes
    while (time(nullptr) - startTime < 10) {  // 10 second discovery window

        int received = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                               (sockaddr*)&fromAddr, &fromLen);

        if (received > 0) {
            buffer[received] = '\0';
            string xmlResponse(buffer);

            LogMessage(LogLevel::INFO_LEVEL, "XMLSadpDiscovery: Received response from " +
                      string(inet_ntoa(fromAddr.sin_addr)) + " (" + to_string(received) + " bytes)");

            // Check if it's a ProbeMatch response (exact Universal Scanner logic)
            if (xmlResponse.find("<ProbeMatch>") != string::npos) {

                CameraInfo camera = {};

                // Extract IPv4Address (exact Universal Scanner parsing)
                size_t ipStart = xmlResponse.find("<IPv4Address>");
                size_t ipEnd = xmlResponse.find("</IPv4Address>");
                if (ipStart != string::npos && ipEnd != string::npos) {
                    ipStart += 13; // length of "<IPv4Address>"
                    camera.currentIP = xmlResponse.substr(ipStart, ipEnd - ipStart);
                } else {
                    camera.currentIP = string(inet_ntoa(fromAddr.sin_addr));
                }

                // Extract DeviceDescription
                size_t descStart = xmlResponse.find("<DeviceDescription>");
                size_t descEnd = xmlResponse.find("</DeviceDescription>");
                if (descStart != string::npos && descEnd != string::npos) {
                    descStart += 18; // length of "<DeviceDescription>"
                    camera.deviceModel = xmlResponse.substr(descStart, descEnd - descStart);
                } else {
                    camera.deviceModel = "HikVision-XMLDiscovered";
                }

                // Extract DeviceSN
                size_t snStart = xmlResponse.find("<DeviceSN>");
                size_t snEnd = xmlResponse.find("</DeviceSN>");
                if (snStart != string::npos && snEnd != string::npos) {
                    snStart += 10; // length of "<DeviceSN>"
                    camera.serialNumber = xmlResponse.substr(snStart, snEnd - snStart);
                } else {
                    camera.serialNumber = "XML-" + camera.currentIP;
                }

                // Extract CommandPort
                size_t portStart = xmlResponse.find("<CommandPort>");
                size_t portEnd = xmlResponse.find("</CommandPort>");
                if (portStart != string::npos && portEnd != string::npos) {
                    portStart += 13; // length of "<CommandPort>"
                    string portStr = xmlResponse.substr(portStart, portEnd - portStart);
                    try {
                        camera.port = static_cast<WORD>(stoi(portStr));
                    } catch (...) {
                        camera.port = 8000; // Default
                    }
                } else {
                    camera.port = 8000; // Default HikVision port
                }

                // Extract MAC address
                size_t macStart = xmlResponse.find("<MAC>");
                size_t macEnd = xmlResponse.find("</MAC>");
                if (macStart != string::npos && macEnd != string::npos) {
                    macStart += 5; // length of "<MAC>"
                    camera.macAddress = xmlResponse.substr(macStart, macEnd - macStart);
                } else {
                    camera.macAddress = "00:00:00:00:00:00";
                }

                // Extract Activated status
                size_t activeStart = xmlResponse.find("<Activated>");
                size_t activeEnd = xmlResponse.find("</Activated>");
                if (activeStart != string::npos && activeEnd != string::npos) {
                    activeStart += 11; // length of "<Activated>"
                    string activeStr = xmlResponse.substr(activeStart, activeEnd - activeStart);
                    camera.isActivated = (activeStr == "true");
                } else {
                    camera.isActivated = false; // Assume needs activation
                }

                // Extract SoftwareVersion
                size_t swStart = xmlResponse.find("<SoftwareVersion>");
                size_t swEnd = xmlResponse.find("</SoftwareVersion>");
                if (swStart != string::npos && swEnd != string::npos) {
                    swStart += 17; // length of "<SoftwareVersion>"
                    camera.softwareVersion = xmlResponse.substr(swStart, swEnd - swStart);
                } else {
                    camera.softwareVersion = "Unknown";
                }

                // Set defaults for simplified structure
                camera.username = "admin";  // Set default username during discovery
                camera.isStreaming = false;
                camera.isConnected = false;

                // Check for duplicates WITHIN this discovery pass (cameras respond multiple times to same broadcast)
                bool isDuplicate = false;
                for (const auto& existing : cameras) {
                    // MAC address match (primary identifier)
                    if (!camera.macAddress.empty() && !existing.macAddress.empty() &&
                        NormalizeMac(camera.macAddress) == NormalizeMac(existing.macAddress)) {
                        isDuplicate = true;
                        LogMessage(LogLevel::INFO_LEVEL,
                                  "XMLSadpDiscovery: Filtering duplicate response within pass (MAC): " + camera.macAddress);
                        break;
                    }
                    // Serial number match (fallback identifier)
                    if (!camera.serialNumber.empty() && !existing.serialNumber.empty() &&
                        camera.serialNumber == existing.serialNumber) {
                        isDuplicate = true;
                        LogMessage(LogLevel::INFO_LEVEL,
                                  "XMLSadpDiscovery: Filtering duplicate response within pass (Serial): " + camera.serialNumber);
                        break;
                    }
                    // IP address match (cameras might respond from same IP multiple times)
                    if (camera.currentIP == existing.currentIP) {
                        isDuplicate = true;
                        LogMessage(LogLevel::INFO_LEVEL,
                                  "XMLSadpDiscovery: Filtering duplicate response within pass (IP): " + camera.currentIP);
                        break;
                    }
                }

                if (!isDuplicate) {
                    cameras.push_back(camera);
                    devicesFound++;
                }

                LogMessage(LogLevel::INFO_LEVEL,
                          "XMLSadpDiscovery: Found HikVision camera: " + camera.currentIP +
                          " [" + camera.deviceModel + "] SN:" + camera.serialNumber +
                          " Port:" + to_string(camera.port) +
                          " Status:" + (camera.isActivated ? "Activated" : "Not Activated"));
            } else {
                LogMessage(LogLevel::INFO_LEVEL, "XMLSadpDiscovery: Received non-ProbeMatch response, ignoring");
            }
        } else if (received == SOCKET_ERROR) {
            int error = WSAGetLastError();
            if (error == WSAETIMEDOUT) {
                // Timeout is expected, continue listening
                continue;
            } else {
                LogMessage(LogLevel::WARN_LEVEL, "XMLSadpDiscovery: Socket error " + to_string(error));
                break;
            }
        }
    }

    // Leave multicast group on all interfaces we joined
    LogMessage(LogLevel::INFO_LEVEL, "XMLSadpDiscovery: Leaving multicast groups on all interfaces...");

    // Re-enumerate adapters to leave multicast groups properly
    ULONG leaveBufferSize = 0;
    GetAdaptersInfo(nullptr, &leaveBufferSize);
    PIP_ADAPTER_INFO leaveAdapterInfo = (PIP_ADAPTER_INFO)malloc(leaveBufferSize);
    if (leaveAdapterInfo && GetAdaptersInfo(leaveAdapterInfo, &leaveBufferSize) == NO_ERROR) {
        PIP_ADAPTER_INFO leaveAdapter = leaveAdapterInfo;
        while (leaveAdapter) {
            if (leaveAdapter->Type != MIB_IF_TYPE_LOOPBACK &&
                strlen(leaveAdapter->IpAddressList.IpAddress.String) > 0 &&
                strcmp(leaveAdapter->IpAddressList.IpAddress.String, "0.0.0.0") != 0) {

                ip_mreq leaveMreq = {};
                inet_pton(AF_INET, "239.255.255.250", &leaveMreq.imr_multiaddr);
                inet_pton(AF_INET, leaveAdapter->IpAddressList.IpAddress.String, &leaveMreq.imr_interface);

                setsockopt(sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&leaveMreq, sizeof(leaveMreq));
            }
            leaveAdapter = leaveAdapter->Next;
        }
        free(leaveAdapterInfo);
    }

    closesocket(sock);
    WSACleanup();

    LogMessage(LogLevel::INFO_LEVEL, "XMLSadpDiscovery: Discovery completed. Found " +
              to_string(devicesFound) + " cameras via XML SADP");

    return !cameras.empty();
}

// EXACT COPY: SimpleSoT merge function from original
bool MergeSimpleSoTWithDiscoveredCameras(vector<CameraInfo>& discoveredCameras) {
    if (!g_SimpleSoT) {
        LogMessage(LogLevel::WARN_LEVEL, "SimpleSoT not initialized - skipping data merge");
        return false;
    }

    LogMessage(LogLevel::INFO_LEVEL, "UNIFIED: Merging discovered cameras with existing SimpleSoT data");

    try {
        // Get all existing SoT records for MAC+Serial deduplication
        vector<SimpleCameraRecord> allSoTRecords = g_SimpleSoT->GetAllCameras();

        // SINGLE PASS: Process each discovered camera once
        for (auto& runtimeCamera : discoveredCameras) {

            // MAC+Serial deduplication in SoT
            bool foundExisting = false;
            SimpleCameraRecord existingSoTRecord;

            // Search for MAC+Serial match (BOTH must be valid and match)
            if (!runtimeCamera.macAddress.empty() && !runtimeCamera.serialNumber.empty()) {
                for (const auto& sotRecord : allSoTRecords) {
                    if (sotRecord.macAddress == runtimeCamera.macAddress &&
                        sotRecord.serialNumber == runtimeCamera.serialNumber) {
                        foundExisting = true;
                        existingSoTRecord = sotRecord;
                        break;
                    }
                }
            }

            if (foundExisting) {
                // EXISTING CAMERA: Update only currentIP and activationStatus, restore runtime data
                LogMessage(LogLevel::INFO_LEVEL, "EXISTING: Found camera in SoT " + runtimeCamera.serialNumber);
                cout << "[EXISTING] Camera " << runtimeCamera.serialNumber << " - updating SoT and restoring data" << endl;

                // Update currentIP if changed
                if (existingSoTRecord.currentIP != runtimeCamera.currentIP) {
                    g_SimpleSoT->UpdateCameraField(runtimeCamera.serialNumber, "currentIP", runtimeCamera.currentIP);
                    LogMessage(LogLevel::INFO_LEVEL, "UPDATED: currentIP " + existingSoTRecord.currentIP + " → " + runtimeCamera.currentIP);
                }

                // Update activationStatus from discovery (discovery wins)
                string discoveryActivationStatus = runtimeCamera.isActivated ? "Activated" : "NotActivated";
                if (existingSoTRecord.activationStatus != discoveryActivationStatus) {
                    g_SimpleSoT->UpdateCameraField(runtimeCamera.serialNumber, "activationStatus", discoveryActivationStatus);
                    LogMessage(LogLevel::INFO_LEVEL, "UPDATED: activationStatus " + existingSoTRecord.activationStatus + " → " + discoveryActivationStatus);
                }

                // Restore preserved data from SoT to runtime
                runtimeCamera.adminPassword = existingSoTRecord.password;
                runtimeCamera.rtspMainStreamURL = existingSoTRecord.rtspMainStreamURL;
                runtimeCamera.rtspSubStreamURL = existingSoTRecord.rtspSubStreamURL;

                if (!existingSoTRecord.password.empty()) {
                    cout << "[PRESERVED] Camera " << runtimeCamera.serialNumber << " - using existing password" << endl;
                }

            } else {
                // NEW CAMERA: Create complete SoT record with defaults + discovery data
                LogMessage(LogLevel::INFO_LEVEL, "NEW: Adding camera to SoT " + runtimeCamera.serialNumber);
                cout << "[NEW] Camera " << runtimeCamera.serialNumber << " - creating SoT record" << endl;

                SimpleCameraRecord newSoTRecord;

                // Discovery data fields
                newSoTRecord.serialNumber = runtimeCamera.serialNumber;
                newSoTRecord.currentIP = runtimeCamera.currentIP;
                newSoTRecord.deviceModel = runtimeCamera.deviceModel;
                newSoTRecord.macAddress = runtimeCamera.macAddress;
                newSoTRecord.username = "admin";
                newSoTRecord.activationStatus = runtimeCamera.isActivated ? "Activated" : "NotActivated";

                // State fields with defaults
                newSoTRecord.networkStatus = "not_started";
                newSoTRecord.streamingStatus = "not_started";

                // Data fields empty (will be populated during processing)
                newSoTRecord.password = "";
                newSoTRecord.rtspMainStreamURL = "";
                newSoTRecord.rtspSubStreamURL = "";

                // Add to SoT
                if (!g_SimpleSoT->AddOrUpdateCamera(newSoTRecord)) {
                    LogMessage(LogLevel::WARN_LEVEL, "Failed to add new camera to SoT " + runtimeCamera.serialNumber);
                }

                // Runtime objects for new cameras have empty preserved data
                runtimeCamera.adminPassword = "";
                runtimeCamera.rtspMainStreamURL = "";
                runtimeCamera.rtspSubStreamURL = "";
            }
        }

        // Save all changes
        if (!g_SimpleSoT->AtomicSave()) {
            LogMessage(LogLevel::ERROR_LEVEL, "Failed to save merged SoT data");
            return false;
        }

        LogMessage(LogLevel::INFO_LEVEL, "UNIFIED: Merge completed successfully");
        return true;

    } catch (const exception& e) {
        LogMessage(LogLevel::ERROR_LEVEL, "Exception in merge operation: " + string(e.what()));
        return false;
    }
}

// MISSING UTILITY FUNCTIONS - COPIED FROM ORIGINAL

// Display discovered cameras
void DisplayDiscoveredCameras(const vector<CameraInfo>& cameras) {
    cout << "Discovered " << cameras.size() << " cameras:" << endl;
    cout << endl;

    int index = 1;
    for (const auto& camera : cameras) {
        cout << "Camera " << index++ << ":" << endl;
        cout << "  IP Address: " << camera.currentIP << endl;
        cout << "  Serial Number: " << camera.serialNumber << endl;
        cout << "  Device Model: " << camera.deviceModel << endl;
        cout << "  MAC Address: " << camera.macAddress << endl;
        cout << "  Port: " << camera.port << endl;
        cout << "  Activated: " << (camera.isActivated ? "Yes" : "No") << endl;
        cout << "  Software Version: " << camera.softwareVersion << endl;
        cout << endl;
    }
}

// Generate RTSP URLs from camera info and password
void GenerateRTSPURLs(CameraInfo& camera) {
    if (camera.currentIP.empty() || camera.adminPassword.empty()) {
        LogMessage(LogLevel::WARN_LEVEL, "Cannot generate RTSP URLs: missing IP or password for " + camera.serialNumber);
        return;
    }

    // Standard HikVision RTSP URL format
    string baseURL = "rtsp://" + camera.username + ":" + camera.adminPassword + "@" + camera.currentIP + ":554";

    // Main stream (channel 1, stream 1)
    camera.rtspMainStreamURL = baseURL + "/Streaming/Channels/101";

    // Sub stream (channel 1, stream 2)
    camera.rtspSubStreamURL = baseURL + "/Streaming/Channels/102";

    LogMessage(LogLevel::INFO_LEVEL, "Generated RTSP URLs for camera " + camera.serialNumber);
}

// Convert CameraInfo to SimpleCameraRecord
SimpleCameraRecord ConvertToSimpleCameraRecord(const CameraInfo& camera) {
    SimpleCameraRecord record;

    // Copy basic info
    record.serialNumber = camera.serialNumber;
    record.currentIP = camera.currentIP;
    record.deviceModel = camera.deviceModel;
    record.macAddress = camera.macAddress;
    record.username = camera.username;
    record.password = camera.adminPassword;

    // Copy RTSP URLs
    record.rtspMainStreamURL = camera.rtspMainStreamURL;
    record.rtspSubStreamURL = camera.rtspSubStreamURL;

    // Set activation status
    record.activationStatus = camera.isActivated ? "Activated" : "NotActivated";

    // Set default status fields
    record.networkStatus = "not_started";
    record.streamingStatus = "not_started";

    return record;
}