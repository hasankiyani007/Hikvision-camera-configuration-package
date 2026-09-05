/*
 * MediaMTX Service - Process Supervisor
 *
 * Simple service that keeps MediaMTX process alive.
 * Stream reconnection handled by FFmpeg timeout/reconnect flags.
 */

#include "MediaMTXService.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <filesystem>

using namespace std;

// Static instance for Windows service callbacks
MediaMTXService* MediaMTXService::s_serviceInstance = nullptr;

MediaMTXService::MediaMTXService()
    : m_serviceRunning(false)
    , m_mediaMTXRunning(false)
    , m_mediaMTXProcess(INVALID_HANDLE_VALUE) {

    ZeroMemory(&m_serviceStatus, sizeof(m_serviceStatus));
    ZeroMemory(&m_mediaMTXProcessInfo, sizeof(m_mediaMTXProcessInfo));

    SetDefaultConfiguration();
}

MediaMTXService::~MediaMTXService() {
    Shutdown();
}

// ============================================================================
// SERVICE ENTRY POINTS
// ============================================================================

void WINAPI MediaMTXService::ServiceMain(DWORD argc, LPTSTR* argv) {
    if (!s_serviceInstance) {
        return;
    }

    // Register service control handler
    s_serviceInstance->m_serviceStatusHandle = RegisterServiceCtrlHandler(
        s_serviceInstance->m_config.serviceName.c_str(),
        ServiceCtrlHandler
    );

    if (!s_serviceInstance->m_serviceStatusHandle) {
        s_serviceInstance->LogEvent("Failed to register service control handler", EVENTLOG_ERROR_TYPE);
        return;
    }

    // Initialize service
    s_serviceInstance->m_serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    s_serviceInstance->m_serviceStatus.dwCurrentState = SERVICE_START_PENDING;
    s_serviceInstance->m_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    s_serviceInstance->UpdateServiceStatus(SERVICE_START_PENDING);

    // Initialize service paths before Run()
    if (!s_serviceInstance->Initialize()) {
        s_serviceInstance->LogEvent("Failed to initialize service", EVENTLOG_ERROR_TYPE);
        s_serviceInstance->UpdateServiceStatus(SERVICE_STOPPED, 1);
        return;
    }

    // Start the service
    s_serviceInstance->Run();
}

void WINAPI MediaMTXService::ServiceCtrlHandler(DWORD ctrl) {
    if (!s_serviceInstance) {
        return;
    }

    switch (ctrl) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        s_serviceInstance->LogEvent("Service stop requested", EVENTLOG_INFORMATION_TYPE);
        s_serviceInstance->UpdateServiceStatus(SERVICE_STOP_PENDING);
        s_serviceInstance->Stop();
        break;
    default:
        break;
    }
}

bool MediaMTXService::Initialize() {
    LogEvent("MediaMTX Service initializing...", EVENTLOG_INFORMATION_TYPE);

    // Resolve paths
    string installPath = GetServiceInstallPath();
    m_config.installPath = installPath;
    m_config.mediaMTXPath = ResolvePath("mediamtx.exe");
    m_config.configPath = ResolvePath("mediamtx.yml");

    LogEvent("Service paths resolved - MediaMTX: " + m_config.mediaMTXPath, EVENTLOG_INFORMATION_TYPE);

    return true;
}

void MediaMTXService::Run() {
    LogEvent("MediaMTX Service starting...", EVENTLOG_INFORMATION_TYPE);

    m_serviceRunning = true;
    UpdateServiceStatus(SERVICE_RUNNING);

    // Launch MediaMTX process
    if (!LaunchMediaMTX()) {
        LogEvent("Failed to launch MediaMTX process", EVENTLOG_ERROR_TYPE);
        UpdateServiceStatus(SERVICE_STOPPED, 1);
        return;
    }

    // Start watchdog thread to keep MediaMTX alive
    m_processWatchdogThread = thread(&MediaMTXService::ProcessWatchdogLoop, this);

    LogEvent("MediaMTX Service running", EVENTLOG_INFORMATION_TYPE);

    // Main service loop
    while (m_serviceRunning) {
        Sleep(1000);
    }

    LogEvent("MediaMTX Service stopping...", EVENTLOG_INFORMATION_TYPE);
    Shutdown();
    UpdateServiceStatus(SERVICE_STOPPED);
}

void MediaMTXService::Stop() {
    m_serviceRunning = false;
}

// ============================================================================
// PROCESS SUPERVISION
// ============================================================================

