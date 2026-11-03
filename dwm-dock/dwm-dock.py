#!/usr/bin/env python3
"""Hover dock + Super+Tab switcher for dwm.

Needs the dwm patch that publishes _DWM_CLIENT_INFO and handles _DWM_FOCUS_CLIENT.
Hover: touch the bottom edge of the left half of a monitor to show the dock there.
Switch: Super+Tab / Super+Shift+Tab cycle; staying on an app for HOLD_MS activates it.
"""
import struct

import gi
gi.require_version("Gtk", "3.0")
gi.require_version("Gdk", "3.0")
from gi.repository import Gdk, GdkPixbuf, GLib, Gtk
from Xlib import X, XK, display, error, protocol

ICON = 48
HOLD_MS = 1000       # staying this long on an app in switcher mode activates it
SHOW_MS = 600        # pointer must rest on the edge this long to show the dock
LEAVE_MS = 400       # pointer outside the dock this long hides it
POLL_MS = 80
EDGE_PX = 1          # bottom rows of the monitor that trigger the dock
MOD = X.Mod4Mask
LOCKS = (0, X.LockMask, X.Mod2Mask, X.LockMask | X.Mod2Mask)

CSS = b"""
window { background: rgba(14,17,23,0.94); border-top: 2px solid #3EE6E0; border-radius: 10px 10px 0 0; }
.item { padding: 8px 8px 4px 8px; border-radius: 8px; }
.item.sel { background: rgba(62,230,224,0.22); }
.item.active { background: rgba(127,216,143,0.14); }
.tag { color: #9aa4b2; font-size: 9pt; }
.title { color: #e6e9ee; font-size: 10pt; padding: 2px 10px 6px 10px; }
"""


