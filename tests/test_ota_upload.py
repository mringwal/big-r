#!/usr/bin/env python3
"""Exercise the real laptop uploader against a local HTTP device simulator."""
import hashlib
import hmac
import http.server
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import threading
import unittest
from unittest.mock import patch

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('ota_upload', root/'tools/ota_upload.py')
ota = importlib.util.module_from_spec(spec)
spec.loader.exec_module(ota)
KEY = bytes(range(32))

class Device(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args): pass
    def reply(self, status, data):
        body = json.dumps(data).encode()
        self.send_response(status)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)
    def do_GET(self):
        self.reply(200, self.server.device)
    def do_POST(self):
        size = int(self.headers['Content-Length'])
        image = self.rfile.read(size)
        nonce = self.headers['X-OTA-Nonce']
        digest = hashlib.sha256(image).hexdigest()
        message = f'BIG-R-OTA-v1\n{nonce}\n{size}\n{digest}\n'.encode()
        expected = hmac.new(KEY, message, hashlib.sha256).hexdigest()
        if (nonce != self.server.device['nonce'] or digest != self.headers['X-OTA-SHA256'] or
            not hmac.compare_digest(expected, self.headers['X-OTA-Auth'])):
            self.reply(403, {'error':'authentication'})
            return
        self.server.received = image
        self.server.device.update(ota.image_info(image))
        self.server.device.update(slot='ota_1', next_slot='ota_0', nonce='22'*16)
        self.reply(200, {'accepted':True})

class UploadTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.path = Path(self.tmp.name)/'app.bin'
        self.key = Path(self.tmp.name)/'key'
        self.key.write_text(KEY.hex())
        image = bytearray(150000)
        image[0] = 0xe9
        struct.pack_into('<H', image, 12, 5)
        struct.pack_into('<I', image, 32, 0xabcd5432)
        image[48:53] = b'2.1.0'
        image[80:85] = b'big_r'
        image[176:208] = bytes(range(32))
        self.path.write_bytes(image)
        self.server = http.server.ThreadingHTTPServer(('127.0.0.1',0), Device)
        self.server.device = dict(project='big_r', version='old', elf_sha256='ff'*32,
                                  slot='ota_0', next_slot='ota_1', max_image_size=0x1e0000,
                                  nonce='11'*16, rollback_pending=False, restarting=False)
        self.server.received = None
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.base = f'http://127.0.0.1:{self.server.server_port}'
    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()
        self.tmp.cleanup()
    def test_upload_and_confirmation(self):
        result = ota.upload(self.base,self.path,self.key)
        self.assertEqual(self.server.received,self.path.read_bytes())
        self.assertEqual(result['slot'],'ota_1')
        self.server.received = None
        ota.upload(self.base,self.path,self.key)
        self.assertIsNone(self.server.received)  # exact build skipped
    def test_wrong_key(self):
        self.key.write_text('ff'*32)
        with self.assertRaisesRegex(ota.UploadError,'403'):
            ota.upload(self.base,self.path,self.key)
        self.assertIsNone(self.server.received)
    def test_image_and_capacity(self):
        self.path.write_bytes(b'not firmware')
        with self.assertRaises(ota.UploadError): ota.upload(self.base,self.path,self.key)
    def test_pending(self):
        self.server.device['rollback_pending'] = True
        with self.assertRaisesRegex(ota.UploadError,'validating'):
            ota.upload(self.base,self.path,self.key)
    def test_oversize(self):
        self.server.device['max_image_size'] = 100
        with self.assertRaisesRegex(ota.UploadError,'exceeds'):
            ota.upload(self.base,self.path,self.key)
    def test_rollback(self):
        new = dict(elf_sha256='new',slot='ota_1',rollback_pending=True)
        old = dict(elf_sha256='old',slot='ota_0')
        with patch.object(ota,'get_status',side_effect=[new,old]), patch.object(ota.time,'sleep'):
            with self.assertRaisesRegex(ota.UploadError,'rolled back'):
                ota.wait_for_image(self.base,{'elf_sha256':'new'},'ota_1',120,'old')

if __name__ == '__main__': unittest.main()
