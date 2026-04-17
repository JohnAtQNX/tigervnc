# vncviewer QNX Wayland Port — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a working `vncviewer` binary on QNX 8.0 / FLTK-Wayland that connects to unencrypted VNC servers.

**Architecture:** Add a `__QNX__` platform branch alongside Win32/macOS/X11. Five new files implement the Wayland platform layer. Existing files receive minimal `#ifdef __QNX__` guards at call sites that use X11 types. Core VNC code is untouched.

**Tech Stack:** FLTK 1.4 (Wayland backend, built from source), xkbcommon (via apk), clang++, CMake.

---

## File Map

**Create:**
- `vncviewer/Surface_Wayland.cxx` — RGBA pixel buffer; `fl_draw_image` for rendering to window
- `vncviewer/KeyboardWayland.h` — `WaylandKeyEvent` struct + `KeyboardWayland` class declaration
- `vncviewer/KeyboardWayland.cxx` — FLTK FL_KEYBOARD/FL_KEYUP event routing
- `vncviewer/wayland.h` — same function signatures as `x11.h` (x11_* names kept for minimal diffs)
- `vncviewer/wayland.cxx` — Wayland/stub implementations of all `x11_*` functions

**Modify:**
- `vncviewer/Surface.h` — add `#elif defined(__QNX__)` branches (no XRender header/members)
- `vncviewer/PlatformPixelBuffer.h` — guard XShm/XImage members with `!__QNX__`
- `vncviewer/PlatformPixelBuffer.cxx` — add `__QNX__` init path (RGBA buffer, no XShmPutImage)
- `vncviewer/Viewport.cxx` — add QNX keyboard include/instantiation; route FL_KEYBOARD to KeyboardWayland
- `vncviewer/touch.h` — add `!__QNX__` to pointer-grab declarations
- `vncviewer/touch.cxx` — guard XInput2 includes and implementation
- `vncviewer/DesktopWindow.cxx` — guard `x11.h` include; add `__QNX__` branches for keyboard/pointer grab
- `vncviewer/OptionsDialog.cxx` — guard `x11.h` include; add `__QNX__` branch for WM detection
- `vncviewer/CMakeLists.txt` — add `elseif(CMAKE_SYSTEM_NAME STREQUAL "QNX")` source/link block

---

## Task 1: Install xkbcommon; build FLTK 1.4 with Wayland backend

**Files:** none (dependency setup)

- [ ] **Step 1: Install xkbcommon development package**

```bash
sudo apk add libxkbcommon-dev
```

Expected: `OK: N packages, M dirs, K MiB`

- [ ] **Step 2: Clone FLTK 1.4**

```bash
git clone https://github.com/fltk/fltk.git -b branch-1.4 --depth 1 /tmp/fltk-src
```

Expected: `Cloning into '/tmp/fltk-src'...` then `done.`

- [ ] **Step 3: Configure FLTK with Wayland backend, X11 off**

```bash
mkdir /tmp/fltk-build
cmake -S /tmp/fltk-src -B /tmp/fltk-build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DOPTION_USE_WAYLAND=ON \
  -DOPTION_USE_X11=OFF \
  -DOPTION_USE_GL=OFF \
  -DFLTK_BUILD_FLUID=OFF \
  -DFLTK_BUILD_FLTK_OPTIONS=OFF \
  -DFLTK_BUILD_TEST=OFF
```

Expected: `-- Build files have been written to: /tmp/fltk-build`

Note: if cmake reports missing `wayland-protocols`, install via `sudo apk add wayland-protocols-dev`.
If it reports missing `libdecor`, install `sudo apk add libdecor-dev` or add `-DOPTION_USE_SYSTEM_LIBDECOR=OFF`.

- [ ] **Step 4: Build and install FLTK**

```bash
cmake --build /tmp/fltk-build -j$(nproc)
sudo cmake --install /tmp/fltk-build
```

Expected: ends with `-- Installing: /usr/local/lib/libfltk.a` (and similar).

- [ ] **Step 5: Verify FLTK is findable**

```bash
/usr/local/bin/fltk-config --version
```

Expected: `1.4.x`

- [ ] **Step 6: Commit checkpoint**

```bash
cd /data/home/qnxuser/dev/tigervnc
git commit --allow-empty -m "chore: FLTK 1.4 Wayland + xkbcommon installed at /usr/local"
```

---

## Task 2: Update Surface.h for QNX

**Files:**
- Modify: `vncviewer/Surface.h`

- [ ] **Step 1: Replace the platform header include block**

In `vncviewer/Surface.h`, find:

```cpp
#if defined(WIN32)
#include <windows.h>
#elif defined(__APPLE__)
// Apple headers conflict with FLTK, so redefine types here
typedef struct CGImage* CGImageRef;
#else
#include <X11/extensions/Xrender.h>
#endif
```

Replace with:

```cpp
#if defined(WIN32)
#include <windows.h>
#elif defined(__APPLE__)
// Apple headers conflict with FLTK, so redefine types here
typedef struct CGImage* CGImageRef;
#elif defined(__QNX__)
// No platform headers needed for Wayland backend
#else
#include <X11/extensions/Xrender.h>
#endif
```

- [ ] **Step 2: Replace the platform member variable block**

Find:

```cpp
#if defined(WIN32)
  RGBQUAD* data;
  HBITMAP bitmap;
#elif defined(__APPLE__)
  unsigned char* data;
#else
  Pixmap pixmap;
  Picture picture;
  XRenderPictFormat* visFormat;
#endif
```

