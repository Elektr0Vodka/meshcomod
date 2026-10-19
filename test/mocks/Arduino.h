#pragma once

#include <cstdint>
#include <cmath>
#include "Stream.h"

inline uint32_t g_mock_millis = 0;

using std::isnan;

inline uint32_t millis() {
  return g_mock_millis;
}

inline void delay(uint32_t ms) {
  g_mock_millis += ms;
}

// ltoa comes with the Arduino cores, newlib and MinGW, but not with glibc, so the native
// tests on Linux (CI) need their own. TxtDataHelpers.cpp uses it.
#if !defined(_WIN32)
inline char* ltoa(long value, char* buf, int radix) {
  char tmp[sizeof(long) * 8 + 1];
  unsigned long v = value < 0 && radix == 10 ? 0UL - (unsigned long)value : (unsigned long)value;
  int i = 0;
  do {
    int d = (int)(v % (unsigned long)radix);
    tmp[i++] = (char)(d < 10 ? '0' + d : 'a' + d - 10);
    v /= (unsigned long)radix;
  } while (v);
  char* p = buf;
  if (value < 0 && radix == 10) *p++ = '-';
  while (i) *p++ = tmp[--i];
  *p = 0;
  return buf;
}
#endif
