/*
  ____ _   _    _  _____ ____
 / ___| \ | |  / \|_   _/ ___|
| |  _|  \| | / _ \ | | \___ \
| |_| | |\  |/ ___ \| |  ___) |
 \____|_| \_/_/   \_\_| |____/  GNATS' Not Accurate Time Server

A tiny and very basic NTP server based on a GPS receiver 
which runs on most ESP32 based boards

*/

#include <Arduino.h>          // framework for platformIO
#include <HardwareSerial.h>   // for access to the hardware serial interface
#include <time.h>             // access to the ESP RTC
#include <Preferences.h>      // to save mclock to NVS
#include "smalldebug.h"       // in lib/
#include "ntp_server.h"       // in lib/
#include "TinyGPSPlus.h"      // loaded with platformio directive


// Sanity checks 

// Dumping NMEA messages requires ENABLE_DBG = 1
#if (SHOW_NMEA>0) 
#undef ENABLE_DBG
#define ENABLE_DBG 1
#endif

// The ESP3C3 Mini with 0.42" OLED uses a different library
// use HAS_OLED = 2 to identify that
#if defined(ARDUINO_ESP32C3_OLED_MINI) and (HAS_OLED > 0)
#undef HAS_OLED
#define HAS_OLED 2
#endif

/******************************************/ 
/* * * Serial Interface to GPS Module * * */
/******************************************/

// Serial interface used to talk to the GPS
#if defined(HDW_SERIAL_INTF)
  // Explicit serial peripheral
#define hdwSerial HDW_SERIAL_INTF
#else
  // Guess which serial peripheral 
#if !defined(ARDUINO_USB_CDC_ON_BOOT)
#define ARDUINO_USB_CDC_ON_BOOT 0
#endif
#if (ARDUINO_USB_CDC_ON_BOOT > 0)
#define hdwSerial Serial0
#else
#define hdwSerial Serial1
#endif
#endif

#if defined(UART_RX_PIN)
  // Explicit UART RX pin to use
static const int RXPin = UART_RX_PIN;
#else
  // Don't change the pin assigned to the serial peripheral
static const int RXPin = -1;    
#endif

// default baud of GPS module
#if defined(GPS_BAUD)
static const unsigned long GPSBaud = GPS_BAUD;
#else
static const unsigned long GPSBaud = 9600;
#endif


/******************************/
/* * * Network Connection * * */
/******************************/

#include <WiFi.h>
#include "secrets.h"              // use secrets.h.template to create this file in src/

bool NetworkConnect(void) {
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

  #ifdef TX_POWER 
  if (WiFi.getTxPower() != TX_POWER) {
    WiFi.setTxPower(TX_POWER);
    delay(25);
  }
  int txpower = WiFi.getTxPower();
  #if (ENABLE_DBG > 0)
  if (txpower != TX_POWER) 
    DBGF("Unable to set Wi-Fi TX power to: %d = %.1f dBm\n", TX_POWER, (float)(TX_POWER)*0.25);
  DBGF("Wi-Fi TX power set to: %d = %.1f dBm\n", txpower, txpower*0.25);
  #endif  
  #endif
 
  static bool wifi_connected = false;  
  unsigned long connect_time = millis();
  while (!WiFi.isConnected()) {
    delay(500);
    #if (ENABLE_DBG > 0)
    Serial.print(".");
    #endif
    if (millis() - connect_time > CONNECT_TIMEOUT) 
      break;
  }
  #if (ENABLE_DBG > 0)
  Serial.println();
  #endif
  
  if (WiFi.isConnected()) {
    DBGF("Connected to %s\n", WiFi.SSID().c_str());
    DBGF("Starting NTP server at %s:%d\n", WiFi.localIP().toString().c_str(), 123);
  } else {
    DBGF("Unable to connect to %s\n", WIFI_SSID);
    DBG("Unable to start NTP server");
  }

  return WiFi.isConnected();
}

/**********************/
/* * * NTP server * * */
/**********************/

NTP_Server NTPServer;

/*********************************/
/* * * Optional OLED display * * */
/*********************************/

#if (ENABLE_DBG > 0) || (HAS_OLED > 0) 

// Used in some DBG/DBGF statements and for displaying time & date
char timeBuffer[9];      // time format:  14:50 (synched) ~14:50~ (or 14*50) (not synched with GPS)
char dateBuffer[12];     // date format: 2023/11/31

