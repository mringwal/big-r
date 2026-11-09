# BIG-R GPIO monitor (RAM only)

This diagnostic is a separate minimal ESP-IDF application, not the Matter firmware. It is built with `CONFIG_APP_BUILD_TYPE_RAM` and `CONFIG_APP_BUILD_TYPE_PURE_RAM_APP`. The runner uses the ROM loader directly to download and execute the image in RAM; it does not write flash. Resetting the MCU restores the existing firmware.

GPIO0–10 and GPIO18–19 are configured as inputs with weak pull-ups. GPIO11–17 (flash) and GPIO20–21 (UART) are excluded and left untouched. GPIO18 is temporarily an input, so the LED stream stops during the test. No monitored pin is driven as an output. A level change must persist across three 20 ms samples before it is reported. A full snapshot is printed every two seconds.

```sh
cd /Users/mringwal/Projects/esp32/big-r
. ./env.sh
cd tools/gpio-monitor
idf.py build
python run_monitor.py --port /dev/tty.usbserial-10
```

**Do not use `idf.py flash` for this test.** The runner loads RAM and preserves the existing firmware/partitions. It keeps the port open to avoid resetting the RAM program when attaching the console, and saves output in `gpio-monitor.log`. Default capture duration is 10 minutes; override with `--duration SECONDS`.

Press and release the installed animation button a few times, holding each press for about half a second. Look for matching `EVENT ... GPIO<n> 1 -> 0` and `0 -> 1` lines. If no pin responds, type `d` and Enter to try pull-downs, or `f` for floating inputs; wait for the pull-mode change to settle before testing again. Type `u` for pull-ups, `s` for a snapshot, or `q`/Ctrl-C to reset and return to the installed firmware. The runner also resets the MCU when its capture duration expires.

## Installed BIG-R result

Five button presses on 7 November 2026 produced exactly five falling and five rising edges on **GPIO10**, with no changes on other monitored pins. The installed button is active low. The migrated application now defaults to GPIO10. The monitor was stopped and the MCU reset back to its existing firmware.
