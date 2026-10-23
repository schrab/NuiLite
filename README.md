# NuiLite & Sidekick for Modern Norns

**NuiLite** is a C++ hardware abstraction library for Monome Norns and custom Fates hardware.  
**Sidekick** is an application switcher daemon that allows launching and toggling between Norns, Orac (Pure Data), and other custom environments directly from hardware buttons.  
**NuiPd** is a Pure Data external providing display and hardware input bindings.

This fork updates NuiLite, Sidekick, and Orac to work with **modern Norns firmware (v3.0.2 / 2.8.1+)** following the upstream removal of the Linux framebuffer device (`/dev/fb0`).

---

## Key Features in this Fork

- **Direct Userspace SPI Driver**: Replaces obsolete `/dev/fb0` kernel framebuffer rendering with high-performance direct userspace SPI transfers via `/dev/spidev0.0` and `libgpiod`.
- **Dual Display Architecture**:
  - **SSD1325**: 128x64 4-bit grayscale OLED for custom **Fates** shields (DC=GPIO17, RST=GPIO4).
  - **SSD1322**: 256x64 4-bit grayscale OLED for standard **Norns shields** (DC=GPIO27, RST=GPIO22).
- **Exclusive Display Handover**: Sidekick cleanly releases the SPI device and GPIO lines upon launching a patch, preventing bus contention, scanlines, and vertical scrolling artifacts.
- **Binary ABI Compatibility**: Exported legacy constructors and stubs allow precompiled MEC binaries (`libmec-api.so`, `mec-app`) and Pure Data externals to link and run without rebuilding Orac from scratch.
- **Robust Audio Stack Handover**: Sequences ALSA soundcard access cleanly between Pure Data (exclusive direct ALSA) and Norns (JACK + SuperCollider).
- **Crash Protection**: Wrapped OSC message parsing in `try/catch` to guard against malformed UDP packets.

---

## Prerequisites & Dependencies

On your Norns / Fates device (Raspbian Buster / Bullseye):

```bash
sudo apt update
sudo apt install -y build-essential cmake git libgpiod-dev libcairo2-dev libfreetype6-dev puredata-dev
```

---

## Compilation

Clone this repository and compile with CMake:

```bash
cd /home/we
git clone https://github.com/schrab/NuiLite.git
cd NuiLite
mkdir -p build && cd build
cmake ..
make -j2
```

### Build Artifacts:
- `build/release/lib/libnuilite.so`: Core hardware library.
- `build/release/bin/sidekick`: Application switcher daemon.
- `build/release/lib/NuiPd.pd_linux`: Pure Data hardware external.
- `build/release/bin/nuitest`: Hardware diagnostic test binary.

---

## Installation & Deployment

Stop running services before copying binaries:
```bash
sudo systemctl stop sidekick.service orac.service mec.service
```

Deploy the updated binaries to their target directories:

```bash
# 1. Deploy libnuilite.so
sudo cp -f /home/we/NuiLite/build/release/lib/libnuilite.so /usr/local/sidekick/libnuilite.so
sudo cp -f /home/we/NuiLite/build/release/lib/libnuilite.so /usr/local/MEC/libnuilite.so
sudo cp -f /home/we/NuiLite/build/release/lib/libnuilite.so /usr/local/orac/externals/libnuilite.so
sudo cp -f /home/we/NuiLite/build/release/lib/libnuilite.so /usr/local/sidekick/nuisdk/libnuilite.so
mkdir -p /home/we/sidekick/pdexternals
cp -f /home/we/NuiLite/build/release/lib/libnuilite.so /home/we/sidekick/pdexternals/libnuilite.so

# 2. Deploy sidekick binary
sudo cp -f /home/we/NuiLite/build/release/bin/sidekick /usr/local/sidekick/sidekick
sudo cp -f /home/we/NuiLite/build/release/bin/sidekick /usr/local/sidekick/nuisdk/sidekick 2>/dev/null || true

# 3. Deploy NuiPd Pure Data external
sudo cp -f /home/we/NuiLite/build/release/lib/NuiPd.pd_linux /usr/local/orac/externals/NuiPd.pd_linux
sudo cp -f /home/we/NuiLite/build/release/lib/NuiPd.pd_linux /usr/local/sidekick/nuisdk/NuiPd.pd_linux 2>/dev/null || true
cp -f /home/we/NuiLite/build/release/lib/NuiPd.pd_linux /home/we/sidekick/pdexternals/NuiPd.pd_linux
```

---

## Configuration (`sidekick.json`)

Sidekick preferences can be set in `/home/we/sidekick/sidekick.json` (and mirrored in `/usr/local/sidekick/sidekick.json`).

### For Fates (SSD1325 OLED):
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

### For Stock Norns Shield (SSD1322 OLED):
```json
  "screenDriver": "ssd1322",
  "screenDcPin": 27,
  "screenResetPin": 22,
```

Sync changes:
```bash
sudo cp -f /home/we/sidekick/sidekick.json /usr/local/sidekick/sidekick.json
```

---

## Patch Handover Scripts

To prevent audio hardware contention and SuperCollider startup failure:

### Norns Scripts (`/home/we/sidekick/patches/Norns/`)

- **`run.sh`**:
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

- **`stop.sh`**:
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

- **`run.sh`**:
  ```bash
  #!/bin/bash
  echo starting orac
  sudo systemctl stop norns-main.service norns-sclang.service norns-jack.service 2>/dev/null || true
  sudo systemctl start orac.service
  sleep 1
  sudo systemctl start mec.service
  ```

- **`stop.sh`**:
  ```bash
  #!/bin/bash
  echo stopping orac
  sudo systemctl stop mec.service
  sudo systemctl stop orac.service
  killall -9 mec-app pd 2>/dev/null || true
  sleep 1
  ```

Make executable and copy to `/usr/local/sidekick/patches/`:
```bash
chmod +x /home/we/sidekick/patches/Norns/*.sh /home/we/sidekick/patches/Orac/*.sh
sudo cp -f /home/we/sidekick/patches/Norns/*.sh /usr/local/sidekick/patches/Norns/
sudo cp -f /home/we/sidekick/patches/Orac/*.sh /usr/local/sidekick/patches/Orac/
```

---

## Service Management

Reload systemd daemon and start Sidekick:
```bash
sudo systemctl daemon-reload
sudo systemctl restart sidekick.service
```

Check status:
```bash
systemctl status sidekick.service --no-pager
```

---

## Hardware Controls & Usage

### Sidekick Menu
- **Activate Menu**: Press and hold **all 3 buttons (K1 + K2 + K3)** for **3 seconds**.
- **Navigate**: Rotate **Enc 1** to select patch.
- **Launch**: Press **K1** to launch selected patch.

### Orac (Organelle Control Scheme on Norns)

#### Main Performance View:
- **K1**: Open Orac Menu.
- **Hold K3 + Turn Enc 2**: Switch active module.
- **Hold K3 + Turn Enc 3**: Switch active page.
- **Enc 1, 2, 3, 4**: Adjust parameters on current page.
- **K2**: Aux button.

#### Menu View (after pressing K1):
- **Enc 1**: Scroll menu items.
- **K1**: Exit menu back to main performance view.
- **K2**: Back / Go up one directory level (`..`).
- **K3**: Enter / Select / OK.
