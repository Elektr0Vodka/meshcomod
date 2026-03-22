#if defined(HAS_HELTEC_V4_CAP_TOUCH) && defined(ESP32)

#include "HeltecV4CapTouch.h"
#include <Arduino.h>
#include <Wire.h>
#include <chsc6x.h>
#include <helpers/ui/MomentaryButton.h>

#ifndef PIN_TOUCH_SDA
  #error PIN_TOUCH_SDA required for HAS_HELTEC_V4_CAP_TOUCH
#endif
#ifndef PIN_TOUCH_SCL
  #error PIN_TOUCH_SCL required for HAS_HELTEC_V4_CAP_TOUCH
#endif
#ifndef PIN_TOUCH_RST
  #define PIN_TOUCH_RST -1
#endif
#ifndef PIN_TOUCH_INT
  #define PIN_TOUCH_INT -1
#endif

#ifndef HELTEC_V4_TOUCH_LONG_MS
  #define HELTEC_V4_TOUCH_LONG_MS 1000
#endif

static chsc6x* s_tp;

void heltecV4CapTouchBegin() {
  static chsc6x instance(&Wire1, PIN_TOUCH_SDA, PIN_TOUCH_SCL, PIN_TOUCH_INT, PIN_TOUCH_RST);
  s_tp = &instance;
  s_tp->chsc6x_init();
}

int heltecV4CapTouchCheck() {
  if (!s_tp) {
    return BUTTON_EVENT_NONE;
  }

  static bool down = false;
  static unsigned long down_at = 0;
  static bool long_dispatched = false;

  uint16_t x = 0, y = 0;
  int r = s_tp->chsc6x_read_touch_info(&x, &y);
  bool now = (r == 0);

  if (now && !down) {
    down = true;
    down_at = millis();
    long_dispatched = false;
  } else if (!now && down) {
    down = false;
    unsigned long dur = (down_at > 0) ? (unsigned long)(millis() - down_at) : 0;
    down_at = 0;
    if (!long_dispatched && dur >= 30 && dur < (unsigned long)HELTEC_V4_TOUCH_LONG_MS) {
      return BUTTON_EVENT_CLICK;
    }
    long_dispatched = false;
  } else if (down && !long_dispatched && down_at > 0 &&
             (unsigned long)(millis() - down_at) >= (unsigned long)HELTEC_V4_TOUCH_LONG_MS) {
    long_dispatched = true;
    return BUTTON_EVENT_LONG_PRESS;
  }

  return BUTTON_EVENT_NONE;
}

#endif
