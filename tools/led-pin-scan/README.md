# BIG-R LED data pin scanner (RAM only)

Build with the BIG-R ESP-IDF environment and run `python run_scan.py --port /dev/cu.usbserial-10`. The host uses 256-byte ROM download blocks and retains the UART connection. No flash is written. The runner selects the first **50 of 573 LEDs at 10% white**; the remaining LEDs are sent black.

Candidate GPIOs are 0–9, 18 and 19. Flash pins 11–17, console UART pins 20–21, and the verified button GPIO10 are excluded. RMT generates WS2812 GRB data with Wi-Fi disabled. Low drive strength is used. The RMT driver retains ownership of the GPIO while a candidate is being tested; its output idles low between frames. The pin is released to an input with a weak pull-down after its off frame and channel deletion. A routing-register check verifies that every frame still uses an RMT peripheral signal. Each pin receives an off frame, an on frame held for four seconds, and an off frame before moving on; the scan repeats.

**When the external strip lights, press and hold the installed animation button briefly while it is lit.** The scanner locks on that pin and prints `FOUND/LOCKED GPIO<n>`. The selected LEDs remain lit until `g` resumes or `q` stops the test. The runner accepts those commands followed by Enter. It clears the strip and resets to the flashed firmware when stopped or after ten minutes.

The device starts by waiting for commands: `1` selects one LED, `5` selects fifty, `a` selects the whole strip (external LED power), and `g` starts. The supplied runner automatically selects `5` then `g`, matching the user's requested USB-powered diagnostic. Output is saved in `led-pin-scan.log`. Do not flash this RAM diagnostic with `idf.py flash`.

The initial scanner mistakenly called `gpio_set_direction(OUTPUT)` before transmitting, which reset the GPIO matrix to ordinary GPIO output and disconnected RMT. Those initial rounds were invalid. The corrected version leaves RMT routing intact and validates the output signal ID before every frame. The user requested that this corrected test continue on USB power only, with mains off.
