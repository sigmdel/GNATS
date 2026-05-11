# GNAT'S Nearly Accurate Time Server

*A tiny and very basic NTP server based on a GPS receiver.  
Runs on most ESP32 SoCs connected to a TinyGPSPlus supported GPS receiver.*

![icon](img/gnat_128x128.png) 

## Changes

2026-05-10: Corrected the hardware serial configuration and added board definitions in `platformio.ini`. Cleaned up the code of `main.cpp`.

2025-10-30: Added a [-?|-h|--help] command line option to the NTP client utilities in `utils/`.

2023-08-03: Added the `ethernet-test` branch in which an ENC28J60 based Ethernet module is used to connect to the local area network instead of Wi-Fi.

2023-07-19: Added optional support for a DS3231 battery powered real time clock. The RTC is not a backup time source; it is only used to set the initial time until the correct time is acquired by the GPS receiver.

## Hardware used

  - ESP32 development board with Wi-Fi such as XIAO ESP32S3, ESP32C3 Super Mini, Lolin32_lite.
  - GPS receiver supported by TinyGPSPlus such as the ATGM336H 5N-31
  - DS3231 battery backed real time clock (optional)
  - SSD1306 128x64 I2C OLED display (optional)
  - [Schematic - XIAO ESP32-xx](img/schematic.jpg)
  - [Schematic - ESP32-C3 Super Mini](img/schematic2.jpg)

## Libraries 

  - [TinyGPSPlus](https://github.com/mikalhart/TinyGPSPlus.git) by Mikal Hart at [Arduiniana](http://arduiniana.org). The library reads the $GNRMC NMEA messages from the GPS receiver and makes available the date and time data, among many other bits of information. Licence: unknown.

  - [ntp_server](lib/ntp_server/ntp_server.h) is a modified version of the NTP server (`ntp_server.h` and `ntp_server.cpp`) in the ElektorLabs [180662 mini NTP with ESP32](https://github.com/ElektorLabs/180662-mini-NTP-ESP32). There is a project description in the ElektorMag [mini-NTP server with GPS](https://www.elektormagazine.com/labs/mini-ntp-server-with-gps). Licence: GPLv3 or later at user choice.

  - [OLED SSD1306 (ESP8266/ESP32/Mbed-OS)](https://github.com/ThingPulse/esp8266-oled-ssd1306)
by ThingPulse is used to print the date and time on a small OLED screen. Licence: MIT.

  - [Rtc](https://github.com/Makuna/Rtc) by Michael Miller (Makuna) is used to read a battery-powered DS3231 real time clock which will provide the initial time to set the ESP real time clock until GPS time is available. Licence: LGPLv3

  - [smalldebug](lib/smalldebug.h) just defines two macros: DBG(...) and DBG(...). These are used throughout the code instead of Serial.println(...) and Serial.printf(...). The advantage of using these macros is that all the print statements will be stripped from the compiled firmware when the ENABLE_DBG directive is set to 0 in `platformio.ini`. Licence: None.

## Further documentation 

[GNATS, a Tiny Basic ESP32 GPS Based NTP Server](https://sigmdel.ca/michel/program/esp32/arduino/esp32_gps_time_server_en.html)

[Using a Local Network Time Server](https://sigmdel.ca/michel/program/esp32/arduino/local_timeserver_en.html)

## Warning

GNATS should not be used as the primary time source. Nevertheless, it may be accurate enough as a backup time source when access to better clocks is lost.

## Configuration before compiling

- Edit [secrets.h.template](src/secrets.h.template) and save it as `secrets.h` in the `src` directory.

- Edit `platformio.ini` :

  - Choose the appropriate board in the `[platformio]` section
  - Define the correct `LOCAL_TIME_ZONE` string for the geographical location of the board
  - Adjust the build flags for the selected dev board. 
  
    -  Specify the hardware serial peripheral that will be used to read from the TX output of the GPS module. It may be necessary to specify the RX pin number (see the `lolin32_lite` environment). 
    - For dev boards with ESP32 SoC without support for USB CDC (USB ACM), specify the `SERIAL_BAUD` for the USB-serial adapter (see the `lolin32_lite` environment).
    - If a supported I2C OLED display is connected to the board set the `HAS_OLED` directive to `1` otherwise to `0`. 
    - If a DS3231 Real time clock module is connected to the board set the `HAS_DS3231` to `1` otherwise to `0`.
    - It may be helpful to specify the Wi-Fi TX power of some ESP32C3 Super mini board (see the `nologo_esp32c3_super_mini` environment). See the `wifi_power_t` definition
      in `...framework-arduinoespressif32/libraries/WiFi/src/WiFiGeneric.h` for possible values of `TX_POWER`.

## Licence

The **BSD Zero Clause** ([SPDX](https://spdx.dev/): [0BSD](https://spdx.org/licenses/0BSD.html)) licence applies to all source not covered by another licence.
