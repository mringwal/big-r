# BIG-R animation simulator

Build with a C compiler and SDL2 (`sdl2-config` must be on PATH):

```sh
make -C tools/animation-simulator
./tools/animation-simulator/simulator
```

The simulator compiles the same `big_r_animation.c`, color conversion and pattern tables as the firmware. It starts with the mapped view at the firmware's default 25% brightness and 20 ms frame interval. The mapped view shows matrix cells used by LEDs, not their exact physical positions: several LEDs share a cell and the real spacing is not represented. The field view shows all 32 × 32 cells.

- **1–4**: select a rainbow animation
- **5**: select Fire
- **6**: select Rain/Snow
- **M**: switch between mapped cells and the full field
- **+ / −**: adjust brightness
- **Space**: pause or resume
- **Q / Escape**: quit

Add animation rendering to the shared component to experiment on both the simulator and firmware. New selectable modes also need to be registered in the firmware API and simulator controls. The simulator models colors and frame timing, not LED optics or SPI transfer time.
