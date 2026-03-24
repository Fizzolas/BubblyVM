# BubblyVM

> Tile-based, per-application-VM operating system — every app runs in its own isolated, disposable KVM virtual machine, wrapped in a nostalgic Aero-style glass UI.

---

## What Is BubblyVM?

BubblyVM is a desktop operating system built around one idea: **each application gets its own virtual machine**. When you click a tile to open an app, a fresh VM boots for it. When you close the window, that VM is destroyed — taking any malware, junk files, or registry rot with it.

The visual design takes inspiration from the "bubbly" era of early Windows Vista/7 — translucent glass borders, glossy icons, Aero Snap/Shake, and smooth animations.

---

## How It Works

| Layer | What It Does |
|---|---|
| **Firmware / Bootloader** | UEFI + GRUB2 loads the minimal hypervisor host |
| **Hypervisor Host** | Stripped-down Alpine Linux (~100 MB) running KVM/QEMU |
| **Security Node VM** | Hardened VM that mediates all hardware access via SELinux sVirt |
| **Tile Launcher** | Aero-style desktop — click a tile, spawn a VM |
| **App VM Manager** | DBus service that provisions, monitors, and destroys app VMs |
| **Resource Scaler** | Adjusts vCPU and RAM live via virtio-balloon and CPU hot-plug |
| **Storage Layer** | qcow2 base images + disposable overlays per VM |

---

## Key Features

- **Strong isolation** — malware in one app VM cannot reach others or the host
- **Instant clean state** — every app launch starts from a fresh image
- **OS mixing** — run Linux, Windows, and legacy apps side by side
- **Auto resource scaling** — VMs shrink when idle, freeing memory automatically
- **Aero UI** — glass effects, Flip 3D, Snap, Shake, animated tiles
- **Hardware friendly** — viable on modest hardware (i5-4200U, 8 GB RAM)

---

## Performance at a Glance

| Workload | Bare Metal | BubblyVM (GPU passthrough) | BubblyVM (vGPU) |
|---|---|---|---|
| CPU-bound | 100% | 95–98% | 95–98% |
| Memory bandwidth | 100% | 98% | 98% |
| GPU game (60 FPS target) | 60 FPS | 55–60 FPS | 40–55 FPS |
| Disk I/O | 100 MB/s | ~92 MB/s | ~92 MB/s |
| Idle RAM (host + sec node) | — | ~1 GB | ~1 GB |

---

## Hardware Requirements

- **CPU:** 64-bit with Intel VT-x or AMD-V; modern multi-core recommended
- **RAM:** 2 GB minimum; 8 GB+ recommended
- **GPU:** Integrated for basic use; discrete GPU with VFIO support for passthrough
- **Storage:** 10 GB minimum; 50 GB+ SSD recommended
- **Network:** Any NIC (virtio-net); Gigabit or Wi-Fi 5 preferred

---

## Repository Layout

```
BubblyVM/
├── docs/               Documentation (architecture, install, security, GPU)
├── host/               Hypervisor host — kernel config, rootfs, init scripts
├── security-node/      Hardened Security Node VM — SELinux policy, proxies
├── tile-launcher/      Aero-style desktop UI — compositor, theme, tile defs
├── app-vm-manager/     DBus daemon that spawns and destroys app VMs
├── resource-scaler/    virtio-balloon / CPU hot-plug scaling daemon
├── storage/            Base image scripts and CLI tools
├── guest-tools/        Guest agent packages for Linux and Windows VMs
└── build/              Top-level build scripts and CI (GitHub Actions)
```

---

## Status

This project is in early **design and scaffolding** phase. No bootable image exists yet. Contributions, feedback, and ideas are welcome.

---

## License

See [LICENSE](LICENSE) for details.
