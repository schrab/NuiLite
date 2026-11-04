#!/bin/bash
# NuiLite & Sidekick Installer for Modern Norns (v3.0.2 / 2.8.1+)
# Supports Stock Norns Shield, Fates v1.8 (SSD1322), and custom SSD1325

set -e

if [ "$EUID" -ne 0 ]; then
    echo "Error: Please run this script with sudo:"
    echo "  sudo ./install.sh [shield|fates]"
    exit 1
fi

TARGET="$1"

if [ -z "$TARGET" ]; then
    echo "Select target hardware:"
    echo "  1) Stock Norns Shield / Shield Pro (SSD1322, DC=GPIO5,  RST=GPIO6) [Default]"
    echo "  2) Fates v1.8                      (SSD1322, DC=GPIO17, RST=GPIO4)"
    echo "  3) Custom Fates Shield             (SSD1325, DC=GPIO17, RST=GPIO4)"
    read -p "Enter choice [1, 2 or 3, default 1]: " choice
    case "$choice" in
        2) TARGET="fates" ;;
        3) TARGET="ssd1325" ;;
        *) TARGET="shield" ;;
    esac
fi

TARGET=$(echo "$TARGET" | tr '[:upper:]' '[:lower:]')

case "$TARGET" in
    shield|norns|shield-pro|shieldpro)
        DRIVER_DESC="Stock Norns Shield / Shield Pro (SSD1322, DC=5, RST=6)"
        CONFIG_SRC="resources/sidekick/sidekick-shield.json"
        ;;
    fates)
        DRIVER_DESC="Fates v1.8 (SSD1322, DC=17, RST=4)"
        CONFIG_SRC="resources/sidekick/sidekick-fates.json"
        ;;
    ssd1325|fates-ssd1325|fates-shield|custom-ssd1325|custom)
        DRIVER_DESC="Custom Fates Shield (SSD1325, DC=17, RST=4)"
        CONFIG_SRC="resources/sidekick/sidekick-custom-ssd1325.json"
        ;;
    *)
        echo "Unknown target: $TARGET. Expected 'shield', 'fates', or 'ssd1325'."
        exit 1
        ;;
esac

echo "=========================================="
echo "Installing NuiLite & Sidekick for: $DRIVER_DESC"
echo "=========================================="

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# 1. Install build prerequisites & libraries
echo "Checking build prerequisites..."
BUILD_DEPS="build-essential cmake git libgpiod-dev libcairo2-dev libfreetype6-dev puredata-dev curl"
MISSING_DEPS=""
for pkg in $BUILD_DEPS; do
    if ! dpkg -s "$pkg" >/dev/null 2>&1; then
        MISSING_DEPS="$MISSING_DEPS $pkg"
    fi
done

if [ -n "$MISSING_DEPS" ]; then
    echo "Installing missing dependencies:$MISSING_DEPS..."
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y $MISSING_DEPS
fi

# 2. Check for base Orac & MEC packages (for vanilla / stock Norns)
if [ ! -d "/usr/local/orac" ] || [ ! -d "/usr/local/MEC" ]; then
    echo "=========================================="
    echo "Vanilla Norns detected (Orac/MEC base packages not found)."
    echo "Installing base packages from apt-norns..."
    echo "=========================================="
    if [ ! -f /etc/apt/sources.list.d/technobear.list ]; then
        echo "deb [trusted=yes] https://thetechnobear.github.io/apt-norns/ ./" > /etc/apt/sources.list.d/technobear.list
    fi
    apt-get update
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends orac mec sidekick
fi

# 3. Compile NuiLite binaries
echo "Compiling NuiLite binaries..."
mkdir -p build && cd build
cmake ..
make clean 2>/dev/null || true
make -j2
cd "$SCRIPT_DIR"

# 4. Stop running services
echo "Stopping existing services..."
systemctl stop sidekick.service orac.service mec.service 2>/dev/null || true

# 5. Create required directories
echo "Creating filesystem directories..."
mkdir -p /usr/local/sidekick/nuisdk
mkdir -p /usr/local/sidekick/patches/Norns
mkdir -p /usr/local/sidekick/patches/Orac
mkdir -p /usr/local/MEC
mkdir -p /usr/local/orac/externals
mkdir -p /home/we/sidekick/pdexternals
mkdir -p /home/we/sidekick/patches/Norns
mkdir -p /home/we/sidekick/patches/Orac
mkdir -p /home/we/sidekick/system

# 6. Deploy libraries and executables
echo "Deploying binaries..."
mkdir -p /usr/local/lib /usr/lib /usr/local/sidekick /usr/local/sidekick/nuisdk /usr/local/MEC /usr/local/orac/externals /home/we/sidekick/pdexternals

