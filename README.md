# KALLISTI: MeshCore firmware for the Wio Tracker L1 Pro

> **Forked from [meshcore-dev/MeshCore](https://github.com/meshcore-dev/MeshCore).** This is an unofficial fork, not supported by the MeshCore project. Current base: MeshCore `companion-v1.17.1`.

Custom companion firmware for the **Seeed Wio Tracker L1 Pro** (nRF52840 + SX1262, SH1106 OLED, joystick, GPS). It is a fork of MeshCore `companion-v1.17.1` that adds a standalone on-device messaging UI, so the tracker is usable without a phone while staying fully compatible with MeshCore phone apps and networks.

MeshCore's radio, routing, encryption, packet processing and phone queues are unchanged. All additions are in the companion UI and its local storage.

<p align="center">
  <img src="docs/img/kallisti-splash.jpg" alt="Wio Tracker L1 Pro showing the KALLISTI startup screen" width="360">
</p>

## Features

- **Persistent inbox**: 64 saved messages plus 8 temporary channel messages, stored on QSPI flash with CRC-checked dual snapshots that survive restart and torn writes. Unread markers, next-unread navigation, delete/clear with default-No confirmation.
- **Temporary channels with auto-read**: channel traffic stays in RAM unless you choose *Keep message*; optional auto-read pages through unread channel messages when idle.
- **Replies and Quick send**: preset texts (OK, NO, YES, THANKS, RECEIVED, NEED HELP, …), custom text via a joystick editor, persistent drafts, and favorites-first contact/channel pickers. Every send shows the exact destination and text before a default-No confirmation.
- **Position sharing**: send saved coordinates or a fresh GPS fix (≤ 30 s old); stale or invalid fixes are refused, never silently substituted.
- **Outbox**: latest 16 UI-initiated sends with ACK status and manual retry.
- **GPS page**: OFF / ACQUIRING / FIX / STALE, satellites, coordinates, altitude, fix age.
- **Actions page**: Bluetooth toggle, send advert, hibernate, clean storage, join/remove channels (Public or `#name`), buzzer, auto-read, UTC clock.
- **KALLISTI splash** with version and build ID.

Details: [docs/wio-ui.md](./docs/wio-ui.md) (current UI, controls, storage) and [docs/wio-inbox.md](./docs/wio-inbox.md) (inbox, replies, snapshot format).

## Supported target

Only one build environment carries these features and is tested:

| Environment | Hardware | Notes |
|---|---|---|
| `WioTrackerL1_companion_radio_ble` | Wio Tracker L1 Pro (OLED) | Enabled via `UI_RECENT_INBOX` |

Other environments inherited from MeshCore still exist in `variants/` but are unmodified upstream builds and are not maintained here.

## Build

Requires [PlatformIO](https://docs.platformio.org).

```sh
FIRMWARE_VERSION=v1.17.1-kallisti \
bash build.sh build-firmware WioTrackerL1_companion_radio_ble
```

This produces `.pio/build/WioTrackerL1_companion_radio_ble/firmware.uf2` and a version-named copy under `out/` (`out/` is replaced on each run).

## Flash

Prebuilt UF2 files with SHA-256 checksums are on the [Releases page](https://github.com/gdamdam/kallisti-meshcore-wio-l1pro/releases). Release tags follow `kallisti-<MeshCore version>-r<revision>`; see [RELEASE.md](./RELEASE.md).

1. Back up identity, channels, contacts and settings with your MeshCore companion app first.
2. Connect over a USB data cable and double-press Reset; a UF2 drive appears.
3. Copy `firmware.uf2` to the drive and wait for the reboot.

To go back, flash the official Wio BLE UF2 from the MeshCore v1.17.1 release the same way. Inbox snapshots use a format older firmware cannot read; identity and settings are stored separately and are unaffected.

## Use

Connect with any MeshCore companion client over BLE (default PIN `123456`), or use the device on its own:

- **Left/Right**: switch pages (Messages, Recent adverts, Radio, Quick send, Outbox, GPS, Actions).
- **Center**: open / select / confirm. **Up/Down**: scroll. **Right** in a message: actions menu.
- **Menu**: back. Double-click Menu toggles the buzzer.

Full control reference: [docs/wio-ui.md](./docs/wio-ui.md).

## Tests

Host tests (C++11, ASan/UBSan) exercise the real UI, storage and routing code with fake hardware adapters:

```sh
python3 test/test_recent_messages/run_tests.py
python3 test/test_recent_messages/run_gps_tests.py
python3 test/test_recent_messages/run_qspi_tests.py /path/to/framework-arduinoadafruitnrf52
```

Host tests do not replace on-device verification (radio delivery, GPS cases, sleep/restart storage, heap headroom).

## Based on MeshCore

This project is built on [MeshCore](https://github.com/meshcore-dev/MeshCore), a lightweight C++ library for multi-hop LoRa mesh networking by Scott Powell / rippleradios.com and contributors. For MeshCore itself, other devices, clients and the protocol, see the [MeshCore documentation](https://docs.meshcore.io) and the upstream repository. Please report issues with this fork here, not upstream.

## License

MIT, as upstream MeshCore. See [license.txt](./license.txt).