#endif

#if (HAS_OLED > 0)

// Move the time and date off centre by small amounts each
// time they are drawn to avoid "burn-in" damage
int jitter[3] = {-1, 0, 1};
int xjit = 0;
int yjit = 1;


#if (HAS_OLED > 1)

#include "SSD1315.h"        // '72x40oled_lib' in .pio/libdeps

SSD1315 display(NO_RESET_PIN); 

void blinkDisplay(void) {
  display.clear();
  display.display();
  delay(10);
  display.invert(true);
  delay(20);
  display.invert(false);
}  

void InitDisplay(void) {
   // SSD1315, 72x40 OLED
  DBG("Initializing OLED display");
  strlcpy(timeBuffer, "--:--", sizeof(timeBuffer));
  strlcpy(dateBuffer, "--/--/--", sizeof(dateBuffer));
  Wire.begin();
  display.begin();
}

void display_drawCenteredString(int y, const char* str, int size) {
  // **NOTE** size should be 5, 12 or 16 corresponding to the 3 fonts in the library
  int xadv = (size <= 5) ? 5 : size / 2;
  int wd = strlen(str)*xadv; // width of string in pixels
  int x = (72-wd)/2 + ((wd < 70) ? jitter[xjit] : 0);
  //DBGF("x: %d, y: %d, size: %d, xadv: %d, wd: %d, str: %s\n", x, y + jitter[yjit], size, xadv, wd, str);
  display.drawString(x, y + jitter[yjit], str, size);
}

#else

#include "SSD1306Wire.h" // 'ESP8266 and ESP32 OLED driver for SSD1306 displays' in .pio/libdeps

  // I2C SDA and SCL pins defined in variant pins_arduino.
SSD1306Wire display(0x3c, SDA, SCL, GEOMETRY_128_64);

void blinkDisplay(void) {
  display.clear();
  display.display();
  delay(10);
  display.invertDisplay();
  delay(20);
  display.normalDisplay();
}  

void InitDisplay(void) {
  DBG("Initializing OLED display");
  strlcpy(timeBuffer, "--:--", sizeof(timeBuffer));
  strlcpy(dateBuffer, "----/--/--", sizeof(dateBuffer));
  display.init();
  display.flipScreenVertically();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_24);
  display.displayOn();
  Show();
}  
#endif // #else part of (HAS_OLED > 0)

void Show(void) {
  display.clear();

  //DBGF("xjit: %d, yjit: %d, jitter[x]: %d, jitter[y]: %d\n", xjit, yjit, jitter[xjit], jitter[yjit]);

  #if (HAS_OLED > 1)

  display_drawCenteredString(2, timeBuffer, 16);
  display_drawCenteredString(22, dateBuffer, 16);  

  #else 

  int x = display.width()/2 + 2 + jitter[xjit];
  int y = 2 + jitter[yjit];
  display.drawString(x, y, timeBuffer);
  display.drawString(x, display.height()/2 + y, dateBuffer);
  xjit = (xjit + 1) % 3;
  yjit = (yjit + 1) % 3;

  #endif

  xjit = (xjit + 1) % 3;
  yjit = (yjit + 1) % 3;
  
  display.display();
}

#endif // HAS_OLED > 0


/******************************************/
/* * * Optional DS3231 - External RTC * * */
/******************************************/

#if (HAS_DS3231 > 0)

#include <Wire.h>             // Arduino I2C library
#include <RtcDS3231.h>        // in .pio/libdeps

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
    #if (ENABLE_DBG > 0)
    if (ExtRtc.LastError()) 
      DBG("Error initializing external real time clock");
    #endif
}

uint32_t extRtcTime(void) {
  if (ExtRtc.IsDateTimeValid()) {
    RtcDateTime drtc = ExtRtc.GetDateTime();
    return drtc.Unix32Time();
  }
  return 0; // error!
}

void setExtRtcTime(time_t now) {
  RtcDateTime drtc;
  drtc.InitWithUnix32Time(now);
  ExtRtc.SetDateTime(drtc);
  #if (ENABLE_DBG > 0)
  if (ExtRtc.LastError()) {
    DBGF("Error trying to save time = %u to external hardware clock", now);
  } else {
    DBGF("Saving time = %u to external hardware clock\n", now);
  }
  #endif
}    

