Unofficial KALLISTI companion firmware for the **Seeed Wio Tracker L1 Pro**, based on MeshCore. Not supported by the MeshCore project.

## What changed

<!-- Fill in before publishing the draft. -->

## Before flashing: back up

Back up identity, channels, contacts and settings with your MeshCore companion app.

## Flash

1. Download the `.uf2` file below and check it against `SHA256SUMS.txt`.
2. Connect the tracker over a USB data cable and double-press Reset. A UF2 drive appears.
3. Copy the `.uf2` file to the drive and wait for the reboot.

## Roll back

Flash the official Wio Tracker L1 BLE companion UF2 from the matching [MeshCore release](https://github.com/meshcore-dev/MeshCore/releases) the same way.
Inbox snapshots use a format the official firmware cannot read. Identity and settings are stored separately and are unaffected.
