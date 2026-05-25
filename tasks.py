import os
import sys
import time
import signal
import subprocess
from invoke import task

# Paths
ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
BACKEND_DIR = os.path.join(ROOT_DIR, 'backend')
FRONTEND_DIR = os.path.join(ROOT_DIR, 'frontend')

# Output directories
BUILD_DIR = os.path.join(ROOT_DIR, '_build')
OUT_DIR = os.path.join(BUILD_DIR, 'out')
TEST_DIR = os.path.join(BUILD_DIR, 'test')
LOG_DIR = os.path.join(BUILD_DIR, 'log')

PID_FILE = os.path.join(LOG_DIR, 'backend.pid')
LOG_FILE = os.path.join(LOG_DIR, 'backend.log')

@task(help={
    'backend': "Clean only backend build files and outputs",
    'frontend': "Clean only frontend build caches and node_modules",
    'logs': "Clean log files as well (default: True)"
})
def clean(c, backend=False, frontend=False, logs=True):
    """Clean build directories, caches, and optionally logs"""
    # If neither is specified, default to cleaning both
    if not backend and not frontend:
        backend = True
        frontend = True

    print("🧹 Cleaning project files...")

    # Clean backend-specific build outputs
    if backend:
        # Clean backend CMake build directory
        backend_build = os.path.join(BACKEND_DIR, 'build')
        if os.path.exists(backend_build):
            c.run(f"rm -rf {backend_build}")
            print("  ✓ Cleaned backend temp build directory.")

        # Clean output and test folders (excluding log directory)
        if os.path.exists(OUT_DIR):
            c.run(f"rm -rf {OUT_DIR}")
            print("  ✓ Cleaned backend build output directory (_build/out).")
        if os.path.exists(TEST_DIR):
            c.run(f"rm -rf {TEST_DIR}")
            print("  ✓ Cleaned backend test output directory (_build/test).")

    # Clean frontend-specific cache/modules
    if frontend:
        next_dir = os.path.join(FRONTEND_DIR, '.next')
        node_dir = os.path.join(FRONTEND_DIR, 'node_modules')
        if os.path.exists(next_dir):
            c.run(f"rm -rf {next_dir}")
            print("  ✓ Cleaned frontend Next.js cache (.next).")
        if os.path.exists(node_dir):
            c.run(f"rm -rf {node_dir}")
            print("  ✓ Cleaned frontend node_modules.")

    # Clean logs if specified
    if logs:
        if os.path.exists(LOG_DIR):
            c.run(f"rm -rf {LOG_DIR}")
            print("  ✓ Cleaned log directory (_build/log).")

    # Clean root _build if it ends up completely empty
    if os.path.exists(BUILD_DIR) and not os.listdir(BUILD_DIR):
        c.run(f"rm -rf {BUILD_DIR}")

    print("✓ Project cleaned successfully.")

@task(help={
    'backend': "Build only the native C++ backend",
    'frontend': "Build only the frontend Docker image"
})
def build(c, backend=False, frontend=False):
    """Compile C++ backend and/or build Next.js Docker image, piping build logs to _build/log"""
    # If neither is specified, default to building both
    if not backend and not frontend:
        backend = True
        frontend = True

    print("🏗️  Building osxmon project...")
    os.makedirs(LOG_DIR, exist_ok=True)

    if backend:
        # 1. Compile C++ Backend Natively
        print("\nCompiling C++ Oat++ Backend natively on macOS...")
        backend_build_dir = os.path.join(BACKEND_DIR, 'build')
        os.makedirs(backend_build_dir, exist_ok=True)
        
        cfg_log = os.path.join(LOG_DIR, 'build_backend_configure.log')
        compile_log = os.path.join(LOG_DIR, 'build_backend_compile.log')
        
        with c.cd(BACKEND_DIR):
            print(f"  - Running CMake configure... (logging to _build/log/build_backend_configure.log)")
            c.run(f"cmake -B build -S . > {cfg_log} 2>&1")
            print(f"  - Compiling C++ binary...     (logging to _build/log/build_backend_compile.log)")
            c.run(f"cmake --build build -j$(sysctl -n hw.ncpu) > {compile_log} 2>&1")
        
        # Check if build was successful and placed in _build/out
        binary_path = os.path.join(OUT_DIR, 'osxmon_server')
        if os.path.exists(binary_path):
            print("  ✓ C++ Backend built successfully in '_build/out/osxmon_server'.")
        else:
            print(f"  ❌ Error: C++ compilation failed. Inspect build log: {compile_log}")
            sys.exit(1)

    if frontend:
        # 2. Build Frontend Docker Container
        frontend_log = os.path.join(LOG_DIR, 'build_frontend.log')
        print(f"\nBuilding frontend Next.js Docker image... (logging to _build/log/build_frontend.log)")
        with c.cd(ROOT_DIR):
            c.run(f"docker compose build > {frontend_log} 2>&1")
        print("  ✓ Frontend Docker image built successfully.")

    print("\n🎉 Build complete! Run 'inv start' to launch the dashboard.")

