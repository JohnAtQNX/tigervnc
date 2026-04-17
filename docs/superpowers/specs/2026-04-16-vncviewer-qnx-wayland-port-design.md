# vncviewer QNX Wayland Port — Design Spec

**Date:** 2026-04-16  
**Status:** Approved  
**Scope:** Port `vncviewer` to QNX 8.0 (Wayland, no X11). TLS-secured servers out of scope.

---

## Goal

Build a working `vncviewer` binary on QNX 8.0 that runs on the XFCE/Wayland desktop. The viewer must be able to connect to unencrypted VNC servers, display the remote desktop, and handle keyboard/mouse/touch input.

---

## Architecture

The port introduces a new QNX/Wayland platform branch alongside the existing Win32, macOS, and X11 branches. All existing code is left untouched. A new `elseif(QNX)` block in `vncviewer/CMakeLists.txt` selects the Wayland platform files when building on QNX.

The core VNC protocol stack (`common/`, `rfb/`, `network/`, `rdr/`) requires zero changes — it is fully platform-agnostic.

---

## Components

### Phase 1 — Dependencies (built from source)

| Library | Version | Notes |
|---|---|---|
| FLTK | 1.4.x | Wayland backend enabled (`-DOPTION_USE_WAYLAND=ON`), X11 disabled. Provides UI widgets, event loop, and Wayland SHM pixel buffer rendering. |
| xkbcommon | latest stable | Core library only, no X11 extension. Used for keysym/keycode mapping. |
| GnuTLS | — | **Skipped.** TLS-secured VNC servers are out of scope. |

### Phase 2 — Platform layer (4 new files)

| New file | Replaces | Responsibility |
|---|---|---|
| `vncviewer/Surface_Wayland.cxx` | `Surface_X11.cxx` | Blits pixel buffers to screen using FLTK's Wayland backend |
| `vncviewer/KeyboardWayland.cxx` | `KeyboardX11.cxx` | Keyboard handling via xkbcommon; reuses existing `xkb_to_qnum.c` table |
| `vncviewer/wayland.cxx` + `wayland.h` | `x11.cxx` + `x11.h` | Window management helpers: maximize, keyboard grab, monitor geometry |
| Touch | `XInputTouchHandler.cxx` | Delegated to FLTK's built-in Wayland touch handling via `BaseTouchHandler` |

### Phase 3 — Build integration

- `vncviewer/CMakeLists.txt`: add `elseif(QNX)` block selecting Wayland platform files, linking xkbcommon, dropping X11 libs
- Top-level `CMakeLists.txt`: add QNX detection, FLTK/xkbcommon find-module paths

---

## Stubs and Omissions

| Feature | Decision |
|---|---|
| `x11_warp_pointer` | Stubbed as no-op — Wayland forbids pointer warping by design |
| XRandR monitor geometry | Replaced by FLTK's `wl_output`-based monitor info |
| `zwp_keyboard_shortcuts_inhibit` | Implemented for full-screen keyboard grab |
| TLS (GnuTLS) | Out of scope — unencrypted connections only |

---

## Success Criteria

- `vncviewer` builds on QNX with clang++ without X11 headers present
- Viewer connects to an unencrypted VNC server and renders the remote desktop
- Keyboard input and mouse input work correctly
- Application runs stably on the XFCE/Wayland desktop
