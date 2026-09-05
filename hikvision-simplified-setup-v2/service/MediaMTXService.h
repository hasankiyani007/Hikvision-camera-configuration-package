/*
 * MediaMTX Service - Process Supervisor
 *
 * Simple service that keeps MediaMTX process alive.
 * Stream reconnection and health handled by FFmpeg flags.
 */

#ifndef MEDIAMTX_SERVICE_H
#define MEDIAMTX_SERVICE_H

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsvc.h>
#include <string>
#include <thread>
#include <atomic>

#pragma comment(lib, "advapi32.lib")

using namespace std;

// Service configuration
struct ServiceConfig {
    string serviceName;
    string displayName;
    string description;
    string installPath;
    string mediaMTXPath;
    string configPath;
};

class MediaMTXService {
private:
    // Service control
    SERVICE_STATUS m_serviceStatus;
    SERVICE_STATUS_HANDLE m_serviceStatusHandle;
    atomic<bool> m_serviceRunning;

    // Process management
    HANDLE m_mediaMTXProcess;
    PROCESS_INFORMATION m_mediaMTXProcessInfo;
    atomic<bool> m_mediaMTXRunning;

    // Watchdog thread
    thread m_processWatchdogThread;

    // Configuration
    ServiceConfig m_config;

public:
    MediaMTXService();
    ~MediaMTXService();

    // Service entry points
    static void WINAPI ServiceMain(DWORD argc, LPTSTR* argv);
    static void WINAPI ServiceCtrlHandler(DWORD ctrl);

    // Service lifecycle
    bool Initialize();
    void Run();
    void Stop();
    void Shutdown();

private:
    // Process supervision
    bool LaunchMediaMTX();
    void TerminateMediaMTX();
    void ProcessWatchdogLoop();
    bool IsMediaMTXProcessAlive();
    void RestartMediaMTXProcess();

    // Utilities
    void UpdateServiceStatus(DWORD currentState, DWORD exitCode = 0);
    string GetServiceInstallPath();
    string ResolvePath(const string& relativePath);

public:
    void LogEvent(const string& message, WORD eventType = EVENTLOG_INFORMATION_TYPE);
    void SetDefaultConfiguration();

    // Static service instance
    static MediaMTXService* s_serviceInstance;
};

// Service installer
struct ServiceInstaller {
    static bool Install(const string& exePath);
    static bool Uninstall();
    static bool ConfigureFailureRecovery(const string& serviceName);
};

#endif // MEDIAMTX_SERVICE_H
