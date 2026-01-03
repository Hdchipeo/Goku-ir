#!/bin/bash

# Configuration
PROJECT_NAME="goku-ir-device"
BUILD_DIR="build"

# Check if surge is installed
if ! command -v surge &> /dev/null; then
    echo "Surge is not installed. Installing... (requires Node.js)"
    npm install -g surge
fi

# Ensure build exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory found. Please build the project first (idf.py build)."
    exit 1
fi

# Create a dist directory for clean deployment
DIST_DIR="dist"
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR"

# Extract version from CMakeLists.txt
VERSION=$(grep 'set(PROJECT_VERSION' CMakeLists.txt | cut -d '"' -f 2)
echo "Deploying Version: $VERSION"

# Create version.txt in dist dir
echo "$VERSION" > "$DIST_DIR/version.txt"
echo "Created $DIST_DIR/version.txt"

# Copy Firmware Binary
if [ -f "$BUILD_DIR/$PROJECT_NAME.bin" ]; then
    cp "$BUILD_DIR/$PROJECT_NAME.bin" "$DIST_DIR/$PROJECT_NAME.bin"
    echo "Copied firmware binary to $DIST_DIR/"
else
    echo "Error: Firmware binary not found at $BUILD_DIR/$PROJECT_NAME.bin"
    exit 1
fi

# Handle CNAME for persistent domain
if [ -f "CNAME" ]; then
    cp CNAME "$DIST_DIR/CNAME"
    echo "Using domain from CNAME file: $(cat CNAME)"
else
    echo "First time setup: Enter your Surge domain to save it (e.g. lopsided-plantation.surge.sh)"
    echo "Press Enter to skip and use a random domain."
    read -p "Domain: " USER_DOMAIN
    
    if [ ! -z "$USER_DOMAIN" ]; then
        echo "$USER_DOMAIN" > "CNAME"
        echo "$USER_DOMAIN" > "$DIST_DIR/CNAME"
        echo "Saved domain to 'CNAME' file."
    fi
fi

echo "==========================================="
echo " Deploying to Surge.sh (Only essential files)..."
echo "==========================================="

# Run surge on dist/ folder
surge "$DIST_DIR"

echo ""
echo "Done! check your URL: https://$(cat $DIST_DIR/CNAME 2>/dev/null || echo 'your-domain.surge.sh')"
