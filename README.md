<div align="center">

# 🖥️ Linux System Monitor

### A from-scratch **C++17** client–server monitoring system for Linux
*Raw `/proc` metrics → TCP + JSON → a live terminal dashboard. No frameworks. No agents. Just Linux.*

</div>

---

## 🎬 The Idea

Most monitoring tools hide how they work behind a framework. This project does the opposite: it talks to the Linux kernel directly, builds its own wire protocol on top of raw TCP sockets, and keeps track of every machine that reports in, all in plain C++17.

```text
   🐧 Linux machine                                   🛰️ Monitor server (port 5000)
 ┌────────────────────┐                            ┌───────────────────────────────┐
 │  /proc/stat        │                            │  Accept → Parse → Validate    │
 │  /proc/meminfo     │     newline-delimited      │        ↓                      │
 │  /proc/uptime      │ ─────── JSON over TCP ───► │  Client Registry              │
 │  statvfs()         │      every few seconds     │        ↓                      │
 │  process list      │                            │  Status  ·  Alerts            │
 └────────────────────┘                            └──────┬──────────┬─────────┬───┘
                                                          ▼          ▼         ▼
                                                     📺 Dashboard  🔔 Alerts  💾 CSV + Logs
```

---

## ⚡ At a Glance

| | |
|---|---|
| 🧠 **Language** | C++17 |
| 🐧 **Platform** | Linux (Ubuntu recommended) |
| 🔌 **Transport** | POSIX TCP sockets, port `5000` |
| 📦 **Wire format** | Newline-delimited JSON ([nlohmann/json](https://github.com/nlohmann/json)) |
| 📊 **Metrics** | CPU · Memory · Disk · Processes · Uptime · Hostname · Kernel |
| 🚦 **States** | `NORMAL` → `WARNING` → `CRITICAL` → `OFFLINE` |
| 💾 **Persistence** | `logs/metrics.csv` and `logs/server.log` |
| 🧪 **Tests** | `make test` (metrics + protocol), plus manual integration tests |

---

## ✨ Features

<table>
<tr>
<td width="50%" valign="top">

### 📡 Live Metric Collection
CPU, memory, disk, process count, uptime, hostname and kernel version, read straight from `/proc` and POSIX APIs.

### 🆔 Persistent Client Identity
Each machine gets a stable ID such as `PC-945742`, so the server always knows who is reporting.

### 🔄 Self-Healing Client
If the server disappears, the client retries automatically and resumes when it returns.

</td>
<td width="50%" valign="top">

### 📺 Real-Time Terminal Dashboard
A refreshing table of every connected client with colour-free, copy-friendly output, no GUI needed.

### 🚦 Smart Status Detection
Thresholds on CPU, memory and disk produce `NORMAL`, `WARNING` or `CRITICAL`. Silent clients become `OFFLINE`.

### 🛑 Graceful Shutdown
`Ctrl+C` stops accepting clients and shuts everything down cleanly instead of dying mid-write.

</td>
</tr>
</table>

---

## 📺 Dashboard Preview

```text
==============================================
           LINUX SYSTEM MONITOR
==============================================

Client ID      Hostname       CPU %     Memory %    Disk %    Processes   Status
--------------------------------------------------------------------------
PC-945742      Ubuntu         24.2      70.7        34.1      310         WARNING

--------------------------------------------------------------------------
Total Clients: 1
Dashboard refresh interval: 2 seconds
```

---

## 🚀 Quick Start

**1. Install dependencies**

```bash
sudo apt update
sudo apt install build-essential nlohmann-json3-dev netcat-openbsd
```

**2. Clone and build**

```bash
git clone <your-repository-url>
cd linux-system-monitor
make
```

**3. Run it** (two terminals)

```bash
# Terminal 1: the server
./build/server

# Terminal 2: a client
./build/client
```

Switch back to Terminal 1 and watch your machine appear on the dashboard. 🎉

<details>
<summary><b>More build commands</b></summary>

```bash
make client      # build the client only
make server      # build the server only
make clean       # remove build artifacts
make test        # build and run automated tests
```

</details>

---

## 🔬 How It Works

### The monitoring cycle

```text
 Linux ──► SystemMonitor ──► SystemData ──► JSON ──► TCP socket ──► Server
                                                                       │
              ┌───────────┬──────────────┬──────────────┬─────────────┤
              ▼           ▼              ▼              ▼             ▼
           Validate   Update client   Determine      Generate      Write CSV
                      registry         status         alerts      + refresh UI
```

### 🧮 Design decisions worth knowing

| Decision | Why |
|---|---|
| **CPU % uses two samples** of `/proc/stat` | The counters are cumulative, so one read can't give a real percentage. Usage is `(Δtotal − Δidle) / Δtotal × 100`. |
| **Memory uses `MemAvailable`, not `MemFree`** | `MemFree` ignores reclaimable cache and makes healthy machines look nearly full. |
| **Newline-delimited JSON** | TCP is a byte stream with no message boundaries. A trailing `\n` gives simple, reliable framing. |
| **Server never trusts input** | Every message is parsed and range-checked before it touches client state. |
| **Heartbeat timeout** | A client silent for ~15 seconds is marked `OFFLINE`. |

---

## 📦 Wire Protocol

Each message is a single JSON object followed by `\n`.

```json
{
    "type": "metrics",
    "client_id": "PC-945742",
    "hostname": "Ubuntu",
    "kernel_version": "7.0.0-38-generic",
    "cpu_usage": 3.53,
    "memory_usage": 70.68,
    "disk_usage": 34.09,
    "process_count": 310,
    "uptime_seconds": 5996.45,
    "timestamp": 1791042240
}
```

### 🛡️ Input validation

The server rejects bad data instead of crashing on it. Tested cases include:

- ❌ Missing fields
- ❌ Malformed JSON
- ❌ Invalid metric values
- ❌ Out-of-range values, e.g. `"cpu_usage": 500`
- ❌ Invalid client messages

---

## 🚦 Status Logic

```text
usage < warning threshold    →  🟢 NORMAL
usage ≥ warning threshold    →  🟡 WARNING
usage ≥ critical threshold   →  🔴 CRITICAL
no data within timeout       →  ⚫ OFFLINE
```

The same rule is applied to CPU, memory and disk. An `AlertManager` tracks state transitions so changes in condition are recognised, not just snapshots.

---

## 💾 Persistence

| File | Contents |
|---|---|
| `logs/metrics.csv` | Historical metrics, one row per sample |
| `logs/server.log` | Server events (connections, disconnects, errors) |

CSV columns:

```text
timestamp,client_id,hostname,kernel_version,cpu_usage,memory_usage,disk_usage,process_count,uptime_seconds
```

---

## 🧱 Architecture

```text
┌─────────────── CLIENT ───────────────┐   ┌──────────────── SERVER ────────────────┐
│ SystemMonitor   collect metrics      │   │ Server          accept + receive       │
│ ClientIdentity  persistent ID        │   │ ClientRegistry  per-client state       │
│ NetworkClient   connect / send /     │   │ AlertManager    status transitions     │
│                 reconnect            │   │ Dashboard       live terminal UI       │
└──────────────────────────────────────┘   │ CsvLogger       metric history         │
                                           │ Logger          server log             │
┌─────────────── COMMON ───────────────┐   └────────────────────────────────────────┘
│ SystemData · Config · ConfigLoader   │
└──────────────────────────────────────┘
```

### 📁 Project structure

```text
linux-system-monitor/
├── client/
│   ├── main.cpp
│   ├── ClientIdentity.{h,cpp}
│   ├── NetworkClient.{h,cpp}
│   └── SystemMonitor.{h,cpp}
├── server/
│   ├── main.cpp
│   ├── Server.{h,cpp}
│   ├── ClientRegistry.{h,cpp}
│   ├── ClientState.h
│   ├── AlertManager.{h,cpp}
│   ├── Dashboard.{h,cpp}
│   ├── CsvLogger.{h,cpp}
│   ├── Logger.{h,cpp}
│   └── test_csv.cpp · test_dashboard.cpp · test_logger.cpp
├── common/
│   ├── SystemData.h
│   ├── Config.h
│   ├── ConfigLoader.{h,cpp}
│   └── test_config.cpp
├── config/
│   └── config.json
├── tests/
│   ├── test_metrics.cpp
│   └── test_protocol.cpp
├── logs/            # metrics.csv, server.log (generated)
├── data/
├── docs/
├── build/           # compiled binaries (generated)
├── Makefile
└── README.md
```

---

## ⚙️ Configuration

`config/config.json` holds the server address, port, client intervals, monitoring thresholds and heartbeat settings, and `ConfigLoader` can read it.

> 📝 **Heads-up:** the alert thresholds currently active at runtime are initialised directly in `Server.cpp`. The config file is not yet the source of truth for them.

---

## 🧪 Testing

```bash
make test
```

| Test | Verifies |
|---|---|
| **Metric test** | CPU, memory, disk and process count return sane values |
| **Protocol test** | System data round-trips correctly through the JSON protocol |

```text
CPU Usage: 0.504414%
Memory Usage: 77.6159%
Disk Usage: 34.0944%
Process Count: 312

Metric tests passed!
Protocol test passed!
```

**Also verified manually**

- ✅ JSON validation and rejection of bad input
- ✅ Alert state transitions
- ✅ Client disconnect → `OFFLINE` after the heartbeat timeout
- ✅ Client reconnection after server restart
- ✅ CSV persistence
- ✅ Graceful shutdown
- ✅ **Five simultaneous client processes** on concurrent TCP connections

Try the malformed-data test yourself:

```bash
echo "this is not json" | nc localhost 5000
```

The server should reject it and keep running.

---

## 🛠️ Built With

| Tool | Role |
|---|---|
| **C++17** | Core implementation |
| **POSIX sockets** | TCP networking |
| **`/proc` filesystem** | CPU, memory, uptime, processes |
| **`statvfs()`** | Disk usage |
| **nlohmann/json** | JSON serialization and parsing |
| **GNU Make** | Build automation |
| **Git** | Version control |

---

## ⚠️ Known Limitations

- **Linux only.** Metrics depend on `/proc`, so it won't run on Windows or macOS.
- **No encryption.** Traffic is plain TCP, with no TLS.
- **No authentication.** Clients are identified by ID only.
- **Terminal UI only.** There is no web or graphical frontend.
- **One identity per machine.** Several client processes on the same host share one persistent ID, so the server sees them as one logical client.
- **Config not fully wired in.** See the [Configuration](#️-configuration) note above.

---

## 🎓 What This Project Demonstrates

`Linux /proc internals` · `C++17` · `POSIX APIs` · `TCP socket programming` · `Client–server architecture` · `Concurrent connections` · `JSON serialization` · `Message framing` · `Input validation` · `State management` · `Logging & CSV persistence` · `Reconnection handling` · `Signal handling` · `Build automation` · `Automated testing`

---

## 📌 Project Status

```text
System Metrics       ✅      Dashboard            ✅
TCP Communication    ✅      Status Detection     ✅
JSON Protocol        ✅      Alert Management     ✅
Client Identity      ✅      CSV Logging          ✅
Client Registry      ✅      Server Logging       ✅
Reconnect Handling   ✅      Graceful Shutdown    ✅
Automated Tests      ✅      Makefile             ✅
```

**Current status: Functional.** The core client–server monitoring workflow is implemented and tested.

---

<div align="center">

### 👨‍💻 Author

**Suniyan Jana**
B.Tech, Computer Science & Engineering

*Built as a hands-on deep dive into Linux system programming, C++ networking and monitoring architecture.*

⭐ If this project helped or interested you, consider giving it a star!

</div>
