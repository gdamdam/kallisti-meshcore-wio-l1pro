# Wio Tracker L1 Pro companion UI

The on-device UI is enabled only for `WioTrackerL1_companion_radio_ble` through `UI_RECENT_INBOX`. MeshCore's radio, encryption, packet processing and phone queues are unchanged, and no new dependency is added. Inbox storage, reply routing and the snapshot format are described in [wio-inbox.md](wio-inbox.md).

## Pages and controls

Left/right moves between Messages, Recent adverts, Radio, Quick send, Outbox, GPS and Actions (plus Sensors on configurations that enable it).

- **Messages:** center opens the inbox. Down opens the newest unread message. In message detail, center jumps to the next unread message; Right opens the actions menu, which also has Next unread. The existing read/delete/clear controls remain.
- **Actions:** up/down chooses Bluetooth, Send advert, Hibernate, Clean storage, Channels, Buzzer, Auto-read or Show clock. Center toggles Bluetooth. Advert and Hibernate have a default-No confirmation; shutdown waits for button release. Left/right leaves this page and cancels a pending confirmation.
- **Quick send:** center opens a recipient picker. Choose Contacts or Channels, browse with up/down and center to select. Four cached rows per page keep the displayed identities stable while contacts change. Right toggles a favorite for the highlighted target; the list then reloads with favorites first. A `*` marks favorites. This workflow does not require an incoming message.
- **Outbox:** center opens the latest 16 UI-initiated sends, newest first. Center reads the full recipient label, timestamp, status and text; up/down scrolls. Right from detail opens a manual retry/resend preview. Left/Menu returns.
- **GPS:** shows on/off, fix status, satellites, coordinates with compass directions, altitude and fix age, without overlapping navigation dots; center toggles GPS. Down opens Share position with a recipient picker and the two position choices.

All sends show the destination and complete outgoing text before a default-No confirmation. Left selects No; Right selects Yes; center confirms. Menu backs out. Up/down does not change send confirmation. Incoming messages cancel a send confirmation without transmitting it. Custom drafts and the frozen reply identity remain available; an evicted incoming history entry cannot redirect its draft to another recipient.

## Presets and custom text

Presets: OK, NO, STATS, YES, THANKS, RECEIVED and NEED HELP. STATS is literal text. Saved position and GPS position remain separate choices. Custom text is available in Reply and Quick send.

The joystick editor supports at most **79 ASCII characters**:

- Down advances and Up reverses through space, lowercase a-z, digits and `. , ! ? ' - : / @ ( )`.
- Holding up/down repeats after 600 ms at 150 ms intervals, then accelerates to 60 ms intervals after 1.8 seconds.
- Left/right moves the cursor; moving right after the last character permits another character until the limit is reached.
- Center opens Continue editing, Preview send, Delete character, Insert space and Discard draft. Menu opens the same editor options. Deletion at the end removes the preceding character; elsewhere it removes the character under the cursor.
- Preview refuses an empty/space-only message, trims trailing spaces, and displays all 79 possible characters across four lines. The editor does not offer capitals.

Held direction presses and their release clicks are captured by the editor. They do not leak extra clicks into its options or confirmation. The first press while asleep wakes the screen; holding that same press does not edit until released and pressed again. Existing Menu triple-click mute and boot rescue behavior remain.

## Favorites and drafts

Favorites are UI-local overrides identified by a full contact public key or channel-key fingerprint. Contact favorites from the phone always remain starred; a local unstar cannot hide them. Up to 32 local overrides persist in separate snapshots. They do not change the phone contact flags, identity or channel configuration. Replacing a channel key does not inherit the old channel's favorite or reply identity. At the override limit, a new override is refused rather than evicting an old one.

One custom draft is retained, including text, cursor and its original recipient identity. Quick send offers **Resume draft** when it exists. Choosing Custom text for that same recipient also resumes it. Starting to edit a different recipient's custom message replaces the single draft. Successful queueing clears the matching draft; failed sending retains it; Discard draft clears it explicitly.

Draft saves are batched after two quiet seconds or within five seconds of continuous editing. Wait until `Saving draft...` disappears before Reset/power loss. UI-controlled shutdown flushes pending UI state. Unfinished text survives screen sleep and, once saved, restart. A missing/replaced recipient cannot be silently substituted at send time. Storage failures remain visible and retain the RAM draft for retry.

## Outbox semantics and retry

- `Waiting for ACK` / `?`: a DM was accepted by the transport and its estimated ACK window has not elapsed.
- `Acknowledged` / `+`: the existing MeshCore ACK table matched the outgoing message; the outbox also matches the full recipient key.
- `Unconfirmed` / `?`: the ACK window expired, or the device restarted while waiting. This does not prove delivery failure.
- `Sent / no receipt` / `#`: a channel message was accepted by the transport. Channel sends do not claim acknowledged delivery.
- `Not sent` / `!`: queueing/target validation failed.

The outbox records only UI sends, not phone-originated traffic. Pending state becomes Unconfirmed after restart because the upstream ACK table and millis deadlines are not durable. Late ACKs can update an unconfirmed entry while the upstream table still tracks it.

Retry is manual and always warns that it may duplicate delivery. A DM still awaiting ACK, or already acknowledged, is not retried from this action. A channel send can be explicitly resent despite having no delivery receipt. Each retry is a new uniquely timestamped attempt and a new outbox record. No automatic resend/retry or transmission happens on boot. GPS-position retries require and preview a fresh fix; stale recorded coordinates are not silently resent as a live GPS position. Saved-position messages retain their explicit Saved position label.

## Storage and boundaries

All UI stores use the existing two-slot, CRC-checked LittleFS snapshot mechanism on the secondary QSPI filesystem:

