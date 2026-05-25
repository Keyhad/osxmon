# osxmon

A high-performance, lightweight macOS system resource monitor and telemetry dashboard. 

The application utilizes a **native C++ backend** powered by the **Oat++ framework** to query Mach kernel and sysctl APIs with zero-overhead, coupled with a **Next.js frontend** rendering animated **SVG Gauges** in a premium, glassmorphic dark-mode interface.

---

## 🏗️ Architecture

To accurately monitor host performance while keeping a modern containerized workflow, `osxmon` uses a hybrid architecture:

*   **Backend (C++ / Oat++)**: Runs natively on the host Mac. Spawning Docker containers on macOS forces execution inside a Linux virtual machine, rendering host system metrics (like macOS process PIDs or Mach memory states) inaccessible. Running the C++ code natively allows direct host telemetry access.
*   **Frontend (Next.js)**: Runs in a lightweight, containerized Alpine environment mapped to port `3000`. It communicates with the host API via `host.docker.internal`.
*   **Orchestration**: Managed via Python `invoke` task runner and `docker-compose`.

---

## 🛠️ Setup

### Prerequisites
Make sure your system has:
1.  **Xcode Command Line Tools** (for compilers): Install by running `xcode-select --install`.
2.  **CMake** (v3.15+): Install via Homebrew: `brew install cmake`.
3.  **Docker Desktop** (for frontend containerization): Download from [docker.com](https://www.docker.com/).

### 1. Initialize Virtual Environment
Initialize the Python virtual environment (`.venv`) and install dependencies (like `invoke`):
```bash
./init.sh
```

### 2. Build the Project
Compile the C++ backend natively and build the Next.js Docker image:
```bash
./.venv/bin/invoke build
```
*   **C++ Executable** compiles to: `_build/out/osxmon_server`
*   **C++ Unit Tests** compile to: `_build/test/`
*   **Build Logs** are saved to: `_build/log/`

### 3. Rebuild (Auto-Stop & Rebuild)
To rebuild backend/frontend when services are already running (this task stops them first before performing the build):
```bash
./.venv/bin/invoke rebuild            # Rebuilds both backend and frontend
./.venv/bin/invoke rebuild --backend  # Or -b: Rebuilds C++ backend only
./.venv/bin/invoke rebuild --frontend # Or -f: Rebuilds frontend image only
```

---

## 💻 Develop

### Project Structure
```
osxmon/
├── backend/                  # C++ Oat++ Backend code
│   ├── src/
│   │   ├── dto/              # Data Transfer Objects (JSON mapping)
│   │   ├── service/          # macOS Mach & sysctl native metrics
│   │   ├── controller/       # HTTP endpoints & OpenAPI annotations
│   │   ├── App.cpp           # Entry point
│   │   └── AppComponent.hpp  # Connection and dependency configuration
│   └── CMakeLists.txt
├── frontend/                 # Next.js App Router project
│   ├── src/
│   │   ├── components/       # SVG Gauges, Sparklines, Process Table
│   │   └── app/              # Globals styling and homepage page.tsx
│   ├── Dockerfile            # Multi-stage production container setup
│   └── package.json
├── tasks.py                  # Python Invoke task definitions
├── docker-compose.yml        # Frontend compose mapper
├── init.sh                   # Environment setup bootstrap script
└── requirements.txt
```

### Run Unit Tests
To run the C++ unit tests (Oat++ Core stress tests and swagger controller tests):
```bash
./.venv/bin/invoke test
```
*   Stdout and stderr logs for tests are written to `_build/log/test_oatppAllTests.log` and `_build/log/test_module-tests.log`.

### Clean Temporary Files
To wipe all builds, caches, and node modules:
```bash
./.venv/bin/invoke clean
```

---

## 🚀 Deploy / Run

Manage the system runtime using the `invoke` tasks wrapper.

### Start the Dashboard
Starts the C++ backend daemon on the host and boots the containerized Next.js frontend:
```bash
./.venv/bin/invoke start
```
You can customize the backend logging level by passing options:
```bash
./.venv/bin/invoke start --verbose           # Or -v: Enable verbose/debug logs
./.venv/bin/invoke start --log-level debug   # Or -l debug: Select log level (verbose, debug, info, warning, error)
```
*   **Frontend URL**: [http://localhost:3000](http://localhost:3000)
*   **Swagger API UI Docs**: [http://localhost:8000/swagger/ui](http://localhost:8000/swagger/ui)
*   **OpenAPI JSON Specification**: [http://localhost:8000/api-docs/oas-3.0.0.json](http://localhost:8000/api-docs/oas-3.0.0.json)
*   **Log Output**: Server daemon outputs are saved directly to `_build/log/backend.log` and PID to `_build/log/backend.pid`. When request logging is enabled (e.g., at `info`, `debug`, or `verbose` levels), incoming requests are logged.

### Check Uptime Status
Verify that both backend and frontend layers are active:
```bash
./.venv/bin/invoke status
```

### Stop the Dashboard
Gracefully shuts down Docker containers and terminates the native C++ backend daemon process group:
```bash
./.venv/bin/invoke stop
```

---

## 📖 User Manual

### 1. Telemetry Gauges
*   **CPU Load**: Displays real-time user-mode vs. kernel/system-mode loads. Features an interactive historical sparkline showing CPU spikes over the last 30 intervals.
*   **Physical RAM**: Renders overall memory usage (wired, active, inactive, and compressed pages). Wired and active states represent memory pressure.
*   **Disk Storage**: Lists your root partition `/` and mounted volumes. Features dynamic capacity progress bars.
*   **Network Interfaces**: Lists active physical ports (like wifi `en0`, bridge adapters, etc.) showing incoming and outgoing bytes and real-time speeds (KB/s, MB/s).

### 2. Active Processes List
*   Monitors resource usage by process ID (PID).
*   **Search**: Filter active processes dynamically by name or PID.
*   **Sorting**: Sort descending/ascending by clicking table headers for **PID**, **Process Name**, **CPU%**, or **Memory**.

### 3. Tuning & Low-Resource Configuration
The control center is situated on the right sidebar and contains configurations to minimize resource overhead:
*   **Pause Button**: Temporarily suspends frontend polling requests. When paused, the C++ backend consumes **0% CPU** as telemetry gathers are strictly calculated on-demand.
*   **Refresh Slider**: Sets the polling interval (0.5 seconds to 10 seconds).
*   **Telemetry Toggles**: Disables tracking for specific blocks (e.g., processes or network). When toggled off, the frontend excludes these fields from active configurations, prompting the C++ backend to skip calling corresponding sysctl or kernel queries, saving extra host CPU cycles.
*   **Row Limit Slider**: Restricts the maximum process rows fetched, reducing sorting calculation times on the backend.
