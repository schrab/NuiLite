# Manual Installation & Architecture Guide

This document details the manual compilation, installation, configuration, and troubleshooting steps for running **NuiLite**, **Sidekick**, and **Orac** on modern Norns firmware (**v3.0.2 / 2.8.1+**) without using the automated installer.

> **Tip:** For automated setup that performs all these steps automatically, see [README.md](README.md) (`sudo ./install.sh`).

---

## 1. Problem Overview & Solution

### The Problem:
On Norns firmware 2.8.1 and 3.0.2+, the legacy Linux kernel framebuffer (`/dev/fb0`) driver was removed. Standard builds of Sidekick, Orac, and custom Pure Data patches that rely on `/dev/fb0` crash on startup with:
```
ERROR (screen) cannot open framebuffer device
```

### The Solution:
- **Userspace SPI Drivers**: NuiLite now draws directly to `/dev/spidev0.0` using `libgpiod` to manage D/C and Reset GPIO lines.
- **SSD1322 OLED Support**: Supports the standard **SSD1322** display (128x64 4-bit grayscale) used by both **Stock Norns Shields** (DC=GPIO5, RST=GPIO6) and **Fates v1.8** (DC=GPIO17, RST=GPIO4).
- **Exclusive Display Handover**: Sidekick cleanly releases `/dev/spidev0.0` and GPIO lines before launching patches, allowing Norns and MEC/Orac full, unhindered access without bus contention.
- **Binary ABI Compatibility**: Exported legacy constructors and stubs ensure precompiled MEC binaries (`libmec-api.so`, `mec-app`) and Pure Data externals load without needing a recompile.

---

## 2. Dependencies & Base Packages

Connect to your Norns device over SSH:
```bash
ssh we@norns.local   # or ssh fates (default password: sleep)
```

### Build Dependencies:
```bash
sudo apt update
sudo apt install -y build-essential cmake git libgpiod-dev libcairo2-dev libfreetype6-dev puredata-dev curl
```

### Base Packages (For Vanilla / Stock Norns Systems):
If you are starting from a clean stock Norns image where Sidekick and Orac were never installed before:

```bash
sudo curl -fsSL https://thetechnobear.github.io/apt-norns/install.sh | sh
sudo apt update
sudo apt install -y sidekick orac mec
```
This sets up Orac's Pure Data sound engine, modules, presets, and the MEC daemon.  
*(On disk images that already ship Orac, these base packages and directories are already present).*

---

## 3. Manual Compilation

Clone and compile `NuiLite`:

```bash
cd /home/we
git clone https://github.com/schrab/NuiLite.git
cd NuiLite
mkdir -p build && cd build
cmake ..
make -j2
cd /home/we/NuiLite
```

This compiles:
- `build/release/lib/libnuilite.so`: Core hardware abstraction library.
- `build/release/bin/sidekick`: Sidekick application launcher daemon.
- `build/release/lib/NuiPd.pd_linux`: Pure Data hardware external.
- `build/release/bin/nuitest`: Hardware diagnostic test binary.

---

## 4. Manual Deployment Steps

### Step 1: Stop Running Services
```bash
sudo systemctl stop sidekick.service orac.service mec.service 2>/dev/null || true
```

### Step 2: Create Target Directories
```bash
sudo mkdir -p /usr/local/sidekick/nuisdk /usr/local/MEC /usr/local/orac/externals
sudo mkdir -p /usr/local/sidekick/patches/Norns /usr/local/sidekick/patches/Orac
mkdir -p /home/we/sidekick/pdexternals /home/we/sidekick/system
mkdir -p /home/we/sidekick/patches/Norns /home/we/sidekick/patches/Orac
```

### Step 3: Deploy Binaries
```bash
# Deploy libnuilite.so
sudo cp -f /home/we/NuiLite/build/release/lib/libnuilite.so /usr/local/lib/libnuilite.so
sudo cp -f /home/we/NuiLite/build/release/lib/libnuilite.so /usr/local/sidekick/libnuilite.so
sudo cp -f /home/we/NuiLite/build/release/lib/libnuilite.so /usr/local/sidekick/nuisdk/libnuilite.so
sudo cp -f /home/we/NuiLite/build/release/lib/libnuilite.so /usr/local/MEC/libnuilite.so
sudo cp -f /home/we/NuiLite/build/release/lib/libnuilite.so /usr/local/orac/externals/libnuilite.so
cp -f /home/we/NuiLite/build/release/lib/libnuilite.so /home/we/sidekick/pdexternals/libnuilite.so
sudo ldconfig

# Deploy sidekick binary
sudo cp -f /home/we/NuiLite/build/release/bin/sidekick /usr/local/sidekick/sidekick
sudo cp -f /home/we/NuiLite/build/release/bin/sidekick /usr/local/sidekick/nuisdk/sidekick 2>/dev/null || true

# Deploy NuiPd Pure Data external
sudo mkdir -p /usr/lib/pd/extra /usr/local/lib/pd/extra /home/we/pd-externals 2>/dev/null || true
sudo cp -f /home/we/NuiLite/build/release/lib/NuiPd.pd_linux /usr/local/orac/externals/NuiPd.pd_linux
sudo cp -f /home/we/NuiLite/build/release/lib/NuiPd.pd_linux /usr/local/sidekick/nuisdk/NuiPd.pd_linux 2>/dev/null || true
sudo cp -f /home/we/NuiLite/build/release/lib/NuiPd.pd_linux /usr/lib/pd/extra/NuiPd.pd_linux 2>/dev/null || true
sudo cp -f /home/we/NuiLite/build/release/lib/NuiPd.pd_linux /usr/local/lib/pd/extra/NuiPd.pd_linux 2>/dev/null || true
cp -f /home/we/NuiLite/build/release/lib/NuiPd.pd_linux /home/we/sidekick/pdexternals/NuiPd.pd_linux
cp -f /home/we/NuiLite/build/release/lib/NuiPd.pd_linux /home/we/pd-externals/NuiPd.pd_linux
```

