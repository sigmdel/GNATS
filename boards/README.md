# Board Definitions and Variant pins_arduino.h for the ESP32-C3 Mini with 0.42" OLED Display

*May 7, 2026*

---

##  Lists of Files and Directories in this Directory

| File | Description | Environment |
| ---  | --- | --- |
| esp32c3_oled_mini.json | pioarduino board definition where SS = 0 | PIO/pio |
| README.md | This file | PIO/pio |
| ArduinoVersion.md | Instructions on running these projects in the Arduino IDE | Arduino |
| oled_board.txt | Board definition for the Arduino IDE | Arduino |

| Directory | Description | Environment |
| ---  | --- | --- |
| esp32c3_oled_mini | Contains variant `pin_arduino.h` | PIO/pio, Arduino |


## Board Definition and Variant `pins_arduino.h` Files in PIO/pio

This directory contains a proposed `.json` board definition manifests form the ESP32-C3 Mini with 0.42" OLED Display. The `SDA` and `SCL` pin defined in the variant `pins_arduino.h` file are GPIO 5 and GPIO 6 respectively. These correspond to the connection between the ESP32-C3 and the onboard I2C OLED display. This means that GPIO 5 and 6 can no longer be attached to the SPI controller which was the usual assignment for Super Mini ESP32C3 boards. Consequently, the following SPI pins are defined in the proposed variant definition:  `MOSI = 10`, `MISO = 9`, `SCK = 7` and `SS = 0`. 

This configuration of the files works very well in PlatformIO / pioarduino. Even when the `pioarduino-espressif32` platform is updated the sketches in this repository should still work. This is true even though an update of the platform means that the Arduino ESP32 core is updated which includes changes to the board definitions and to the variant `pins_arduino.h` files. The only foreseeable problem would be if the updated platform contained a board manifest with a name that conflicts with `esp32c3_oled_mini.json` or a variant directory with a name that conflicts with `esp32c3_oled_mini`. That must be rather unlikely.

## Arduino IDE

See [ArduinoVersion.md](ArduinoVersion.md) for summary instructions for using these definitions in the Arduino IDE.