#endif // HAS_DS3231 > 0


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
    DBG("Time moving backwards! Time not saved.");
    return;
  }
  mclock = newvalid;
  
  #if (HAS_DS3231 > 0)
    // update hardware real time clock with GPS time
    RtcDateTime drtc;
    drtc.InitWithUnix32Time(mclock);
    ExtRtc.SetDateTime(drtc);
    #if (ENABLE_DBG > 0)
    if (ExtRtc.LastError()) {
      DBG("Error trying to save mclock to external hardware clock");
    } else {
      DBGF("Saving mclock = %u to external hardware clock\n", mclock);
    }  
    #endif
  #endif

  preferences.begin("mclock", false);
  preferences.putULong("time", mclock);   // save mclock value in NVS
  preferences.end();
  DBGF("Saving mclock = %u to NVS\n", mclock);
}

/*
// never used...
void clearmclock(void) {
  preferences.begin("mclock", false);
  preferences.putULong("time", 0);
  preferences.end();
}
*/


// Set the current system time (ESP32 RTC controller) from whichever 
// source of time is ahead. The possible sources are:
//  - the firmware compilation time
//  - the last saved time in NVS
//  - the external real time clock (if available).
// This function is call only once on reboot.
void loadmclock(void) {
  uint32_t comptime = COMPILE_TIME; // Unix timestamp macro set in platformio.ini
  DBGF("Compile time: %d\n", comptime);

  preferences.begin("mclock", false);
  uint32_t mvstime = preferences.getULong("time", 0);  // default 0 if not already defined
  DBGF("Time saved in NVS: %d\n", mvstime);
  preferences.end();

  #if (HAS_DS3231 > 0)
  uint32_t xrtcnow = extRtcTime();
  DBGF("Time from external real time clock: %d\n", xrtcnow);
  #else
  uint32_t xrtcnow = 0;
  #endif

  int source = 0;
  mclock = 0;
  if (comptime) {
    mclock = comptime; 
    source = 1;
  }
  
  if (mvstime > mclock) {
    mclock = mvstime;
    source = 2;
  }

  if (xrtcnow > mclock) {
    mclock = xrtcnow;
    source = 3;
  }

  #if (ENABLE_DBG > 0)
  if (source == 3) 
    DBG("Using the time from the external real time clock")
  else if (source == 2)  
    DBG("Using the time saved in non volatile storage")
  else if (source == 1)
    DBG("Using the firmware compilation time")
  else {
    DBG("No valid time source found");
    return; 
  }
  #endif

  // update the system time to mclock
  timeval tv;
  tv.tv_sec = mclock ;
  tv.tv_usec = 0;
  int res = settimeofday(&tv, NULL);
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

  // update the time in NVS and hardware real time clock
  savemclock();
}


/***************/
/* * * GPS * * */
/***************/

TinyGPSPlus gps;

// Set to true as soon as the ESP RTC is updated with time from the GPS
// and set to false when a valid time value has not been obtained from 
// the GPS for longer than GPS_TIMEOUT milliseconds.
bool timesynched = false;

// Time between attempts at updating the ESP32 RTC from GPS time 
unsigned long timePollInterval = SYNC_POLL_TIME;
// SYNC_POLL_TIME is the initial value that will  be changed 
// to GPS_POLL_TIME after the first successful update from the GPS

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
    #if (ENABLE_DBG > 0)
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
    DBG("Valid gps time data obtained, update the system time (ESP RTC)");
    gpssetime(gps.date.value(), gps.time.value(), gps.time.age());
    return true;
  }
  return false;
}

/***************************/
/* * * fillTimeBuffers * * */
/***************************/

#if (ENABLE_DBG > 0) || (HAS_OLED > 0)

#if defined(LOCAL_TIME_ZONE)
  const char* timeZone = LOCAL_TIME_ZONE;
#else
  const char* timeZone = "UTC0";
#endif

const char* synchedTimeFormat = "%H:%M";       // 24 hour clock such as 15:40
#if (HAS_OLED > 1)
const char* notSynchedTimeFormat = "%H*%M";    // separating * shows time is "approximate" (not GPS based)
const char* dateFormat = "%y/%m/%d";           // 26/03/20 (no room for 10 8x16 chars in a 72 pixel wide display)
#else
const char* notSynchedTimeFormat = "~%H:%M~";  // tildes show time is "approximate" (not GPS based)
const char* dateFormat = "%Y/%m/%d";           // 2026/03/20 ISO 8601 is "%F"= "%Y-%m-%d"
#endif       

