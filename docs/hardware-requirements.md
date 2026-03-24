# Hardware Requirements

This document covers the minimum and recommended hardware to run BubblyVM, along with notes on what each component affects.

---

## Quick Reference

| Component | Minimum | Recommended |
|---|---|---|
| **CPU** | 64-bit, Intel VT-x or AMD-V | Modern multi-core (i5-4200U or better) |
| **RAM** | 2 GB | 8 GB+ |
| **GPU** | Integrated graphics | Discrete GPU with VFIO/IOMMU support |
| **Storage** | 10 GB free | 50 GB+ SSD |
| **Network** | Any NIC | Gigabit Ethernet or Wi-Fi 5 |

---

## CPU

BubblyVM requires a 64-bit processor with **hardware virtualization support**:

- **Intel:** VT-x (virtualization) + VT-d (IOMMU, needed for GPU passthrough)
- **AMD:** AMD-V (virtualization) + AMD-Vi (IOMMU, needed for GPU passthrough)

These features must be **enabled in your BIOS/UEFI**. Most modern CPUs have them but they are sometimes off by default.

To verify after boot:
```bash
dmesg | grep -i virtual
lscpu | grep Virtualization
```

A faster, more modern CPU directly improves how many app VMs can run simultaneously and how responsive each one feels. The hypervisor host itself uses only 2%–5% of CPU capacity, leaving the rest for your VMs.

---

## RAM

RAM is the most important resource for BubblyVM because every running VM consumes its own share.

**Fixed overhead (always consumed):**
- Hypervisor host: ~100 MB
- Security Node VM: ~400 MB
- Total baseline: ~500 MB

**Per app VM (approximate):**
| VM Type | Typical RAM Usage |
|---|---|
| Minimal Linux terminal | 256 MB |
| Full Linux desktop app | 512 MB – 1 GB |
| Windows application | 1 GB – 2 GB |
| Gaming VM | 2 GB – 4 GB |

With 8 GB total RAM you can comfortably run the host + Security Node + 4–6 typical Linux app VMs simultaneously. The Resource Scaler will automatically shrink idle VMs to free memory for active ones.

---

## GPU

GPU setup has the biggest impact on visual and gaming performance.

### Integrated Graphics (minimum)
- Works for all basic desktop use, productivity, and terminal apps.
- The Aero UI glass effects run via software rendering (virgl).
- Gaming inside a VM will see a 5%–35% FPS drop compared to native.

### Discrete GPU with VFIO Passthrough (recommended for gaming)
- GPU is handed directly to a specific app VM.
- Performance is within 1–2% of running the game natively.
- Requires **IOMMU** enabled in BIOS and the GPU in its own IOMMU group.
- Only one VM can hold the GPU at a time (unless you have multiple GPUs).

### Checking IOMMU support:
```bash
dmesg | grep -i iommu
lspci -nnk | grep -i vga
```

### Multi-GPU setups
If your machine has more than one GPU (e.g. integrated + discrete), you can assign the discrete GPU to a gaming VM while the Security Node and other VMs use the integrated GPU. This is the ideal configuration.

---

## Storage

BubblyVM uses disk images (qcow2 format) for each OS. Storage space adds up quickly.

**Typical image sizes:**
| Image | Size on Disk |
|---|---|
| BubblyVM host + Security Node | ~500 MB |
| Alpine Linux base image | ~150 MB |
| Ubuntu base image | ~2–4 GB |
| Windows 10 LTSC base image | ~10–15 GB |

Each running VM also creates a small writable **overlay** on top of its base image. Overlays are discarded on shutdown, so they don't accumulate over time.

An **SSD is strongly recommended.** Boot time drops from ~10 seconds on a spinning hard drive to ~2–3 seconds on an SSD. VM launch speed is also noticeably faster.

---

## Network

All VMs share the host's network connection through **virtio-net** virtual network cards. No special hardware is required.

- Each VM gets its own virtual NIC and can be on the same or different virtual networks.
- The Security Node can act as a network filter/firewall between VMs and the physical network.
- For lowest latency, a Gigabit Ethernet connection is preferred over Wi-Fi.

---

## Example: Tested Reference System

> **Intel Core i5-4200U · 8 GB RAM · Intel HD Graphics 4400 (integrated) · 120 GB SSD**

- Host + Security Node consume ~1 GB RAM at idle.
- 6 GB+ available for app VMs.
- Aero UI runs smoothly via virgl.
- Gaming inside a VM: ~40–55 FPS on titles that run at 60 FPS natively (no GPU passthrough on this system).
- Boot to desktop: ~2–3 seconds.

---

## BIOS/UEFI Checklist

Before installing BubblyVM, verify these settings in your firmware:

- [ ] **Intel VT-x** or **AMD-V** — enabled
- [ ] **Intel VT-d** or **AMD-Vi** — enabled (required for GPU passthrough)
- [ ] **Secure Boot** — optional but supported
- [ ] **TPM 2.0** — optional, enables credential vault in Security Node

---

## Related Documents

- [Architecture Overview](architecture.md)
- [GPU & Graphics Handling](gpu-passthrough.md)
- [Installation Guide](installation.md)
