<div align="center">

<img src="reloaded.jpg" alt="abreloaded_nx" width="160">

# abreloaded_nx2

**Angry Birds Reloaded on Nintendo Switch and Switch 2**

An unofficial Nintendo Switch wrapper for the Android version of
**Angry Birds Reloaded**.

[![Switch](https://img.shields.io/badge/nintendo_switch_2-homebrew-crimson?style=for-the-badge)](#)
[![Version](https://img.shields.io/badge/Version-1.0.0-4C8BF5?style=for-the-badge)](#)
[![Unity](https://img.shields.io/badge/Unity-2022.3.7f1-222C37?style=for-the-badge&logo=unity&logoColor=white)](#)

</div>

---

## About

`abreloaded_nx2` is a native wrapper that runs the ARM64 Android build of
**Angry Birds Reloaded** on Nintendo Switch. It recreates the Android and JNI
services expected by the game under Horizon OS.

This release targets **Angry Birds Reloaded 2.2.16218**, built with
**Unity 2022.3.7f1**, IL2CPP and ARM64.

The repository does not include the game, APK, libraries or assets. You must
provide your own legally obtained copy.

---

## Controls

| Input | Action |
| --- | --- |
| **Touchscreen** | Native touch controls |
| **Left Stick** | Move the cursor |
| **A / ZL / ZR** | Tap or drag |
| **B** | Back |
| **L / R** | Recenter the cursor |
| **+** | Show or hide the cursor |
| **–** | Enable or disable gyro aiming |
| **D-Pad Up / Down** | Adjust cursor sensitivity |

A USB mouse can also move the cursor and click. Cursor settings are saved in
`pointer.cfg`.

---

## Build

The project uses devkitPro's GNU toolchain and Makefile. Build commands below
are run from a Linux shell. On Windows, use WSL2 with an Ubuntu distribution;
run the commands inside Ubuntu rather than PowerShell. The Switch 2 build uses
[ounce-devkit](https://github.com/ouncebrew/ounce-devkit), which adds libounce,
OunceVK and libkaihen to a devkitPro installation. Its installer and Makefile
integration are documented in the [ounce-devkit README](https://github.com/ouncebrew/ounce-devkit#install).

### 1. Install devkitPro

Install devkitPro pacman by following the official
[devkitPro setup instructions](https://devkitpro.org/wiki/devkitPro_pacman).
Make sure `DEVKITPRO` is set in the shell. The default Linux location is
`/opt/devkitpro`:

```bash
export DEVKITPRO=/opt/devkitpro
```

If your devkitPro installation is in another directory, set `DEVKITPRO` to
that path instead.

On Ubuntu or WSL, install the host tools before continuing:

```bash
sudo apt update
sudo apt install git make curl
```

### 2. Install the project and portlibs

Clone this repository and install the common portlibs used by both targets:

```bash
git clone https://github.com/githubuser77777/abreloaded_nx2.git
cd abreloaded_nx2
sudo pacman -S switch-dev switch-sdl2 switch-zlib switch-libpng
```

The default Switch 1 target also uses Mesa and Nouveau:

```bash
sudo pacman -S switch-mesa switch-libdrm_nouveau
```

For Switch 2, install ounce-devkit. The installer downloads a versioned release
and installs its files under `DEVKITPRO`:

```bash
git clone https://github.com/ouncebrew/ounce-devkit.git
cd ounce-devkit
sh install.sh
cd ../abreloaded_nx2
```

To install a particular ounce-devkit release, pass its version to the installer
(`sh install.sh VERSION`). See the ounce-devkit project for the
[current release and installation details](https://github.com/ouncebrew/ounce-devkit).

### 3. Build for Nintendo Switch

From the `abreloaded_nx2` repository root, build the default Switch 1 NRO:

```bash
make -j"$(nproc)"
```

The output is `abreloaded_nx.nro` in the repository root. To rebuild after
changing target settings, clean first:

```bash
make clean
make -j"$(nproc)"
```

### 4. Build for Nintendo Switch 2

From the same repository root, select the Switch 2 target explicitly:

```bash
make clean
make SWITCH2=1 -j"$(nproc)"
```

This selects ounce-devkit's `switch2_rules`, Switch 2 compiler flags, OunceVK's
EGL/OpenGL ES libraries, and libkaihen. The Switch 1 Mesa/Nouveau libraries are
not used by this target. The output is also `abreloaded_nx.nro` in the repository
root; copy it somewhere else before building another target if you need to keep
both builds.

The Switch 2 loader uses kaiHEN's `ldr:ro` service for executable image mapping
and code patch publishing. Launch the NRO through kaiHEN's hbloader so the app
receives that service override. A plain launch path without `ldr:ro` will fail
at startup.

### Build troubleshooting

* `DEVKITPRO is not set`: export the path to your devkitPro installation in the
  current shell, then rerun Make.
* `switch2_rules: No such file or directory`: run `sh install.sh` from the
  ounce-devkit checkout, and confirm it installed into the same `DEVKITPRO`
  path used by this shell.
* Missing SDL2, zlib or PNG headers/libraries: install `switch-sdl2`,
  `switch-zlib` and `switch-libpng` with `dkp-pacman`.
* Do not commit the generated `build/`, `.elf`, `.nro` or `.nacp` files. They are
  build outputs, not source files.

Include the devkitPro and ounce-devkit versions in build issue reports, along
with the full compiler error and the target command you ran.

---

## Running

Extract the Android game files from your own APK and create this folder on the
SD card:

```text
sd:/switch/abreloaded/
├── abreloaded_nx.nro
├── libmain.so
├── libunity.so
├── libil2cpp.so
├── cursor_pointer.png
├── cursor_grab.png
└── assets/
```

The three libraries are found in `lib/arm64-v8a/` inside the APK. Copy the
complete `assets/` directory without changing its layout.

On Switch 1, launch through title override for full application memory: hold
**R** while opening an installed game, then start `abreloaded_nx` from the
Homebrew Menu. On Switch 2, launch from kaiHEN's hbloader as described in the
[Switch 2 build instructions](#4-build-for-nintendo-switch-2) so the required
`ldr:ro` service is supplied.

---

## Status

Gameplay, assets, audio, cutscenes, touchscreen input and controller cursor
input are working. Startup and scene transitions can still take longer than on
the original platforms.

The wrapper is built specifically for game version **2.2.16218**. Libraries
from another release may require different patches and are not supported.

---

## Credits

**Angry Birds Reloaded Nintendo Switch port** — aks796

**Fruit Ninja Classic+ Nintendo Switch port and base wrapper** — ChanseyIsTheBest

The loader and compatibility layer derive from the open-source Switch `.so`
loader work by Andy Nguyen, fgsfds and ChanseyIsTheBest, building on
TheOfficialFloW's Vita and Switch loader work. This project also draws from the
Zookeeper DX, PvZ Fusion and Animal Crossing: Pocket Camp ports. The inherited
wrapper code is MIT-licensed.

**Angry Birds Reloaded** was developed and published by Rovio Entertainment.

---

## Contributing

Bug reports and tested improvements are welcome. Include the build version,
steps to reproduce and the relevant `debug.log` when reporting an issue.

---

## Disclaimer

This is an unofficial fan project and is not affiliated with, sponsored by or
endorsed by Rovio Entertainment. Angry Birds Reloaded and all related artwork,
audio, trademarks and game assets belong to their respective owners.

This repository contains only the compatibility code required by the Nintendo
Switch port and does not distribute proprietary game files.
