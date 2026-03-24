# Contributing to BubblyVM

Thanks for your interest in contributing! BubblyVM is in early design phase, so contributions of all kinds are valuable — code, documentation, testing, and ideas.

---

## Getting Started

1. **Fork** the repository on GitHub.
2. **Clone** your fork locally:
   ```bash
   git clone https://github.com/YOUR_USERNAME/BubblyVM.git
   cd BubblyVM
   ```
3. **Create a branch** for your work:
   ```bash
   git checkout -b feature/your-feature-name
   ```
4. Make your changes, then **commit** with a clear message:
   ```bash
   git commit -m "component: short description of change"
   ```
5. **Push** your branch and open a **Pull Request** against `main`.

---

## Commit Message Format

Use a short prefix matching the component you changed:

| Prefix | Use for |
|---|---|
| `host:` | Hypervisor host changes |
| `security-node:` | Security Node VM changes |
| `tile-launcher:` | Desktop UI changes |
| `app-vm-manager:` | App VM daemon changes |
| `resource-scaler:` | Resource scaling daemon |
| `storage:` | Image/overlay tooling |
| `guest-tools:` | Guest agent packages |
| `docs:` | Documentation only |
| `build:` | Build scripts, CI |
| `scaffold:` | Folder/file structure (no logic) |

Example: `tile-launcher: add glass border shader`

---

## What to Work On

- Check the [Issues](https://github.com/Fizzolas/BubblyVM/issues) tab for open tasks.
- If you have a new idea, open an issue first so it can be discussed before work begins.
- Documentation improvements are always welcome, especially for the `docs/` folder.

---

## Code Style

- **Shell scripts:** POSIX-compatible where possible; use `#!/bin/sh` unless bash-specific features are needed.
- **Python:** Follow PEP 8; use 4-space indentation.
- **C:** K&R style, 4-space indentation.
- **JSON (tile definitions):** 2-space indentation, all keys lowercase with hyphens.

---

## Reporting Bugs

Open a GitHub Issue and include:
- What you were doing
- What you expected to happen
- What actually happened
- Your hardware (CPU model, RAM, GPU, whether IOMMU is enabled)

---

## License

By contributing, you agree that your contributions will be licensed under the same license as this project. See [LICENSE](LICENSE).
