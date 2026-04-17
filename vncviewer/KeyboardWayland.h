#ifndef __KEYBOARDWAYLAND_H__
#define __KEYBOARDWAYLAND_H__

#include "Keyboard.h"

struct WaylandKeyEvent {
  int type;
  unsigned keysym;
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
