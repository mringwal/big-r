#!/usr/bin/env python3
"""Run OTA server sanitizer tests and laptop upload integration tests.

Requires cc and OpenSSL development headers (brew install openssl@3 on macOS).
"""
from pathlib import Path
import os
import shlex
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
prefix = Path(os.environ.get('OPENSSL_PREFIX','/opt/homebrew/opt/openssl@3'))
if prefix.is_dir():
    flags = ['-I'+str(prefix/'include'), '-L'+str(prefix/'lib'),
             '-Wl,-rpath,'+str(prefix/'lib'), '-lcrypto']
else:
    flags = shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','openssl'],text=True))
with tempfile.TemporaryDirectory(prefix='big-r-ota-test-') as temporary:
    binary = str(Path(temporary)/'test_ota_server')
    subprocess.run(['cc','-std=c11','-fsanitize=address,undefined','-g',
                    '-I'+str(root/'tests/ota_stubs'),
                    '-I'+str(root/'components/lan_ota/include'),
                    str(root/'tests/test_ota_server.c'),*flags,'-o',binary],check=True)
    subprocess.run([binary],check=True)
subprocess.run([sys.executable,str(root/'tests/test_ota_upload.py')],check=True)
