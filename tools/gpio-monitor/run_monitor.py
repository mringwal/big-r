#!/usr/bin/env python3
"""Load the monitor into RAM via ROM, retain serial connection, log GPIO events."""
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
log_path = root / 'gpio-monitor.log'
with detect_chip(args.port, baud=115200) as esp:
    if esp.CHIP_NAME != 'ESP32-C3':
        raise SystemExit('This monitor is for ESP32-C3 only.')
    # Keep ROM transfers small over the installed USB/UART bridge.
    esp.ESP_RAM_BLOCK = 0x400
    try:
        load_ram(esp, str(root / 'build/big_r_gpio_monitor.bin'))
    except BaseException:
        esp.hard_reset()
        raise
    port = esp._port
    port.timeout = 0.2
    print(f'Capturing for up to {args.duration:g}s to {log_path}', flush=True)
    print('Type u/d/f/s to send a command; q or Ctrl-C resets to the original firmware.', flush=True)
    end = time.monotonic() + args.duration
    try:
        with log_path.open('a', buffering=1) as log:
            log.write('\n--- RAM monitor session ---\n')
            while time.monotonic() < end:
                data = port.read(port.in_waiting or 1)
                if data:
                    line = data.decode('utf-8', errors='replace')
                    sys.stdout.write(line)
                    sys.stdout.flush()
                    log.write(line)
                if select.select([sys.stdin], [], [], 0)[0]:
                    cmd = sys.stdin.readline().strip()
                    if cmd == 'q':
                        break
                    if cmd in ('u', 'd', 'f', 's'):
                        port.write(cmd.encode('ascii'))
    except KeyboardInterrupt:
        pass
    finally:
        print('\nResetting back to the existing flash firmware.', flush=True)
        esp.hard_reset()
