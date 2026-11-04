#!/bin/bash
# NuiLite & Sidekick Uninstaller for Modern Norns
# Restores standard Norns auto-booting and removes Sidekick services and binaries

set -e

if [ "$EUID" -ne 0 ]; then
    echo "Error: Please run this script with sudo:"
    echo "  sudo ./uninstall.sh [--purge]"
    exit 1
fi

echo "=========================================="
echo "Uninstalling NuiLite & Sidekick"
echo "=========================================="

# 1. Stop and disable Sidekick & Orac services
echo "Stopping Sidekick, Orac, and MEC services..."
systemctl stop sidekick.service orac.service mec.service 2>/dev/null || true
systemctl disable sidekick.service sidekick-init.service sidekick.target orac.service mec.service 2>/dev/null || true

# 2. Remove systemd unit files
echo "Removing systemd unit files..."
rm -f /etc/systemd/system/sidekick.service
rm -f /etc/systemd/system/sidekick.target
rm -f /etc/systemd/system/sidekick-init.service
rm -f /etc/systemd/system/orac.service
rm -f /etc/systemd/system/mec.service
systemctl daemon-reload

# 3. Remove deployed NuiLite / Sidekick binaries
echo "Removing deployed binaries and libraries..."
rm -f /usr/local/lib/libnuilite.so* /usr/local/lib/libcjson.so* /usr/local/lib/liboscpack.so* /usr/lib/libnuilite.so*
rm -rf /usr/local/sidekick
rm -f /usr/local/MEC/libnuilite.so
rm -f /usr/local/orac/externals/libnuilite.so
rm -f /usr/local/orac/externals/NuiPd.pd_linux
rm -f /usr/lib/pd/extra/NuiPd.pd_linux /usr/local/lib/pd/extra/NuiPd.pd_linux
rm -f /etc/ld.so.conf.d/nuilite.conf
ldconfig 2>/dev/null || true

# 4. Clean user directories
if [ "$1" = "--purge" ] || [ "$1" = "-f" ]; then
    echo "Purging /home/we/sidekick..."
    rm -rf /home/we/sidekick
else
    echo "Cleaning Sidekick runtime binaries from /home/we/sidekick..."
    rm -rf /home/we/sidekick/pdexternals
    rm -f /home/we/sidekick/sidekick.json /home/we/sidekick/sidekick-state.json
    echo "Note: Custom patches in /home/we/sidekick/patches were kept. Pass --purge to delete them."
fi

# 5. Restore standard Norns startup
echo "Restoring standard Norns booting..."
systemctl reset-failed 2>/dev/null || true
systemctl enable norns.target 2>/dev/null || true

systemctl restart norns-jack.service 2>/dev/null || true
systemctl restart norns-sclang.service 2>/dev/null || true

if [ -f /etc/systemd/system/norns-main.service ] || [ -f /lib/systemd/system/norns-main.service ]; then
    echo "Detected Norns 3.x layout (norns-main)..."
    systemctl enable norns-main.service 2>/dev/null || true
    systemctl restart norns-main.service 2>/dev/null || true
    systemctl start norns.target 2>/dev/null || true
    echo ""
    echo "=========================================="
    echo "Uninstallation complete!"
    echo "Standard Norns environment restored."
    echo "=========================================="
    systemctl status norns-main.service --no-pager || true
elif [ -f /etc/systemd/system/norns-matron.service ] || [ -f /lib/systemd/system/norns-matron.service ]; then
    echo "Detected Norns 2.x layout (norns-matron + norns-crone)..."
    systemctl enable norns-matron.service norns-crone.service 2>/dev/null || true
    systemctl restart norns-crone.service 2>/dev/null || true
    systemctl restart norns-matron.service 2>/dev/null || true
    systemctl start norns.target 2>/dev/null || true
    echo ""
    echo "=========================================="
    echo "Uninstallation complete!"
    echo "Standard Norns environment restored."
    echo "=========================================="
    systemctl status norns-matron.service --no-pager || true
else
    echo "Starting norns.target..."
    systemctl start norns.target 2>/dev/null || true
    echo ""
    echo "=========================================="
    echo "Uninstallation complete!"
    echo "Standard Norns environment restored."
    echo "=========================================="
    systemctl status norns.target --no-pager || true
fi
