/*
  ____ _   _    _  _____ ____
 / ___| \ | |  / \|_   _/ ___|
| |  _|  \| | / _ \ | | \___ \
| |_| | |\  |/ ___ \| |  ___) |
 \____|_| \_/_/   \_\_| |____/  GNATS' Not Accurate Time Server

A tiny and very basic NTP server based on a GPS receiver and running
on the SeeedStudio XIAO ESP32C3 or XIAO ESP32S3

*/

#include <Arduino.h>              // framework for platformIO
#include <HardwareSerial.h>       // for access to the hardware serial interface
#include <WiFi.h>                 //
#include <time.h>                 // access to the ESP RTC
#include <Preferences.h>          // save mclock to NVS
#include "smalldebug.h"           // in lib/
#include "ntp_server.h"           // in lib/
#include "secrets.h"              // use secrets.h.template to create this file
#include "TinyGPSPlus.h"          // loaded with platformio directive

#if (HAS_DS3231 > 0)
#include <Wire.h>                 // Arduino I2C library
#include <RtcDS3231.h>            // in .pio/libdeps
#endif

#if (HAS_OLED > 0)
#include "SSD1306Wire.h"          // hardware driver for SSD1306 OLED display in .pio/libdeps
#endif

#if (SHOW_NMEA>0) && (!ENABLE_DGB)
#undef ENABLE_DBG
#define ENABLE_DBG 1
#endif

// Time between attempts at updating the ESP32 RTC
// This is the initial value,

#if defined(SYNC_POLL_TIME)
unsigned long timePollInterval = SYNC_POLL_TIME;
#else
unsigned long timePollInterval = 10000;  // 10 seconds, for Arduino
#endif

// This is the delay after the first successful update
#if !defined(GPS_POLL_TIME)
#define GPS_POLL_TIME = 3600000;        // 1 hours, for Arduino
#endif

/**********************/
/* * * NTP server * * */
/**********************/

NTP_Server NTPServer;


/*********************************/
/* * * DS3231 - External RTC * * */
/*********************************/

#if (HAS_DS3231 > 0)

RtcDS3231<TwoWire> ExtRtc(Wire);

void InitExtRtc(void) {
    DBG("Initializing external real time clock - DS3231");
    ExtRtc.Begin();
    #if defined(WIRE_HAS_TIMEOUT)
      Wire.setWireTimeout(3000 /* us */, true /* reset_on_timeout */);
    #endif
    if (!ExtRtc.GetIsRunning())
      ExtRtc.SetIsRunning(true);
    // Default configuration - disable everything
    ExtRtc.Enable32kHzPin(false);
    ExtRtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone);
}

uint32_t extRtcTime(void) {
  if (ExtRtc.IsDateTimeValid()) {
    RtcDateTime drtc = ExtRtc.GetDateTime();
    return drtc.Unix32Time();
  }
  return 0; // error!
}

#endif

/**********************/
/* * * Saved time * * */
/**********************/

Preferences preferences;

// A timestamp that is updated at regular intervals and saved to non-volatile storage
// which can be used to set the ESP RTC on booting even before an update from a
// better time source is available. It is also used to ensure that updates of the
// ESP RTC time move forward. This is very much like the systemd-timesyncd clock file
// see https://man.archlinux.org/man/systemd-timesyncd.8#FILES
time_t mclock = 0;

// Save the current ESP RTC time to mclock, to an external RTC, and to NVS
// assuming it is greater or equal to mclock
void savemclock(void) {
  time_t newvalid;
  time(&newvalid); // read current time from the ESP RTC
  if (newvalid < mclock) {
    // keep time moving along
    DBGF("Time moving backwards!");
    return;
  }
  mclock = newvalid;
  #if (HAS_DS3231 > 0)
    // update hardware real time clock with GPS time
    RtcDateTime drtc;
    drtc.InitWithUnix32Time(mclock);
    ExtRtc.SetDateTime(drtc);
    DBGF("Saving mclock = %u to hardware external clock\n", mclock);
  #endif
  preferences.begin("mclock", false);
  preferences.putULong("time", mclock);   // save mclock value in NVS
  preferences.end();
  DBGF("Saving mclock = %u to NVS\n", mclock);
}

