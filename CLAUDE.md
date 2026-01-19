# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

This is a suckless X11 utilities build repository containing three minimal, single-header-file configured tools:
- **dwm** (Dynamic Window Manager) - Window tiling manager (~2.8k LOC)
- **dmenu** (Dynamic Menu) - Lightweight application launcher (~800 LOC)
- **slock** (Simple Lock) - Screen locker (~400 LOC)

All tools follow the suckless philosophy: code-based configuration with no external config files.

## Build Commands

```bash
# Build and install all tools (requires sudo)
make all

# Build individual tools
make dwm         # Window manager
make dmenu       # Menu system
make slock       # Screen locker
```

Each target removes existing `config.h`, regenerates from `config.def.h`, and runs `sudo make -C ./[tool] clean install`.

## Configuration System

All three tools use the **suckless config.h pattern**:

1. Source configuration: `[tool]/config.def.h` (always tracked in git)
2. Generated configuration: `[tool]/config.h` (auto-generated, not tracked)
3. Build settings: `[tool]/config.mk` (compiler flags, paths)

**To modify configuration**:
1. Edit `[tool]/config.def.h`
2. Run `make [tool]` - automatically regenerates config.h and rebuilds

**Example**: Change dwm border from 3px to 5px:
```bash
cd dwm
# Edit line 6: static const unsigned int borderpx = 5;
cd ..
make dwm
```

## dwm Architecture

**Key features applied via patches** (stored in `dwm/patches/`):
- `systray` - System tray support in status bar
- `pertag` - Per-tag layout settings
- `shift-tools` - Enhanced tool selection
- `autostart` - Run custom startup scripts
- `colorbar` - Additional color scheme support
- `statuscolors` - Dynamic status bar coloring
- `restartsig`, `restoreafterrestart` - Session state preservation
- `focusmonmouse`, `focusonclick` - Mouse input handling

**Status bar color schemes** (config.def.h, lines 23-46):
- `col_status_norm` - Default status text
- `col_status_low` - Teal/cyan for info
- `col_status_medium` - Yellow for medium alerts
- `col_status_high` - Orange for warnings
- `col_status_critical` - Red/pink for critical
- `col_status_success` - Green for success

The status bar is updated via `~/.dwm/statusbar/statusbar.sh` which calls helper scripts:
- `battery.sh` - Battery percentage with color thresholds
- `cpu-usage.sh`, `memory-usage.sh` - System monitoring
- `package-updates.sh` - Update count
- `pipewire.sh`, `bluetooth.sh` - Audio and Bluetooth status

## Patch Management

Patches are organized by date and stored in `dwm/patches/`:
```
dwm/patches/
├── 2024-08-12_1_systray/
│   ├── dwm-systray-6.5.diff (original)
│   └── dwm-systray-6.5-applied.diff (post-application state)
├── 2024-08-13_1_shift-tools/
└── ...
```

**To apply a new patch**:
```bash
cd dwm
patch -p1 < patches/[date]_[name]/dwm-*.diff
# Edit config.def.h for any patch-specific settings
cd ..
make dwm
```

**Patch history** shows which patches have been applied and when - useful for tracking cumulative changes to dwm.c.

## Dependencies

**System packages** (Arch Linux):
```bash
# Core X11
libx11 libxinerama libxft libxrandr libfontconfig

# Development headers needed for building
libx11-dev libxinerama-dev libxft-dev libxrandr-dev libfontconfig-dev
```

**Build tools**: gcc/clang, make

**Compiler flags** (from config.mk):
- `-std=c99 -pedantic -Wall` - Strict C99 with warnings
- `-Os` - Size optimization
- `-Wno-deprecated-declarations` - Suppress XSS warnings

## Git Workflow

Repository tracks both:
- **origin** (GitHub fork) - Personal modifications
- **upstream** (suckless.org) - Original source for updates

**Key branches**: `main` tracks active development

**Recent commits** show active patches being applied and indentation standardization (tabs vs spaces).

## Testing & Verification

No formal test suite. Verification through:
1. **Compilation**: `make clean && make` should produce zero errors/warnings
2. **Runtime**: Start X session and test dwm behavior
3. **Git history**: Track which patches are applied and when

Build artifacts (`.o` files, compiled binaries) are ignored via `.gitignore`.

## Project Structure

```
├── dwm/
│   ├── config.def.h        # Configuration template (227 lines)
│   ├── config.mk           # Build settings
│   ├── dwm.c               # Main source (~2800 LOC with patches)
│   ├── drw.c/drw.h         # Drawing utilities (Xft wrapper)
│   ├── util.c/util.h       # General utilities
│   ├── Makefile            # Per-project build rules
│   └── patches/            # Timestamped patch history
│
├── dmenu/
│   ├── config.def.h        # Configuration (31 lines)
│   ├── config.mk
│   ├── dmenu.c             # Main source (~800 LOC)
│   ├── stest.c             # Test utility
│   ├── dmenu_run, dmenu_path  # Helper scripts
│   └── Makefile
│
├── slock/
│   ├── config.def.h        # Configuration (15 lines)
│   ├── config.mk
│   ├── slock.c             # Main source (~400 LOC)
│   ├── explicit_bzero.c    # Secure memory clearing
│   └── Makefile
│
├── Makefile                # Root build orchestrator
├── README.md               # Applied and planned patches
└── .gitignore              # Excludes config.h and build artifacts
```

## Key Implementation Details

**Status bar integration** (recent change):
- Old approach: C program slstatus calling shell scripts
- New approach: Simple `~/.dwm/update-statusbar.sh` wrapper calling `~/.dwm/statusbar/statusbar.sh` every 1 second
- Uses escape sequences in status string for dynamic coloring

**Indentation standardization** (Jan 2026):
- Entire codebase normalized to use tabs instead of mixed indentation

**Session persistence**:
- `restartsig` and `restoreafterrestart` patches enable dwm to save/restore window state across restarts
- Session data stored in `/tmp/dwm-session` (defined in config.def.h line 3)

## Performance Characteristics

All tools are extremely lightweight:
- dwm ~2.8k LOC including patches
- dmenu ~800 LOC
- slock ~400 LOC
- Total compiled size typically <100KB each
- Minimal dependencies (X11 core only, no GTK/Qt)

## Common Issues & Solutions

**Config.h changes not taking effect**:
- Root Makefile always removes config.h before building
- Must run `make [tool]` from repo root, not from tool subdirectory

**Patches failing to apply**:
- Check that dwm source is at expected version (6.5)
- Verify patch file path and format
- Some patches may conflict if applied in different order

**Build requires sudo**:
- All binaries install to `/usr/local/bin` which requires sudo
- Needed for: dwm (exec), dmenu (general bin), slock (for root access)
