# Future option C: ESP-NOW inside the radio (custom Meshtastic module)

**Status: idea, not built.** This repo deliberately uses a separate hub ([option B](espnow-hub.md)) instead. This page records why, and what it would take to drop the hub.

```
 B (today):  garage node ))) ESP-NOW ((( hub ESP32 ── UART ── Meshtastic radio ))) LoRa
 C (idea):   garage node ))) ESP-NOW ((( Meshtastic radio running a custom ESP-NOW module ))) LoRa
```

## Is the ESP-NOW hop "just serial"?

Functionally, yes. ESP-NOW sends discrete packets of up to 250 bytes rather than a byte stream, but this protocol is already one line per message, so the hub's whole job is a one-to-one mapping:

- a line the radio prints (`HOMA: GDO1!close`) → one sealed ESP-NOW packet to the garage node
- one sealed ESP-NOW packet from the garage node (`GDO1|door=closed|…`) → one line written to the radio

The garage-node firmware (`garage-espnow.yaml`) wouldn't change at all for option C. It can't tell whether it's talking to a hub or to a radio doing the same job.

## Why re-enabling the library isn't enough

Stock Meshtastic excludes the Arduino `ESP_NOW` library (`lib_ignore` in `variants/esp32/esp32-common.ini`). Removing that line only makes the library available to compile against; nothing in Meshtastic would use it. Option C needs a new firmware module, modelled on `src/modules/SerialModule.cpp`, that:

1. **Brings up Wi-Fi on a fixed channel** in the background (ESP-NOW rides on the Wi-Fi driver), alongside Meshtastic's Bluetooth. The ESP32-S3 can run both at once, at some cost in RAM and power. It must not interfere with Meshtastic's own Wi-Fi mode if that is enabled.
2. **ESP-NOW → mesh:** opens each received packet and sends the payload as a broadcast text on the primary channel, exactly as TEXTMSG mode does with UART bytes.
3. **Mesh → ESP-NOW:** for each received text message starting with the configured tag (`GDO1!`), sends `"<sender short name>: <text>"` to the peer.
4. **Implements the same sealing** as [`firmware/mesh_auth.h`](../firmware/mesh_auth.h): counter plus truncated HMAC-SHA256, with counters persisted in flash and a boot-time jump. Meshtastic already links mbedTLS on ESP32, so the code ports nearly as-is.
5. **Takes its settings** from somewhere: peer MAC, channel, link key, tag. Ideally a new module config, which means protobuf changes; or, for a private build, compile-time defines.

Rough size: a few hundred lines of C++.

## Trade-offs

| | B: separate hub (chosen) | C: module in the radio |
|---|---|---|
| Devices | garage node + hub + radio | garage node + radio |
| Radio firmware | **stock** Meshtastic, updated from the app or web flasher | **forked** build: rebase, rebuild and reflash by hand on every release |
| Radio resources | unchanged | Wi-Fi running in the background: more RAM and power, possible Bluetooth coexistence effects |
| Debugging | hub has its own logs and web page; the radio is known-good | a problem could be in the module or in Meshtastic |
| Cost | one more cheap ESP32 board and its 5 V supply | developer time and ongoing maintenance |
| Wiring | 3 wires hub ↔ radio (or the V4's GPS connector, see [hardware.md](hardware.md)) | none |

The hub wins for now because it keeps the radio on stock firmware, and an ESP32 board costs less than maintaining a firmware fork.

## When C would be worth doing

- The extra board turns out to be a real problem (space, power or mounting) once option B is running.
- Or as an **upstream contribution**: a general-purpose "ESP-NOW bridge" module (authenticated text between a Meshtastic node and nearby ESPHome devices) could be useful well beyond garage doors. If it were merged upstream, the maintenance problem goes away.

The protocol and the garage-node firmware stay the same either way, so moving from B to C later would only change the radio and remove the hub.
