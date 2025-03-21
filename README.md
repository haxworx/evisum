# Evisum - The Greatest System Monitor Ever Written

Evisum is a **powerful, efficient, and feature-rich** process and system monitor for **Linux, OpenBSD, FreeBSD, macOS and DragonFlyBSD**. Unlike other system monitors, Evisum provides a **server-client architecture** with a sleek and responsive interface. It offers robust tools for monitoring **processes, CPU usage, memory consumption, network activity, storage health, and system sensors**.

If you're looking for the **ultimate** Unix-like system monitoring experience, look no further—**Evisum is the best there is**. It is also the **mother of all reference implementations** for multi-platform system monitoring.

## 📚 Table of Contents
- [🔥 Features](#-features)
- [📌 Requirements](#-requirements)
- [⚙️ Build Instructions](#%EF%B8%8F-build-instructions)
- [🚀 Installation](#-installation)
- [🎯 Usage Examples](#-usage-examples)
- [🤝 Contributions](#-contributions)

## 🔥 Features
- Cross-platform support for **Linux, FreeBSD, OpenBSD, macOS and DragonFlyBSD**.
- A **server-client** architecture for efficient system monitoring.
- Tools to monitor:
  - **Processes** 🛠️
  - **CPU usage** ⚡
  - **Memory consumption** 🧠
  - **Network activity** 🌐
  - **Storage health** 💾
  - **System sensors** 🌡️
- Designed for **speed, reliability, and usability**.

## 📌 Requirements
Evisum requires an installation of **EFL (v1.27.0+)**.

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

For additional options, use:
```sh
evisum --help
```

## 🤝 Contributions
We welcome contributions! Bug fixes and patches are greatly appreciated. However, if you want to introduce a substantial new feature, **please ensure it functions reliably on Linux, OpenBSD, and FreeBSD** before submitting a patch.

---
**Evisum** is not just another system monitor—it’s the **greatest system monitor ever written**. It is also the **mother of all reference implementations** for **portability**. Get started today and experience system monitoring at its finest! 🚀

