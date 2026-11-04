# NuiLite & Sidekick for Modern Norns

**NuiLite** is a C++ hardware abstraction library for Monome Norns, Norns Shield, and Fates hardware.  
**Sidekick** is an application switcher daemon that allows launching and toggling between Norns, Orac (Pure Data), and other custom environments directly from hardware buttons.  
**NuiPd** is a Pure Data external providing display and hardware input bindings.

This fork updates NuiLite, Sidekick, and Orac to work with **modern Norns firmware (v3.0.2 / 2.8.1+)** following the upstream removal of the Linux framebuffer device (`/dev/fb0`).

---

## Key Features in this Fork

- **Direct Userspace SPI Driver**: Replaces obsolete `/dev/fb0` kernel framebuffer rendering with high-performance direct userspace SPI transfers via `/dev/spidev0.0` and `libgpiod`.
- **SSD1322 OLED Support**: Native 128x64 4-bit grayscale display support for standard **Norns Shields** (DC=GPIO5, RST=GPIO6) and **Fates v1.8** (DC=GPIO17, RST=GPIO4). Also supports custom SSD1325 builds.
- **Exclusive Display Handover**: Sidekick cleanly releases the SPI device and GPIO lines upon launching a patch, preventing bus contention, scanlines, and vertical scrolling artifacts.
- **Binary ABI Compatibility**: Exported legacy constructors and stubs allow precompiled MEC binaries (`libmec-api.so`, `mec-app`) and Pure Data externals to link and run without rebuilding Orac from scratch.
- **Robust Audio Stack Handover**: Sequences ALSA soundcard access cleanly between Pure Data (exclusive direct ALSA) and Norns (JACK + SuperCollider).
- **Crash Protection**: Wrapped OSC message parsing in `try/catch` to guard against malformed UDP packets.

---

## Quick Installation

Connect to your Norns device over SSH:

```bash
ssh we@norns.local   # or ssh fates (default password: sleep)
```

Clone the repository and run the automated installer:

```bash
cd /home/we
git clone https://github.com/schrab/NuiLite.git
cd NuiLite

# For Stock Norns Shield (SSD1322, DC=5, RST=6):
sudo ./install.sh shield

# For Fates v1.8 (SSD1322, DC=17, RST=4):
sudo ./install.sh fates
```
*(If run without arguments, the script will prompt you to select your hardware).*

### What the Installer Does Automatically:
1. **Installs System Dependencies**: Checks and installs all required build tools and libraries (`build-essential`, `cmake`, `libgpiod-dev`, `libcairo2-dev`, `libfreetype6-dev`, `puredata-dev`) via `apt`.
2. **Sets Up Vanilla Norns**: If Orac or MEC are not found, automatically sets up `apt-norns` and installs the base sound engine, modules, presets, and MEC daemon.
3. **Builds Modern SPI Binaries**: Compiles `libnuilite.so`, `sidekick`, and `NuiPd.pd_linux` with direct SPI support.
4. **Prepares Filesystem**: Automatically creates all required directories under `/usr/local/` and `/home/we/`.
5. **Configures Display Pins**: Deploys the correct `sidekick.json` configuration for your hardware (Shield or Fates).
6. **Installs Patch Handover Scripts**: Configures clean audio/display release scripts for Norns and Orac.
7. **Installs & Starts System Services**: Deploys systemd service units and enables and starts `sidekick.service`.

> **Note:** For a complete step-by-step manual installation guide without the automated script, see [INSTALL.md](INSTALL.md).

---

## Hardware Controls & Usage

### Sidekick Menu
- **Activate Menu**: Press and hold **all 3 buttons (K1 + K2 + K3)** simultaneously for **3 seconds**.
- **Navigate**: Rotate **Enc 1** to select a patch (`Norns`, `Orac`, etc.).
- **Launch**: Press **K1** to launch the selected patch.

### Orac (Organelle Control Scheme on Norns)

#### Main Performance View:
- **K1**: Open Orac Menu.
- **Hold K3 + Turn Enc 2**: Switch active module slot (`A1`, `B1`, `C1`, `S1`, `P1`, etc.).
- **Hold K3 + Turn Enc 3**: Switch active parameter page.
- **Enc 1, 2, 3, 4**: Adjust parameters on current page.
- **K2**: Aux button.

#### Menu View (after pressing K1):
- **Enc 1**: Scroll menu items.
- **K1**: Exit menu back to main performance view.
- **K2**: Back / Go up one directory level (`..`).
- **K3**: Enter / Select / OK.

---

## Uninstallation / Returning to Stock Norns

### Temporary Return to Stock Norns:
If you want to temporarily disable Sidekick and boot straight into the standard Norns environment without removing files:

```bash
# Works on both Norns 2.x and 3.x:
sudo systemctl disable --now sidekick.service
sudo systemctl enable --now norns-matron.service norns-crone.service norns-main.service norns.target 2>/dev/null || true
```

To re-enable Sidekick later:

```bash
sudo systemctl disable --now norns-matron.service norns-crone.service norns-main.service norns-sclang.service 2>/dev/null || true
sudo systemctl enable --now sidekick.service
```

### Full Uninstallation:
To completely remove Sidekick, its systemd services, and deployed binaries, and restore stock Norns:

```bash
cd /home/we/NuiLite
sudo ./uninstall.sh
```
*(Pass `--purge` if you also want to remove `/home/we/sidekick` completely).*
