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
  return { (uint32_t)systemKeyCode };
}

unsigned KeyboardWayland::getLEDState()
{
  return rfb::ledUnknown;
}

void KeyboardWayland::setLEDState(unsigned /*state*/)
{
}
