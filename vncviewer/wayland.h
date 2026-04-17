#ifndef __WAYLAND_H__
#define __WAYLAND_H__

class Fl_Window;

bool x11_has_wm();
bool x11_wm_supports(const char* atom);

void x11_win_maximize(Fl_Window* win);
bool x11_win_is_maximized(Fl_Window* win);

void x11_win_get_coords(Fl_Window* win, int* x, int* y, int* w, int* h);

void x11_win_may_grab(Fl_Window* win);

bool x11_grab_keyboard(Fl_Window* win);
void x11_ungrab_keyboard();

void x11_warp_pointer(unsigned x, unsigned y);

bool x11_is_pointer_on_same_screen(Fl_Window* win);

#endif