Replace with:

```cpp
#if defined(WIN32)
  RGBQUAD* data;
  HBITMAP bitmap;
#elif defined(__APPLE__) || defined(__QNX__)
  unsigned char* data;
#else
  Pixmap pixmap;
  Picture picture;
  XRenderPictFormat* visFormat;
#endif
```

- [ ] **Step 3: Verify the file compiles (syntax check only)**

```bash
cd /data/home/qnxuser/dev/tigervnc
clang++ -std=gnu++11 -fsyntax-only -D__QNX__ -I. -Icommon \
  -I/usr/local/include vncviewer/Surface.h 2>&1 | head -20
```

Expected: no output (no errors).

- [ ] **Step 4: Commit**

```bash
git add vncviewer/Surface.h
git commit -m "port: add __QNX__ branch to Surface.h (no XRender)"
```

---

## Task 3: Implement Surface_Wayland.cxx

**Files:**
- Create: `vncviewer/Surface_Wayland.cxx`

The pixel format used by `PlatformPixelBuffer` on QNX will be RGBA (R=byte0, G=byte1, B=byte2, A=byte3). `fl_draw_image` with D=3, L=width*4 reads bytes 0,1,2 as R,G,B and skips byte 3.

- [ ] **Step 1: Create the file**

```cpp
// vncviewer/Surface_Wayland.cxx
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdexcept>

#include <FL/Fl_RGB_Image.H>
#include <FL/fl_draw.H>

#include "Surface.h"

void Surface::clear(unsigned char r, unsigned char g, unsigned char b,
                    unsigned char a)
{
  unsigned char* p = data;
  unsigned char pm_r = (unsigned)r * a / 255;
  unsigned char pm_g = (unsigned)g * a / 255;
  unsigned char pm_b = (unsigned)b * a / 255;

  for (int i = 0; i < w * h; i++) {
    *p++ = pm_r;
    *p++ = pm_g;
    *p++ = pm_b;
    *p++ = a;
  }
}

void Surface::draw(int src_x, int src_y, int dst_x, int dst_y,
                   int dst_w, int dst_h)
{
  // D=3: reads R,G,B from bytes 0,1,2; byte 3 (A) skipped.
  // L=w*4: stride in bytes.
  fl_draw_image(data + (src_y * w + src_x) * 4,
                dst_x, dst_y, dst_w, dst_h, 3, w * 4);
}

void Surface::draw(Surface* dst, int src_x, int src_y,
                   int dst_x, int dst_y, int dst_w, int dst_h)
{
  for (int y = 0; y < dst_h; y++) {
    memcpy(dst->data + ((dst_y + y) * dst->w + dst_x) * 4,
           data     + ((src_y + y) *      w + src_x) * 4,
           dst_w * 4);
  }
}

static void blend_row(const unsigned char* src, unsigned char* dst,
                      int w, int a)
{
  for (int x = 0; x < w; x++) {
    unsigned char sa = (unsigned)src[3] * a / 255;
    dst[0] = (unsigned)src[0] * sa / 255 + (unsigned)dst[0] * (255 - sa) / 255;
    dst[1] = (unsigned)src[1] * sa / 255 + (unsigned)dst[1] * (255 - sa) / 255;
    dst[2] = (unsigned)src[2] * sa / 255 + (unsigned)dst[2] * (255 - sa) / 255;
    dst[3] = sa + (unsigned)dst[3] * (255 - sa) / 255;
    src += 4;
    dst += 4;
  }
}

void Surface::blend(int src_x, int src_y, int dst_x, int dst_y,
                    int dst_w, int dst_h, int a)
{
  // Blend to window: software-blend into a temporary RGB buffer then draw.
  unsigned char* tmp = new unsigned char[dst_w * dst_h * 3];
  unsigned char* out = tmp;

  for (int y = 0; y < dst_h; y++) {
    const unsigned char* row = data + ((src_y + y) * w + src_x) * 4;
    for (int x = 0; x < dst_w; x++) {
      unsigned char sa = (unsigned)row[3] * a / 255;
      *out++ = (unsigned)row[0] * sa / 255;
      *out++ = (unsigned)row[1] * sa / 255;
      *out++ = (unsigned)row[2] * sa / 255;
      row += 4;
    }
  }

  fl_draw_image(tmp, dst_x, dst_y, dst_w, dst_h, 3);
  delete[] tmp;
}

void Surface::blend(Surface* dst, int src_x, int src_y,
                    int dst_x, int dst_y, int dst_w, int dst_h, int a)
{
  for (int y = 0; y < dst_h; y++) {
    blend_row(data    + ((src_y + y) *      w + src_x) * 4,
              dst->data + ((dst_y + y) * dst->w + dst_x) * 4,
              dst_w, a);
  }
}

void Surface::alloc()
{
  data = new unsigned char[w * h * 4]();
}

void Surface::dealloc()
{
  delete[] data;
  data = nullptr;
}

void Surface::update(const Fl_RGB_Image* image)
{
  const unsigned char* in;
  unsigned char* out;

  assert(image->w() == w);
  assert(image->h() == h);

  in  = (const unsigned char*)image->data()[0];
  out = data;

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      switch (image->d()) {
      case 1:
        *out++ = in[0]; *out++ = in[0]; *out++ = in[0]; *out++ = 0xff;
        break;
      case 2:
        *out++ = (unsigned)in[0] * in[1] / 255;
        *out++ = (unsigned)in[0] * in[1] / 255;
        *out++ = (unsigned)in[0] * in[1] / 255;
        *out++ = in[1];
        break;
      case 3:
        *out++ = in[0]; *out++ = in[1]; *out++ = in[2]; *out++ = 0xff;
        break;
      case 4:
        *out++ = (unsigned)in[0] * in[3] / 255;
        *out++ = (unsigned)in[1] * in[3] / 255;
        *out++ = (unsigned)in[2] * in[3] / 255;
        *out++ = in[3];
        break;
      }
      in += image->d();
    }
    if (image->ld() != 0)
      in += image->ld() - image->w() * image->d();
  }
}
```

