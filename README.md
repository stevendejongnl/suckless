# Suckless

## Dwm

### Applied Patches (16 total)

#### Core Functionality (4 patches)
- [autostart](https://dwm.suckless.org/patches/autostart) - Run startup scripts on DWM init
- [systray](https://dwm.suckless.org/patches/systray) - System tray support in status bar
- [shift tools](https://dwm.suckless.org/patches/shift-tools) - Enhanced workspace navigation
- [pertag](https://dwm.suckless.org/patches/pertag) - Per-tag layout management

#### Visual Enhancements (3 patches)
- [colorbar/statuscolors](https://dwm.suckless.org/patches/statuscolors) - Dynamic status bar colors
- **[scratchpad](https://dwm.suckless.org/patches/scratchpad)** (2026-01-19) - Toggle-able floating terminal
  - Keybinding: MODKEY+grave
  - Uses dedicated tag for persistence
- [vanitygaps](https://dwm.suckless.org/patches/vanitygaps) (2026-01-19) - Customizable window gaps
  - MODKEY+Alt+U/Shift+U: Adjust all gaps
  - MODKEY+Alt+I/Shift+I: Inner gaps only
  - MODKEY+Alt+O/Shift+O: Outer gaps only
  - MODKEY+Alt+6: Toggle gaps on/off

#### Input & Window Management (6 patches)
- [focusmonmouse](https://dwm.suckless.org/patches/focusmonmouse) - Focus monitor with mouse
- [focusonclick](https://dwm.suckless.org/patches/focusonclick) - Focus on click
- **[statuscmd](https://dwm.suckless.org/patches/statuscmd)** (2026-01-19, custom modified)
  - Clickable status bar sections
  - Custom byte range 0x10-0x1F (avoids conflicts with statuscolors 0x01-0x0C)
  - Left/Middle/Right clicks send signals to statusbar daemon
- **[movestack](https://dwm.suckless.org/patches/movestack)** (2026-01-19)
  - MODKEY+Shift+J/K: Move windows in stack
- **[swallow](https://dwm.suckless.org/patches/swallow)** (2026-01-19, community 6.5 version)
  - Terminal-launched GUI apps replace terminal temporarily
  - Close GUI to restore terminal
  - Requires libxcb for window introspection

#### Session Management (2 patches)
- [restartsig](https://dwm.suckless.org/patches/restartsig) - MODKEY+Ctrl+Shift+Q restarts DWM
- [restoreafterrestart](https://dwm.suckless.org/patches/restoreafterrestart) - Restore windows after restart

### Patches todo

- [alwayscenter](https://dwm.suckless.org/patches/alwayscenter) - Center new floating windows
- [ewmhtags](https://dwm.suckless.org/patches/ewmhtags) - Extended window manager hints
- [placemouse](https://github.com/bakkeby/patches/blob/master/dwm/dwm-placemouse-6.5.diff) - Place new window near mouse

---

## Implementation Notes

### Custom Statuscmd Modification (2026-01-19)

The statuscmd patch was modified to use byte range **0x10-0x1F** instead of the default **0x01-0x1F** to avoid conflicts with the existing statuscolors patch which uses **0x01-0x0C**.

**Status bar marker bytes:**
- `\x0C` (12): Claude API usage (statuscolors)
- `\x08` (8): Date/time (statuscolors)
- `\x01` (1): Reset/normal (statuscolors)
- `\x10` (16): Battery section (statuscmd, clickable)
- `\x11` (17): Date section (statuscmd, clickable)
- `\x12` (18): Package updates (statuscmd, clickable)

**Click handlers:**
- Button 1 (left click): Signal 1 to statusbar daemon
- Button 2 (middle click): Signal 2 to statusbar daemon
- Button 3 (right click): Signal 3 to statusbar daemon

Status bar script location: `~/.dwm/statusbar/statusbar.sh`

### Vanitygaps Compatibility

The vanitygaps patch was a 6.2 patch adapted to dwm 6.5. The tile() function from the original dwm.c was removed and replaced with the vanitygaps version (included via `vanitygaps.c`).

**Default gap values:**
```c
gappih = 10  /* horizontal inner gaps */
gappiv = 10  /* vertical inner gaps */
gappoh = 10  /* horizontal outer gaps */
gappov = 10  /* vertical outer gaps */
smartgaps = 0  /* disable smart gap toggling */
```

### Swallow Configuration

Terminal applications are marked with `isterminal: 1` in rules to enable swallowing.

**Swallow settings in config.def.h:**
```c
swallowfloating = 0    /* don't swallow floating windows by default */
swterminheritfs = 1    /* terminal inherits fullscreen on unswallow */
```

### Build Dependencies

**Required packages (Arch Linux):**
```bash
sudo pacman -S libx11 libxinerama libxft libxrandr libfontconfig
sudo pacman -S libxcb xcb-util-wm  # for swallow patch
```

### Rebuild Instructions

After modifying config.def.h:
```bash
cd ~/workspace/builds/suckless
make dwm
# Install requires sudo - will prompt for password
```

The Makefile automatically:
1. Removes old `dwm/config.h`
2. Regenerates from `dwm/config.def.h`
3. Rebuilds dwm
4. Installs to `/usr/local/bin/dwm`

### Patch History

All patches are tracked in `dwm/patches/` with timestamps:
```
dwm/patches/2026-01-19_1_scratchpad/      # scratchpad
dwm/patches/2026-01-19_2_statuscmd/       # statuscmd (custom modified)
dwm/patches/2026-01-19_3_movestack/       # movestack
dwm/patches/2026-01-19_4_vanitygaps/      # vanitygaps
dwm/patches/2026-01-19_5_swallow/         # swallow
```

Each patch directory contains:
- Original patch file (upstream)
- Applied diff showing actual changes made to this codebase