void fillTimeBuffers(time_t lastUTCTime) {
  struct tm timeinfo;
  // want the local time, so set the timezone
  setenv("TZ", timeZone, 1);
  localtime_r(&lastUTCTime, &timeinfo);
  strftime(timeBuffer, sizeof(timeBuffer), 
     (timesynched) ? synchedTimeFormat : notSynchedTimeFormat, &timeinfo);
  strftime(dateBuffer, sizeof(dateBuffer), dateFormat, &timeinfo);
}

#endif  // (ENABLE_DBG > 0) or (HAS_OLED > 0) 


/*****************/
/* * * setup * * */
/*****************/

void setup() {
  #ifdef SERIAL_BAUD
  Serial.begin(SERIAL_BAUD);
  #else
  Serial.begin();  // Serial over USB CDC, i.e. ESP32C3, ESP32S2, ESP32S3...
  delay(3000);
  #endif

  delay(5000);     // time to start the serial monitor

  #if (ENABLE_DBG > 0)
  DBG("Time Server");
  #else
  Serial.println("Time Server");
  Serial.println("Serial output disabled.");
  #endif
  DBG("setup()...");

  #if (HAS_DS3231 > 0)
    InitExtRtc();
  #endif

  #if (HAS_OLED > 0)
    InitDisplay();
    Show();
  #endif  // HAS_OLED > 0

  DBG("Initializing serial connection to the GPS");
  hdwSerial.begin(GPSBaud, SERIAL_8N1, RXPin);
  delay(1000);

   // set RTC with mclock, the last known time or failing that the compile time
  loadmclock();
 
  // Connect to the local network and start the NTP server if possible  
  if (NetworkConnect()) {  
    if (!NTPServer.begin(123)) {
      DBG("Unable to start NTP server");
    }  
  }
  DBG("Completed setup(), starting loop()");
}

/****************/
/* * * loop * * */
/****************/

// System millis tick count of the last attempt to perform an update of the ESP32 RTC
// Not keeping track of whether it was a success or not.
unsigned long lastRtcUpdate = 0;

// System millis tick count of the last attept to save the current RTC time to
// non-volatile storage. This is done independently of whether the RTC has been
// updated by the GPS or not.
unsigned long mclocktimer = 0;

// System millis tick count of the last time the NO GPS FOUND message was shown
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

 if (millis() - lastRtcUpdate >= timePollInterval) {
    //DBG("Time to update the system (ESP RTC) time"); // too much chatter
    lastRtcUpdate = millis();
    updateRTC();
  }

  if (millis() - mclocktimer >= SAVE_CLOCK_TIME) {
    DBG("Time to set mclock and save it to NVS");
    mclocktimer = millis();
    savemclock();
  }

  // The following bit is only needed to handle a time display or the debug output to the serial monitor.
  // It is not required to run the NTP server

  #if (ENABLE_DBG > 0) || (HAS_OLED > 0)  

  // has the link to GPS been made or been lost 
  bool connected = (gps.time.age() < GPS_TIMEOUT);
  bool statusChanged = false;
  if (connected != timesynched) {  // status changed
    if (connected) {
      DBG("GPS acquired")
    } else if (timesynched) {
      DBG("GPS lost")
    } else { 
      DBG("No GPS found");
    }  
    timesynched = connected;
    statusChanged = true;
  }

  // display the current time at the start of a new minute
  time_t lastUTCTime;
  bool topOfMinute = (time(&lastUTCTime) % 60 == 0);
  if (topOfMinute || statusChanged) {
    #if (HAS_OLED > 0) 
    if (statusChanged) blinkDisplay();
    #endif  
    statusChanged = false;
    fillTimeBuffers(lastUTCTime);
    DBGF("Local time: %s %s (utc %u)\n", dateBuffer, timeBuffer, lastUTCTime);
    #if (HAS_OLED > 0)
    Show();
    #endif
    if (topOfMinute) delay(1100);  // delay for longer than the 0 second window at top of minute
  }  
  #endif
 }