- [ ] **Step 2: Syntax-check the new file**

```bash
cd /data/home/qnxuser/dev/tigervnc
clang++ -std=gnu++11 -fsyntax-only -D__QNX__ -DHAVE_CONFIG_H \
  -I. -Icommon -I/usr/local/include \
  vncviewer/Surface_Wayland.cxx 2>&1 | head -20
```

Expected: no errors.

- [ ] **Step 3: Commit**

```bash
git add vncviewer/Surface_Wayland.cxx
git commit -m "port: add Surface_Wayland.cxx (RGBA buffer, fl_draw_image)"
```

---

## Task 4: Update PlatformPixelBuffer for QNX

**Files:**
- Modify: `vncviewer/PlatformPixelBuffer.h`
- Modify: `vncviewer/PlatformPixelBuffer.cxx`

- [ ] **Step 1: Guard XShm/XImage includes and members in PlatformPixelBuffer.h**

Find:

```cpp
#if !defined(WIN32) && !defined(__APPLE__)
#include <X11/Xlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif
```

Replace with:

```cpp
#if !defined(WIN32) && !defined(__APPLE__) && !defined(__QNX__)
#include <X11/Xlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif
```

Find:

```cpp
#if !defined(WIN32) && !defined(__APPLE__)
protected:
  bool setupShm(int width, int height);

protected:
  XShmSegmentInfo *shminfo;
  XImage *xim;
#endif
```

Replace with:

```cpp
#if !defined(WIN32) && !defined(__APPLE__) && !defined(__QNX__)
protected:
  bool setupShm(int width, int height);

protected:
  XShmSegmentInfo *shminfo;
  XImage *xim;
#endif
```

- [ ] **Step 2: Add QNX branch to PlatformPixelBuffer.cxx constructor**

Find the entire constructor body:

```cpp
PlatformPixelBuffer::PlatformPixelBuffer(int width, int height) :
  FullFramePixelBuffer(rfb::PixelFormat(32, 24, false, true,
                                        255, 255, 255, 16, 8, 0),
                       0, 0, nullptr, 0),
  Surface(width, height)
#if !defined(WIN32) && !defined(__APPLE__)
  , shminfo(nullptr), xim(nullptr)
#endif
{
#if !defined(WIN32) && !defined(__APPLE__)
  if (!setupShm(width, height)) {
    xim = XCreateImage(fl_display, (Visual*)CopyFromParent, 32,
                       ZPixmap, 0, nullptr, width, height, 32, 0);
    if (!xim)
      throw std::runtime_error("XCreateImage");

    xim->data = (char*)malloc(xim->bytes_per_line * xim->height);
    if (!xim->data)
      throw std::bad_alloc();

    vlog.debug("Using standard XImage");
  }

  setBuffer(width, height, (uint8_t*)xim->data,
            xim->bytes_per_line / (getPF().bpp/8));

  // On X11, the Pixmap backing this Surface is uninitialized.
  clear(0, 0, 0);
#else
  setBuffer(width, height, (uint8_t*)Surface::data, width);
#endif
}
```

Replace with:

```cpp
PlatformPixelBuffer::PlatformPixelBuffer(int width, int height) :
#if defined(__QNX__)
  FullFramePixelBuffer(rfb::PixelFormat(32, 24, false, true,
                                        255, 255, 255, 0, 8, 16),
                       0, 0, nullptr, 0),
#else
  FullFramePixelBuffer(rfb::PixelFormat(32, 24, false, true,
                                        255, 255, 255, 16, 8, 0),
                       0, 0, nullptr, 0),
#endif
  Surface(width, height)
#if !defined(WIN32) && !defined(__APPLE__) && !defined(__QNX__)
  , shminfo(nullptr), xim(nullptr)
#endif
{
#if defined(__QNX__)
  // Use RGBA pixel buffer; Surface::data is allocated by Surface(width,height)
  setBuffer(width, height, (uint8_t*)Surface::data, width);
#elif !defined(WIN32) && !defined(__APPLE__)
  if (!setupShm(width, height)) {
    xim = XCreateImage(fl_display, (Visual*)CopyFromParent, 32,
                       ZPixmap, 0, nullptr, width, height, 32, 0);
    if (!xim)
      throw std::runtime_error("XCreateImage");

    xim->data = (char*)malloc(xim->bytes_per_line * xim->height);
    if (!xim->data)
      throw std::bad_alloc();

    vlog.debug("Using standard XImage");
  }

  setBuffer(width, height, (uint8_t*)xim->data,
            xim->bytes_per_line / (getPF().bpp/8));

  // On X11, the Pixmap backing this Surface is uninitialized.
  clear(0, 0, 0);
#else
  setBuffer(width, height, (uint8_t*)Surface::data, width);
#endif
}
```

- [ ] **Step 3: Add QNX branch to getDamage()**

Find this block inside `getDamage()`:

```cpp
#if !defined(WIN32) && !defined(__APPLE__)
  if (r.width() == 0 || r.height() == 0)
    return r;

  GC gc;

  gc = XCreateGC(fl_display, pixmap, 0, nullptr);
  if (shminfo) {
    XShmPutImage(fl_display, pixmap, gc, xim,
                 r.tl.x, r.tl.y, r.tl.x, r.tl.y,
                 r.width(), r.height(), False);
    // Need to make sure the X server has finished reading the
    // shared memory before we return
    XSync(fl_display, False);
  } else {
    XPutImage(fl_display, pixmap, gc, xim,
              r.tl.x, r.tl.y, r.tl.x, r.tl.y, r.width(), r.height());
  }
  XFreeGC(fl_display, gc);
#endif
```

Replace with:

```cpp
#if !defined(WIN32) && !defined(__APPLE__) && !defined(__QNX__)
  if (r.width() == 0 || r.height() == 0)
    return r;

  GC gc;

  gc = XCreateGC(fl_display, pixmap, 0, nullptr);
  if (shminfo) {
    XShmPutImage(fl_display, pixmap, gc, xim,
                 r.tl.x, r.tl.y, r.tl.x, r.tl.y,
                 r.width(), r.height(), False);
    // Need to make sure the X server has finished reading the
    // shared memory before we return
    XSync(fl_display, False);
  } else {
    XPutImage(fl_display, pixmap, gc, xim,
              r.tl.x, r.tl.y, r.tl.x, r.tl.y, r.width(), r.height());
  }
  XFreeGC(fl_display, gc);
#endif
```

- [ ] **Step 4: Guard the XShm helper and includes**

At the top of `PlatformPixelBuffer.cxx`, find:

```cpp
#if !defined(WIN32) && !defined(__APPLE__)
#include <sys/ipc.h>
#include <sys/shm.h>
#endif
```

Replace with:

```cpp
#if !defined(WIN32) && !defined(__APPLE__) && !defined(__QNX__)
#include <sys/ipc.h>
#include <sys/shm.h>
#endif
```

And wrap the `setupShm` implementation at the bottom:

Find:

```cpp
#if !defined(WIN32) && !defined(__APPLE__)

static bool caughtError;
```

Replace with:

```cpp
#if !defined(WIN32) && !defined(__APPLE__) && !defined(__QNX__)

static bool caughtError;
```

Also guard `<FL/x.H>` include in PlatformPixelBuffer.cxx:

Find:

```cpp
#include <FL/Fl.H>
#include <FL/x.H>
```

Replace with:

```cpp
#include <FL/Fl.H>
#if !defined(__QNX__)
#include <FL/x.H>
#endif
```

- [ ] **Step 5: Syntax-check both files**

```bash
cd /data/home/qnxuser/dev/tigervnc
clang++ -std=gnu++11 -fsyntax-only -D__QNX__ -DHAVE_CONFIG_H \
  -I. -Icommon -I/usr/local/include \
  vncviewer/PlatformPixelBuffer.cxx 2>&1 | head -30
```

Expected: no errors (may warn about missing rfb headers — that is OK for a syntax check without full includes).

- [ ] **Step 6: Commit**

```bash
git add vncviewer/PlatformPixelBuffer.h vncviewer/PlatformPixelBuffer.cxx
git commit -m "port: guard PlatformPixelBuffer XShm/XImage code for __QNX__"
```

---

## Task 5: Implement KeyboardWayland

**Files:**
- Create: `vncviewer/KeyboardWayland.h`
- Create: `vncviewer/KeyboardWayland.cxx`

On QNX with FLTK Wayland, keyboard events reach `Viewport::handle()` as `FL_KEYBOARD` / `FL_KEYUP`. FLTK's `Fl::event_key()` returns the XKB keysym (same values as X11). The system key code (used to match press/release) is the keysym itself.

- [ ] **Step 1: Create KeyboardWayland.h**

```cpp
// vncviewer/KeyboardWayland.h
#ifndef __KEYBOARDWAYLAND_H__
#define __KEYBOARDWAYLAND_H__

#include "Keyboard.h"

// Event struct passed from Viewport::handle() on QNX.
struct WaylandKeyEvent {
  int type;        // FL_KEYBOARD or FL_KEYUP
  unsigned keysym; // Fl::event_key() value
};

class KeyboardWayland : public Keyboard
{
public:
  KeyboardWayland(KeyboardHandler* handler);
  virtual ~KeyboardWayland();

  bool handleEvent(const void* event) override;
  std::list<uint32_t> translateToKeySyms(int systemKeyCode) override;

  unsigned getLEDState() override;
  void setLEDState(unsigned state) override;
};

#endif
```

- [ ] **Step 2: Create KeyboardWayland.cxx**

```cpp
// vncviewer/KeyboardWayland.cxx
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>

#include <FL/Fl.H>

#include <rfb/ledStates.h>

#include "KeyboardWayland.h"

KeyboardWayland::KeyboardWayland(KeyboardHandler* handler_)
  : Keyboard(handler_)
{
}

KeyboardWayland::~KeyboardWayland()
{
}

bool KeyboardWayland::handleEvent(const void* event)
{
  const WaylandKeyEvent* wev = (const WaylandKeyEvent*)event;

  assert(event);

  if (wev->type == FL_KEYBOARD) {
    // Use keysym as both systemKeyCode and keySym.
    // qnum (hardware keycode) is 0: server falls back to keysym matching.
    handler->handleKeyPress((int)wev->keysym, 0, wev->keysym);
    return true;
  } else if (wev->type == FL_KEYUP) {
    handler->handleKeyRelease((int)wev->keysym);
    return true;
  }

  return false;
}

std::list<uint32_t> KeyboardWayland::translateToKeySyms(int systemKeyCode)
{
  // systemKeyCode IS the keysym on this platform.
  return { (uint32_t)systemKeyCode };
}

unsigned KeyboardWayland::getLEDState()
{
  // LED state detection requires xkbcommon + Wayland keyboard protocol.
  // Not implemented in this initial port; server uses its own LED state.
  return rfb::ledUnknown;
}

void KeyboardWayland::setLEDState(unsigned /*state*/)
{
  // Setting LED state on Wayland requires compositor support.
  // Not implemented in this initial port.
}
```

