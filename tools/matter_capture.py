#!/usr/bin/env python3
"""Capture Matter UART logs and optionally request the device's console factory reset."""
import argparse
from pathlib import Path
import select
import sys
import time
import serial

p=argparse.ArgumentParser()
p.add_argument('--port', required=True)
p.add_argument('--log', required=True)
p.add_argument('--duration', type=float, default=1200)
p.add_argument('--factory-reset', action='store_true')
p.add_argument('--interactive', action='store_true', help='accept console commands on stdin')
a=p.parse_args()
path=Path(a.log)
path.parent.mkdir(parents=True, exist_ok=True)
s=serial.Serial(port=None, baudrate=115200, timeout=0.2)
s.dtr=False
s.rts=False
s.port=a.port
s.open()
start=time.monotonic()
sent=not a.factory_reset
stdin_open=True
with path.open('a', buffering=1) as log:
    path.chmod(0o600)
    log.write('\n--- Matter diagnostic capture ---\n')
    print(f'Capturing {a.port} to {path}. Ctrl-C stops capture.',flush=True)
    try:
        while time.monotonic()-start<a.duration:
            data=s.read(s.in_waiting or 1)
            if data:
                text=data.decode('utf-8', errors='replace')
                log.write(text)
                print(text,end='',flush=True)
            if not sent and time.monotonic()-start>=6:
                command='matter esp factoryreset'
                log.write(f'\n[HOST sending: {command}]\n')
                print(f'\n[HOST sending: {command}]',flush=True)
                s.write((command+'\r\n').encode())
                s.flush()
                sent=True
            if a.interactive and stdin_open and select.select([sys.stdin],[],[],0)[0]:
                cmd=sys.stdin.readline()
                if not cmd:
                    stdin_open=False
                elif cmd.strip()=='q':
                    break
                else:
                    s.write((cmd.strip()+'\r\n').encode())
                    s.flush()
    except KeyboardInterrupt:
        pass
    finally:
        s.close()
        print('\nCapture closed.',flush=True)
