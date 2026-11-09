# lan_ota: local OTA protocol

`lan_ota_start()` loads or creates a 32-byte NVS key (`big_r_ota` / `key`; the original namespace is retained for existing devices) and starts an ESP-IDF HTTP server on port 8080. Call after NVS, networking, Matter and application initialization. `lan_ota_export_key()` prints the key only when explicitly invoked through the UART console. It is not exposed by HTTP.

- `GET /ota/status`: project/version, app ELF SHA256, running and next slot, maximum image size, first-boot/restart flags and a random 16-byte nonce encoded in lowercase hex. Each GET replaces the previous challenge, so use one uploader at a time.
- `POST /ota/upload`: raw app image with Content-Length and headers `X-OTA-Nonce`, `X-OTA-SHA256` (full file digest) and `X-OTA-Auth` (HMAC-SHA256). All binary header values are lowercase hex. Chunked transfer is not supported.

The HMAC message is exactly this ASCII sequence, including the final newline and decimal Content-Length:

```text
BIG-R-OTA-v1\n<nonce>\n<length>\n<sha256>\n
```

A challenge expires after 90 seconds and is consumed upon successful authentication even if the subsequent image fails validation. Authentication precedes flash erasure. Data is streamed into the inactive slot; only a matching file digest and successful `esp_ota_end()` allow selecting the new boot slot. The server reboots approximately one second after acknowledging acceptance.

Requests are handled serially by the HTTP server task. A rejected POST closes its connection to discard unread body bytes. Uploads are refused while validating a pending image or awaiting reboot. GET status is unauthenticated and contains no secret. The protocol does not provide confidentiality or authenticate status responses; the laptop's confirmation is operational feedback on a trusted LAN, not remote attestation.

First-boot validation requires successful application startup, 30 seconds of uptime, Wi-Fi association and IPv4. A failed check after 90 seconds rolls back. ESP-IDF's bootloader handles resets/crashes before validation. See the project README for installation and recovery.