@task(help={
    'backend': "Rebuild only the native C++ backend",
    'frontend': "Rebuild only the frontend Docker image"
})
def rebuild(c, backend=False, frontend=False):
    """Stop running services and rebuild backend, frontend, or both"""
    # If neither is specified, default to rebuilding both
    if not backend and not frontend:
        backend = True
        frontend = True

    print("🔄 Starting rebuild process...")
    # Stop services first
    stop(c)
    print("")

    # Clean specified components, but preserve logs
    clean(c, backend=backend, frontend=frontend, logs=False)
    print("")

    # Build specified components
    build(c, backend=backend, frontend=frontend)

@task(help={
    'verbose': "Enable verbose logging for the C++ backend",
    'log_level': "Set log level for the C++ backend (verbose, debug, info, warning, error)"
})
def start(c, verbose=False, log_level='info'):
    """Start native C++ backend and containerized frontend"""
    print("🚀 Starting osxmon services...")

    # 1. Start C++ Backend Natively in Background
    binary_path = os.path.join(OUT_DIR, 'osxmon_server')
    if not os.path.exists(binary_path):
        print("❌ Error: Backend binary not found. Please run 'inv build' first.")
        sys.exit(1)

    # Ensure log directory exists
    os.makedirs(LOG_DIR, exist_ok=True)

    # Check if already running
    if os.path.exists(PID_FILE):
        with open(PID_FILE, 'r') as f:
            old_pid = f.read().strip()
        if old_pid:
            try:
                os.kill(int(old_pid), 0)
                print(f"⚠️  C++ Backend is already running natively (PID: {old_pid}).")
            except (ProcessLookupError, ValueError):
                os.remove(PID_FILE)

    if not os.path.exists(PID_FILE):
        print("  - Launching C++ Backend natively...")
        
        # Build executable arguments
        args = [binary_path]
        if verbose:
            args.append("-v")
        elif log_level:
            args.extend(["--log-level", log_level])

        with open(LOG_FILE, 'w') as log:
            # Spawn in a separate process group to daemonize
            p = subprocess.Popen(
                args,
                stdout=log,
                stderr=log,
                preexec_fn=os.setsid,
                cwd=ROOT_DIR
            )
            # Write PID file
            with open(PID_FILE, 'w') as f:
                f.write(str(p.pid))
            print(f"  ✓ C++ Backend started in background (PID: {p.pid}, logs: _build/log/backend.log).")

    # 2. Start Frontend Docker Container
    print("  - Launching Next.js frontend via Docker Compose...")
    c.run("docker compose up -d")
    print("  ✓ Frontend container started successfully.")

    # 3. Print URLs
    print("\n=======================================================")
    print(" 🎉 osxmon services are active!")
    print(" 🖥️  Frontend Dashboard: http://localhost:3000")
    print(" 🛠️  Swagger UI docs:    http://localhost:8000/swagger/ui")
    print(" 📁 Backend Logfile:    _build/log/backend.log")
    print("=======================================================\n")

@task
def stop(c):
    """Stop the native C++ backend and frontend Docker container"""
    print("🛑 Stopping osxmon services...")

    # 1. Stop Frontend container
    print("  - Stopping Next.js Docker container...")
    c.run("docker compose down")
    print("  ✓ Frontend container stopped.")

    # 2. Stop C++ Backend
    if os.path.exists(PID_FILE):
        print("  - Terminating native C++ Backend...")
        with open(PID_FILE, 'r') as f:
            pid_str = f.read().strip()
        if pid_str:
            try:
                pid = int(pid_str)
                # Terminate process group
                os.killpg(os.getpgid(pid), signal.SIGTERM)
                print(f"  ✓ Process group (PID: {pid}) terminated.")
            except (ProcessLookupError, ValueError):
                print(f"  ⚠️  Process (PID: {pid_str}) was not found running.")
            except Exception as e:
                print(f"  ⚠️  Failed to terminate process cleanly: {e}")
        os.remove(PID_FILE)
    else:
        print("  ✓ C++ Backend was not running.")

    print("✓ All osxmon services stopped.")

