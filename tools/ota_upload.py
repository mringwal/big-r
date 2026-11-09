#!/usr/bin/env python3
"""Authenticated BIG-R LAN firmware upload; Python standard library only."""
import argparse
import hashlib
import hmac
import http.client
import json
from pathlib import Path
import struct
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[1]

class UploadError(Exception):
    pass

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None

OPENER = urllib.request.build_opener(urllib.request.ProxyHandler({}), NoRedirect())

def image_info(prefix):
    if len(prefix) < 288 or prefix[0] != 0xE9 or struct.unpack_from('<H', prefix, 12)[0] != 5:
        raise UploadError('Expected a standalone ESP32-C3 app .bin, not an ELF or merged flash image')
    if struct.unpack_from('<I', prefix, 32)[0] != 0xABCD5432:
        raise UploadError('Missing ESP-IDF application descriptor')
    def string(offset):
        data = prefix[offset:offset+32]
        if b'\0' not in data:
            raise UploadError('Invalid application descriptor string')
        return data.split(b'\0',1)[0].decode('utf-8')
    if string(80) != 'big_r':
        raise UploadError('Image project is not big_r')
    return {'version':string(48), 'elf_sha256':prefix[176:208].hex()}

def get_status(base):
    with OPENER.open(base+'/ota/status', timeout=5) as reply:
        result = json.loads(reply.read(4096))
    if result.get('project') != 'big_r':
        raise UploadError('The address did not return a BIG-R OTA status response')
    return result

def signature(key, nonce, size, digest):
    message = f'BIG-R-OTA-v1\n{nonce}\n{size}\n{digest}\n'.encode('ascii')
    return hmac.new(key, message, hashlib.sha256).hexdigest()

def wait_for_image(base, expected, slot, timeout, previous_elf):
    end = time.monotonic()+timeout
    seen_new = False
    last = None
    while time.monotonic() < end:
        try:
            last = get_status(base)
            match = last.get('elf_sha256') == expected['elf_sha256'] and last.get('slot') == slot
            if match:
                seen_new = True
                if not last.get('rollback_pending') and not last.get('restarting'):
                    return last
            elif seen_new and last.get('elf_sha256') == previous_elf:
                raise UploadError('New firmware rolled back; the previous build is running')
        except (OSError, urllib.error.URLError, json.JSONDecodeError, http.client.HTTPException):
            pass  # The device closes its sockets during reboot.
        time.sleep(2)
    detail = f" Last response: version={last.get('version')}, slot={last.get('slot')}." if last else ''
    raise UploadError('Could not confirm the new image and first-boot health check.'+detail)

def upload(base, path, key_path, force=False, wait=120):
    key = bytes.fromhex(key_path.read_text().strip())
    if len(key) != 32:
        raise UploadError('OTA key must contain exactly 32 bytes in hexadecimal')
    with path.open('rb') as firmware:
        info = image_info(firmware.read(288))
        firmware.seek(0)
        sha = hashlib.sha256()
        size = 0
        while chunk := firmware.read(65536):
            sha.update(chunk)
            size += len(chunk)
        digest = sha.hexdigest()
        status = get_status(base)
        if status.get('rollback_pending') or status.get('restarting'):
            raise UploadError('Device is restarting or validating its previous update; wait and retry')
        if not force and status.get('elf_sha256') == info['elf_sha256']:
            print(f"Already running this exact build ({status['version']}, {status['slot']}).")
            return status
        if size > status.get('max_image_size',0):
            raise UploadError('Image exceeds the OTA slot, or device needs the two-slot USB installation')
        target_slot = status.get('next_slot')
        if target_slot not in ('ota_0','ota_1'):
            raise UploadError('Device did not advertise a valid inactive OTA slot')
        print(f"Uploading {info['version']}: {size:,} bytes, {status.get('slot')} -> {target_slot}",flush=True)
        url = urllib.parse.urlsplit(base)
        connection_type = http.client.HTTPSConnection if url.scheme == 'https' else http.client.HTTPConnection
        conn = connection_type(url.hostname, url.port, timeout=30)
        try:
            conn.putrequest('POST',url.path.rstrip('/')+'/ota/upload')
            for name,value in {
                'Content-Type':'application/octet-stream', 'Content-Length':str(size),
                'X-OTA-Nonce':status['nonce'], 'X-OTA-SHA256':digest,
                'X-OTA-Auth':signature(key,status['nonce'],size,digest), 'Connection':'close',
            }.items(): conn.putheader(name,value)
            conn.endheaders()
            firmware.seek(0)
            sent = 0
            while sent < size:
                chunk = firmware.read(min(65536,size-sent))
                if not chunk: raise UploadError('Firmware file changed while reading')
                conn.send(chunk)
                sent += len(chunk)
            reply = conn.getresponse()
            body = reply.read(4096).decode('utf-8',errors='replace')
            if reply.status != 200:
                raise UploadError(f'Upload rejected: HTTP {reply.status}: {body}')
            if not json.loads(body).get('accepted'):
                raise UploadError('Unexpected OTA response: '+body)
        finally:
            conn.close()
        print('Image verified by device. Waiting for reboot and first-boot validation...',flush=True)
        result = wait_for_image(base,info,target_slot,wait,status.get('elf_sha256'))
        print(f"Update confirmed: {result['version']} in {result['slot']}; rollback check passed.")
        return result

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('address', help='Device base URL, e.g. http://192.168.3.14:8080')
    p.add_argument('image', nargs='?', type=Path, default=ROOT/'build-matter/big_r.bin')
    p.add_argument('--key',type=Path,default=ROOT/'private/ota-key.hex')
    p.add_argument('--status',action='store_true')
    p.add_argument('--force',action='store_true',help='Upload even if the identical build is already running')
    p.add_argument('--wait',type=float,default=120,help='Post-update verification timeout in seconds')
    a = p.parse_args()
    base = a.address.rstrip('/')
    url = urllib.parse.urlsplit(base)
    if url.scheme not in ('http','https') or not url.hostname or url.username or url.password or url.query or url.fragment:
        p.error('Use an http(s) base URL without credentials, query or fragment')
    try:
        if a.status:
            status = get_status(base)
            status.pop('nonce',None)
            print(json.dumps(status,indent=2))
        else:
            upload(base,a.image,a.key,a.force,a.wait)
    except (OSError, ValueError, UploadError, urllib.error.URLError, http.client.HTTPException) as error:
        print(f'OTA failed: {error}',file=sys.stderr)
        return 1
    return 0

if __name__ == '__main__':
    sys.exit(main())