- [ ] **Step 3: Syntax-check both files**

```bash
cd /data/home/qnxuser/dev/tigervnc
clang++ -std=gnu++11 -fsyntax-only -D__QNX__ -DHAVE_CONFIG_H \
  -I. -Icommon -I/usr/local/include \
  vncviewer/KeyboardWayland.cxx 2>&1 | head -20
```

Expected: no errors.

- [ ] **Step 4: Commit**

```bash
git add vncviewer/KeyboardWayland.h vncviewer/KeyboardWayland.cxx
git commit -m "port: add KeyboardWayland (FLTK FL_KEYBOARD event routing)"
```

---

## Task 6: Implement wayland.h and wayland.cxx

**Files:**
- Create: `vncviewer/wayland.h`
- Create: `vncviewer/wayland.cxx`

These provide the same `x11_*` function names as `x11.h`/`x11.cxx` so that `DesktopWindow.cxx` and `OptionsDialog.cxx` need only an include-guard change.

- [ ] **Step 1: Create wayland.h**

```cpp
// vncviewer/wayland.h
#ifndef __WAYLAND_H__
#define __WAYLAND_H__

class Fl_Window;

// Returns false: Wayland compositors are always present when FLTK runs.
bool x11_has_wm();

// Returns false: Wayland does not expose EWMH-style atoms.
bool x11_wm_supports(const char* atom);

void x11_win_maximize(Fl_Window* win);
bool x11_win_is_maximized(Fl_Window* win);

// Reads position from FLTK's window object directly.
void x11_win_get_coords(Fl_Window* win, int* x, int* y, int* w, int* h);

// No-op: XWayland MAY_GRAB not applicable on native Wayland.
void x11_win_may_grab(Fl_Window* win);

// Keyboard grab via FLTK's Fl::grab() mechanism.
bool x11_grab_keyboard(Fl_Window* win);
void x11_ungrab_keyboard();

// No-op: pointer warping is not supported on Wayland.
void x11_warp_pointer(unsigned x, unsigned y);

// Always returns true (single-screen assumption on Wayland).
bool x11_is_pointer_on_same_screen(Fl_Window* win);

#endif
```

- [ ] **Step 2: Create wayland.cxx**

```cpp
// vncviewer/wayland.cxx
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <FL/Fl.H>
#include <FL/Fl_Window.H>

#include "wayland.h"

bool x11_has_wm()
{
  // Wayland compositor is always the WM.
  return true;
}

bool x11_wm_supports(const char* /*atom*/)
{
  // EWMH atoms are an X11 concept; report unsupported.
  return false;
}

void x11_win_maximize(Fl_Window* win)
{
  if (win)
    win->maximize();
}

bool x11_win_is_maximized(Fl_Window* win)
{
  if (!win)
    return false;
  return win->maximize_active();
}

void x11_win_get_coords(Fl_Window* win, int* x, int* y, int* w, int* h)
{
  if (!win || !x || !y || !w || !h)
    return;
  *x = win->x_root();
  *y = win->y_root();
  *w = win->x_root() + win->w();
  *h = win->y_root() + win->h();
}

void x11_win_may_grab(Fl_Window* /*win*/)
{
  // No-op: XWayland MAY_GRAB message is X11-specific.
}

bool x11_grab_keyboard(Fl_Window* win)
{
  // Use FLTK's compositor-safe grab.
  Fl::grab(win);
  return true;
}

void x11_ungrab_keyboard()
{
  Fl::grab(nullptr);
}

void x11_warp_pointer(unsigned /*x*/, unsigned /*y*/)
{
  // Pointer warping is intentionally unsupported on Wayland.
}

bool x11_is_pointer_on_same_screen(Fl_Window* /*win*/)
{
  // Wayland has a single logical screen; always true.
  return true;
}
```

- [ ] **Step 3: Syntax-check**

```bash
cd /data/home/qnxuser/dev/tigervnc
clang++ -std=gnu++11 -fsyntax-only -D__QNX__ -DHAVE_CONFIG_H \
  -I. -I/usr/local/include \
  vncviewer/wayland.cxx 2>&1 | head -20
```

Expected: no errors.

Note: `win->maximize()` and `win->maximize_active()` are FLTK 1.4 API. If they produce "no member" errors at full build time, replace `x11_win_maximize` and `x11_win_is_maximized` with no-op stubs.

- [ ] **Step 4: Commit**

```bash
git add vncviewer/wayland.h vncviewer/wayland.cxx
git commit -m "port: add wayland.h/wayland.cxx (x11_* stubs for Wayland)"
```

---

## Task 7: Update touch.h and touch.cxx for QNX

**Files:**
- Modify: `vncviewer/touch.h`
- Modify: `vncviewer/touch.cxx`

- [ ] **Step 1: Add __QNX__ guard in touch.h**

Find:

