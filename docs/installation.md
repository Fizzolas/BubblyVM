# Installation Guide

This guide walks through installing BubblyVM from scratch on a physical machine or inside a test VM.

---

## Before You Start

Make sure your hardware meets the requirements in [hardware-requirements.md](hardware-requirements.md). The most important things to confirm:

- CPU has VT-x/AMD-V and VT-d/AMD-Vi
- Both are **enabled in BIOS/UEFI**
- You have at least 10 GB free storage (50 GB+ recommended)
- You have a USB drive (4 GB or larger) to write the installer to

---

## Step 1 — Download the ISO

> **Note:** BubblyVM is currently in early development. No official ISO exists yet. This guide describes the intended installation process for when the ISO becomes available.

Download the BubblyVM host ISO (~200 MB) from the official releases page:
```
https://github.com/Fizzolas/BubblyVM/releases
```

Verify the checksum before proceeding:
```bash
sha256sum bubbly-host.iso
# Compare output against the checksum posted on the releases page
```

---

## Step 2 — Write ISO to USB

**On Linux/macOS:**
```bash
dd if=bubbly-host.iso of=/dev/sdX bs=4M status=progress
sync
```
Replace `/dev/sdX` with your USB drive (e.g. `/dev/sdb`). Use `lsblk` to find it. **Do not** write to your system drive.

**On Windows:**
Use [Rufus](https://rufus.ie) — select the ISO, choose your USB drive, leave all other settings default, and click Start.

---

## Step 3 — Boot from USB

1. Insert the USB into the target machine.
2. Reboot and enter your BIOS/UEFI boot menu (usually F12, F2, Del, or Esc at startup — varies by manufacturer).
3. Select the USB drive as the boot device.
4. The BubblyVM installer will load automatically.

---

## Step 4 — Run the Installer

The installer is text-based and will guide you through:

1. **Disk partitioning** — creates an EFI System Partition (ESP) and a root partition.
   - Minimum root partition: 10 GB
   - Recommended: 50 GB+ on an SSD
2. **Hostname** — set a name for this machine.
3. **Admin password** — used for host-level maintenance only (not for daily use).
4. **IOMMU setup** — installer automatically adds `intel_iommu=on` or `amd_iommu=on` to kernel parameters based on detected CPU.
5. **Security Node image** — installer copies the signed Security Node squashfs to disk.

The installer will then install GRUB2 to the ESP and reboot.

---

## Step 5 — First Boot

On first boot the system will:
1. Load the hypervisor host kernel.
2. Start `libvirtd`, `dbus`, and `udev`.
3. Launch the Security Node VM automatically.
4. Present the Tile Launcher desktop.

If you reach the tile grid, the installation was successful.

---

## Step 6 — Enable Hardware Virtualization (if not done)

If the Security Node fails to start, virtualization may not be enabled in firmware.

1. Reboot into BIOS/UEFI.
2. Find settings labeled **Intel Virtualization Technology** (VT-x) and **Intel VT for Directed I/O** (VT-d), or the AMD equivalents.
3. Set both to **Enabled**.
4. Save and reboot.

Verify after boot:
```bash
dmesg | grep -i "iommu\|vt-x\|amd-v"
```

---

## Step 7 — Configure the Security Node

Edit the Security Node config file:
```bash
nano /etc/bubbly/security-node.conf
```

Key settings:
```ini
default_memory=512        # RAM in MB given to Security Node
cpu_hotplug=true          # Allow live CPU scaling
gpu_passthrough_prefer=true  # Use VFIO passthrough if available
```

Restart the Security Node service to apply:
```bash
systemctl restart bubbly-security-node
```

---

## Step 8 — Add Your First Tile

Tile definitions live in `/etc/bubbly/tiles/` as `.json` files.

Example — a minimal Linux terminal tile:
```json
{
  "label": "Terminal",
  "icon": "/usr/share/bubbly/icons/term.svg",
  "os_image": "alpine-base.qcow2",
  "memory_mb": 256,
  "vcpu_max": 2,
  "passthrough": [],
  "environment": {
    "TERM": "xterm-256color"
  }
}
```

Save this as `/etc/bubbly/tiles/terminal.json`, then reload the tile grid:
```bash
bubbly-tilectl reload
```

You should see the Terminal tile appear on the desktop. Click it to launch a fresh Alpine Linux shell in its own VM.

---

## Step 9 — Optional: GPU Passthrough Setup

If you have a discrete GPU and want near-native gaming or GPU performance:

1. Find your GPU's PCI address:
   ```bash
   lspci -nn | grep -i vga
   # Example output: 01:00.0 VGA compatible controller [0300]: NVIDIA ...
   ```

2. Add the address to the Security Node config:
   ```ini
   passthrough_gpu="0000:01:00.0"
   ```

3. Reboot. The Security Node will bind the GPU to the first VM that requests passthrough.

See [gpu-passthrough.md](gpu-passthrough.md) for detailed IOMMU group verification and multi-GPU setup.

---

## Troubleshooting

**Security Node won't start:**
- Check that VT-x/AMD-V and VT-d/AMD-Vi are enabled in BIOS.
- Run `dmesg | grep kvm` to confirm KVM modules loaded.

**SELinux denial errors:**
```bash
ausearch -m avc -ts recent | grep qemu
```
Restore file labels if needed:
```bash
restorecon -v /var/lib/libvirt/images/*.qcow2
```

**Tile doesn't appear after `bubbly-tilectl reload`:**
- Check your JSON file for syntax errors: `python3 -m json.tool /etc/bubbly/tiles/terminal.json`
- Check that the `os_image` filename exists in `/var/lib/bubbly/images/`.

**VM window doesn't open when clicking a tile:**
- Check the App VM Manager log: `journalctl -u bubbly-appmanager -n 50`

---

## Related Documents

- [Hardware Requirements](hardware-requirements.md)
- [Security Model](security-model.md)
- [GPU & Graphics Handling](gpu-passthrough.md)
- [Architecture Overview](architecture.md)
