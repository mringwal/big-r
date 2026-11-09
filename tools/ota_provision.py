#!/usr/bin/env python3
"""Retrieve BIG-R's per-device OTA key over its physical UART console once."""
import argparse
import os
from pathlib import Path
import re
import tempfile
import time
import serial

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--port', required=True)
p.add_argument('--key',type=Path,default=ROOT/'private/ota-key.hex')
a=p.parse_args()
s=serial.Serial(port=None,baudrate=115200,timeout=0.2)
s.dtr=False
s.rts=False
s.port=a.port
s.open()
try:
    end=time.monotonic()+35
    next_command=time.monotonic()+6
    buffer=b''
    while time.monotonic()<end:
        buffer=(buffer+s.read(s.in_waiting or 1))[-16384:]
        match=re.search(rb'BIG_R_OTA_KEY=([0-9a-f]{64})',buffer)
        if match:
            a.key.parent.mkdir(parents=True,exist_ok=True,mode=0o700)
            with tempfile.NamedTemporaryFile(dir=a.key.parent,delete=False,mode='wb') as f:
                temporary=Path(f.name)
                os.chmod(temporary,0o600)
                f.write(match.group(1)+b'\n')
            os.replace(temporary,a.key)
            print(f'OTA key stored at {a.key} (not printed). Keep this file for future uploads.')
            break
        if time.monotonic()>=next_command:
            s.write(b'\r\nmatter esp ota-key\r\n')
            s.flush()
            next_command=time.monotonic()+3
    else:
        raise SystemExit('No OTA key received. Install the OTA-capable firmware first and close other serial monitors.')
finally:
    s.close()