```cpp
#if !defined(WIN32) && !defined(__APPLE__)
bool x11_grab_pointer(Window window);
void x11_ungrab_pointer(Window window);
#endif
```

Replace with:

```cpp
#if !defined(WIN32) && !defined(__APPLE__) && !defined(__QNX__)
bool x11_grab_pointer(Window window);
void x11_ungrab_pointer(Window window);
#endif
```

- [ ] **Step 2: Guard XInput2 includes in touch.cxx**

Find:

```cpp
#if defined(WIN32)
#include <windows.h>
#include <commctrl.h>
#elif !defined(__APPLE__)
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XI2.h>
#endif
```

Replace with:

```cpp
#if defined(WIN32)
#include <windows.h>
#include <commctrl.h>
#elif !defined(__APPLE__) && !defined(__QNX__)
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XI2.h>
#endif
```

- [ ] **Step 3: Guard `<FL/x.H>` include in touch.cxx**

Find:

```cpp
#include <FL/Fl.H>
#include <FL/x.H>
```

Replace with:

```cpp
#include <FL/Fl.H>
#if !defined(__QNX__)
#include <FL/x.H>
#endif
```

- [ ] **Step 4: Guard the xi_major variable and the XInput handler map**

Find:

```cpp
#if !defined(WIN32) && !defined(__APPLE__)
static int xi_major;
#endif

typedef std::map<Window, class BaseTouchHandler*> HandlerMap;
static HandlerMap handlers;
```

Replace with:

```cpp
#if !defined(WIN32) && !defined(__APPLE__) && !defined(__QNX__)
static int xi_major;
typedef std::map<Window, class BaseTouchHandler*> HandlerMap;
static HandlerMap handlers;
#endif
```

- [ ] **Step 5: Guard enable_touch() and disable_touch() X11 bodies**

The `enable_touch()` and `disable_touch()` functions contain X11/XInput2 code under `!WIN32 && !APPLE`. Add `!__QNX__` guards throughout those functions.

Find in `enable_touch()`:

```cpp
#if !defined(WIN32) && !defined(__APPLE__)
```

And in `disable_touch()`:

```cpp
#if !defined(WIN32) && !defined(__APPLE__)
```

In both, change to:

```cpp
#if !defined(WIN32) && !defined(__APPLE__) && !defined(__QNX__)
```

Also change the corresponding `#endif` comments if any to keep them clear.

- [ ] **Step 6: Guard x11_grab_pointer and x11_ungrab_pointer implementations**

Find in touch.cxx:

```cpp
bool x11_grab_pointer(Window window)
```

This function and `x11_ungrab_pointer` must be wrapped:

Find the start of `x11_grab_pointer`:

```cpp
bool x11_grab_pointer(Window window)
{
```

Add before it:

```cpp
#if !defined(WIN32) && !defined(__APPLE__) && !defined(__QNX__)
```

Find the closing brace of `x11_ungrab_pointer`:

```cpp
void x11_ungrab_pointer(Window window)
{
  ...
  dynamic_cast<XInputTouchHandler*>(handlers[window])->ungrabPointer();
}
```

Add after that closing brace:

```cpp
#endif // !WIN32 && !APPLE && !QNX
```

- [ ] **Step 7: Commit**

```bash
git add vncviewer/touch.h vncviewer/touch.cxx
git commit -m "port: guard XInput2 and pointer grab code for __QNX__"
```

---

## Task 8: Update Viewport.cxx for QNX keyboard

**Files:**
- Modify: `vncviewer/Viewport.cxx`

- [ ] **Step 1: Add QNX keyboard include**

Find:

```cpp
#include "KeyboardX11.h"
```

(This is in the `#else` branch after `#ifdef WIN32` / `#elif defined(__APPLE__)`)

The full block looks like:

```cpp
#ifdef WIN32
#include "KeyboardWin32.h"
#elif defined(__APPLE__)
#include "KeyboardMacOS.h"
#else
#include "KeyboardX11.h"
#endif
```

Replace with:

```cpp
#ifdef WIN32
#include "KeyboardWin32.h"
#elif defined(__APPLE__)
#include "KeyboardMacOS.h"
#elif defined(__QNX__)
#include "KeyboardWayland.h"
#else
#include "KeyboardX11.h"
#endif
```

- [ ] **Step 2: Add QNX keyboard instantiation**

Find:

```cpp
  keyboard = new KeyboardX11(this);
```

(In the constructor, in the `#else` branch)

The full block looks like:

```cpp
#if defined(WIN32)
  keyboard = new KeyboardWin32(this);
#elif defined(__APPLE__)
  keyboard = new KeyboardMacOS(this);
#else
  keyboard = new KeyboardX11(this);
#endif
```

Replace with:

```cpp
#if defined(WIN32)
  keyboard = new KeyboardWin32(this);
#elif defined(__APPLE__)
  keyboard = new KeyboardMacOS(this);
#elif defined(__QNX__)
  keyboard = new KeyboardWayland(this);
#else
  keyboard = new KeyboardX11(this);
#endif
```

- [ ] **Step 3: Route FL_KEYBOARD/FL_KEYUP to KeyboardWayland in handle()**

Find in `Viewport::handle()`:

```cpp
  case FL_KEYDOWN:
  case FL_KEYUP:
    // Just ignore these as keys were handled in the event handler
    return 1;
```

Replace with:

