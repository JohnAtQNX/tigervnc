#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <FL/Fl.H>
#include <FL/Fl_Window.H>

#include "wayland.h"

bool x11_has_wm()
{
  return true;
}

bool x11_wm_supports(const char* /*atom*/)
{
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
}

bool x11_grab_keyboard(Fl_Window* win)
{
  Fl::grab(win);
  return true;
}

void x11_ungrab_keyboard()
{
  Fl::grab(nullptr);
}

void x11_warp_pointer(unsigned /*x*/, unsigned /*y*/)
{
}

bool x11_is_pointer_on_same_screen(Fl_Window* /*win*/)
{
  return true;
}
