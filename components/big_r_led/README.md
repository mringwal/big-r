# BIG-R LED component

Copy this directory into an ESP-IDF application's `components/`. It depends on ESP-IDF 6.0.2 and the managed `espressif/led_strip` 3.x component; it has no Matter dependency.

`big_r_led.h` exposes the renderer API. Initialize once with `big_r_led_init()` and a data GPIO; retain the returned handle for power, brightness (0–100), hue (0–360), saturation (0–100), temperature (Kelvin), and CIE XY (Matter uint16 coordinates) setters. Color setters select solid mode and enable output. `big_r_led_next_animation()` cycles the four original patterns and off and returns the new power state.

All setters run in task context and protect render state with a mutex. A separate task owns the framebuffer, LED buffer, and SPI2 DMA transfers. This implementation is a singleton for the installed letter R; initialization is for the application's lifetime. Configure physical LED count (minimum 573), data GPIO, and frame delay through Kconfig. The component permanently owns SPI2. GPIO debounce, Matter attributes, commissioning and factory reset belong to the application.

`big_r_patterns.h` preserves the CC0 original geometry and pattern tables. `color_format.c` and the matching headers are derived from Espressif's Apache-2.0 device HAL and retain their license notices.
