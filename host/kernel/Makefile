# =============================================================================
# bubblyVM -- Kernel Build System
# File: host/kernel/Makefile
#
# Builds the Tier 0 kernel binary (kernel.elf) from:
#   - Assembly entry stubs (entry.asm, boot32.asm)
#   - C kernel code (kernel.c)
#
# Requirements (install on your build machine):
#   Linux/WSL:
#     sudo apt install nasm gcc-multilib binutils grub-pc-bin grub-efi-amd64-bin
#     sudo apt install xorriso mtools qemu-system-x86
#   Or use cross-compiler: x86_64-elf-gcc (see docs/BUILD_SETUP.md)
#
# Usage:
#   make          -- Build kernel.elf
#   make iso      -- Build bootable ISO (requires GRUB tools)
#   make qemu     -- Build ISO and launch in QEMU (BIOS mode)
#   make qemu-efi -- Build ISO and launch in QEMU (UEFI mode, needs OVMF)
#   make clean    -- Remove build artifacts
# =============================================================================

# --- Toolchain ---
# Use cross-compiler if available, fall back to system gcc with multilib
CC      := $(shell which x86_64-elf-gcc 2>/dev/null || echo gcc)
AS      := nasm
LD      := $(shell which x86_64-elf-ld 2>/dev/null || echo ld)

# --- Flags ---
CFLAGS  := -ffreestanding -nostdlib -nostdinc \
           -Wall -Wextra -Werror \
           -m32 \
           -O2 \
           -std=c11 \
           -fno-stack-protector \
           -fno-pie -fno-pic

ASFLAGS := -f elf32

LDFLAGS := -T kernel.ld \
           -m elf_i386 \
           --oformat elf32-i386

# --- Source files ---
ASM_SRCS := arch/x86_64/boot/entry.asm \
            arch/x86_64/boot/boot32.asm

C_SRCS   := kernel.c

# --- Object files ---
OBJ_DIR  := ../../build/obj/kernel
ASM_OBJS := $(ASM_SRCS:%.asm=$(OBJ_DIR)/%.o)
C_OBJS   := $(C_SRCS:%.c=$(OBJ_DIR)/%.o)
ALL_OBJS := $(ASM_OBJS) $(C_OBJS)

# --- Output ---
KERNEL_ELF := ../../build/kernel.elf
ISO_DIR    := ../../build/iso
ISO_FILE   := ../../build/bubblyvm.iso

# =============================================================================
# Targets
# =============================================================================

.PHONY: all iso qemu qemu-efi clean

all: $(KERNEL_ELF)
	@echo "[BUILD] Kernel binary: $(KERNEL_ELF)"

# --- Compile C sources ---
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "[CC] $<"
	$(CC) $(CFLAGS) -c $< -o $@

# --- Assemble ASM sources ---
$(OBJ_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	@echo "[AS] $<"
	$(AS) $(ASFLAGS) $< -o $@

# --- Link kernel ---
$(KERNEL_ELF): $(ALL_OBJS)
	@mkdir -p $(dir $@)
	@echo "[LD] Linking kernel..."
	$(LD) $(LDFLAGS) -o $@ $(ALL_OBJS)
	@echo "[LD] kernel.elf size:"
	@size $@

# --- Build bootable ISO ---
iso: $(KERNEL_ELF)
	@echo "[ISO] Building bootable ISO..."
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(KERNEL_ELF) $(ISO_DIR)/boot/kernel.elf
	@cp ../../build/grub/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	grub-mkrescue -o $(ISO_FILE) $(ISO_DIR)
	@echo "[ISO] Built: $(ISO_FILE)"

# --- Launch in QEMU (BIOS mode) ---
qemu: iso
	@echo "[QEMU] Launching bubblyVM in QEMU (BIOS)..."
	../../build/scripts/run-qemu.sh

# --- Launch in QEMU (UEFI mode) ---
qemu-efi: iso
	@echo "[QEMU] Launching bubblyVM in QEMU (UEFI)..."
	../../build/scripts/run-qemu-uefi.sh

# --- Clean build artifacts ---
clean:
	@echo "[CLEAN] Removing build artifacts..."
	@rm -rf $(OBJ_DIR) $(KERNEL_ELF) $(ISO_DIR) $(ISO_FILE)
	@echo "[CLEAN] Done."
