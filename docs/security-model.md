# Security Model

This document explains how BubblyVM isolates applications from each other and from the host, and how the Security Node VM enforces that isolation.

---

## Core Philosophy

BubblyVM operates on one security principle: **no app should be able to affect anything outside its own VM.** Even if an application is fully compromised by malware, it cannot reach other running apps, the host system, or your credentials. This is enforced at the hardware and kernel level, not just by software rules.

---

## Trusted Computing Base (TCB)

The TCB is the set of components that must be trusted and kept secure for the whole system to be safe. In BubblyVM it is kept as small as possible:

| Component | Trusted? | Notes |
|---|---|---|
| Host kernel (KVM) | Yes | Minimal Alpine kernel, read-only rootfs |
| QEMU | Yes | Runs privileged, kept minimal |
| libvirt | Yes | Manages VM lifecycle |
| Security Node VM | Yes | Hardened, SELinux enforcing, read-only |
| App VMs | **No** | Treated as untrusted by default |
| Tile Launcher UI | No | Runs inside Security Node, isolated from host |

Anything outside the TCB is treated as potentially hostile. App VMs are always untrusted regardless of what OS they run.

---

## Security Node VM

The Security Node is the most critical component in the system. It is a hardened VM that:

- Boots from a **read-only, signed squashfs image** — cannot be modified at runtime.
- Runs **SELinux in enforcing mode** with a minimal, locked-down policy.
- Owns all physical hardware — no app VM ever talks to hardware directly.
- Mediates every device request through proxies (display, input, audio, network, USB).
- Hosts the **Tile Launcher** compositor — the desktop the user sees and interacts with.
- Runs a **credential vault** for passwords and keys, sealed with TPM-bound keys.
- Is updated atomically via signed OTA payloads — no partial or unsigned updates are applied.

Compromising an app VM gives an attacker zero ability to affect the Security Node or host.

---

## SELinux sVirt Isolation

Every VM process on the host is confined by **SELinux sVirt** (Security-Enhanced Linux, virtualization labels).

### How it works:

1. When a VM is started, libvirt assigns it a unique **category pair**, for example:
   ```
   svirt_t:s0:c412,c700
   ```
2. The QEMU process, the VM's disk image, its virtual sockets, and its devices all receive **the same label**.
3. The Linux kernel enforces that no process with a **different label** can read or write those resources.
4. This means even if two VMs are running simultaneously, neither can access the other's memory, disk, or sockets — not even if both are fully compromised.

### Verifying sVirt labels:
```bash
ps -eZ | grep qemu          # Shows each QEMU process and its SELinux label
ls -Z /var/lib/bubbly/overlays/  # Shows disk overlay labels
```

### If you see SELinux denial errors:
```bash
ausearch -m avc -ts recent | grep qemu
restorecon -v /var/lib/libvirt/images/*.qcow2
```

---

## Hardware Mediation

All physical devices are owned by the Security Node. App VMs access hardware through **virtio proxy devices**:

| Physical Device | Proxy Mechanism | Notes |
|---|---|---|
| Display | virtio-GPU / VFIO passthrough | Compositor maps VM framebuffer to window |
| Keyboard / Mouse | virtio-input via input broker | Broker routes events to focused VM only |
| Audio | virtio-sound via audio proxy | Security Node mixes audio streams |
| Network | virtio-net via virtual switch | Security Node can filter/block traffic |
| USB devices | virtio-serial or USB passthrough | Security Node applies whitelist/blacklist |
| Storage | virtio-block (qcow2 overlay) | Each VM sees only its own overlay |

No app VM has direct hardware access unless GPU passthrough is explicitly configured for it.

---

## Input Broker

The input broker runs inside the Security Node and handles all keyboard and mouse input. It:

- Tracks which VM window currently has focus.
- Routes keystrokes and mouse events **only to the focused VM's virtio-input device**.
- Prevents any VM from reading input intended for another VM (cross-VM keylogging is not possible).
- Intercepts a reserved key combo (e.g. Ctrl+Alt+Shift) that always returns focus to the Security Node — even if a VM tries to grab exclusive input.

---

## Credential Vault

The Security Node includes a small credential vault service for storing passwords, SSH keys, and certificates:

- Vault data is encrypted and sealed using **TPM 2.0 bound keys** (if TPM is present).
- Without TPM, vault is encrypted with a passphrase entered at boot.
- App VMs can request credentials from the vault via a narrow DBus API — they receive only what they are explicitly authorized for, never raw vault access.
- The vault process runs with a unique SELinux label, isolated even within the Security Node.

---

## Clean State Guarantee

Every app VM launch starts from a **read-only base image** with a fresh writable overlay on top. On shutdown:

1. ACPI shutdown signal sent to VM.
2. Guest OS shuts down cleanly.
3. libvirt destroys the VM.
4. The writable overlay is **deleted**.

This means malware, tracking cookies, temp files, and any other session data are completely erased after every use. The next launch of that tile starts from a perfectly clean state.

Optional: users can snapshot the overlay before closing if they want to preserve session state.

---

## Network Security

- Each VM gets a virtual NIC connected to a virtual switch inside the Security Node.
- The Security Node can apply **per-VM firewall rules** — for example, blocking a sandboxed app from accessing the internet entirely.
- VMs cannot communicate with each other over the network unless explicitly configured.
- The physical NIC is never directly exposed to an app VM (unless passthrough is configured).

---

## Security Node Updates

The Security Node image is distributed as a **signed, read-only squashfs**. The update process:

1. Host downloads the new signed squashfs payload.
2. Signature is verified against a public key baked into the host firmware.
3. If valid, the new image is written to the update partition.
4. On next reboot, the host mounts the new Security Node image.
5. If verification fails at any step, the update is rejected and the current image remains.

This ensures no unsigned or tampered Security Node image can ever run.

---

## Threat Model Summary

| Threat | Mitigated? | Mechanism |
|---|---|---|
| Malware in one app VM | Yes | SELinux sVirt, VM isolation |
| Cross-VM data theft | Yes | Unique sVirt labels per VM |
| Keylogging across VMs | Yes | Input broker in Security Node |
| Persistent malware | Yes | Overlay discarded on shutdown |
| Credential theft | Yes | TPM-sealed vault, narrow API |
| Tampered Security Node | Yes | Signed squashfs, verified at boot |
| Host kernel exploit | Partial | Minimal TCB, read-only rootfs reduces attack surface |
| Physical hardware attack | No | Out of scope — requires physical access |

---

## Related Documents

- [Architecture Overview](architecture.md)
- [Hardware Requirements](hardware-requirements.md)
- [GPU & Graphics Handling](gpu-passthrough.md)
- [Installation Guide](installation.md)