```cpp
  case FL_KEYDOWN:
#ifdef __QNX__
    if (hasFocus()) {
      WaylandKeyEvent ev = { FL_KEYBOARD, (unsigned)Fl::event_key() };
      keyboard->handleEvent(&ev);
    }
#endif
    return 1;

  case FL_KEYUP:
#ifdef __QNX__
    if (hasFocus()) {
      WaylandKeyEvent ev = { FL_KEYUP, (unsigned)Fl::event_key() };
      keyboard->handleEvent(&ev);
    }
#endif
    return 1;
```

- [ ] **Step 4: Guard <FL/x.H> include in Viewport.cxx**

Find:

```cpp
#include <FL/x.H>
```

Replace with:

```cpp
#if !defined(__QNX__)
#include <FL/x.H>
#endif
```

- [ ] **Step 5: Syntax-check Viewport.cxx**

```bash
cd /data/home/qnxuser/dev/tigervnc
clang++ -std=gnu++11 -fsyntax-only -D__QNX__ -DHAVE_CONFIG_H \
  -I. -Icommon -I/usr/local/include \
  vncviewer/Viewport.cxx 2>&1 | grep -v "^In file\|note:" | head -30
```

Expected: errors only about missing rfb/ headers (not about undefined x11 types or XEvent). If `WaylandKeyEvent` appears undefined, verify `KeyboardWayland.h` is included.

- [ ] **Step 6: Commit**

```bash
git add vncviewer/Viewport.cxx
git commit -m "port: route keyboard events to KeyboardWayland on __QNX__"
```

---

## Task 9: Update DesktopWindow.cxx and OptionsDialog.cxx

**Files:**
- Modify: `vncviewer/DesktopWindow.cxx`
- Modify: `vncviewer/OptionsDialog.cxx`

- [ ] **Step 1: Replace x11.h include in DesktopWindow.cxx**

Find (near line 62):

```cpp
#include "x11.h"
```

(It is in the `#else` branch after WIN32/APPLE checks — the structure is:)

```cpp
#if defined(WIN32)
#include "win32.h"
#elif defined(__APPLE__)
#include "cocoa.h"
#include <Carbon/Carbon.h>
#else
#include "x11.h"
#endif
```

Replace with:

```cpp
#if defined(WIN32)
#include "win32.h"
#elif defined(__APPLE__)
#include "cocoa.h"
#include <Carbon/Carbon.h>
#elif defined(__QNX__)
#include "wayland.h"
#else
#include "x11.h"
#endif
```

- [ ] **Step 2: Guard <FL/x.H> in DesktopWindow.cxx**

Find (around line 54):

```cpp
#include <FL/x.H>
```

Replace with:

```cpp
#if !defined(__QNX__)
#include <FL/x.H>
#endif
```

- [ ] **Step 3: Guard grabKeyboard() X11 branch in DesktopWindow.cxx**

Find inside `grabKeyboard()`:

```cpp
#else
  bool ret;

  ret = x11_grab_keyboard(this);
  if (!ret) {
    vlog.error(_("Failure grabbing control of the keyboard"));
    addOverlayError(_("Failure grabbing control of the keyboard"));
    return;
  }
#endif
```

Replace with:

```cpp
#elif defined(__QNX__)
  x11_grab_keyboard(this);  // wayland.cxx: uses Fl::grab(), always succeeds
#else
  bool ret;

  ret = x11_grab_keyboard(this);
  if (!ret) {
    vlog.error(_("Failure grabbing control of the keyboard"));
    addOverlayError(_("Failure grabbing control of the keyboard"));
    return;
  }
#endif
```

- [ ] **Step 4: Guard ungrabKeyboard() X11 branch in DesktopWindow.cxx**

Find inside `ungrabKeyboard()`:

```cpp
#else
  // FLTK has a grab so lets not mess with it
  if (Fl::grab())
    return;

  x11_ungrab_keyboard();
#endif
```

Replace with:

```cpp
#elif defined(__QNX__)
  x11_ungrab_keyboard();  // wayland.cxx: calls Fl::grab(nullptr)
#else
  // FLTK has a grab so lets not mess with it
  if (Fl::grab())
    return;

  x11_ungrab_keyboard();
#endif
```

- [ ] **Step 5: Guard grabPointer() and ungrabPointer() X11 calls**

Find in `grabPointer()`:

```cpp
#if !defined(WIN32) && !defined(__APPLE__)
  // We also need to grab the pointer as some WMs like to grab buttons
  // combined with modifies (e.g. Alt+Button0 in metacity).

  // Having a button pressed prevents us from grabbing, we make
  // a new attempt in fltkHandle()
  if (!x11_grab_pointer(fl_xid(this)))
    return;
#endif
```

Replace with:

```cpp
#if !defined(WIN32) && !defined(__APPLE__) && !defined(__QNX__)
  // We also need to grab the pointer as some WMs like to grab buttons
  // combined with modifies (e.g. Alt+Button0 in metacity).

  // Having a button pressed prevents us from grabbing, we make
  // a new attempt in fltkHandle()
  if (!x11_grab_pointer(fl_xid(this)))
    return;
#endif
```

Find in `ungrabPointer()`:

```cpp
#if !defined(WIN32) && !defined(__APPLE__)
  x11_ungrab_pointer(fl_xid(this));
#endif
```

Replace with:

```cpp
#if !defined(WIN32) && !defined(__APPLE__) && !defined(__QNX__)
  x11_ungrab_pointer(fl_xid(this));
#endif
```

- [ ] **Step 6: Replace x11.h include in OptionsDialog.cxx**

Find (around line 54):

```cpp
#if !defined(WIN32) && !defined(__APPLE__)
#include "x11.h"
#endif
```