### Step 4: Configure `sidekick.json`

For **Stock Norns Shield (SSD1322, DC=5, RST=6)**:
```bash
cp -f /home/we/NuiLite/resources/sidekick/sidekick-shield.json /home/we/sidekick/sidekick.json
sudo cp -f /home/we/NuiLite/resources/sidekick/sidekick-shield.json /usr/local/sidekick/sidekick.json
```

For **Fates v1.8 (SSD1322, DC=17, RST=4)**:
```bash
cp -f /home/we/NuiLite/resources/sidekick/sidekick-fates.json /home/we/sidekick/sidekick.json
sudo cp -f /home/we/NuiLite/resources/sidekick/sidekick-fates.json /usr/local/sidekick/sidekick.json
```

> **Hardware Verification:** Run `build/release/bin/nuitest` with `sidekick.json` in the current directory to verify that display patterns and buttons/encoders respond.

### Step 5: Deploy Patch Handover Scripts
The patch scripts manage clean transitions between Norns (JACK + SuperCollider) and Pure Data (direct ALSA):

```bash
cp -f /home/we/NuiLite/resources/sidekick/patches/Norns/*.sh /home/we/sidekick/patches/Norns/
cp -f /home/we/NuiLite/resources/sidekick/patches/Orac/*.sh /home/we/sidekick/patches/Orac/
sudo cp -f /home/we/NuiLite/resources/sidekick/patches/Norns/*.sh /usr/local/sidekick/patches/Norns/
sudo cp -f /home/we/NuiLite/resources/sidekick/patches/Orac/*.sh /usr/local/sidekick/patches/Orac/
chmod +x /home/we/sidekick/patches/Norns/*.sh /home/we/sidekick/patches/Orac/*.sh
sudo chmod +x /usr/local/sidekick/patches/Norns/*.sh /usr/local/sidekick/patches/Orac/*.sh
```

### Step 6: Install Systemd Services
```bash
sudo cp -f /home/we/NuiLite/resources/sidekick/sidekick.service /etc/systemd/system/
sudo cp -f /home/we/NuiLite/resources/sidekick/sidekick.target /etc/systemd/system/
sudo cp -f /home/we/NuiLite/resources/sidekick/sidekick-init.service /etc/systemd/system/

sudo chown -R we:we /home/we/sidekick
sudo systemctl daemon-reload
sudo systemctl enable sidekick.service
sudo systemctl restart sidekick.service
```

Verify service status:
```bash
systemctl status sidekick.service --no-pager
```

---

## 5. Controls & Keybindings

### Sidekick Menu
- **Activate Menu**: Press and hold **all 3 buttons (K1 + K2 + K3)** simultaneously for **3 seconds**.
- **Navigate**: Rotate **Enc 1** to select a patch (`Norns`, `Orac`, etc.).
- **Launch**: Press **K1** to launch the selected patch.

### Controls in Orac
Orac operates using Organelle-style controls on Norns:

#### Main Performance View:
- **K1**: Open Orac Menu.
- **Hold K3 + Turn Enc 2**: Switch active module slot.
- **Hold K3 + Turn Enc 3**: Switch active parameter page.
- **Enc 1, 2, 3, 4**: Adjust parameters on current page.
- **K2**: Aux button.

#### Menu View (after pressing K1):
- **Enc 1**: Scroll menu items.
- **K1**: Exit menu back to main view.
- **K2**: Back / Go up one directory.
- **K3**: Enter / Select / OK.

---

## 6. Uninstallation & Returning to Stock Norns

### Temporary Return to Stock Norns:
```bash
# Works on both Norns 2.x and 3.x:
sudo systemctl disable --now sidekick.service
sudo systemctl enable --now norns-matron.service norns-crone.service norns-main.service norns.target 2>/dev/null || true
```

To re-enable Sidekick:
```bash
sudo systemctl disable --now norns-matron.service norns-crone.service norns-main.service norns-sclang.service 2>/dev/null || true
sudo systemctl enable --now sidekick.service
```

### Full Uninstallation:
Run the included uninstaller:
```bash
cd /home/we/NuiLite
sudo ./uninstall.sh
```
*(Add `--purge` to delete `/home/we/sidekick` completely).*
