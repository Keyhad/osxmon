#!/bin/bash
set -e

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${BLUE}=== osxmon Environment Initialization ===${NC}"

# 1. Verify python3 is installed
if ! command -v python3 &> /dev/null; then
    echo -e "${RED}Error: python3 is not installed or not in PATH.${NC}"
    echo -e "Please install Python 3.9+ and try again."
    exit 1
fi

PYTHON_VERSION=$(python3 -c 'import sys; print(".".join(map(str, sys.version_info[:2])))')
echo -e "Found Python version: ${GREEN}${PYTHON_VERSION}${NC}"

# 2. Setup virtual environment
if [ -d ".venv" ]; then
    echo -e "${YELLOW}Virtual environment (.venv) already exists.${NC}"
    echo -e "Re-installing requirements..."
else
    echo -e "Creating virtual environment in ${GREEN}.venv${NC}..."
    python3 -m venv .venv
    echo -e "✓ Virtual environment created."
fi

# 3. Upgrade pip and install dependencies
echo -e "Upgrading pip and installing requirements..."
./.venv/bin/python3 -m pip install --upgrade pip
./.venv/bin/pip install -r requirements.txt

echo -e "\n${GREEN}✓ Setup complete!${NC}"
echo -e "You can now run commands using the virtual environment."
echo -e "Examples:"
echo -e "  Build project:      ${BLUE}./.venv/bin/invoke build${NC}"
echo -e "  Start services:     ${BLUE}./.venv/bin/invoke start${NC}"
echo -e "  Check status:       ${BLUE}./.venv/bin/invoke status${NC}"
echo -e "  Stop services:      ${BLUE}./.venv/bin/invoke stop${NC}"