Replace with:

```cpp
#if defined(__QNX__)
#include "wayland.h"
#elif !defined(WIN32) && !defined(__APPLE__)
#include "x11.h"
#endif
```

- [ ] **Step 7: Guard OptionsDialog WM detection for QNX**

Find in OptionsDialog.cxx (around line 1182):

```cpp
#else
    // FLTK will emulate multihead support without a WM
    if (!x11_has_wm())
      supportsMultihead = true;
    else
      supportsMultihead =
        x11_wm_supports("_NET_WM_FULLSCREEN_MONITORS");
#endif
```

Replace with:

```cpp
#elif defined(__QNX__)
    // Wayland: compositor handles fullscreen; assume single monitor for safety
    supportsMultihead = false;
#else
    // FLTK will emulate multihead support without a WM
    if (!x11_has_wm())
      supportsMultihead = true;
    else
      supportsMultihead =
        x11_wm_supports("_NET_WM_FULLSCREEN_MONITORS");
#endif
```

- [ ] **Step 8: Commit**

```bash
git add vncviewer/DesktopWindow.cxx vncviewer/OptionsDialog.cxx
git commit -m "port: add __QNX__ guards in DesktopWindow and OptionsDialog"
```

---

## Task 10: Update vncviewer/CMakeLists.txt

**Files:**
- Modify: `vncviewer/CMakeLists.txt`

- [ ] **Step 1: Add QNX source/link block**

Find in `vncviewer/CMakeLists.txt`:

```cmake
else()
  target_sources(vncviewer PRIVATE x11.cxx)
  target_sources(vncviewer PRIVATE GestureHandler.cxx XInputTouchHandler.cxx)
  target_sources(vncviewer PRIVATE KeyboardX11.cxx xkb_to_qnum.c)
  target_sources(vncviewer PRIVATE Surface_X11.cxx)
endif()
```

Replace with:

```cmake
elseif(CMAKE_SYSTEM_NAME STREQUAL "QNX")
  target_sources(vncviewer PRIVATE wayland.cxx)
  target_sources(vncviewer PRIVATE KeyboardWayland.cxx)
  target_sources(vncviewer PRIVATE Surface_Wayland.cxx)
else()
  target_sources(vncviewer PRIVATE x11.cxx)
  target_sources(vncviewer PRIVATE GestureHandler.cxx XInputTouchHandler.cxx)
  target_sources(vncviewer PRIVATE KeyboardX11.cxx xkb_to_qnum.c)
  target_sources(vncviewer PRIVATE Surface_X11.cxx)
endif()
```

- [ ] **Step 2: Remove X11 link libraries for QNX**

Find at the bottom of the file:

```cmake
else()
  target_link_libraries(vncviewer ${X11_Xi_LIB})

  if(X11_Xrandr_LIB)
    target_link_libraries(vncviewer ${X11_Xrandr_LIB})
  endif()
endif()
```

Replace with:

```cmake
elseif(NOT CMAKE_SYSTEM_NAME STREQUAL "QNX")
  target_link_libraries(vncviewer ${X11_Xi_LIB})

  if(X11_Xrandr_LIB)
    target_link_libraries(vncviewer ${X11_Xrandr_LIB})
  endif()
endif()
```

- [ ] **Step 3: Commit**

```bash
git add vncviewer/CMakeLists.txt
git commit -m "port: add QNX Wayland source/link block to vncviewer CMakeLists"
```

---

## Task 11: Configure, build, and smoke-test

**Files:** none (build verification)

- [ ] **Step 1: Configure the build for QNX**

```bash
mkdir -p /tmp/tigervnc-build
cmake -S /data/home/qnxuser/dev/tigervnc \
  -B /tmp/tigervnc-build \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DFLTK_DIR=/usr/local/lib/cmake/fltk \
  -DBUILD_VIEWER=ON \
  2>&1 | tail -30
```

Expected: ends with `-- Build files have been written to: /tmp/tigervnc-build`

If cmake can't find FLTK, try adding:
```
  -DCMAKE_PREFIX_PATH=/usr/local
```

If cmake tries to build the Xorg server (`unix/xserver`), add:
```
  -DBUILD_XORG_SERVER=OFF -DBUILD_XVNC=OFF
```

- [ ] **Step 2: Build vncviewer only**

```bash
cmake --build /tmp/tigervnc-build --target vncviewer -j$(nproc) 2>&1 | tail -50
```

Expected: ends with `[100%] Linking CXX executable vncviewer` and no errors.

If errors appear:
- Undefined `fl_display` / `XImage` / `Pixmap`: a QNX guard was missed — add it and rebuild.
- Undefined `Fl_Window::maximize()`: FLTK version doesn't have it — replace with no-op in `wayland.cxx`.
- Undefined `WaylandKeyEvent`: check the `KeyboardWayland.h` include in `Viewport.cxx`.

- [ ] **Step 3: Smoke-test — help output**

```bash
/tmp/tigervnc-build/vncviewer/vncviewer --help 2>&1 | head -10
```

Expected: usage/help text starting with `TigerVNC Viewer` or similar.

- [ ] **Step 4: Smoke-test — launch GUI**

```bash
/tmp/tigervnc-build/vncviewer/vncviewer &
```

Expected: the TigerVNC server dialog appears on the XFCE desktop. Close it.

- [ ] **Step 5: Final commit**

```bash
cd /data/home/qnxuser/dev/tigervnc
git commit --allow-empty -m "port: vncviewer QNX Wayland port complete — smoke tested"
```
