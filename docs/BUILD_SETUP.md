# bubblyVM — Build Environment Setup

This guide walks you through setting up your machine to build and test bubblyVM. You do not need to be a programmer to follow this — every step is explained.

---

## What You Will Need

| Tool | What It Does |
|------|--------------|
| `nasm` | Assembles the low-level CPU startup code |
| `gcc` (multilib) | Compiles the C kernel code |
| `ld` (binutils) | Links object files into a kernel binary |
| `grub-mkrescue` | Packages the kernel into a bootable ISO |
| `xorriso` | Required by grub-mkrescue to create ISOs |
| `mtools` | Required by grub-mkrescue for FAT handling |
| `qemu-system-x86` | Runs bubblyVM in a virtual machine for testing |
| `ovmf` | UEFI firmware for QEMU (UEFI boot testing) |

---

## Install on Linux or WSL (Windows Subsystem for Linux)

Open a terminal and run:

```bash
sudo apt update
sudo apt install -y \
    nasm \
    gcc \
    gcc-multilib \
    binutils \
    grub-pc-bin \
    grub-efi-amd64-bin \
    xorriso \
    mtools \
    qemu-system-x86 \
    ovmf
```

If you are on **Windows with WSL**, this is the recommended approach. Install WSL first:
1. Open PowerShell as Administrator
2. Run: `wsl --install`
3. Restart your PC
4. Open the Ubuntu app and run the apt commands above

---

## Install on Fedora / RHEL

```bash
sudo dnf install -y \
    nasm gcc binutils \
    grub2-tools grub2-efi-x64-modules \
    xorriso mtools \
    qemu-system-x86 edk2-ovmf
```

---

## Install on Arch Linux

```bash
sudo pacman -S nasm gcc binutils grub xorriso mtools qemu ovmf
```

---

## Building the Kernel

1. Clone or download the bubblyVM repository
2. Open a terminal in the repo folder
3. Navigate to the kernel source:
   ```bash
   cd host/kernel
   ```
4. Build the kernel binary:
   ```bash
   make
   ```
5. Build a bootable ISO:
   ```bash
   make iso
   ```
6. Launch in QEMU (BIOS mode):
   ```bash
   make qemu
   ```
7. Launch in QEMU (UEFI mode):
   ```bash
   make qemu-efi
   ```

---

## What You Should See

When the build succeeds and QEMU launches, you should see:

```
bubblyVM v0.0.1-tier0
======================
[BOOT] Serial console: OK
[BOOT] VGA console:    OK
[BOOT] Multiboot2 info @ 0x...
[MEM]  Memory map (N entries):
  [0] Base=0x0 Len=0x9fc00 Type=Usable RAM
  ...
[BOOT] Tier 0 boot complete. Halting.
```

This output appearing means Tier 0 is working correctly.

---

## Troubleshooting Build Errors

**`nasm: command not found`** — Run `sudo apt install nasm`

**`grub-mkrescue: command not found`** — Run `sudo apt install grub-pc-bin xorriso mtools`

**`qemu-system-x86_64: command not found`** — Run `sudo apt install qemu-system-x86`

**Linker error: `cannot find -lgcc`** — Run `sudo apt install gcc-multilib`

**OVMF not found (UEFI script)** — Run `sudo apt install ovmf`, then retry `make qemu-efi`

**QEMU shows blank screen** — The VGA output may not show. Check the terminal window where you ran the script — serial output appears there.

---

## Cross-Compiler (Optional, Advanced)

For the cleanest builds, use a dedicated x86_64-elf cross-compiler instead of system gcc. This avoids some host-system interference. Instructions for building it are in `docs/cross-compiler.md` (coming in Tier 1).

---

*Questions? Open an issue on the [BubblyVM GitHub repo](https://github.com/Fizzolas/BubblyVM).*