- `/ui_inbox0`, `/ui_inbox1`: up to 64 saved received messages and reply identity metadata.
- `/ui_outbox0`, `/ui_outbox1`: 16 UI sends, text, recipient and status.
- `/ui_fav0`, `/ui_fav1`: 32 local favorite overrides.
- `/ui_draft0`, `/ui_draft1`: one draft.
- `/ui_settings0`, `/ui_settings1`: the Auto-read preference, retained by Clean storage.

The primary identity filesystem and preference/channel files are not rewritten by these features. UI files contain plaintext messages and any explicitly composed coordinates; they contain public recipient keys/channel fingerprints, never channel secrets or the private identity key. Logical deletion is not forensic secure erasure, and an older valid slot may be used after corruption of a newer slot.

Older `WIB1` inbox snapshots are readable, but their messages cannot be replied to. New arrivals carry full routing metadata in `WIB2` snapshots.

## Tests

`python3 test/test_recent_messages/run_tests.py` runs the real OLED input/render code and routing/ACK methods with fake hardware and radio adapters, plus the persistent stores and editor, under AddressSanitizer and UndefinedBehaviorSanitizer. See [wio-inbox.md](wio-inbox.md#tests) for the GPS and QSPI test runners. No radio messages are sent by the tests.

## Buzzer shortcut and Clean storage

The small Menu button toggles the buzzer on double-click (triple-click remains supported). ACTIONS is the last page. Its Clean storage action, with a default-No confirmation, clears received messages, the UI outbox and draft by removing both snapshots for each. Favorites, contacts, identity and settings are retained. A cleanup failure is shown and may require retry; do not assume deletion succeeded after an error.

At 64 saved messages the inbox shows a full warning. A new DM replaces the oldest read saved message first, or the oldest saved unread message if all are unread. Delete messages to free history slots. Temporary channel traffic never evicts saved messages. The filesystem can fail independently of this capacity; ordinary saves retain the previous valid snapshot on failure. Confirmed cleanup uses file removal so it need not allocate a new snapshot on a full filesystem.

Message times use `snt:` / `rcv:` and two-digit years. The GPS panel shows OFF, ACQUIRING, FIX or STALE, satellite count, coordinates, altitude and fix age. Disabled/invalid GPS does not display cached coordinates as a live fix. GPS data availability and accuracy remain dependent on the receiver.

The contact picker skips the eight reserved anonymous slots; app favorites stored in device contact flags always remain starred, even if an old UI-local unstar exists.

## Channels and buzzer

ACTIONS also contains Buzzer ON/OFF and Channels. Join Public uses the standard MeshCore Public key. Join #channel uses a lower-case name (up to 30 letters, digits or hyphens, without the leading #); its key is derived by the existing SHA-256 convention. Select Preview join, then Right for Yes and center to confirm. Remove channel browses configured channels and confirms the exact displayed channel. Existing unrelated channels are never replaced when slots are full, and a changed channel cannot be removed using a stale selection. Adding private channels with externally supplied keys remains available through the phone app.

Channel writes use a checked temporary file, verify its contents and atomically rename it over the existing LittleFS channels file. A failed save leaves the old file and restores the in-memory channel. This does not format storage or change identity/radio settings. The phone app may need to refresh its channel list after an on-device change.

## Saved DMs and temporary channels

The inbox holds **64 saved messages plus 8 temporary channel messages**. Incoming DMs are saved. New channel messages stay in RAM unless selected with **Right → Keep message**. Keeping a channel message uses one of the 64 saved slots; if all are occupied, the UI asks you to delete a saved message first.

**ACTIONS → Auto-read ON/OFF** defaults to ON and survives restart. After five seconds without a control event, while the display is awake on a home page other than ACTIONS, it opens unread temporary channel messages, advances through their text every three seconds, then removes them and returns home. It does not interrupt the editor, recipient picker or confirmation screens. Pressing a control stops automatic paging so you can read, reply or choose Keep message. A manually read temporary message disappears when leaving its detail or advancing to the next unread message. Unread temporary messages wait in the eight-slot buffer; overflow replaces its oldest read entry first, then its oldest unread entry. Temporary messages do not survive restart. Auto-read OFF keeps manual browsing and the same temporary/saved policy.

UI storage splits QSPI reads and writes into 128-byte chunks, because larger unaligned transfers bypass the LittleFS cache and are rejected by the nRF52 QSPI driver.

## Splash, clock and GPS freshness

The Wio startup splash shows the MeshCore logo, a large centered **KALLISTI** title, the version once, and a small build ID footer. **ACTIONS → Show clock** displays the device date and time in UTC, refreshed once per second; center or Menu returns to ACTIONS. Inbox and outbox message years both use two digits.

GPS freshness measures elapsed milliseconds since a received valid position, independently of the device clock. FIX is at most 30 seconds old; a retained older position is STALE. Changing the clock does not make a current position stale or revive an old one. Restarting/disabling GPS clears its previous validity and buffered serial data. The clock only synchronizes from newly received RMC sentences with valid calendar dates, after three advancing timestamps; serial silence cannot synchronize it from cached data. Missing or expired altitude is displayed as `Alt --`. A GPS send preview expires based on the age of the coordinates actually previewed, even if new fixes arrive meanwhile.

Auto-read never enters from ACTIONS (including its clock view and confirmations), processes control events first, and waits five seconds after a control event elsewhere. Leaving home clears any pending ACTIONS confirmation. Un-starring a local favorite frees its slot. Discarding a resumed draft returns to the recipient type menu. Invalid channel names remain editable, and a failed join preserves the name for correction/retry.