@task
def status(c):
    """Check the status of native and containerized services"""
    print("🔍 Checking osxmon service status...\n")

    backend_running = False
    frontend_running = False

    # Check backend
    if os.path.exists(PID_FILE):
        with open(PID_FILE, 'r') as f:
            pid_str = f.read().strip()
        if pid_str:
            try:
                pid = int(pid_str)
                os.kill(pid, 0)
                print(f"🟢 C++ Backend: RUNNING (PID: {pid}, host port 8000)")
                backend_running = True
            except (ProcessLookupError, ValueError):
                print("🔴 C++ Backend: STOPPED (PID file exists but process is dead)")
    else:
        print("🔴 C++ Backend: STOPPED")

    # Check frontend (via docker compose ps)
    try:
        res = subprocess.run(
            ["docker", "compose", "ps", "--format", "json"],
            capture_output=True,
            text=True
        )
        if "osxmon-frontend" in res.stdout:
            if '"State":"running"' in res.stdout or '"running"' in res.stdout:
                print("🟢 Next.js Frontend: RUNNING (Docker, port 3000)")
                frontend_running = True
            else:
                print("🟡 Next.js Frontend: CONTAINER EXISTS BUT NOT RUNNING")
        else:
            print("🔴 Next.js Frontend: STOPPED")
    except Exception as e:
        print(f"⚪ Next.js Frontend: UNABLE TO PROBE DOCKER ({e})")

    if backend_running and frontend_running:
        print("\n✨ All systems nominal! Visit http://localhost:3000")
    else:
        print("\n⚠️  Some services are offline. Run 'inv start' to boot them.")

@task(name='test')
def test(c):
    """Run all compiled test suites in _build/test, piping output to _build/log"""
    print("🧪 Running osxmon C++ test suites...")
    
    # Check if backend is running (which can cause port conflicts/failures in test suites)
    if os.path.exists(PID_FILE):
        print("⚠️  Warning: C++ Backend appears to be running. Some test suites (like ClientRetryTest) may fail due to port conflicts.")
        print("💡 Suggestion: Run 'inv stop' before running tests for a clean environment.\n")

    os.makedirs(LOG_DIR, exist_ok=True)
    
    if not os.path.exists(TEST_DIR):
        print("❌ Error: Test directory '_build/test' not found. Please run 'inv build' first.")
        sys.exit(1)

    test_binaries = ['oatppAllTests', 'module-tests']
    failed = False
    
    for test_bin in test_binaries:
        path = os.path.join(TEST_DIR, test_bin)
        if os.path.exists(path):
            log_path = os.path.join(LOG_DIR, f"test_{test_bin}.log")
            print(f"\n🏃 Running test suite: {test_bin}... (piping logs to _build/log/test_{test_bin}.log)")
            
            # Redirect stdout & stderr to the log file
            res = c.run(f"{path} > {log_path} 2>&1", warn=True)
            if res.failed:
                print(f"  ❌ Test suite {test_bin} failed! Check logs: {log_path}")
                failed = True
            else:
                print(f"  ✓ Test suite {test_bin} passed successfully.")
        else:
            print(f"  ⚠️  Warning: Test suite {test_bin} not found at {path}")
            
    if failed:
        print("\n❌ Some test suites failed!")
        sys.exit(1)
    else:
        print("\n🎉 All test suites completed successfully!")

@task(name='help')
def help_task(c, task_name=None):
    """List all tasks or show help for a specific task. Usage: inv help [--task-name <task>]"""
    if task_name:
        c.run(f"{sys.argv[0]} --help {task_name}")
    else:
        print("\n🛠️  osxmon Orchestration CLI Help")
        print("================================")
        print("To run a task, use: inv <task> [options]")
        print("\nAvailable Tasks:")
        c.run(f"{sys.argv[0]} --list")
        print("\nFor help on a specific task, run:")
        print("  inv help --task-name <task_name>")
        print("  or use invoke's built-in flag: invoke --help <task_name>\n")