// Set the current RTC time from the last saved mclock value
// or from the firmware compilation time whichever is ahead.
// Call only once on reboot
void loadmclock(void) {
  preferences.begin("mclock", false);
  mclock = preferences.getULong("time", 0);  // default 0 if not already defined
  // In the arduino IDE, use https://github.com/sigmdel/mdBuildTime
  //
  // setenv("TZ", timeZone, 1);
  // time_t compileTime = unixbuildtime();
  // if (mclock < compileTime) {
  //  mclock = compileTime;
  //  ...

  #if (ENABLE_DBG > 0)
  if (mclock) {
    DBG("Using time saved to NVS");
  }
  #endif

  #if (HAS_DS3231 > 0)
  uint32_t xrtcnow = extRtcTime();
  if (mclock < xrtcnow) {
    mclock = xrtcnow;
    DBG("Using external RTC time as last known time");
  }
  #endif

  if (mclock < COMPILE_TIME) {
    mclock = COMPILE_TIME; // Unix timestamp macro set in platformio.ini
    DBG("Using compile time as last known time");
  }

  if (mclock) {
    timeval tv;
    tv.tv_sec = mclock ;
    tv.tv_usec = 0;
    int res = settimeofday(&tv, NULL);
    preferences.putULong("time", mclock);   // save mclock value in NVS
    #if (ENABLE_DBG > 0)
      if (res) {
        DBG("Unable to set the initial time of day");
      } else {
        struct tm* tminfo;
        tminfo = gmtime(&tv.tv_sec);
        char s[51];
        strftime(s, 50, "%A, %B %d %Y %H:%M:%S", tminfo);
        DBGF("UTC time set from last known time: %s\n", s);
      }
    #endif
  }
  preferences.end();
}

/*
// never used...
void clearmclock(void) {
  preferences.begin("mclock", false);
  preferences.putULong("time", 0);
  preferences.end();
}
*/

/***************/
/* * * GPS * * */
/***************/

TinyGPSPlus gps;
static const uint32_t GPSBaud = 9600;

// Set to true as soon as the ESP RTC is updated with time from the GPS
bool timesynched = false;

// Serial interface used to talk to the GPS
#if defined(HDW_SERIAL_INTF)
#define hdwSerial HDW_SERIAL_INTF
#else
#define hdwSerial Serial2
#endif

#if defined(UART_RX_PIN)
static const int RXPin = UART_RX_PIN;
#else
static const int RXPin = -1;
#endif

#if defined(UART_TX_PIN)
static const int TXPin = UART_TX_PIN;
#else
static const int TXPin = -1;
#endif

// Updates the ESP32 RTC with the given GPS data which is UTC time
// to the nearest second and use gpsAge to calculate fractions of a second
void gpssetime(uint32_t gpsDate, uint32_t gpsTime, uint32_t gpsAge) {
  DBGF("gpssetime date: %u, time: %u\n", gpsDate, gpsTime);
  time_t now = 0;
  struct tm timeinfo;

  div_t delta = div( gpsAge, 1000);
  gpsTime += delta.quot;
  timeinfo.tm_sec = (gpsTime / 100) % 100;
  timeinfo.tm_min = (gpsTime / 10000) % 100;
  timeinfo.tm_hour = gpsTime / 1000000;
  timeinfo.tm_mday = gpsDate / 10000;
  timeinfo.tm_mon = ((gpsDate / 100) % 100) - 1;
  timeinfo.tm_year = (2000 - 1900) + (gpsDate % 100);
  timeinfo.tm_wday = -1;
  timeinfo.tm_yday = -1;
  timeinfo.tm_isdst = 0;  // UTC does not have daylight saving time
  // mktime returns the epoch representing the values in the tm structure timeinfo
  // which it interprets as a **local** time.
  setenv("TZ", "UTC0", 1);  // revert to UTC time
  now = mktime(&timeinfo);  // calculates epoch and updates the last three fields
  if (now <= mclock) {
    DBG("*** Error: Time going backward ***");
    return;
  }

  //DBG("after mktime, timeinfo.tm_hour = %d\n", timeinfo.tm_hour);

  timeval tv;
  tv.tv_sec = now;
  tv.tv_usec = 1000 * delta.rem; // milliseconds to microseconds
  if (settimeofday(&tv, NULL)) { /// defined in ~/.platformio/packages/toolchain-xtensa-esp32/xtensa-esp32-elf/sys-include/sys/time.h
    DBGF("Error setting time, errno = %d\n", errno);
  } else {
    #if (ENABLE_DBG == 1)
      // read time back
      struct tm* tinfo;  // defined in ~/.platformio/packages/toolchain-xtensa-esp32/xtensa-esp32-elf/sys-include/time.h
      time(&now);
      tinfo = gmtime(&now);
      char s[51];
      strftime(s, 50, "%A, %B %d %Y %H:%M:%S", tinfo);
      DBGF("UTC time set from GPS: %s.%.6u (epoch = %u)\n", s, tv.tv_usec, now);
    #endif
    timesynched = true;
    // now that the time is synchronized, wait longer before performing updates from the GPS data
    timePollInterval = GPS_POLL_TIME;
  }
}


