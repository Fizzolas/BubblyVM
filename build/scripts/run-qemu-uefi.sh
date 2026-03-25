#!/bin/bash
# =============================================================================
# bubblyVM -- QEMU Launch Script (UEFI mode)
# File: build/scripts/run-qemu-uefi.sh
#
# Launches bubblyVM in QEMU using UEFI firmware (OVMF).
# Use this to test UEFI boot path (required for Tier 0 UEFI validation).
#
# Requirements: qemu-system-x86_64 + OVMF firmware
#   Install: sudo apt install qemu-system-x86 ovmf
#   OVMF path may vary; common locations:
#     /usr/share/OVMF/OVMF_CODE.fd       (Debian/Ubuntu)
#     /usr/share/edk2/x64/OVMF_CODE.fd   (Fedora)
#
# Serial output goes to your terminal (stdio).
# Use Ctrl+C to exit QEMU.
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ISO_FILE="$SCRIPT_DIR/../bubblyvm.iso"

# Find OVMF firmware
OVMF_PATHS=(
    "/usr/share/OVMF/OVMF_CODE.fd"
    "/usr/share/edk2/x64/OVMF_CODE.fd"
    "/usr/share/qemu/OVMF.fd"
)

OVMF_FILE=""
for path in "${OVMF_PATHS[@]}"; do
    if [ -f "$path" ]; then
        OVMF_FILE="$path"
        break
    fi
done

if [ -z "$OVMF_FILE" ]; then
    echo "[ERROR] OVMF firmware not found."
    echo "        Install with: sudo apt install ovmf"
    echo "        Then re-run this script."
    exit 1
fi

if [ ! -f "$ISO_FILE" ]; then
    echo "[ERROR] ISO not found: $ISO_FILE"
    echo "        Run 'make iso' in host/kernel/ first."
    exit 1
fi

echo "[QEMU] Booting bubblyVM (UEFI mode)..."
echo "[QEMU] ISO:  $ISO_FILE"
echo "[QEMU] OVMF: $OVMF_FILE"
echo "[QEMU] Serial output will appear below."
echo "[QEMU] Press Ctrl+C to exit."
echo "---------------------------------------"

qemu-system-x86_64 \
    -cdrom "$ISO_FILE" \
    -bios "$OVMF_FILE" \
    -m 256M \
    -smp 2 \
    -serial stdio \
    -display sdl \
    -no-reboot \
    -no-shutdown \
    -d int,cpu_reset 2>&1 | grep -v "^$"
