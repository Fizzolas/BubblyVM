# GPU & Graphics Handling

This document explains how BubblyVM handles graphics — from basic integrated GPU rendering all the way to full discrete GPU passthrough for near-native gaming performance.

---

## Two Graphics Paths

BubblyVM supports two ways of giving a VM access to graphics:

| Path | How It Works | Performance | Best For |
|---|---|---|---|
| **vGPU (virgl)** | Virtual GPU rendered by host | 5–35% FPS drop | General use, productivity |
| **VFIO Passthrough** | Real GPU handed to VM directly | Within 1–2% of native | Gaming, GPU compute |

The system automatically uses passthrough if your GPU supports it and IOMMU is enabled. Otherwise it falls back to vGPU.

---

## vGPU Path (virgl / virtio-GPU)

When no passthrough GPU is available, BubblyVM uses **virgl** — a virtual GPU that translates the guest's OpenGL/Vulkan calls and renders them on the host's GPU.

- Works with any GPU, including integrated Intel/AMD graphics.
- The Aero UI glass effects and compositor run fine on virgl.
- Gaming performance degrades 5–35% depending on how GPU-intensive the title is.
- No extra setup required — this is the default.

**Virgl limitations:**
- Not all Vulkan extensions are supported.
- High-end games with complex shaders may see larger performance drops.
- No hardware video decode acceleration in the guest.

---

## VFIO Passthrough Path

VFIO (Virtual Function I/O) lets the host hand a real physical GPU directly to a VM. The VM gets exclusive, direct hardware access — no virtualization layer between the game and the GPU.

**Result:** performance within 1–2% of running the game on bare metal.

### Requirements for VFIO passthrough:
- A discrete GPU (separate from your display/integrated GPU).
- CPU with IOMMU support (Intel VT-d or AMD-Vi).
- IOMMU enabled in BIOS/UEFI.
- The GPU must be in its own **IOMMU group** (not sharing with other devices you need).

---

## Setting Up GPU Passthrough

### Step 1 — Verify IOMMU is active
```bash
dmesg | grep -i iommu
# Should show: IOMMU enabled
```

### Step 2 — Find your GPU's PCI address
```bash
lspci -nn | grep -i vga
# Example output:
# 00:02.0 VGA compatible controller: Intel HD Graphics (integrated)
# 01:00.0 VGA compatible controller: NVIDIA GeForce RTX 3060
```

The address format is `BB:DD.F` (bus:device.function) — in this example `01:00.0` is the discrete GPU.

### Step 3 — Check the GPU's IOMMU group
```bash
for d in /sys/kernel/iommu_groups/*/devices/*; do
  echo "Group $(basename $(dirname $d)): $(lspci -nns ${d##*/})"
done | grep -i "01:00"
```

You want the GPU to be **alone in its group**, or grouped only with its own audio device (e.g. `01:00.1` HDMI audio). If it shares a group with other critical devices, passthrough may not be possible without an ACS patch.

### Step 4 — Configure passthrough in BubblyVM

Edit `/etc/bubbly/security-node.conf`:
```ini
gpu_passthrough_prefer=true
passthrough_gpu="0000:01:00.0"
```

The `0000:` prefix is the PCI domain — almost always `0000` on consumer hardware.

### Step 5 — Reboot

On reboot, the Security Node binds the GPU to the VFIO driver instead of the normal GPU driver. The GPU is then available for assignment to an app VM that requests it.

### Step 6 — Assign GPU to a tile

In your tile's `.json` definition, add the device to the passthrough list:
```json
{
  "label": "Gaming VM",
  "icon": "/usr/share/bubbly/icons/game.svg",
  "os_image": "windows10-ltsc.qcow2",
  "memory_mb": 4096,
  "vcpu_max": 4,
  "passthrough": ["0000:01:00.0"],
  "environment": {}
}
```

When this tile is launched, the VM receives the GPU directly.

---

## Multi-GPU Setup

If your machine has both an integrated GPU and a discrete GPU (very common on laptops and many desktops), you can run both paths simultaneously:

- **Integrated GPU** → used by the Security Node and all vGPU VMs (normal desktop use).
- **Discrete GPU** → passed through to one gaming/compute VM.

This is the ideal configuration. The Security Node and tile grid remain responsive on the integrated GPU while the gaming VM gets full discrete GPU performance.

If you have **two discrete GPUs**, each can be assigned to a different VM simultaneously.

---

## GPU Driver Notes

### Inside a Linux guest VM:
- Install `virtio-gpu` drivers for vGPU path (included in most modern kernels).
- For VFIO passthrough with NVIDIA: use the open-source NVIDIA kernel module (`nvidia-open`).
- For VFIO passthrough with AMD: standard `amdgpu` driver works out of the box.

### Inside a Windows guest VM:
- For vGPU: install the `viogpu` driver from the [virtio-win driver package](https://github.com/virtio-win/virtio-win-pkg-scripts).
- For VFIO passthrough: install your normal GPU driver (NVIDIA/AMD) — Windows sees it as real hardware.

### bubbly-gpu-tools
BubblyVM provides a helper package `bubbly-gpu-tools` that automates driver installation inside common guest images. Usage:
```bash
bubbly-gpu-tools install --guest alpine
bubbly-gpu-tools install --guest windows10
```

---

## Performance Reference

| Workload | Bare Metal | VFIO Passthrough | virgl (vGPU) |
|---|---|---|---|
| GPU game (60 FPS target) | 60 FPS | 55–60 FPS | 40–55 FPS |
| GPU compute (normalize) | 100% | 98–99% | 65–90% |
| Aero UI compositor | — | N/A | Smooth at 1080p |
| Video playback (1080p) | Smooth | Smooth | Smooth |
| Video playback (4K) | Smooth | Smooth | May drop frames |

---

## Troubleshooting

**IOMMU not showing in dmesg:**
- Check BIOS: VT-d (Intel) or AMD-Vi (AMD) must be enabled.
- Confirm kernel parameters include `intel_iommu=on` or `amd_iommu=on`:
  ```bash
  cat /proc/cmdline
  ```

**GPU not binding to VFIO driver:**
```bash
dmesg | grep vfio
lspci -nnk -d [vendor:device]   # Check which driver is bound
```
Force bind manually:
```bash
echo "0000:01:00.0" > /sys/bus/pci/drivers/vfio-pci/bind
```

**VM boots but no display output:**
- Confirm the tile JSON lists the correct PCI address.
- Check that no other driver claimed the GPU before VFIO: `lspci -nnk | grep -A 3 "01:00.0"`.
- Check App VM Manager logs: `journalctl -u bubbly-appmanager -n 50`.

**NVIDIA GPU passthrough — Code 43 error in Windows guest:**
- Add `hidden state` to the VM's hypervisor flags in its libvirt XML to hide the hypervisor from the NVIDIA driver.
- This is handled automatically by `bubbly-gpu-tools` when targeting a Windows guest.

---

## Related Documents

- [Architecture Overview](architecture.md)
- [Hardware Requirements](hardware-requirements.md)
- [Security Model](security-model.md)
- [Performance](performance.md)
