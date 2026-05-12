# Arduino IDE Version

*April 24, 2026*

---

There is a way of ***temporarily*** adding the variant `pins_arduino.h` files to the `esp32` package board definitions and of adding the corresponding board definition to the `boards.txt` configuration file. The board definition `.json` manifest is not used in the Arduino environment. Thankfully, this installation is a relatively simple two-step procedure.

1. Paste the content of the [oled_board.txt](./oled_board.txt) at the end of the `boards.txt` file found in the `~/.arduino15/packages/esp32/hardware/esp32/3.x.x/` directory.

2. Copy the variant directory `esp32c3_oled_mini` into the `~/.arduino15/packages/esp32/hardware/esp32/3.3.8/variants` directory.

The actual path to the `esp32` package depends on the operating system. Those shown above are for Linux (remember the tilde represents the home directory). The [Open the Arduino15 folder](https://support.arduino.cc/hc/en-us/articles/360018448279-Open-the-Arduino15-folder) article by the Arduino team shows in detail where the Arduino 15 directory is found in Windows, macOS and Linux.


Unfortunately, manual changes made as described above will be lost whenever the installed `esp32` boards manager by Espressif is updated in the Arduino IDE. A manual reinstall will be required after each update.

Of course, the projects will not compile in the Arduino environment as currently structured because the Arduino constraints about directory names and source code names are not respected. Rename the source file `src/main.cpp` to `src/gnats.ino`. Then rename the `src` directory to `gnats`.
