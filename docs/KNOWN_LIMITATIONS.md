# bubblyVM — Known Limitations

> This file tracks confirmed limitations that are not yet fixed. Every entry must include a workaround (if any) and a planned fix version.

---

## Active Limitations

| # | Issue Description | Affected Hardware / Config | Workaround | Planned Fix Version | Verification Test When Fixed |
|---|-------------------|---------------------------|------------|---------------------|------------------------------|
| 1 | Project is at pre-Tier-0 state — no bootable kernel yet | All platforms | N/A — development has not reached boot stage | Tier 0 milestone | Boot to kernel console in QEMU |
| 2 | No initramfs or userland exists yet | All platforms | N/A | Tier 1 milestone | Shell accessible after boot |
| 3 | No installer exists | All platforms | Manual QEMU disk image setup | Tier 2 milestone | Guided installer completes on QEMU |
| 4 | No hardware driver layer beyond planning | All platforms | QEMU virtio devices only | Tier 1–2 milestone | Real NIC, USB, display detected |
| 5 | No settings system implemented | All platforms | Config files edited manually | Tier 2 milestone | Settings app reads/writes values |

---

## Resolved Limitations

*Entries moved here once the planned fix is verified.*

---

## Rules

- Every known limitation must have at minimum a description and affected scope.
- If there is no workaround, write "None — blocking".
- If there is no planned fix timeline, write "TBD".
- Limitations must not be removed until the verification test passes.