bool updateRTC(void) {
  if (gps.date.isValid() && gps.time.isValid() && (gps.date.value())) {
    // NMEA messages such $GNRMC,,V,,,,,,,,,,M*4E return gps.date.isValid() = true
    // and gps.time.isValid() = true even when UTC Time == 0 and Date == 0
    // so a test that date of !0 is needed!
    gpssetime(gps.date.value(), gps.time.value(), gps.time.age());
    return true;
  }
  return false;
}

/************************/
/* * * OLED display * * */
/************************/

char timeBuffer[9];     // time format:  14:50 (synched) ~14:60~ (not synched or old)
char dateBuffer[12];    // date format: 2023:11:31

#if (HAS_OLED > 0)

//#define SDA  6   // defined in  ~/.platformio/packages/framework-arduinoespressif32/variants/XIAO_ESP32C3/pins_arduino.h
//#define SCL  7

SSD1306Wire display(0x3c, SDA, SCL, GEOMETRY_128_64);

int jitter[3] = {-1, 0, 1};
int xjit = 0;
int yjit = 1;


void Show(void) {
  display.clear();
  display.drawString(64+jitter[xjit], 2+jitter[yjit], timeBuffer);
  xjit = xjit % 3;
  yjit = yjit % 3;
  display.drawString(64+jitter[xjit], 32+jitter[yjit], dateBuffer);
  display.display();
  xjit = xjit % 3;
  yjit = yjit % 3;
}

#endif // HAS_OLED

/*****************/
/* * * setup * * */
/*****************/

#if defined(LOCAL_TIME_ZONE)
  const char* timeZone = LOCAL_TIME_ZONE;
#else
  const char* timeZone = "AST4ADT,M3.2.0,M11.1.0";  // or perhaps "UTC0"
#endif

void setup() {
  #ifdef SERIAL_BAUD
  Serial.begin(SERIAL_BAUD);
  #else
  Serial.begin();  // Serial over USB CDC, i.e. ESP32C3, ESP32S2, ESP32S3
  #endif

  delay(5000);

  DBG("Time Server");
  DBG("setup()...");

  #if (HAS_DS3231 > 0)
    InitExtRtc();
  #endif

  // set RTC with mclock, the last known time or failing that the compile time
  loadmclock();

  DBG("Initializing serial connection to the GPS");
  hdwSerial.begin(GPSBaud, SERIAL_8N1, RXPin, TXPin);
  delay(1000);

  #if (HAS_OLED > 0)
  DBG("Initializing OLED display");
  strlcpy(timeBuffer, "--:--", sizeof(timeBuffer));
  strlcpy(dateBuffer, "----.--.--.", sizeof(dateBuffer));
  display.init();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_24);
  display.displayOn();
  Show();
  #endif

  IPAddress staip, gateway, mask;
  staip.fromString(WIFI_STAIP);
  gateway.fromString(WIFI_GATEWAY);
  mask.fromString(WIFI_MASK);
  DBGF("Connecting to %s\n", WIFI_SSID);
  DBGF("  static IP: %s\n", staip.toString().c_str());
  DBGF("  gateway:   %s\n", gateway.toString().c_str());
  DBGF("  mask:      %s\n", mask.toString().c_str());
  WiFi.config(staip, gateway, mask);
  WiFi.begin(WIFI_SSID, WIFI_PSWD);
  while (WiFi.status() != WL_CONNECTED) {
      delay(50);
  }
  DBGF("Connected to %s\n", WiFi.SSID().c_str());
  delay(100);
  DBGF("Starting NTP server at %s:%d\n", WiFi.localIP().toString().c_str(), 123);
  NTPServer.begin(123); // 123 is the default port
  DBG("Completed setup(), starting loop()");
}

