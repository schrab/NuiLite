# NuiLite, Sidekick & Orac Installation Guide for Modern Norns

This document details the compilation, installation, configuration, and troubleshooting steps for running **NuiLite**, **Sidekick**, and **Orac** on modern Norns firmware (**v3.0.2 / 2.8.1+** on Raspbian Buster / Bullseye) on **Fates** (SSD1325 OLED) and **Stock Norns Shields** (SSD1322 OLED).

---

## 1. Background & Architecture

Starting with Norns version **231114 (v2.8.1)**, the Linux kernel framebuffer device (`/dev/fb0`) was completely removed in favor of direct userspace SPI drawing. Previous builds of Orac and Sidekick depended directly on `/dev/fb0`, causing them to fail on newer firmware with:
```
ERROR (screen) cannot open framebuffer device
```

### The Solution:
- **Userspace SPI Drivers**: NuiLite now draws directly to `/dev/spidev0.0` using `libgpiod` to manage D/C and Reset GPIO lines.
- **Dual Display Support**: Supports both **SSD1325** (Fates: 128x64 4-bit grayscale, DC=GPIO17, RST=GPIO4) and **SSD1322** (Stock Norns Shield: 256x64 4-bit grayscale).
- **Exclusive Display Handover**: Sidekick cleanly releases `/dev/spidev0.0` and GPIO lines before launching patches, allowing Norns and MEC/Orac full, unhindered access without bus contention.
- **Binary ABI Compatibility**: Exported legacy constructors and stubs ensure precompiled MEC binaries (`libmec-api.so`, `mec-app`) and Pure Data externals load without needing a recompile.

---

## 2. Dependencies & Prerequisites

Connect to your Norns device over SSH:
```bash
ssh we@norns.local   # or ssh fates (default password: sleep)
```

Install the required build dependencies:
```bash
sudo apt update
sudo apt install -y build-essential cmake git libgpiod-dev libcairo2-dev libfreetype6-dev puredata-dev
```

---

## 3. Compiling NuiLite & Sidekick

Clone the updated repository from GitHub:

```bash
cd /home/we
git clone https://github.com/schrab/NuiLite.git
cd NuiLite
```

Create a build directory and compile:

```bash
mkdir -p build && cd build
cmake ..
make -j2
```

This compiles:
- `build/release/lib/libnuilite.so`: Core hardware abstraction library.
- `build/release/bin/sidekick`: Sidekick application launcher daemon.
- `build/release/lib/NuiPd.pd_linux`: Pure Data hardware external.
- `build/release/bin/nuitest`: Hardware diagnostic test binary.

---

## 4. Deploying Binaries

Stop any active services before deploying:
```bash
sudo systemctl stop sidekick.service orac.service mec.service
```

Deploy the newly built binaries to all required locations on the filesystem:

```bash
# 1. Deploy libnuilite.so
sudo cp -f /home/we/NuiLite/build/release/lib/libnuilite.so /usr/local/sidekick/libnuilite.so
sudo cp -f /home/we/NuiLite/build/release/lib/libnuilite.so /usr/local/MEC/libnuilite.so
sudo cp -f /home/we/NuiLite/build/release/lib/libnuilite.so /usr/local/orac/externals/libnuilite.so
sudo cp -f /home/we/NuiLite/build/release/lib/libnuilite.so /usr/local/sidekick/nuisdk/libnuilite.so
mkdir -p /home/we/sidekick/pdexternals
cp -f /home/we/NuiLite/build/release/lib/libnuilite.so /home/we/sidekick/pdexternals/libnuilite.so

# 2. Deploy sidekick executable
sudo cp -f /home/we/NuiLite/build/release/bin/sidekick /usr/local/sidekick/sidekick
sudo cp -f /home/we/NuiLite/build/release/bin/sidekick /usr/local/sidekick/nuisdk/sidekick 2>/dev/null || true

# 3. Deploy NuiPd Pure Data external
sudo cp -f /home/we/NuiLite/build/release/lib/NuiPd.pd_linux /usr/local/orac/externals/NuiPd.pd_linux
sudo cp -f /home/we/NuiLite/build/release/lib/NuiPd.pd_linux /usr/local/sidekick/nuisdk/NuiPd.pd_linux 2>/dev/null || true
cp -f /home/we/NuiLite/build/release/lib/NuiPd.pd_linux /home/we/sidekick/pdexternals/NuiPd.pd_linux
```

---

