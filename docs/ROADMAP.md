# bubblyVM — Development Roadmap

> This roadmap tracks progress from pre-boot to daily-driver OS across six tiers.  
> Each tier has a concrete definition of done. No tier is "complete" until all checkboxes are verified.

---

## Current Status: Pre-Tier 0 — Foundation Setup

Project scaffolding, specification documents, and error tracking are being established. No bootable kernel exists yet.

---

## Tier 0 — Boots to Kernel Console

**Definition of Done:** System boots in QEMU, kernel initializes, prints to serial/framebuffer console. No userland required.

### Boot Domain
- [ ] Choose and configure bootloader (GRUB2 recommended for UEFI/BIOS compat)
- [ ] Define boot partition layout (EFI System Partition + kernel partition)
- [ ] Write or adapt kernel entry point for x86_64 long mode
- [ ] Establish serial output (COM1) for early debug logging
- [ ] Establish framebuffer output (VESA/UEFI GOP) as fallback display
- [ ] Kernel prints version string and halts cleanly
- [ ] Boot confirmed in QEMU (BIOS mode)
- [ ] Boot confirmed in QEMU (UEFI mode via OVMF)

### Kernel Baseline
- [ ] x86_64 long mode entry established
- [ ] GDT and IDT initialized
- [ ] Basic interrupt handling (at minimum: divide-by-zero, page fault, general protection)
- [ ] Physical memory map parsed from firmware (e820 or UEFI memory map)
- [ ] Page allocator (buddy allocator or bitmap) functional
- [ ] Kernel heap (slab or kmalloc equivalent) functional
- [ ] Kernel stack established with guard page
- [ ] Timer interrupt firing (PIT or APIC timer)
- [ ] Panic handler: prints message, halts, does not silently reboot

### Validation Criteria (Tier 0 Complete)
- [ ] QEMU BIOS boot: kernel console output visible on serial
- [ ] QEMU UEFI boot: kernel console output visible on framebuffer
- [ ] Panic test: deliberate null deref produces clean panic message, not hang
- [ ] No silent failures: all early init steps log success or failure

---

## Tier 1 — Usable Text-Mode OS

**Definition of Done:** User can boot to a shell, run commands, read/write files, and access basic networking.

### Userland
- [ ] Init process (PID 1) starts after kernel
- [ ] Shell (busybox ash or custom) launches from init
- [ ] Core utilities: ls, cat, cp, mv, rm, mkdir, rmdir, echo, pwd, cd, ps, kill, mount, umount
- [ ] Environment variables functional (PATH, HOME)
- [ ] Shell history (up arrow recalls previous command)
- [ ] Tab completion (at minimum: file path completion)
- [ ] Help / usage text for all builtins

### Storage
- [ ] Block device layer: read/write sectors to SATA or virtio-blk device
- [ ] GPT partition table reader
- [ ] Filesystem: ext4 read/write OR custom FS with equivalent feature set
- [ ] Mount root filesystem from disk
- [ ] /tmp as tmpfs or ramdisk
- [ ] Persistent writes survive reboot

### Process / Task Management
- [ ] Fork / exec model functional
- [ ] Processes get unique PIDs
- [ ] ps command lists running processes
- [ ] kill command terminates processes by PID
- [ ] Zombie reaping by init
- [ ] Signals: SIGKILL, SIGTERM, SIGINT delivered correctly

### Networking (Basic)
- [ ] Ethernet NIC detected (virtio-net in QEMU)
- [ ] DHCP client obtains IP address
- [ ] DNS resolver functional (resolv.conf equivalent)
- [ ] ping equivalent confirms connectivity
- [ ] Basic socket API (TCP/UDP)

### Logging
- [ ] Kernel log buffer accessible (dmesg equivalent)
- [ ] Service/init logs written to /var/log or equivalent
- [ ] Boot log preserved across reboots

### Validation Criteria (Tier 1 Complete)
- [ ] Boot to shell in <20 seconds on QEMU 2vCPU / 2GB RAM
- [ ] Create file, write to it, reboot, read it back — content preserved
- [ ] Ping 8.8.8.8 from shell succeeds
- [ ] ps shows kernel threads and userspace shell
- [ ] Launch background process, kill it by PID, confirm gone
- [ ] No kernel panic or hang during 1-hour idle test

---

## Tier 2 — Basic Desktop OS

**Definition of Done:** User can boot to a graphical environment, open a file manager and terminal, and change basic settings.

*Tier 2 planning begins after Tier 1 validation is complete.*

### Planned Areas
- [ ] Graphical compositor / display server
- [ ] Tile launcher (bubblyVM's Aero-style VM-per-app UI)
- [ ] File manager
- [ ] Terminal emulator
- [ ] Settings app skeleton (Display, Sound, Network, Accounts)
- [ ] Notification framework
- [ ] Login screen
- [ ] USB device detection and mount
- [ ] Audio output (ALSA or equivalent)
- [ ] Wi-Fi management UI

---

## Tier 3 — Daily Driver

*Planning begins after Tier 2 validation.*

- Full accessibility implementation
- App ecosystem and package manager
- Installer (guided + advanced)
- Recovery environment
- Update system with rollback
- Backup and restore
- Broad peripheral support
- Full settings system (all 20 categories)

---

## Tier 4 — Broad PC Support + Portable Architecture

*Planning begins after Tier 3 validation.*

- Hardware compatibility database populated
- Real hardware testing (beyond QEMU)
- Architecture abstraction layer formalized
- ARM64 feasibility document complete
- Installer works on diverse real hardware

---

## Tier 5 — Multi-Architecture

*Planning begins after Tier 4 validation.*

- ARM64 port in development
- RISC-V exploratory build
- Architecture-neutral subsystem interfaces verified

---

## Milestone Tracking

| Milestone | Target Tier | Status |
|-----------|-------------|--------|
| Repo scaffolding and spec docs | Pre-Tier 0 | ✅ Complete |
| Bootloader configured and tested in QEMU | Tier 0 | 🔲 Not started |
| Kernel boots to console | Tier 0 | 🔲 Not started |
| Shell accessible, basic utilities work | Tier 1 | 🔲 Not started |
| Persistent storage read/write | Tier 1 | 🔲 Not started |
| Basic networking functional | Tier 1 | 🔲 Not started |
| Graphical environment boots | Tier 2 | 🔲 Not started |
| Tile launcher functional | Tier 2 | 🔲 Not started |
| Full settings app | Tier 3 | 🔲 Not started |
| Installer complete | Tier 3 | 🔲 Not started |
| Recovery environment | Tier 3 | 🔲 Not started |
