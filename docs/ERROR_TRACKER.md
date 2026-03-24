# bubblyVM — Error Tracker

> Maintained by: AI Project Manager  
> Updated: 2026-03-24  
> Format: One row per discovered issue. Never delete rows — mark resolved issues with ✅ in Fix Status.

---

## Active Issues

| # | Timestamp | Section | File / Function | Error Type | Symptoms | Logs / Evidence | Suspected Cause | Fix Attempted | Fix Status | Verification Test |
|---|-----------|---------|-----------------|------------|----------|-----------------|-----------------|---------------|------------|-------------------|
| — | — | — | — | — | No issues logged yet | — | — | — | — | — |

---

## Resolved Issues

*Resolved issues will be moved here with ✅ Fix Status and the verification method noted.*

---

## Usage Rules

- **Log immediately** when a validation check fails — do not wait until the end of a session.
- **Do not proceed** to the next validation check until root cause is hypothesized and a fix is planned.
- **After applying fix:** Re-run only the failed check, then 2× more for intermittency, then continue.
- **Weekly review:** Look for patterns across failures (e.g., 3 USB storage failures → driver issue).
- **Archive:** Move entries older than 2 weeks with verified fixes to the Resolved section.

---

## Error Type Reference

| Code | Meaning |
|------|---------|
| `BOOT` | Bootloader or early kernel failure |
| `KERNEL` | Kernel panic, memory fault, scheduler issue |
| `DRIVER` | Device driver failure or misdetection |
| `FS` | Filesystem corruption, mount failure, I/O error |
| `NET` | Network connectivity, DHCP, DNS, Wi-Fi failure |
| `UI` | UI rendering, focus, input handling failure |
| `SETTINGS` | Settings backend or UI failure |
| `SECURITY` | Auth, permissions, or privilege failure |
| `RECOVERY` | Recovery environment or safe mode failure |
| `PERF` | Performance below target threshold |
| `A11Y` | Accessibility feature failure |
| `DOCS` | Documentation inaccuracy or gap |
| `COMPAT` | Hardware compatibility regression |
| `PKG` | Package manager or update failure |