/****************/
/* * * loop * * */
/****************/

// System millis tick count of the last attempt to perform an update of the ESP32 RTC
// Not keeping track of whether it was a success or not.
unsigned long lastRtcUpdate = 0;

// System millis tick count of the last successful update of the ESP32 RTC from GPS data
// Used to signify that the time is "approximate"
unsigned long lastRtcCorrection = 0;

// System millis tick count of the last attept to save the current RTC time to
// non-volatile storage. This is done independently of whether the RTC has been
// updated by the GPS or not.
unsigned long mclocktimer = 0;

// System millis tock count of the last time the NO GPS FOUND message was shown
unsigned long lastWarning = 0;

#if (SHOW_NMEA > 0)
String nmea;
#endif

void loop(void) {

  // feed gps with incoming serial data
  while (hdwSerial.available() > 0) {
    char c = hdwSerial.read();
    #if (SHOW_NMEA > 0)
      if (c == 10) {
        #if (SHOW_NMEA == 1)
          if ( nmea.startsWith("$GNRMC") || nmea.startsWith("$GPRMC") || nmea.startsWith("$GNZDA") )
        #endif
          DBGF("NMEA: %s\n", nmea.c_str());
        nmea = "";
      } else if (c != 13)
        nmea += c;
    #endif
    gps.encode(c);
  }

  // timePollInterval = SYNC_POLL_TIME (=10000) initially and then
  // = GPS_POLL_TIME (=360000) after first update
  if (millis() - lastRtcUpdate >= timePollInterval) {
    DBG("Time to update the RTC");
    lastRtcUpdate = millis();
    if (updateRTC())
      lastRtcCorrection = millis();
  }

  if (millis() - mclocktimer >= SAVE_CLOCK_TIME) {
    DBG("Time to set mclock and save it to NVS");
    mclocktimer = millis();
    savemclock();
  }

  if ((millis() - lastWarning > GPS_WARNING_TIME) && (gps.charsProcessed() < 10))  {
    lastWarning = millis();
    DBG("No GPS detected");
    #if (HAS_OLED > 0)
    display.clear();
    display.drawString(64, 2, "NO GPS");
    display.drawString(64, 32, "FOUND");
    display.display();
    #endif
  }

  const char* synchedTimeFormat = "%H:%M";
  const char* notSynchedTimeFormat = "~%H:%M~";  // tildes to show time is "approximate"

  // Update clock on OLED at the 0 second mark, allowing for a few seconds window
  // in case the system is busy for a full second at the start of the "new" minute
  #define MINUTE_WINDOW 1
  time_t lastUTCTime;
  if  (time(&lastUTCTime) % 60 == 0) {
    struct tm timeinfo;
    // want to show local time, so set the timezone
    setenv("TZ", timeZone, 1);
    localtime_r(&lastUTCTime, &timeinfo);
    strftime(timeBuffer, sizeof(timeBuffer), ((timesynched)  && (millis() - lastRtcCorrection <= 2*GPS_POLL_TIME))
      ? synchedTimeFormat       
      : notSynchedTimeFormat, &timeinfo);
    strftime(dateBuffer, sizeof(dateBuffer), "%F", &timeinfo);
    DBGF("Local time: %s %s (utc %u)\n", dateBuffer, timeBuffer, lastUTCTime);
    #if (HAS_OLED > 0)
    Show();
    #endif
    delay(MINUTE_WINDOW*1100);  // delay for longer than the 0 second window
  }
}