## 5. Configuration (`sidekick.json`)

Ensure `/home/we/sidekick/sidekick.json` (and `/usr/local/sidekick/sidekick.json`) are configured for your hardware shield.

### For Fates (SSD1325):
```json
{
  "_description_": "Sidekick configuration for Fates",
  "systemDir": "./system",
  "patchDir": "./patches",
  "screenDriver": "ssd1325",
  "screenDcPin": 17,
  "screenResetPin": 4,
  "pollTime": 1,
  "activeTime": 3000,
  "loadOnStartup": true,
  "menuOnStartup": false,
  "stateFile": "./sidekick-state.json",
  "pdOpts" : "-nogui -rt -audiobuf 4 -alsamidi",
  "scOpts" : "",
  "scHome" : ".",
  "listenPort" : 3001,
  "sendPort" : 3000
}
```

### For Stock Norns Shield (SSD1322):
Change `screenDriver`, `screenDcPin`, and `screenResetPin`:
```json
  "screenDriver": "ssd1322",
  "screenDcPin": 27,
  "screenResetPin": 22,
```

Sync to both locations:
```bash
sudo cp -f /home/we/sidekick/sidekick.json /usr/local/sidekick/sidekick.json
```

---

## 6. Patch Scripts Setup

To prevent audio device locking and SuperCollider failures, the patch scripts must cleanly manage JACK vs. Pure Data.

### Norns Scripts (`/home/we/sidekick/patches/Norns/`)

**`run.sh`**:
```bash
#!/bin/bash
echo starting norns
sudo systemctl stop norns.target 2>/dev/null
sleep 1
sudo systemctl restart norns-jack.service
sleep 2
sudo systemctl reset-failed norns-main.service 2>/dev/null
sudo systemctl start norns.target
```

**`stop.sh`**:
```bash
#!/bin/bash
echo stopping norns
if [ -f ~/dust/data/system.state ]; then
    cp ~/dust/data/system.state /tmp/system.state && sed 's/norns.state.clean_shutdown = false/norns.state.clean_shutdown = true/' < /tmp/system.state > ~/dust/data/system.state
fi
sudo systemctl stop norns.target norns-main.service norns-matron.service norns-crone.service norns-sclang.service norns-maiden.service norns-jack.service
sleep 1
```

### Orac Scripts (`/home/we/sidekick/patches/Orac/`)

**`run.sh`**:
```bash
#!/bin/bash
echo starting orac
sudo systemctl stop norns-main.service norns-sclang.service norns-jack.service 2>/dev/null || true
sudo systemctl start orac.service
sleep 1
sudo systemctl start mec.service
```

**`stop.sh`**:
```bash
#!/bin/bash
echo stopping orac
sudo systemctl stop mec.service
sudo systemctl stop orac.service
killall -9 mec-app pd 2>/dev/null || true
sleep 1
```

Make scripts executable and sync:
```bash
chmod +x /home/we/sidekick/patches/Norns/*.sh /home/we/sidekick/patches/Orac/*.sh
sudo cp -f /home/we/sidekick/patches/Norns/*.sh /usr/local/sidekick/patches/Norns/
sudo cp -f /home/we/sidekick/patches/Orac/*.sh /usr/local/sidekick/patches/Orac/
```

---

## 7. Starting the Service

Reload systemd and start Sidekick:
```bash
sudo systemctl daemon-reload
sudo systemctl restart sidekick.service
```

Check status:
```bash
systemctl status sidekick.service --no-pager
```

---

## 8. Operating Instructions & Keybindings

### Accessing Sidekick Menu
- Press and hold **all 3 buttons (K1 + K2 + K3)** down simultaneously for **3 seconds**.
- The running application will stop and the Sidekick menu will appear.
- Rotate **Enc 1** to scroll through patches (`Norns`, `Orac`, etc.).
- Press **K1** to launch the selected patch.

### Controls in Orac
Orac operates using Organelle-style controls on Norns:

#### Main Performance View:
- **K1**: Open Orac Menu.
- **Hold K3 + Turn Enc 2**: Switch active module.
- **Hold K3 + Turn Enc 3**: Switch active page.
- **Enc 1, 2, 3, 4**: Adjust parameters on current page.
- **K2**: Aux button.

#### Menu View (after pressing K1):
- **Enc 1**: Scroll menu items.
- **K1**: Exit menu back to main view.
- **K2**: Back / Go up one directory.
- **K3**: Enter / Select / OK.
