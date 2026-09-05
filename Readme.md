# Hikvision Camera Setup Tool

A clean, open source CLI tool for automatic discovery and configuration of Hikvision IP cameras. No proprietary SDK required.

## Features

- Zero Dependencies - single standalone executable
- Automatic Camera Discovery via SADP protocol (UDP multicast)
- RTSP URL Generation for all discovered cameras
- Automatic MediaMTX Configuration
- Windows Background Service for 24/7 streaming
- Camera Database stored as human readable JSON
- No Hikvision SDK Required

---

## Build Instructions

### Requirements
- MinGW-w64 GCC Compiler
- Windows 10 / 11

### Build Command
```batch
build_simplified.bat
```

Or manually:
```batch
g++ -std=c++17 -static ^
    SimplifiedCameraSetup.cpp ^
    shared/*.cpp ^
    -lws2_32 -liphlpapi -lwinhttp -ladvapi32 ^
    -o bin/HikvisionCameraSetupCLI.exe
```

Output: `bin/HikvisionCameraSetupCLI.exe` (~1.8MB)

---

## Usage

### 1. Camera Setup
```batch
cd bin
HikvisionCameraSetupCLI.exe
```

**Workflow:**
1. Tool scans local network for all Hikvision cameras
2. Enter universal admin password for cameras
3. RTSP URLs are automatically generated
4. `mediamtx.yml` is edited and configured automatically
5. All camera data is saved to database

### 2. Install Background Service (Optional)
Run as Administrator:
```batch
cd service
install_service.bat
```

This will:
- Install Windows Service that runs on system boot
- Configure Windows Firewall rules
- Start MediaMTX automatically
- Auto-recover on crashes

To uninstall:
```batch
uninstall_service.bat
```

---

## Camera Database Format

All discovered cameras are saved to: `bin/data/camera_discovery.json`

```json
{
  "version": "1.0",
  "lastUpdated": "2026-04-13T04:53:00Z",
  "cameras": [
    {
      "serialNumber": "DS-2CD1123G2-LIU00000000EXAMPLE0000000",
      "deviceModel": "DS-2CD1123G2-LIU",
      "currentIP": "192.168.0.100",
      "macAddress": "00-11-22-33-44-55",
      "username": "admin",
      "password": "yourpassword",
      "rtspMainStreamURL": "rtsp://admin:yourpassword@192.168.0.100:554/Streaming/Channels/101",
      "rtspSubStreamURL": "rtsp://admin:yourpassword@192.168.0.100:554/Streaming/Channels/102",
      "activationStatus": "Activated",
      "dhcpStatus": "enabled"
    }
  ]
}
```

---

## MediaMTX Configuration

The tool automatically edits `mediamtx.yml` adding all discovered cameras as RTSP sources with optimized streaming settings:

```yaml
paths:
  camera_192_168_0_100:
    source: rtsp://admin:yourpassword@192.168.0.100:554/Streaming/Channels/101
    sourceProtocol: tcp
    sourceOnDemand: yes
    sourceOnDemandStartTimeout: 10s
    sourceOnDemandCloseAfter: 30s
    sourceRetry: 5s
    rtspTransport: tcp
    disableCodecVerification: yes
    runOnDemandStartTimeout: 10s
    runOnDemandCloseAfter: 30s
```

### Optimized Streaming Flags:

| Setting | Value | Purpose |
|---------|-------|---------|
| `sourceProtocol: tcp` | Force TCP | Avoid UDP packet loss and corruption |
| `rtspTransport: tcp` | Force TCP | Reliable stream delivery over local networks |
| `sourceRetry: 5s` | Auto-retry | Reconnect automatically if camera drops connection |
| `sourceOnDemand: yes` | On demand | Only pull stream from camera when client is connected |

### Supported Output Protocols:
- RTSP (TCP:8554)
- RTMP (TCP:1935)
- HLS (TCP:8888)
- WebRTC (UDP:8189 + TCP:8889)
- SRT (UDP:8890)
- HLS Low Latency
- MPEG-TS

---

## Network Ports

| Port | Protocol | Purpose |
|------|----------|---------|
| 37020 | UDP | SADP Camera Discovery |
| 554 | TCP | Camera RTSP |
| 8554 | TCP | MediaMTX RTSP Server |
| 8888 | TCP | HLS Web Stream |
| 8889 | TCP | WebRTC |
| 8189 | UDP | WebRTC Media |
| 9997 | TCP | MediaMTX API (localhost only) |

---

## Project Structure

```
hikvision-camera-setup/
├── bin/
│   ├── HikvisionCameraSetupCLI.exe   Main executable
│   ├── mediamtx.exe                  Open source RTSP server
│   ├── mediamtx.yml                  Configuration file
│   └── data/                         Camera database
├── shared/                           Core business logic
│   ├── CameraSharedLib_Simplified    SADP discovery
│   ├── SimpleSoT                     Database storage
│   ├── MediaMTXConfigManager         mediamtx.yml editor
│   └── jsoncpp                       JSON parser
├── service/                          Windows background service
├── SimplifiedCameraSetup.cpp         Main application
└── build_simplified.bat              Build script
```

---

## License

MIT License. This project contains no proprietary Hikvision code. All protocols are implemented from public documentation.