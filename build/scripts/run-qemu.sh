#!/bin/bash
# =============================================================================
# bubblyVM -- QEMU Launch Script (BIOS mode)
# File: build/scripts/run-qemu.sh
#
# Launches bubblyVM in QEMU using traditional BIOS boot.
# Use this for Tier 0 and Tier 1 development -- fastest iteration.
#
# Requirements: qemu-system-x86_64
#   Install: sudo apt install qemu-system-x86  (Linux/WSL)
#
# Serial output goes to your terminal (stdio).
# Use Ctrl+C to exit QEMU.
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ISO_FILE="$SCRIPT_DIR/../bubblyvm.iso"

if [ ! -f "$ISO_FILE" ]; then
    echo "[ERROR] ISO not found: $ISO_FILE"
    echo "        Run 'make iso' in host/kernel/ first."
    exit 1
fi

echo "[QEMU] Booting bubblyVM (BIOS mode)..."
echo "[QEMU] ISO: $ISO_FILE"
echo "[QEMU] Serial output will appear below."
echo "[QEMU] Press Ctrl+C to exit."
echo "---------------------------------------"

qemu-system-x86_64 \
    -cdrom "$ISO_FILE" \
    -m 256M \
    -smp 2 \
    -serial stdio \
    -display sdl \
    -no-reboot \
    -no-shutdown \
    -d int,cpu_reset 2>&1 | grep -v "^$"
