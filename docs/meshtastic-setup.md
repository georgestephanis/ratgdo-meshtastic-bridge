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

  ⚠ That short name comes from **the bridge radio's own node list**, not from the sender. A radio that was just flashed or reset, or a sender renamed recently, can show an old name (e.g. `a1b2` instead of `HOMA`) until the sender re-broadcasts its node info, which happens every few hours. Commands from a listed sender are then refused. The hub's log shows the name as received (`radio: a1b2: GDO1!status`). To refresh it, have the bridge radio request the sender's node info (the Meshtastic Python API: `sendData(<own User>, destinationId=<sender>, portNum=NODEINFO_APP, wantResponse=True)`), or wait.
- Its own outgoing messages are not echoed back while `echo` is `false`.

## The house radios

Nothing special: they only need to be on the same channel, with the HA Meshtastic integration connected. Their short names go in the bridge's `allowed_senders`.

## Checking the link both ways

LoRa links can work in one direction only. A house radio can hear the garage and the rest of the mesh perfectly while nothing it sends gets out, so its HA shows the door fine but `status` requests (and anything else it sends) never arrive. In the original install, one house radio ran like this for 10 days without anyone noticing: from its own side and in its HA it looked healthy.

- **"Last heard" only covers the receive direction.** A node's own node list shows what it hears. To know whether others hear *it*, look at its entry in another node's list, or test.
- **Traceroute in both directions.** From the Python API: `sendData(mesh_pb2.RouteDiscovery(), destinationId=<node>, portNum=TRACEROUTE_APP, wantResponse=True)`, once from each end. A reply lists the relays and the SNR of each hop. If the request gets through but the reply doesn't, the sender logs `MAX_RETRANSMIT`.
- **`relayNode`** in received packets (firmware 2.6+) is the last byte of the node ID that rebroadcast the packet. It shows which relay is actually carrying traffic.
- **Hop limit:** `hopStart − hopLimit` on a received packet is how many hops it used. If packets from a site arrive with no hops left, raise `lora.hop_limit` on that site's radio (the default is 3; the sender's setting is what counts).
- **A one-way gap** with comparable radios on both ends points at the weak side's transmit path: a loose U.FL antenna connector, a damaged antenna, or a power amplifier damaged by transmitting without an antenna. A side-by-side check with a known-good radio shows it: two healthy radios a few feet apart should hear each other at similar RSSI. Moving the weak radio higher or into a window, or adding a relay beside it (a second radio with role `ROUTER_LATE`), fixed the original case.
- `lora.tx_power` above the board's maximum (e.g. 30 on a ~22 dBm board) is silently capped by the firmware, so raising it past that doesn't help.

## Airtime

On LongFast, a ~50-byte text takes about 1 s of airtime, and every relay repeats it. The bridge sends one message per state change, folds bursts together, and sends a heartbeat every 6 h. Don't shorten the heartbeat much on a busy mesh.

## ESP-NOW?

Stock Meshtastic firmware can't receive ESP-NOW: its ESP32 build config explicitly excludes the `ESP_NOW` library. If the radio can't be wired to the opener, put a small ESP32 **hub** beside the radio instead ([espnow-hub.md](espnow-hub.md)); the radio is configured exactly as above, wired to the hub.