class Dock:
    def __init__(self):
        self.d = display.Display()
        self.root = self.d.screen().root
        a = self.d.intern_atom
        self.A = {n: a(n) for n in (
            "_NET_CLIENT_LIST", "_NET_ACTIVE_WINDOW", "_NET_WM_NAME", "_NET_WM_ICON",
            "_DWM_CLIENT_INFO", "_DWM_FOCUS_CLIENT", "UTF8_STRING")}
        self.icons = {}
        self.clients = []          # [(win, title, cls, tag, pixbuf)]
        self.sel = -1
        self.switching = False
        self.hold_id = None
        self.leave_id = None
        self.items = []

        self.win = Gtk.Window(type=Gtk.WindowType.POPUP)
        vis = self.win.get_screen().get_rgba_visual()
        if vis:
            self.win.set_visual(vis)
        css = Gtk.CssProvider()
        css.load_from_data(CSS)
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(), css, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        outer = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=4)
        self.row.set_margin_start(8)
        self.row.set_margin_end(8)
        self.row.set_margin_top(6)
        self.title = Gtk.Label(label="")
        self.title.get_style_context().add_class("title")
        self.title.set_width_chars(40)
        self.title.set_max_width_chars(48)
        self.title.set_xalign(0)
        self.title.set_ellipsize(3)
        outer.pack_start(self.row, False, False, 0)
        outer.pack_start(self.title, False, False, 0)
        self.win.add(outer)

        self.root.change_attributes(event_mask=X.PropertyChangeMask)
        self.grab_keys()
        GLib.io_add_watch(self.d.fileno(), GLib.IO_IN, self.on_x)
        self.trigger_since = None
        GLib.timeout_add(POLL_MS, self.poll)

    # ---- X helpers -------------------------------------------------------
    def grab_keys(self):
        kc = self.d.keysym_to_keycode(XK.XK_Tab)
        for shift in (0, X.ShiftMask):
            for lock in LOCKS:
                ec = error.CatchError(error.BadAccess)
                self.root.grab_key(kc, MOD | shift | lock, True,
                                   X.GrabModeAsync, X.GrabModeAsync, onerror=ec)
                self.d.sync()
                if ec.get_error():
                    print("dwm-dock: cannot grab Super+Tab (already bound?)")
                    return

    def prop(self, w, name):
        try:
            p = w.get_full_property(self.A[name], X.AnyPropertyType)
        except error.XError:
            return None
        return p.value if p else None

    def title_of(self, w):
        v = self.prop(w, "_NET_WM_NAME")
        if v:
            return bytes(v).decode("utf-8", "replace")
        try:
            return w.get_wm_name() or ""
        except error.XError:
            return ""

    def icon_of(self, w, cls):
        theme = Gtk.IconTheme.get_default()
        names = [n.lower() for n in cls] + list(cls)
        for n in names:
            if n and theme.has_icon(n):
                return theme.load_icon(n, ICON, 0)
        v = self.prop(w, "_NET_WM_ICON")
        if v is not None and len(v):
            best, i = None, 0
            v = list(v)
            while i + 2 <= len(v):
                iw, ih = v[i], v[i + 1]
                if iw * ih == 0 or i + 2 + iw * ih > len(v):
                    break
                if best is None or iw * ih > best[0] * best[1]:
                    best = (iw, ih, v[i + 2:i + 2 + iw * ih])
                i += 2 + iw * ih
            if best:
                iw, ih, px = best
                data = b"".join(struct.pack("BBBB", (p >> 16) & 255, (p >> 8) & 255, p & 255, p >> 24)
                                for p in px)
                pb = GdkPixbuf.Pixbuf.new_from_bytes(
                    GLib.Bytes.new(data), GdkPixbuf.Colorspace.RGB, True, 8, iw, ih, iw * 4)
                return pb.scale_simple(ICON, ICON, GdkPixbuf.InterpType.BILINEAR)
        return theme.load_icon("application-x-executable", ICON, 0)

    def refresh(self):
        lst = self.prop(self.root, "_NET_CLIENT_LIST") or []
        out = []
        for wid in lst:
            w = self.d.create_resource_object("window", wid)
            info = self.prop(w, "_DWM_CLIENT_INFO")
            if info is None or not info[0]:
                continue                      # unmanaged or scratchpad
            try:
                cls = w.get_wm_class() or ("", "")
            except error.XError:
                continue
            if wid not in self.icons:
                try:
                    self.icons[wid] = self.icon_of(w, cls)
                except Exception:
                    self.icons[wid] = None
            tag = (int(info[0]) & -int(info[0])).bit_length()
            out.append((wid, self.title_of(w), cls, tag, self.icons[wid]))
        self.icons = {k: v for k, v in self.icons.items() if k in {c[0] for c in out}}
        self.clients = out

    def active(self):
        v = self.prop(self.root, "_NET_ACTIVE_WINDOW")
        return v[0] if v else None

    def activate(self, wid):
        ev = protocol.event.ClientMessage(
            window=self.d.create_resource_object("window", wid),
            client_type=self.A["_DWM_FOCUS_CLIENT"], data=(32, [0, 0, 0, 0, 0]))
        self.root.send_event(ev, event_mask=X.SubstructureRedirectMask | X.SubstructureNotifyMask)
        self.d.flush()

    # ---- UI --------------------------------------------------------------
    def monitor_at_pointer(self):
        disp = Gdk.Display.get_default()
        _, x, y = disp.get_default_seat().get_pointer().get_position()
        return disp.get_monitor_at_point(x, y), x, y

    def build(self):
        for c in self.row.get_children():
            self.row.remove(c)
        self.items = []
        act = self.active()
        for i, (wid, title, cls, tag, pb) in enumerate(self.clients):
            ev = Gtk.EventBox()
            box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
            box.pack_start(Gtk.Image.new_from_pixbuf(pb) if pb else Gtk.Label(label="?"), False, False, 0)
            lab = Gtk.Label(label=str(tag))
            lab.get_style_context().add_class("tag")
            box.pack_start(lab, False, False, 0)
            ev.add(box)
            ctx = ev.get_style_context()
            ctx.add_class("item")
            if wid == act:
                ctx.add_class("active")
            ev.add_events(Gdk.EventMask.ENTER_NOTIFY_MASK)
            ev.connect("button-press-event", lambda _w, _e, wid=wid: self.click(wid))
            ev.connect("enter-notify-event", lambda _w, _e, i=i: self.hover(i))
            self.row.pack_start(ev, False, False, 0)
            self.items.append(ev)
        self.set_sel(self.sel)

    def set_sel(self, i):
        self.sel = i
        for j, ev in enumerate(self.items):
            ctx = ev.get_style_context()
            (ctx.add_class if j == i else ctx.remove_class)("sel")
        self.title.set_text(self.clients[i][1] if 0 <= i < len(self.clients) else "")

    def show(self):
        mon, _, _ = self.monitor_at_pointer()
        self.build()
        self.row.show_all()
        self.title.show()
        self.win.show_all()
        self.win.resize(1, 1)
        g = mon.get_geometry()
        _, nat = self.win.get_preferred_size()
        self.win.move(g.x, g.y + g.height - nat.height)
        self.win.get_window().raise_()

    def hide(self):
        self.switching = False
        for attr in ("hold_id", "leave_id"):
            if getattr(self, attr):
                GLib.source_remove(getattr(self, attr))
                setattr(self, attr, None)
        self.win.hide()

    def hover(self, i):
        if not self.switching:
            self.set_sel(i)

    def click(self, wid):
        self.activate(wid)
        self.hide()
        return True

    # ---- hover trigger ---------------------------------------------------
    def inside_dock(self, x, y):
        if not self.win.get_visible():
            return False
        wx, wy = self.win.get_position()
        ww, wh = self.win.get_size()
        return wx <= x < wx + ww and wy - 6 <= y < wy + wh + 6

    def poll(self):
        if self.switching:
            return True
        mon, x, y = self.monitor_at_pointer()
        g = mon.get_geometry()
        in_trigger = (y >= g.y + g.height - EDGE_PX and g.x <= x < g.x + g.width // 2)
        if not self.win.get_visible():
            if not in_trigger:
                self.trigger_since = None
            elif self.trigger_since is None:
                self.trigger_since = GLib.get_monotonic_time()
            elif GLib.get_monotonic_time() - self.trigger_since >= SHOW_MS * 1000:
                self.trigger_since = None
                self.refresh()
                if self.clients:
                    self.sel = -1
                    self.show()
        elif self.inside_dock(x, y) or in_trigger:
            if self.leave_id:
                GLib.source_remove(self.leave_id)
                self.leave_id = None
        elif not self.leave_id:
            self.leave_id = GLib.timeout_add(LEAVE_MS, self.leave)
        return True

    def leave(self):
        self.leave_id = None
        if not self.switching:
            self.hide()
        return False

    # ---- Super+Tab -------------------------------------------------------
    def cycle(self, step):
        if not self.switching:
            self.refresh()
            if not self.clients:
                return
            self.switching = True
            act = self.active()
            ids = [c[0] for c in self.clients]
            self.sel = ids.index(act) if act in ids else (0 if step > 0 else 0)
            self.show()
        n = len(self.clients)
        if n == 0:
            return
        self.set_sel((self.sel + step) % n)
        if self.hold_id:
            GLib.source_remove(self.hold_id)
        self.hold_id = GLib.timeout_add(HOLD_MS, self.commit)

    def commit(self):
        self.hold_id = None
        if 0 <= self.sel < len(self.clients):
            self.activate(self.clients[self.sel][0])
        self.hide()
        return False

    # ---- X events --------------------------------------------------------
    def on_x(self, *_):
        while self.d.pending_events():
            ev = self.d.next_event()
            if ev.type == X.KeyPress:
                state = ev.state & ~(X.LockMask | X.Mod2Mask)
                if state & MOD:
                    self.cycle(-1 if state & X.ShiftMask else 1)
            elif ev.type == X.PropertyNotify and self.win.get_visible() and not self.switching:
                if ev.atom in (self.A["_NET_CLIENT_LIST"], self.A["_NET_ACTIVE_WINDOW"]):
                    self.refresh()
                    if self.clients:
                        self.build()
                    else:
                        self.hide()
        return True


if __name__ == "__main__":
    Dock()
    Gtk.main()
