#!/usr/bin/env python3
"""Run the RAM-only LED pin scanner, first 50 LEDs at 10% white."""
import argparse
from pathlib import Path
import select
import sys
import time
from esptool.cmds import detect_chip, load_ram

parser = argparse.ArgumentParser()
parser.add_argument('--port', default='/dev/tty.usbserial-10')
parser.add_argument('--duration', type=float, default=600)
args = parser.parse_args()
root = Path(__file__).resolve().parent
log_path = root / 'led-pin-scan.log'
with detect_chip(args.port, baud=115200) as esp:
    if esp.CHIP_NAME != 'ESP32-C3':
        raise SystemExit('This monitor is for ESP32-C3 only.')
    # Keep ROM transfers small over the installed USB/UART bridge.
    esp.ESP_RAM_BLOCK = 0x100
    try:
        load_ram(esp, str(root / 'build/big_r_led_pin_scan.bin'))
    except BaseException:
        esp.hard_reset()
        raise
    port = esp._port
    port.timeout = 0.2
    print(f'Capturing for up to {args.duration:g}s to {log_path}', flush=True)
    print('Press the installed button WHILE LIT to lock the pin. g resumes; q clears LEDs and resets.', flush=True)
    end = time.monotonic() + args.duration
    startup = ''
    started = False
    try:
        with log_path.open('a', buffering=1) as log:
            log.write('\n--- RAM LED scanner v2 (RMT route verified), first 50 at 10% white ---\n')
            while time.monotonic() < end:
                data = port.read(port.in_waiting or 1)
                if data:
                    line = data.decode('utf-8', errors='replace')
                    sys.stdout.write(line)
                    sys.stdout.flush()
                    log.write(line)
                    if not started:
                        startup = (startup + line)[-4096:]
                        if 'WAITING:' in startup:
                            port.write(b'5g')
                            started = True
                if select.select([sys.stdin], [], [], 0)[0]:
                    cmd = sys.stdin.readline().strip()
                    if cmd == 'q':
                        break
                    if cmd in ('g', 'q'):
                        port.write(cmd.encode('ascii'))
    except KeyboardInterrupt:
        pass
    finally:
        port.write(b'q')
        port.flush()
        time.sleep(0.5)
        print('\nResetting back to the flashed firmware.', flush=True)
        esp.hard_reset()
