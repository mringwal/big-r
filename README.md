# BIG-R Matter RGB light

BIG-R is single large light-box in the shape of the letter 'R'. It contains a power supply, and ESP32-C3 and an WS2812 LED strip with 573 LEDs, of which 454 are lit.

The ESP32-C3 firmware is based on Espressif's official ESP-Matter extended-color light example. The `big_r_led` ESP-IDF component implements BIG-R geometry and four animation tables. A 32×32 framebuffer maps the 454 lit LED positions onto the WS2812 strip. Gaps sta dark.

## Build

Use **ESP-Matter release/v1.6.1 with ESP-IDF v6.0.2**, the pairing recommended by the [release README](https://github.com/espressif/esp-matter/blob/release/v1.6.1/README.md). Exact revisions are recorded in `sdk-versions.txt`; managed dependencies are pinned in `dependencies.lock`. The shared `../esp-idf` checkout and its local changes are preserved; this firmware uses `../esp-idf-v6.0.2`.

```sh
cd /Users/mringwal/Projects/esp32/big-r
. ./env.sh
idf.py -B build-matter build
```

`env.sh` uses the installed IDF 6.0 Python 3.13 environment. Set `BIG_R_PYTHON_ENV_PATH` before sourcing it to use another environment installed for IDF 6.0. New machines must install IDF's ESP32-C3 tools, check out the recorded SDK revisions and their ESP32 submodules, and run ESP-Matter's `install.sh --no-host-tool` in the IDF environment.

The default build is version **2.1.0**, Matter software version **3**. For a subsequent release, increase both values:

```sh
idf.py -B build-matter -DBIG_R_VERSION=2.1.1 -DBIG_R_VERSION_NUMBER=4 build
```

These CMake cache values persist in that build directory. The uploader identifies builds by their ELF SHA256, so changed code can also be tested without changing the version string.

## Manual firmware upload over the LAN

BIG-R runs an authenticated HTTP upload service on TCP port **8080** whenever its normal firmware is running. The laptop sends the application directly to BIG-R over its existing Wi-Fi connection. No webserver, Home Assistant integration, Matter OTA provider, or Internet access is needed.


### One-time USB installation

The confirmed hardware has **4 MB flash**. The new default `partitions.csv` provides two 1,966,080-byte (1.875 MiB) application slots, `ota_0` and `ota_1`, plus OTA metadata. This changes the old single-application partition layout and requires one USB flash.

1. Connect USB using the same power arrangement used successfully during earlier flashing. Close other serial monitors. Replace `/dev/cu.YOUR_UART` below with the current port (previously `/dev/tty.usbserial-10`).
2. Take a **fresh full-flash backup** before changing the partition table. The November 7 backup predates the successful Home Assistant pairing and does not contain the current fabric credentials.

   ```sh
   cd /Users/mringwal/Projects/esp32/big-r
   . ./env.sh
   mkdir -p private
   chmod 700 private
   umask 077
   python -m esptool --chip esp32c3 --port /dev/cu.YOUR_UART read-flash 0 0x400000 private/pre-ota-flash.bin
   ```

3. Build and install the new firmware:

   ```sh
   idf.py -B build-matter build
   idf.py -B build-matter -p /dev/cu.YOUR_UART flash
   ```

   **Do not run `erase-flash` or factory reset.** The main NVS partition and its key partition retain their original addresses and sizes, so normal flashing preserves Wi-Fi and Matter pairing data. This migration also relocates the unused example factory-data partition; it is intended for this device's example attestation configuration, not devices provisioned with custom factory data.

4. Retrieve the device's upload key once through the physical UART:

   ```sh
   python tools/ota_provision.py --port /dev/cu.YOUR_UART
   ```

   The firmware generates a random per-device key and stores it in NVS. The helper uses the console command `matter esp ota-key` and saves the key to `private/ota-key.hex` with mode 0600 without printing it. `private/` is ignored by version control. Keep this file for future uploads and back it up securely. The IDF Python environment supplies the required `pyserial` package. If the helper times out, check that the new firmware is installed, the port is correct, and no monitor holds the port.

5. Disconnect USB and restore the normal power arrangement. Find BIG-R's IPv4 address in your router's client list, Home Assistant device diagnostics, or the UART Wi-Fi log. A DHCP reservation makes the address stable. No `big-r.local` hostname is configured by this implementation.

### Everyday updates

From the project directory, build and upload:

```sh
. ./env.sh
idf.py -B build-matter build
python tools/ota_upload.py http://192.168.1.123:8080
```

Replace the example IP with BIG-R's address. The default image is `build-matter/big_r.bin`, and the default key is `private/ota-key.hex`. The uploader itself needs only Python 3.8 or newer and its standard library.

An explicit image/key and a status query are also supported:

```sh
python tools/ota_upload.py http://192.168.1.123:8080 build-matter/big_r.bin --key private/ota-key.hex
python tools/ota_upload.py http://192.168.1.123:8080 --status
```

Upload only the standalone **application `.bin`**. Do not upload an ELF, bootloader, partition table, or merged flash image. Normal LAN updates do not change the partition table or bootloader. Changes requiring either still need USB.

The helper checks the image target/project, uploads it, then waits up to 120 seconds for the expected ELF hash and slot to boot and pass the health check. Identical builds are skipped. Use `--force` to exercise an update of the same build into the other slot, or `--wait 180` to allow a longer confirmation timeout.

### Authentication and recovery

Each upload uses a single-use, 90-second challenge and HMAC-SHA256 authentication covering the image size and SHA256 digest. The secret key is never sent over the LAN or embedded in the downloadable firmware. The server verifies the streamed image hash and ESP-IDF's native image validation before changing the boot slot. Invalid, interrupted, or corrupt uploads leave the running image selected.

The transfer uses **plain HTTP**: firmware and status are visible to the local network. Use it on your trusted LAN and do not forward port 8080 from the Internet. HMAC authenticates updates; it does not encrypt them. Anyone holding the key can install firmware. If the key file is lost, retrieve it again over USB with `ota_provision.py`. Erasing NVS, or a factory reset that clears its OTA namespace, requires retrieving the newly generated key again.

ESP-IDF rollback is enabled. After an OTA update, the new image must stay running for at least 30 seconds and have an associated Wi-Fi connection with an IPv4 address; failure to meet the check within 90 seconds triggers rollback. LED, Matter, button and upload-service initialization must also have succeeded. A crash or reset before validation causes the bootloader to select the previous valid slot. This check does not verify visible LED output or Matter controller reachability.

The first USB installation has no previous OTA image to fall back to. Automatic rollback becomes useful after a subsequent successful LAN update populates the second slot. Keep the USB recovery path and backup available.

If the helper loses contact, run `--status` after the device returns. Its version, ELF hash, active slot and `rollback_pending` flag show the outcome. An upload is not reported as confirmed merely because its HTTP transfer completed. USB flashing remains available if the device cannot join Wi-Fi or both images are unusable.

The implementation is the `components/lan_ota` component; laptop helpers are in `tools/`. This service is independent of Matter OTA, whose requestor remains disabled.

## Controls

- Short button press: first animation → second → third → fourth → all LEDs off → first animation. Pressing from a solid color starts the first animation.
- A Matter hue/saturation, XY, or color-temperature attribute update selects a solid color and turns the light on, including when leaving the local off mode. ColorControl defaults to ExecuteIfOff so a color request can leave off mode. Explicit command options that suppress execution while off are respected. Repeating the stored color also exits an animation.
- Matter brightness adjusts the current solid color or animation. Zero brightness is supported.
- Matter On/Off gates the current animation/color. Turning on from the button's off mode restores the last solid color.
- Button changes update the Matter On/Off attribute. There is no long-press factory reset; the console's factory-reset command remains available.

## Wiring

Run `idf.py -B build-matter menuconfig` to adjust **BIG-R LEDs** and **BIG-R application**.

| Setting | Default | Source |
| --- | --- | --- |
| Target | ESP32-C3 revision v0.3 or newer | Confirmed hardware |
| Flash | 4 MB | Confirmed by esptool |
| WS2812 data | GPIO0 | Verified with the RAM scanner and visible LED response |
| Physical LEDs | 573 | Highest mapped index is 572 |
| Animation button | GPIO10, active low, internal pull-up | Verified with five button presses using RAM GPIO monitor |
| Frame delay | 20 ms after each transfer | Plus approximately 17 ms for the strip transfer |

The LED component owns SPI2 and uses DMA to transmit WS2812 data on MOSI, without connecting a clock pin. SPI2 cannot be shared with another peripheral. The console uses UART0 through the board's CP2102 USB/UART bridge. Secondary native USB output is disabled.

## Verification

The OTA-capable ESP32-C3 firmware builds with ESP-IDF v6.0.2 and ESP-Matter release/v1.6.1. The current image is 1,662,208 bytes, leaving approximately 15% free in either application slot. The previous 2.0.0 firmware's LEDs, animations and Home Assistant pairing were confirmed on the installed hardware. Version 2.1.0 was installed over USB on 8 November 2026. Esptool verified the writes; UART confirms both saved Matter fabrics, Wi-Fi at `192.168.3.14`, and the OTA service on port 8080. The per-device key was retrieved into `private/ota-key.hex` (mode 0600). A fresh pre-migration backup is `private/pre-ota-flash-20261108.bin`, with an adjacent SHA256 file. The laptop could not reach the device over the LAN during this session, so an actual LAN update remains to be tested. Installation captures are under `logs/ota-*-20261108.log`.

```sh
python3 tests/run_host_tests.py
python3 tests/run_ota_tests.py
```

Renderer tests compile the real component with AddressSanitizer/UndefinedBehaviorSanitizer and exercise geometry, four original patterns, button/off cycle, Matter solid-color requests, power, zero brightness and XY dimming.

OTA tests compile the real server handlers with a fake flash/SDK backend and AddressSanitizer/UndefinedBehaviorSanitizer. They cover authentication, expired/replayed requests, fragmented reads, interrupted uploads, wrong project/chip, size limits, corrupt payloads, SDK validation failure, key persistence and first-boot health/rollback. The Python helper is exercised against a loopback HTTP device simulator, including transfer, confirmation, invalid keys and rollback detection. These tests require a C compiler and OpenSSL development headers (`brew install openssl@3` on Apple Silicon macOS; alternatively `OPENSSL_PREFIX` or `pkg-config`). They do not replace hardware power-loss, Wi-Fi and flash testing.

## Home Assistant commissioning and device names

This firmware uses the SDK example DAC/PAI and test VID/PID (0xFFF1/0x8000). Home Assistant Matter Server's **Enable test-net DCL usage** setting must be enabled, followed by a Matter Server restart; see its [configuration documentation](https://github.com/home-assistant/apps/blob/master/matter_server/DOCS.md). Pairing succeeded after enabling this setting.

Node label, Matter product name and DNS-SD commissioning device name are `BIG-R`. BLE uses the `BIG-R-` prefix with the discriminator suffix. Product and discovery strings are in `main/CHIPProjectConfig.h`; the node label is set in `main/app_main.cpp`. Apple's initial pairing sheet may still say “Accessory”. The setup code is **34970112332**; the QR code is `matter-setup.png`.

