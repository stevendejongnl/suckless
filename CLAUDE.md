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
# Always run from the suckless repo root (the root Makefile wraps `sudo make -C …`).
# Sudo is required; it will prompt for password or fingerprint.
make all          # Build + install all tools
make dwm          # Window manager
make dmenu        # Menu system
make slock        # Screen locker
```

Each target removes existing `config.h`, regenerates from `config.def.h`, and runs `sudo make -C ./[tool] clean install`. Do **not** run `make` inside a tool subdirectory for install — the root Makefile is the canonical entry point.

### Applying dwm changes (hot reload)

`dwm` has the `restartsig` + `restoreafterrestart` patches, so after installing a new binary:

| Keybind                            | Action                                 |
|------------------------------------|----------------------------------------|
| `Super + Ctrl + Shift + Q`         | Restart dwm in place (session preserved) |
| `Super + Shift + Q`                | Quit dwm entirely                      |

The restart keybind picks up the new `/usr/local/bin/dwm` and keeps open windows/tags/layouts via `/tmp/dwm-session`.

From a shell, restart the same way with `kill -HUP $(pgrep -x dwm)`. Never use `SIGUSR1`: dwm has no handler, it just dies, nothing is saved and every window falls back to tag 1.

### Media and volume keys

dwm binds the `XF86Audio*` keys in `config.def.h`. Bluetooth headphones send their buttons as the same keys, through an AVRCP input device.

| Key                | Command                      |
|--------------------|------------------------------|
| `XF86AudioPlay`    | `playerctl play-pause`       |
| `XF86AudioPause`   | `playerctl pause`            |
| `XF86AudioNext`    | `playerctl next`             |
| `XF86AudioPrev`    | `playerctl previous`         |
| Volume up / down   | `pamixer --increase/--decrease 5` |
| Mute               | `pamixer --toggle-mute`      |

`playerctl` controls any MPRIS player, for example Spotify. Check the headphone buttons with `evtest` on the `(AVRCP)` device in `/dev/input`.

### Tag 9: YouTube or Spotify

Tag 9 holds YouTube (a Chromium app window) or Spotify, never both.

- `Mod+9` runs `~/.local/bin/tag9-media`: it jumps to the open YouTube or Spotify window, or shows a yad popup (class `claude-git-guard`, so dwm floats it) with a Spotify and a YouTube button when neither is open.
- When one of them opens, dwm closes the other (`closesharedtagapps`). The class is read again on `WM_CLASS` changes, because Spotify sets it after the window is mapped.
- The tag is green while Spotify plays and red while a YouTube window is open. It looks normal when neither is active.
- Spotify's play state comes from `~/.dwm/statusbar/shared-tag-playing.sh`. It writes `playing` or `stopped` to `~/.local/state/dwm-shared-tag-playing`, and dwm reads the file on each status update (once per second).
- The apps, their tag colours and the playing requirement are the `sharedtagapps` table in `config.def.h`.

Moving a window to another monitor (`Mod+Shift+,` and `Mod+Shift+.`) goes through `sendmon`:

- A fullscreen window is resized to the new monitor. `showhide` skips fullscreen clients, so without this the window stays drawn on the old monitor.
- A YouTube or Spotify window keeps tag 9, and the target monitor switches to tag 9 (`viewonmonitor`) so the window stays visible. Other windows take the tags the target monitor shows.

### Status bar clicks

A byte `0x10`-`0x1F` in the status text marks a clickable segment (it is not drawn). A left click on the segment makes dwm run `~/.dwm/statusbar/click.sh <byte> <button>`, with the byte as a decimal number.

| Byte | Segment         | Left click opens                      |
|------|-----------------|---------------------------------------|
| 17   | date (`0x11`)   | Google Calendar                       |
| 18   | updates (`0x12`)| nothing yet                           |
| 19   | Keito (`0x13`)  | the Keito app (`app.keito.ai`)        |

The Keito badge is green at first, yellow after 2h and orange after 4h. Status escapes `0x02` and `0x03` draw the yellow and orange badge (`SchemeStatusBadgeMedium` and `SchemeStatusBadgeHigh`). Keep these two last in the scheme enum, after `SchemeBadgeRed`, because the escape is mapped by offset.

`drw_text` puts all glyphs on the baseline of the first font, so the Nerd Font icons line up with the text.

### Window memory

dwm remembers the last tag and monitor per window class + instance in `~/.local/state/dwm-windows` (`MEMORY_FILE`). A new window with no rule tag opens there, also after logout. Rules win, terminals are skipped, and popups are skipped too (window type dialog, utility or splash, or a window with a transient-for hint). Moving the pointer onto a window or onto another monitor makes that monitor and window active (`enternotify`, `motionnotify`), so `selmon` is the cursor's monitor. A popup opens on the monitor and tags of the cursor (`selmon`) and is never saved. If the saved monitor is gone the first monitor is used (monitors are matched by their x,y origin).

## slock

`slock` is started through `~/.dwm/lock.sh` (the `lock.desktop` launcher), not directly. The script pauses all media (`playerctl -a pause`), mutes the sound and unmutes it after unlock (only when it was on before), builds a blurred copy of the current wallpaper for every monitor in `~/.cache/slock/bg.ppm`, and runs `slock`. `lock.sh --prepare` only builds the image. The image is cached until the wallpaper or the monitors change.

slock reads that image as a raw 8-bit PPM (P6), so the script writes it with `-depth 8`. A 16-bit PPM is silently ignored and the lock screen falls back to the plain colour.

On top of the image slock draws, with Xft, a clock and date on every monitor (`timefmt`, `datefmt`), a state bar (grey idle, cyan while typing, red after a wrong password) and the number of failed attempts. All of it is in `config.def.h`.

After `panicafter` seconds (120) without a key, slock shows an endless fake kernel panic. Any key ends it and shows the lock screen again. That key is not part of the password. Set `panicafter` to 0 to turn it off.

To test without locking the real session, build a copy with a fixed test hash and no privilege drop, and run it on `Xvfb`.

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
- `systray ignore list` - `systrayignore[]` in `config.def.h` refuses tray icons by WM_CLASS (find one with `xprop -id <icon window> WM_CLASS`)
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
- Check that dwm source is at expected version (6.8)
- Verify patch file path and format
- Some patches may conflict if applied in different order

**Build requires sudo**:
- All binaries install to `/usr/local/bin` which requires sudo
- Needed for: dwm (exec), dmenu (general bin), slock (for root access)