bool MediaMTXService::LaunchMediaMTX() {
    LogEvent("Launching MediaMTX: " + m_config.mediaMTXPath, EVENTLOG_INFORMATION_TYPE);

    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_SHOW;

    string commandLine = "\"" + m_config.mediaMTXPath + "\" \"" + m_config.configPath + "\"";

    BOOL result = CreateProcess(
        m_config.mediaMTXPath.c_str(),
        const_cast<char*>(commandLine.c_str()),
        nullptr,
        nullptr,
        FALSE,
        CREATE_NEW_CONSOLE,
        nullptr,
        m_config.installPath.c_str(),
        &si,
        &m_mediaMTXProcessInfo
    );

    if (!result) {
        DWORD error = GetLastError();
        LogEvent("Failed to create MediaMTX process. Error: " + to_string(error), EVENTLOG_ERROR_TYPE);
        return false;
    }

    m_mediaMTXProcess = m_mediaMTXProcessInfo.hProcess;
    m_mediaMTXRunning = true;

    LogEvent("MediaMTX launched. PID: " + to_string(m_mediaMTXProcessInfo.dwProcessId), EVENTLOG_INFORMATION_TYPE);
    return true;
}

void MediaMTXService::TerminateMediaMTX() {
    if (m_mediaMTXProcess != INVALID_HANDLE_VALUE) {
        LogEvent("Terminating MediaMTX process", EVENTLOG_INFORMATION_TYPE);
        TerminateProcess(m_mediaMTXProcess, 0);
        WaitForSingleObject(m_mediaMTXProcess, 5000);  // Wait up to 5 seconds
        CloseHandle(m_mediaMTXProcess);
        CloseHandle(m_mediaMTXProcessInfo.hThread);
        m_mediaMTXProcess = INVALID_HANDLE_VALUE;
        m_mediaMTXRunning = false;
    }
}

void MediaMTXService::ProcessWatchdogLoop() {
    LogEvent("Process watchdog started", EVENTLOG_INFORMATION_TYPE);

    while (m_serviceRunning) {
        if (m_mediaMTXRunning && !IsMediaMTXProcessAlive()) {
            LogEvent("MediaMTX process died - attempting restart", EVENTLOG_WARNING_TYPE);
            RestartMediaMTXProcess();
        }

        Sleep(5000);  // Check every 5 seconds
    }

    LogEvent("Process watchdog stopped", EVENTLOG_INFORMATION_TYPE);
}

bool MediaMTXService::IsMediaMTXProcessAlive() {
    if (m_mediaMTXProcess == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD exitCode;
    if (!GetExitCodeProcess(m_mediaMTXProcess, &exitCode)) {
        return false;
    }

    return exitCode == STILL_ACTIVE;
}

void MediaMTXService::RestartMediaMTXProcess() {
    LogEvent("Restarting MediaMTX process...", EVENTLOG_INFORMATION_TYPE);

    TerminateMediaMTX();
    Sleep(3000);  // Wait for process to fully terminate

    if (LaunchMediaMTX()) {
        LogEvent("MediaMTX process restarted successfully", EVENTLOG_INFORMATION_TYPE);
    } else {
        LogEvent("Failed to restart MediaMTX process", EVENTLOG_ERROR_TYPE);
    }
}

// ============================================================================
// UTILITIES
// ============================================================================

void MediaMTXService::SetDefaultConfiguration() {
    m_config.serviceName = "CameraStreamingService";
    m_config.displayName = "HikVision Camera Streaming Service";
    m_config.description = "Keeps MediaMTX streaming server alive";
}

string MediaMTXService::GetServiceInstallPath() {
    char path[MAX_PATH];
    DWORD size = GetModuleFileName(nullptr, path, MAX_PATH);
    if (size == 0) {
        return "C:\\Program Files\\HikVision\\Camera Setup\\";
    }

    string fullPath(path);
    size_t lastSlash = fullPath.find_last_of("\\/");
    if (lastSlash != string::npos) {
        return fullPath.substr(0, lastSlash + 1);
    }

    return "C:\\Program Files\\HikVision\\Camera Setup\\";
}

string MediaMTXService::ResolvePath(const string& relativePath) {
    if (relativePath.find(':') != string::npos) {
        return relativePath;  // Already absolute
    }
    return m_config.installPath + relativePath;
}

void MediaMTXService::LogEvent(const string& message, WORD eventType) {
    auto now = chrono::system_clock::now();
    auto time_t = chrono::system_clock::to_time_t(now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&time_t));

    string levelStr = (eventType == EVENTLOG_ERROR_TYPE) ? "ERROR" :
                    (eventType == EVENTLOG_WARNING_TYPE) ? "WARN" : "INFO";

    string formattedMessage = string("[") + timestamp + "] [" + levelStr + "] " + message;

    cout << formattedMessage << endl;

    try {
        string logPath = ResolvePath("MediaMTXService.log");
        ofstream logFile(logPath, ios::app);
        if (logFile.is_open()) {
            logFile << formattedMessage << endl;
        }
    } catch (...) {
        // Ignore logging errors
    }
}