cp -f build/release/lib/*.so* /usr/local/lib/ 2>/dev/null || true
cp -f build/release/lib/*.so* /usr/lib/ 2>/dev/null || true
cp -f build/release/lib/*.so* /usr/local/sidekick/ 2>/dev/null || true
cp -f build/release/lib/*.so* /usr/local/sidekick/nuisdk/ 2>/dev/null || true
cp -f build/release/lib/*.so* /usr/local/MEC/ 2>/dev/null || true
cp -f build/release/lib/*.so* /usr/local/orac/externals/ 2>/dev/null || true
cp -f build/release/lib/*.so* /home/we/sidekick/pdexternals/ 2>/dev/null || true

echo "/usr/local/lib" > /etc/ld.so.conf.d/nuilite.conf
echo "/usr/local/sidekick" >> /etc/ld.so.conf.d/nuilite.conf
ldconfig 2>/dev/null || true

cp -f build/release/bin/sidekick /usr/local/sidekick/sidekick
cp -f build/release/bin/sidekick /usr/local/sidekick/nuisdk/sidekick 2>/dev/null || true
chmod +x /usr/local/sidekick/sidekick /usr/local/sidekick/nuisdk/sidekick 2>/dev/null || true

# Deploy NuiPd Pure Data external
mkdir -p /usr/lib/pd/extra /usr/local/lib/pd/extra /home/we/pd-externals 2>/dev/null || true
cp -f build/release/lib/NuiPd.pd_linux /usr/local/orac/externals/NuiPd.pd_linux
cp -f build/release/lib/NuiPd.pd_linux /usr/local/sidekick/nuisdk/NuiPd.pd_linux 2>/dev/null || true
cp -f build/release/lib/NuiPd.pd_linux /home/we/sidekick/pdexternals/NuiPd.pd_linux
cp -f build/release/lib/NuiPd.pd_linux /usr/lib/pd/extra/NuiPd.pd_linux 2>/dev/null || true
cp -f build/release/lib/NuiPd.pd_linux /usr/local/lib/pd/extra/NuiPd.pd_linux 2>/dev/null || true
cp -f build/release/lib/NuiPd.pd_linux /home/we/pd-externals/NuiPd.pd_linux 2>/dev/null || true

# Deploy demo patches
if [ -d "demos" ]; then
    # Clean up legacy demo directories from root patches and old flat Demos folder
    rm -rf /home/we/sidekick/patches/SimplePd /home/we/sidekick/patches/PolyPd /home/we/sidekick/patches/simple /home/we/sidekick/patches/grainbuf /home/we/sidekick/patches/livecoding /home/we/sidekick/patches/default /home/we/sidekick/patches/Demos 2>/dev/null || true
    rm -rf /usr/local/sidekick/patches/SimplePd /usr/local/sidekick/patches/PolyPd /usr/local/sidekick/patches/simple /usr/local/sidekick/patches/grainbuf /usr/local/sidekick/patches/livecoding /usr/local/sidekick/patches/default /usr/local/sidekick/patches/Demos 2>/dev/null || true

    mkdir -p /home/we/sidekick/patches/demos /usr/local/sidekick/patches/demos
    cp -rf demos/pd /home/we/sidekick/patches/demos/ 2>/dev/null || true
    cp -rf demos/sc /home/we/sidekick/patches/demos/ 2>/dev/null || true
    cp -rf demos/pd /usr/local/sidekick/patches/demos/ 2>/dev/null || true
    cp -rf demos/sc /usr/local/sidekick/patches/demos/ 2>/dev/null || true
    chown -R we:we /home/we/sidekick/patches 2>/dev/null || true
fi

# 7. Deploy configuration (sidekick.json)
echo "Configuring sidekick.json ($DRIVER_DESC)..."
cp -f "$CONFIG_SRC" /home/we/sidekick/sidekick.json
cp -f "$CONFIG_SRC" /usr/local/sidekick/sidekick.json

# 8. Deploy patch scripts
echo "Deploying patch launcher scripts..."
cp -f resources/sidekick/patches/Norns/*.sh /home/we/sidekick/patches/Norns/
cp -f resources/sidekick/patches/Orac/*.sh /home/we/sidekick/patches/Orac/
cp -f resources/sidekick/patches/Norns/*.sh /usr/local/sidekick/patches/Norns/
cp -f resources/sidekick/patches/Orac/*.sh /usr/local/sidekick/patches/Orac/
chmod +x /home/we/sidekick/patches/Norns/*.sh /home/we/sidekick/patches/Orac/*.sh
chmod +x /usr/local/sidekick/patches/Norns/*.sh /usr/local/sidekick/patches/Orac/*.sh

# 9. Deploy systemd service units
echo "Installing systemd unit files..."
cp -f resources/sidekick/sidekick.service /etc/systemd/system/
cp -f resources/sidekick/sidekick.target /etc/systemd/system/
cp -f resources/sidekick/sidekick-init.service /etc/systemd/system/

if [ ! -f /etc/systemd/system/orac.service ]; then
    cp -f resources/sidekick/orac.service /etc/systemd/system/
fi
if [ ! -f /etc/systemd/system/mec.service ]; then
    cp -f resources/sidekick/mec.service /etc/systemd/system/
fi

# 10. Set permissions
chown -R we:we /home/we/sidekick 2>/dev/null || true

# 11. Enable and restart service
echo "Reloading systemd and starting sidekick.service..."
systemctl daemon-reload
systemctl enable sidekick.service
systemctl restart sidekick.service

echo ""
echo "Installation complete!"
systemctl status sidekick.service --no-pager
