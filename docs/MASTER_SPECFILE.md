# bubblyVM — Master Specification File

> **Version:** 1.0  
> **Status:** Active Reference  
> **Authority:** This file is the single source of truth for all bubblyVM design decisions. Every subsystem, feature, and milestone must trace back to a section here.

---

## PRIMARY DIRECTIVE

bubblyVM is a real, usable operating system — not a demo, not a toy kernel, not a shell running on another OS. It must be bootable, usable by a normal end user, recoverable when broken, and extensible with applications. It targets modern x86_64 PCs first, with architecture abstraction planned for ARM64 and RISC-V in later tiers.

---

## ABSOLUTE SUCCESS STANDARD

bubblyVM is not considered "an OS" unless it supports the full path of:

1. Installation or boot
2. Hardware detection
3. Login / session handling
4. File and storage interaction
5. Application launching
6. System settings changes
7. Network usage
8. Peripheral support
9. System recovery and troubleshooting
10. Software updates
11. Safe shutdown / restart / sleep
12. Accessibility and user guidance
13. Developer extensibility
14. Future portability to non-x86 architectures

---

## DEVELOPMENT PHILOSOPHY

- Prioritize real usability, not just technical novelty.
- Favor modularity even if the first kernel is monolithic.
- Use practical staging: CLI first → settings framework → GUI → richer desktop.
- Every subsystem must define: user-facing behavior, fallback behavior, failure behavior, and test criteria.
- Every user action must produce understandable feedback.
- Every hardware-dependent area must be abstracted behind interfaces where possible.
- All design decisions must consider future multi-architecture and multi-device support.
- All components must be documented for both developers and end users.
- Design for ordinary users, not just technical users.
- Favor stability, recovery, and clarity over flashy visuals early on.

---

## RELEASE TIERS

| Tier | Name | Goal |
|------|------|------|
| 0 | Kernel Console | Boots to kernel console — proof of life |
| 1 | Text-Mode OS | Usable text-mode OS: shell, storage, basic networking |
| 2 | Basic Desktop | Graphical environment, file manager, settings skeleton |
| 3 | Daily Driver | Full desktop, app ecosystem, accessibility, recovery |
| 4 | Broad PC Support | Wide hardware compatibility, installer polish, portability design |
| 5 | Multi-Arch | ARM64 port execution, RISC-V exploration |

---

## SUPPORTED USER TYPES

The system must explicitly support:

| User Type | Key Needs |
|-----------|-----------|
| Casual home user | Simple UI, understandable errors, easy setup |
| Low-technical user | Guided flows, plain language, no terminal required |
| Power user | Terminal, advanced settings, package management |
| Developer | Build tools, kernel module support, debug modes |
| Tester | Diagnostics, logs, repeatable test environments |
| Accessibility-focused user | Screen reader, keyboard nav, high contrast, text scaling |
| Offline-only user | Manual update path, local package install |
| Laptop user | Battery, suspend/resume, lid close, touchpad |
| Desktop user | Multi-monitor, peripherals, external drives |
| Recovery/emergency user | Recovery boot, safe mode, repair tools |
| Future OEM/integrator | Installer customization, deployment scripting |

---

## CORE OS DOMAINS (Non-Negotiable)

### A. Boot Domain
Bootloader, kernel loading, initramfs, secure/recoverable boot, boot parameters, logs, fallback entries, recovery boot, debug boot, single-user mode, installer/live mode.

### B. Kernel Domain
Process/task model, thread model, scheduling, interrupt handling, memory management, virtual memory, privilege model, syscall interface, timer, synchronization, IPC, crash/panic framework, driver model, hardware abstraction, architecture abstraction layer.

### C. Hardware Domain
Device enumeration, bus support, driver binding, hotplug, power state awareness, device failure handling, removable device management, firmware interface, hardware capability API.

### D. Storage Domain
Partitions, block devices, filesystems, permissions, metadata, mount/unmount, removable media, disk health, filesystem repair, formatting, encryption planning, snapshot planning.

### E. Userland Domain
Shell, core utilities, service manager/init, configuration system, app runtime, session management, package management, terminal utilities, log access, admin tools.

### F. UI Domain
Text console, graphical environment, settings app, notifications, launcher/menu, task switching, window management, file dialogs, permission prompts, visual consistency, accessibility hooks, onboarding flow.

### G. Network Domain
Network stack, DHCP, DNS, IPv4 (IPv6 planned), Wi-Fi management, Ethernet, hostname, proxy planning, firewall framework, network diagnostics.

### H. Security Domain
Accounts, authentication, permissions, privilege elevation, process isolation, secure settings, package trust, update integrity, audit logging, device access control, credential storage.

### I. Recovery Domain
Boot repair, filesystem check, safe mode, rollback, configuration reset, recovery shell, user data preservation, repair utilities, support logs.

### J. Accessibility Domain
Keyboard navigation, screen scaling, high contrast, text scaling, screen reader architecture, captions planning, reduced motion, sticky/filter keys planning, remappable shortcuts, color-safe design, simple language mode.

---

## PLATFORM SCOPE

**Primary target:** Modern x86_64 PCs — UEFI-first, legacy BIOS if practical, commodity desktops and laptops, QEMU/VirtualBox/VMware for development.

**Future expansion:** ARM64 desktops/laptops/SBCs, RISC-V experimentation.

**Hardware assumptions to support:**
- Multi-core CPUs
- Integrated and discrete graphics
- SATA and NVMe drives
- USB storage
- Standard HID keyboards and mice
- Laptop touchpads
- Common network adapters
- Common audio hardware
- Standard monitors with modern resolutions
- Suspend/resume on laptops

---

## THE 10-QUESTION GATE

Every proposed feature or subsystem must answer all 10 before acceptance:

1. What does a normal user see or experience from this?
2. What settings control it?
3. What happens when it fails?
4. How is it diagnosed?
5. What is the fallback or recovery path?
6. What is the test criteria for "done"?
7. What documentation is required?
8. What are the hardware dependencies?
9. What are the security implications?
10. How would this port to another CPU architecture?

---

## SECTION INDEX

Detailed requirements for each domain are tracked in subsystem spec documents under `docs/subsystems/`:

| Doc | Domain |
|-----|--------|
| `boot.md` | Boot domain (Sections A, 5, 7) |
| `kernel.md` | Kernel domain (Section B, 9) |
| `hardware.md` | Hardware/driver domain (Section C, 11, 12, 13, 14, 15, 16, 33) |
| `storage.md` | Storage domain (Section D, 10, 22, 30, 32) |
| `userland.md` | Userland/shell (Section E, 23, 24) |
| `ui.md` | UI/desktop (Section F, 13, 26, 27) |
| `network.md` | Networking (Section G, 15) |
| `security.md` | Security/accounts (Section H, 18, 19, 20) |
| `recovery.md` | Recovery/diagnostics (Section I, 28, 29, 31) |
| `accessibility.md` | Accessibility (Section J, 26) |
| `settings.md` | Settings system (Section 21) |
| `installer.md` | Installer (Section 30) |
| `packages.md` | Package/update system (Section 25) |
| `portability.md` | Architecture abstraction (Section 6) |
| `knowledgebase.md` | PC hardware knowledgebase (Section 34) |
| `hardware-compat.md` | Hardware compatibility database (Section 35) |

---

*This document is version-controlled. Do not edit manually without updating the version number and noting the change in `docs/CHANGELOG.md`.*
