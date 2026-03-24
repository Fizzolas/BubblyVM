# BubblyVM Architecture

This document describes the full layered architecture of BubblyVM — how each component fits together from firmware all the way up to the desktop UI.

---

## Layer Overview

```
┌─────────────────────────────────────────────────────┐
│                  Tile Launcher (Desktop UI)          │
│         Aero glass UI · tile grid · Snap/Shake       │
├─────────────────────────────────────────────────────┤
│                  Security Node VM                    │
│    Hardware mediation · SELinux sVirt · input broker │
├──────────────┬──────────────────────────────────────┤
│  App VM 1    │  App VM 2  │  App VM 3  │  ...        │
│  (Linux)     │  (Windows) │  (DOS)     │             │
├──────────────┴──────────────────────────────────────┤
│              App VM Manager  +  Resource Scaler      │
│         DBus daemon · libvirt · virtio-balloon       │
├─────────────────────────────────────────────────────┤
│               Hypervisor Host (KVM/QEMU)             │
│          Alpine Linux ~100 MB · OpenRC · virtio      │
├─────────────────────────────────────────────────────┤
│            Firmware / Bootloader (UEFI + GRUB2)      │
└─────────────────────────────────────────────────────┘
```

---

## Layer Descriptions

### 1. Firmware / Bootloader
- UEFI with Secure Boot validates signatures before anything loads.
- GRUB2 (minimal config) loads the host kernel and a small initramfs (~2 MB).
- Kernel parameters set here: `intel_iommu=on` or `amd_iommu=on` for VFIO support.

### 2. Hypervisor Host
- Stripped-down **Alpine Linux** (~100 MB on disk, ~100 MB RAM).
- Runs **KVM/QEMU** as the hypervisor engine.
- **OpenRC** instead of systemd — faster init, smaller footprint.
- Rootfs is mounted **read-only**; a tmpfs overlay holds runtime state.
- Starts three core services on boot: `udev`, `dbus`, `libvirtd`.
- Does not run a desktop, browser, or any user-facing app directly.

### 3. Security Node VM
- The first VM launched by the hypervisor host — always running.
- **Hardened:** read-only rootfs, SELinux enforcing mode, minimal init.
- **Hardware mediation:** all physical devices (USB, audio, display, network) are owned by the Security Node. App VMs access hardware only through proxies running here.
- Hosts the **Tile Launcher** compositor (the desktop the user sees).
- Runs a **secure input broker** — routes keyboard/mouse to the correct app VM, preventing cross-VM keylogging.
- Holds a **credential vault** sealed with TPM-bound keys.
- Updated via signed, read-only squashfs images (atomic OTA).

### 4. App VMs
- Each visible application window is backed by its own VM.
- Supports **Linux, Windows, legacy DOS**, or any OS with a qcow2 image.
- Each VM boots from a **read-only base image** plus a **writable overlay**.
- On window close → ACPI shutdown signal → VM destroyed → overlay discarded.
- This guarantees a **clean state** on every launch with no leftover files.
- Optional: user can snapshot the overlay before closing to preserve state.

### 5. App VM Manager
- A **DBus service** (`org.bubbly.AppManager`) running on the host.
- Listens for tile-click events from the Tile Launcher.
- Reads the tile's `.json` definition from `/etc/bubbly/tiles/`.
- Calls **libvirt** (`virsh define` + `virsh start`) to provision and boot the VM.
- Maps the VM's virtual display output to a compositor surface (window).
- Tears down the VM and discards the overlay on window close.

### 6. Resource Scaler
- A daemon that continuously polls per-VM CPU and memory stats via libvirt.
- Compares host-side stats against guest-reported usage from **virtio-balloon**.
- Issues `virsh setmem` and `virsh setvcpus` to grow or shrink resources live.
- Batches adjustments to ≤ 5% change per second, keeping overhead **< 1%**.
- Idle VMs are automatically deflated to free memory for active ones.

### 7. Tile Launcher (Desktop UI)
- Runs **inside the Security Node VM** (not on the bare host).
- Built on **Weston (Wayland)** or **Mutter** with a custom shell.
- Renders the Aero-style UI: glass borders, taskbar, tile grid, Flip 3D.
- Tile click → DBus call to App VM Manager → new VM window appears.
- Window decorations (snap, shake, thumbnails) handled by the compositor.

---

## Boot Sequence

```
1. Power on
2. UEFI validates signatures → loads bubbly-bootx64.efi
3. GRUB2 loads host kernel + initramfs
4. Host kernel mounts read-only rootfs, starts udev + dbus + libvirtd
5. libvirt starts Security Node VM
6. Security Node starts Tile Launcher compositor
7. User sees desktop (tile grid)
8. User clicks tile → DBus → App VM Manager → new app VM boots → window appears
```

Total time from power-on to usable desktop: **~2–3 seconds** on SSD hardware.

---

## SELinux sVirt Isolation

Every VM process gets a unique SELinux category pair, for example `svirt_t:s0:c412,c700`. The QEMU process, disk image, sockets, and devices for that VM all inherit the same label. The kernel enforces that no process with a different label can read or write those resources — preventing any cross-VM data leakage even if a VM is fully compromised.

---

## Data Flow: Tile Click to Running App

```
User clicks tile
    → Tile Launcher sends DBus signal: org.bubbly.AppManager.Launch("firefox")
    → App VM Manager reads /etc/bubbly/tiles/firefox.json
    → Clones base qcow2 overlay → virsh define + virsh start
    → VM boots Firefox OS image
    → virtio-GPU presents framebuffer to host compositor
    → Compositor maps surface to window on desktop
    → User interacts via Security Node input broker
    → On close: ACPI shutdown → VM destroyed → overlay deleted
```

---

## Related Documents

- [Hardware Requirements](hardware-requirements.md)
- [Security Model](security-model.md)
- [GPU & Graphics Handling](gpu-passthrough.md)
- [Performance](performance.md)
