# Wio Tracker L1 Pro inbox, replies and storage

For pages, controls, Quick send, Outbox, favorites and drafts, see [wio-ui.md](wio-ui.md). This page covers how received messages are stored, attributed and replied to.

The feature is enabled only in `WioTrackerL1_companion_radio_ble` by `UI_RECENT_INBOX`. Other builds keep the upstream MeshCore message previews.

## Display and attribution

- `*` marks a locally unread message. `D` means direct message, `#` channel message, `S` a signed message relayed via a contact.
- New messages update the unread count without taking over the current screen. An arrival wakes the display only when no phone app is connected.
- A new arrival cancels any open delete/clear menu or reply confirmation, so a destructive action or send target can never change underneath you.
- Both the sender timestamp and the local receipt time are stored. Times outside 2000–2099 show as unknown. Sender clocks can be wrong.
- Channel name and index are captured at receipt, so later renames do not relabel history. Sender names inside channel text are part of the text and are not treated as authenticated identities.
- Signed messages show the relay/contact name plus the four-byte public sender prefix, rather than attributing the body to the relay.
- Message bodies hold up to `MAX_PACKET_PAYLOAD` bytes (184). Longer text or origin names are flagged as truncated. Unsupported UTF-8 characters are drawn as a block; the stored text is unchanged. Newlines are preserved.

## Quick replies and positions

From the inbox list or a message, press Right, choose Reply, then pick a preset (OK, NO, STATS, YES, THANKS, RECEIVED, NEED HELP), custom text, Saved position or GPS position. STATS sends the literal word, not telemetry. Every reply shows the destination and exact text before a default-No confirmation.

- **Saved position** sends the coordinates stored in node preferences, labeled `Saved position`. The unset value (0,0) is refused. These may be an older configured position.
- **GPS position** requires GPS to be on, a valid fix, and a fix no older than 30 seconds. The preview shows a snapshot of those coordinates and re-checks their age before sending. It never turns GPS on, changes privacy settings or sends an advert. There is no silent fallback from GPS to saved coordinates.

Routing rules:

- Direct replies use the original full 32-byte contact public key, never a name or short prefix.
- Channel replies go to the same channel, visible to all its members. A stored SHA-256 fingerprint of the channel key prevents sending to a slot whose key was replaced. Channel secrets are never stored in the inbox.
- Removed contacts, replaced or empty channels, and signed relay messages without a full original identity cannot be replied to.
- Replies use MeshCore's existing send functions, encryption and ACK tracking. `Reply queued` means accepted by the transport, not delivered. Delivery status is tracked in the Outbox page.

## Privacy

- Inbox snapshots contain received messages in plaintext, plus public keys and channel fingerprints. They never contain channel secrets or the private identity key.
- Nothing is sent automatically. Every outgoing message or position needs on-device confirmation.
- Deletion is logical, not a secure erase. The older snapshot slot may still hold deleted messages until it is overwritten.

## Snapshot format

`InboxPersistence.h` writes streaming snapshots with a 20-byte header (`WIB2` magic and version, generation, count, record size, CRC32) followed by fixed 265-byte records. Integers are little-endian, and compiler padding or unused string bytes are never written from RAM.

- Snapshots live in two files on the secondary QSPI LittleFS (`/ui_inbox0`, `/ui_inbox1`). The adapter never falls back to the primary identity filesystem.
- A save writes the inactive slot, closes it and reads it back before treating RAM as saved. A torn or corrupt newest snapshot falls back to the previous valid one.
- If both slots are invalid, the UI shows an error and starts with an empty inbox. It never formats the filesystem or resets identity.
- Saves run after two quiet seconds, or at most five seconds into continuous changes. `Saving...` is shown until the save finishes; a reset during that window can lose the latest changes. UI-controlled shutdown flushes pending changes.
- A failed save shows `Inbox storage error`, keeps the changes in RAM and retries after 30 seconds.
- Reads and writes are split into 128-byte chunks. Larger unaligned transfers bypass the LittleFS cache and are rejected by the nRF52 QSPI driver.

Older `WIB1` snapshots still load, but those messages have no reliable reply target and show `Reply unavailable`. Official MeshCore firmware cannot read `WIB2`. After downgrading, the local inbox may be empty or show older messages; identity and settings are stored separately and are unaffected.

## Source layout

- `ui-new/RecentMessages.h`: bounded history, read flags, stable selection and fixed-font text helpers.
- `ui-new/InboxPersistence.h`, `ui-new/WioInboxStorage.h`: snapshot format and QSPI storage adapter.
- `ui-new/UITask.{h,cpp}`: inbox screen, unread badge, navigation and wake behavior.
- `AbstractUITask.h`: message metadata callback, with a default adapter to the original preview callback.
- `MyMesh.cpp`: passes message metadata to the UI and implements reply routing. Radio, protocol, encryption and acknowledgement handling are unchanged.
- `variants/wio-tracker-l1/`: up/down button instances and the build flag.

## Tests

```sh
python3 test/test_recent_messages/run_tests.py
python3 test/test_recent_messages/run_gps_tests.py
python3 test/test_recent_messages/run_qspi_tests.py /path/to/framework-arduinoadafruitnrf52
```

The host tests compile the real screen, routing and storage code with C++11, `-Wall -Wextra -Werror`, AddressSanitizer and UndefinedBehaviorSanitizer, replacing only hardware, radio and clock adapters. The GPS runner uses the MicroNMEA library installed by a PlatformIO build. The QSPI runner checks aligned transfers against the bundled LittleFS from the given nRF52 framework path. No radio messages are sent by the tests.
