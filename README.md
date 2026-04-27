# Evisum - An Enlightened System Monitor

I sincerely hope you can enjoy this application. It's fairly comprehensive and very
portable. The user interface is designed with my personal preferences in mind.
Having gotten into computer science in the early 90s there is a nostalgic feel to
the program and also some homage is paid to EFL and Enlightenment.

It does not rely on any third-party libraries outside of EFL which was quite some undertaking.

I'd like to express that schizophrenia doesn't exclude anyone from following
their dreams as long as you hold onto a touch of realism and your expectations 
aren't too high.

![Evisum Screenshot](data/images/ss.jpg "Evisum on Debian GNU/Linux")

## 📚 Table of Contents
- [🔥 Features](#-features)
- [🏗️ Architecture](#️-architecture)
- [📦 Enigmatic Library](#-enigmatic-library)
- [📌 Requirements](#-requirements)
- [⚙️ Build Instructions](#%EF%B8%8F-build-instructions)
- [🚀 Installation](#-installation)
- [🎯 Usage Examples](#-usage-examples)
- [🤝 Contributions](#-contributions)

## 🔥 Features
- Cross-platform support for **Linux, FreeBSD, OpenBSD, macOS and DragonFlyBSD**.
- A daemon-backed architecture using **enigmatic** + **enigmatic_client**:
  - `enigmatic` performs system polling and writes structured log events.
  - `enigmatic_client` follows that stream and builds typed snapshots.
  - evisum consumes one shared background update signal, so windows stay in sync
    without each view implementing its own low-level polling loop.
  - External programs can use the same client API/library to build their own
    monitors and tooling.
- Tools to monitor:
  - **Processes** 🛠️
  - **CPU usage** ⚡
  - **Memory consumption** 🧠
  - **Network activity** 🌐
  - **Storage health** 💾
  - **System sensors** 🌡️
- Designed for **speed, reliability, and usability**.

## 🏗️ Architecture
Evisum now uses a single data pipeline based on the `enigmatic` daemon and the
`enigmatic_client` log-streaming API.

- `enigmatic` (daemon):
  - Polls the operating system.
  - Writes structured events/snapshots to the Enigmatic log stream.
- `libenigmatic_client`:
  - Follows and parses the Enigmatic log.
  - Maintains an in-memory `Snapshot` with typed objects (`Cpu_Core`, `Meminfo`,
    `Sensor`, `Network_Interface`, `File_System`, `Proc_Info_Log`).
- evisum engine/background:
  - Starts/attaches to `enigmatic`.
  - Exposes snapshot-backed data to UI views.
  - Uses one background update signal so windows react to new stream data
    instead of polling independently.
- UI windows:
  - Consume engine/background updates.
  - Keep per-view rendering and formatting logic only.

This removes the old duplicated system-querying path and keeps runtime data flow
centered on one stream source.

## 📦 Enigmatic Library
`Enigmatic_Client` is available as an external installable library so programs
outside evisum can consume Enigmatic data directly (like the examples in
`src/bin/enigmatic/examples`).

Installed artifacts include:
- Shared library: `libenigmatic_client.so`
- pkg-config file: `enigmatic_client.pc`
- Public headers under `include/enigmatic/`:
  - `Enigmatic_Client.h`
  - `Events.h`
  - `enigmatic_util.h`
  - `system/machine.h`
  - `system/file_systems.h`
  - `system/process.h`

### Using the library in your own program

```sh
cc my_program.c $(pkg-config --cflags --libs enigmatic_client) -o my_program
```

Typical include:

```c
#include "Enigmatic_Client.h"
```

At runtime, open a client and register snapshot/event callbacks to receive live
stream updates.

## 📌 Requirements
Evisum requires an installation of **EFL (v1.27.0+)**.

Example EFL development package installs:

### Debian / Ubuntu
```sh
sudo apt update
sudo apt install efl-all-dev
```

### Fedora
```sh
sudo dnf install efl-devel
```

Build tools are also required:
```sh
sudo apt install meson ninja-build pkg-config
# or on Fedora:
sudo dnf install meson ninja-build pkgconf-pkg-config
```

Ensure your `PKG_CONFIG_PATH` environment variable is set correctly if EFL is installed in a custom location (e.g., `/opt`):

```sh
export PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/opt/libdata/pkgconfig"
```

## ⚙️ Build Instructions

Compile Evisum using `meson` and `ninja`:

```sh
meson setup build
ninja -C build
```

## 🚀 Installation

Once built, install Evisum with:

```sh
ninja -C build install
```

## 🎯 Usage Examples

### Open the process view:
```sh
evisum
```

### Inspect a specific process:
```sh
evisum <pid>
```

### Open the CPU monitor:
```sh
evisum -c
```

### Other command-line flags:
```sh
evisum -m      # memory view
evisum -d      # storage view
evisum -n      # network view
evisum -s      # sensors view
```

For additional options, use:
```sh
evisum --help
```

## 🤝 Contributions
We welcome contributions! Bug fixes and patches are greatly appreciated. However, if you want to introduce a substantial new feature, **please ensure it functions reliably on Linux, OpenBSD, and FreeBSD** before submitting a patch.
