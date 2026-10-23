# Meshtastic radio setup

## The garage radio

1. Flash current Meshtastic firmware and join it to your private channel (same channel key as the house radios). Set the region and modem preset to match.
2. Give it a recognisable name, e.g. long "Garage Bridge", short `GDO1`.
3. Enable the **Serial module** in **TEXTMSG** mode on two free GPIOs:

   ```
   uvx --from meshtastic python meshtastic/configure_radio.py \
       --port /dev/ttyACM0 --rxd 48 --txd 47 --baud 9600
   ```

   **Pin numbers depend on the radio's chip.** On ESP32 radios they're GPIO numbers (Heltec V3/V4: e.g. 48/47). On **nRF52 radios they're the board's Arduino pin numbers**, not P0.xx/P1.xx: Meshtastic passes them straight to `Serial.setRx/setTx`. On a **Seeed XIAO nRF52840 kit** (Wio-SX1262), use D7/D6 → `--rxd 7 --txd 6`. The kit's GPS UART also sits on D6/D7, and the firmware starts it at boot **even with GPS disabled**, so it fights the Serial module for D6 and nothing reaches the ESP32 (you see `$PCAS`/`MSG…`-style GNSS setup text instead). Move the GPS UART to the unused NFC pads: set `position.rx_gpio` 31 and `position.tx_gpio` 30 (and `position.gps_mode DISABLED` if no GNSS is fitted). Verified on 2.7.26. That XIAO also reports itself over USB as `XIAO_BOOT` while running Meshtastic; that doesn't mean it's in the bootloader.

   `--rxd` is the radio pin that **receives** from the ESP32, i.e. the garage node (option A) or the hub (option B); wire it to `mesh_tx_pin`; `--txd` sends to the ESP32 (`mesh_rx_pin`). Close any phone app connected to the radio first: it takes the only client slot, and writes silently don't apply. Read the settings back with `--show`.

4. Role: `CLIENT` is fine. If this radio is also your relay node at that site, `ROUTER_LATE` works too; the serial module doesn't care.

Equivalent settings, if you prefer the app or CLI:

| Setting | Value |
|---|---|
| `serial.enabled` | `true` |
| `serial.mode` | `TEXTMSG` |
| `serial.rxd` / `serial.txd` | your two GPIOs |
| `serial.baud` | `BAUD_9600` (must match `mesh_baud` in the firmware) |
| `serial.timeout` | `250` ms (silence that ends one message) |
| `serial.echo` | `false` |

## How TEXTMSG mode behaves

From the firmware's `SerialModule.cpp`:

- Bytes arriving on the serial port are sent as a **text message, broadcast, on channel index 0** (the primary channel). A message ends when the line goes quiet for `serial.timeout` ms, so the bridge writes each status line in one burst with no newline.
- Every text message the radio receives (any channel, including direct messages) is printed as `\r\n<sender short name>: <text>\r\n`. Only the short name is printed, not the node ID, which is why `allowed_senders` is a list of short names.
- Its own outgoing messages are not echoed back while `echo` is `false`.

## The house radios

Nothing special: they only need to be on the same channel, with the HA Meshtastic integration connected. Their short names go in the bridge's `allowed_senders`.

## Airtime

On LongFast, a ~50-byte text takes about 1 s of airtime, and every relay repeats it. The bridge sends one message per state change, folds bursts together, and sends a heartbeat every 6 h. Don't shorten the heartbeat much on a busy mesh.

## ESP-NOW?

Stock Meshtastic firmware can't receive ESP-NOW: its ESP32 build config explicitly excludes the `ESP_NOW` library. If the radio can't be wired to the opener, put a small ESP32 **hub** beside the radio instead ([espnow-hub.md](espnow-hub.md)); the radio is configured exactly as above, wired to the hub.
