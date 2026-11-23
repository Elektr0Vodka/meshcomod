#include <Arduino.h>
#include "target.h"
#if defined(HAS_TOUCH_UI)
  #include <helpers/esp32/TouchPrefsStore.h>   // touchPrefsGetGpsBaud
#endif

TDeckBoard board;

#if defined(P_LORA_SCLK)
  static SPIClass spi;
  RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, spi);
#else
  RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY);
#endif

WRAPPER_CLASS radio_driver(radio, board);

SPIClass* tdeckSharedSPI() {
#if defined(P_LORA_SCLK)
  return &spi;     // already begun (SCLK/MISO/MOSI) by radio.std_init
#else
  return nullptr;
#endif
}

ESP32RTCClock fallback_clock;
AutoDiscoverRTCClock rtc_clock(fallback_clock);
NmeaGpsDriver gps_driver(Serial1);
MicroNMEALocationProvider gps(gps_driver, &rtc_clock);
EnvironmentSensorManager sensors(gps);

#ifdef DISPLAY_CLASS
  DISPLAY_CLASS display;
  MomentaryButton user_btn(PIN_USER_BTN, 1000, true);
  UITask ui_task(&board, &display);
#endif

bool radio_init() {
  fallback_clock.begin();
  rtc_clock.begin(Wire);
  Wire.begin(18, 8);
#if defined(HAS_TOUCH_UI)
  // Touch builds keep a user-selectable GPS baud in NVS (T-Deck Plus = 38400,
  // T-Deck v1.0 = 9600), so it can be matched to the hardware without a rebuild.
  gps_driver.setBaudRate(touchPrefsGetGpsBaud(GPS_BAUD_RATE));
#endif

#if defined(P_LORA_SCLK)
  return radio.std_init(&spi);
#else
  return radio.std_init();
#endif
}

mesh::LocalIdentity radio_new_identity() {
  RadioNoiseListener rng(radio);
  return mesh::LocalIdentity(&rng); // create new random identity
}
