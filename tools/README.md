# tools/

Shell scripts and utilities for managing a KProx device from the command line.

## Prerequisites

All scripts require:
- `bash`
- `curl`
- `python3`
- `openssl`

Additionally:
- `get_settings.sh`, `save_settings.sh`, `get_registers.sh`, `save_registers.sh` require `jq`
- `mDNS.py` requires Python package `zeroconf` (`pip install zeroconf`)

## Configuration

All scripts source `kprox_crypto.sh` and respect these environment variables:

| Variable | Default | Description |
|---|---|---|
| `KPROX_API_KEY` | `kprox1337` | API key for authentication |
| `KPROX_API_ENDPOINT` | `kprox.local` | Device hostname or IP address |
| `KPROX_BATCH_SIZE` | `100` | Characters per batch for `kpipe.sh` |
| `KPROX_DELAY_MS` | `50` | Delay between batches in ms for `kpipe.sh` |

Example:
```bash
export KPROX_API_KEY="mysecretkey"
export KPROX_API_ENDPOINT="192.168.1.42"
```

## Scripts

### `kpipe.sh`
Reads text from stdin and sends it to the device as HID keystrokes in batches. Non-ASCII characters are stripped.
```bash
echo "Hello World" | ./kpipe.sh
cat script.txt | ./kpipe.sh
```

### `set_api_key.sh` / `update_api_key.sh`
Sets a new API key on the device. Requires the current key to be set in `KPROX_API_KEY`. The new key must be at least 8 characters.
```bash
./set_api_key.sh <old_key> <new_key>
```

### `get_settings.sh`
Fetches and pretty-prints the current device settings to stdout.
```bash
./get_settings.sh
```

### `save_settings.sh`
Downloads device settings and saves them to `settings.json` in the current directory.
```bash
./save_settings.sh
```

### `load_settings.sh`
Uploads `settings.json` from the current directory to the device.
```bash
./load_settings.sh
```

### `get_registers.sh`
Fetches and pretty-prints all registers to stdout.
```bash
./get_registers.sh
```

### `save_registers.sh`
Downloads all registers and saves them to `registers.json` in the current directory.
```bash
./save_registers.sh
```

### `load_registers.sh`
Uploads `registers.json` from the current directory to the device.
```bash
./load_registers.sh
```

### `clear_registers.sh`
Deletes all registers from the device. Prompts for confirmation before proceeding.
```bash
./clear_registers.sh
```

### `clear_settings.sh`
Wipes all device settings back to firmware defaults. Prompts for confirmation before proceeding.
```bash
./clear_settings.sh
```

### `mDNS.py`
Scans the local network for KProx devices via mDNS and prints their IP, hostname, and port.
```bash
python3 mDNS.py
```

## Reference Files

`registers.json` and `settings.json` are example files showing the expected format for `load_registers.sh` and `load_settings.sh` respectively. They can also be used as templates when creating configurations offline.