void MediaMTXService::UpdateServiceStatus(DWORD currentState, DWORD exitCode) {
    m_serviceStatus.dwCurrentState = currentState;
    m_serviceStatus.dwWin32ExitCode = exitCode;
    m_serviceStatus.dwWaitHint = 0;

    SetServiceStatus(m_serviceStatusHandle, &m_serviceStatus);
}

void MediaMTXService::Shutdown() {
    LogEvent("Service shutdown initiated", EVENTLOG_INFORMATION_TYPE);

    m_serviceRunning = false;

    if (m_processWatchdogThread.joinable()) {
        m_processWatchdogThread.join();
    }

    TerminateMediaMTX();

    LogEvent("Service shutdown complete", EVENTLOG_INFORMATION_TYPE);
}

// ============================================================================
// MAIN & INSTALLER
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc > 1) {
        string command = argv[1];

        if (command == "--install") {
            return ServiceInstaller::Install(argv[0]) ? 0 : 1;
        } else if (command == "--uninstall") {
            return ServiceInstaller::Uninstall() ? 0 : 1;
        } else if (command == "--console") {
            MediaMTXService service;
            service.Initialize();

            cout << "Running in console mode. Press Enter to stop..." << endl;
            service.Run();
            cin.get();
            service.Stop();
            return 0;
        }
    }

    // Normal service mode
    MediaMTXService service;
    MediaMTXService::s_serviceInstance = &service;

    SERVICE_TABLE_ENTRY serviceTable[] = {
        { const_cast<char*>("CameraStreamingService"), MediaMTXService::ServiceMain },
        { nullptr, nullptr }
    };

    if (!StartServiceCtrlDispatcher(serviceTable)) {
        service.LogEvent("Failed to start service control dispatcher", EVENTLOG_ERROR_TYPE);
        return 1;
    }

    return 0;
}

// Service installer (unchanged)
bool ServiceInstaller::Install(const string& exePath) {
    SC_HANDLE scManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!scManager) {
        cout << "Failed to open Service Control Manager" << endl;
        return false;
    }

    SC_HANDLE service = CreateService(
        scManager,
        "CameraStreamingService",
        "HikVision Camera Streaming Service",
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        exePath.c_str(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );

    if (!service) {
        DWORD error = GetLastError();
        if (error == ERROR_SERVICE_EXISTS) {
            cout << "Service already exists" << endl;
        } else {
            cout << "Failed to create service. Error: " << error << endl;
        }
        CloseServiceHandle(scManager);
        return false;
    }

    SERVICE_DESCRIPTION description;
    string desc = "Keeps MediaMTX streaming server alive - stream recovery handled by FFmpeg";
    description.lpDescription = const_cast<char*>(desc.c_str());
    ChangeServiceConfig2(service, SERVICE_CONFIG_DESCRIPTION, &description);

    ServiceInstaller::ConfigureFailureRecovery("CameraStreamingService");

    CloseServiceHandle(service);
    CloseServiceHandle(scManager);

    cout << "Service installed successfully" << endl;
    return true;
}

bool ServiceInstaller::Uninstall() {
    SC_HANDLE scManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scManager) {
        return false;
    }

    SC_HANDLE service = OpenService(scManager, "CameraStreamingService", SERVICE_STOP | DELETE);
    if (!service) {
        CloseServiceHandle(scManager);
        return false;
    }

    SERVICE_STATUS status;
    if (ControlService(service, SERVICE_CONTROL_STOP, &status)) {
        Sleep(3000);
    }

    if (!DeleteService(service)) {
        CloseServiceHandle(service);
        CloseServiceHandle(scManager);
        return false;
    }

    CloseServiceHandle(service);
    CloseServiceHandle(scManager);
    return true;
}

bool ServiceInstaller::ConfigureFailureRecovery(const string& serviceName) {
    SC_HANDLE scManager = OpenSCManager(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scManager) {
        return false;
    }

    SC_HANDLE service = OpenService(scManager, serviceName.c_str(), SERVICE_CHANGE_CONFIG);
    if (!service) {
        CloseServiceHandle(scManager);
        return false;
    }

    SC_ACTION actions[3];
    actions[0].Type = SC_ACTION_RESTART;
    actions[0].Delay = 30000;
    actions[1].Type = SC_ACTION_RESTART;
    actions[1].Delay = 60000;
    actions[2].Type = SC_ACTION_RESTART;
    actions[2].Delay = 120000;

    SERVICE_FAILURE_ACTIONS failureActions;
    failureActions.dwResetPeriod = 86400;
    failureActions.lpRebootMsg = nullptr;
    failureActions.lpCommand = nullptr;
    failureActions.cActions = 3;
    failureActions.lpsaActions = actions;

    ChangeServiceConfig2(service, SERVICE_CONFIG_FAILURE_ACTIONS, &failureActions);

    CloseServiceHandle(service);
    CloseServiceHandle(scManager);
    return true;
}
