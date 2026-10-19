# Option B: ESP-NOW hub (three devices)

Use this when the Meshtastic radio can't be wired to the opener: the radio needs a high window or an outside wall for LoRa range, and the opener is on a ceiling somewhere else.

```
 ┌──────────── garage ceiling ───────────┐            ┌──────── window / attic / wall ────────┐
 │ opener ── garage node (ratgdo ESP32)  │ ))) ESP-NOW (((  │ hub ESP32 ── UART ── Meshtastic radio │ ))) LoRa
 │   red/white/black   garage-espnow.yaml│   2.4 GHz  │ mesh-hub.yaml     3 wires             │
 └───────────────────────────────────────┘            └───────────────────────────────────────┘
```

| Device | Firmware | Wired to |
|---|---|---|
| Garage node | `firmware/garage-espnow.yaml` | the opener (red/white/black), see [hardware.md](hardware.md) |
| Hub | `firmware/mesh-hub.yaml` | the radio's serial pins (TX, RX, GND), same as the wired option |
| Radio | stock Meshtastic, serial module in TEXTMSG mode | the hub |

Both ESP32s can be either supported board (DevKit V1 or XIAO C6), in any mix.

## Why a hub rather than ESP-NOW inside the radio?

Stock Meshtastic firmware deliberately leaves out the ESP-NOW library (`lib_ignore` in `variants/esp32/esp32-common.ini`), so a radio can't listen for it. A custom Meshtastic build would mean a home-made module carried through every firmware release, and ESP-NOW would share the 2.4 GHz radio with Meshtastic's Bluetooth and Wi-Fi. A separate hub keeps the radio on stock firmware: the hub is just the "wire" from the wired option, made wireless.

## What crosses the ESP-NOW link

Exactly the same lines as on the wire:

| Direction | Payload |
|---|---|
| hub → garage | a line the radio printed, e.g. `HOMA: GDO1!close`. The hub forwards **only** `<sender>: <gdo_tag>!…` lines, so ordinary channel chat never leaves the hub. |
| garage → hub | a status line, e.g. `GDO1\|door=closed\|light=off\|obst=clear\|ev=change`; the hub writes it to the radio, which broadcasts it. |

The garage node, not the hub, decides whether a command is allowed (`allowed_senders`, `allow_open`), just as in the wired setup.

## Link security

ESPHome's `espnow` component sends frames **unencrypted**, and a peer MAC is trivial to spoof. Without protection, anyone within Wi-Fi range of the garage could record a "close" (or "open") frame and replay it, or forge one. So every frame is sealed (`firmware/mesh_auth.h`):

```
<counter>|<payload>|<tag>        tag = HMAC-SHA256(link_key, "<counter>|<payload>"), first 8 bytes, hex
```

- A frame with a wrong tag is dropped (wrong key, forged or corrupted).
- A frame whose counter isn't higher than the last accepted one is dropped (replay).
- Counters live in flash; each boot jumps the send counter ahead by 1000, so a counter that wasn't saved before a power cut is never reused.
- The payload is readable over the air. It's the same text the mesh carries (encrypted there by the channel key), so the ESP-NOW link reveals door state to someone standing nearby with a sniffer. Accept that, or keep the link short.

**If you replace one device,** the other still remembers the old counter and drops everything until the new one passes it. Press **"Reset ESP-NOW replay counter"** on the side that's dropping frames (its log says `dropped replayed/old frame`).

## Setup

1. **Secrets:** in `firmware/secrets.yaml`, set `espnow_link_key` to a long random value, the same on both devices:
   ```
   python3 -c "import secrets; print(secrets.token_hex(16))"
   ```
2. **MAC addresses:** flash both devices once (the placeholder `peer_mac` is fine for this), then read each one's Wi-Fi MAC from its boot log or its web page (`esphome logs`, or the maintenance AP at http://192.168.4.1). Put the **hub's** MAC in `garage-espnow.yaml` → `peer_mac`, and the **garage node's** MAC in `mesh-hub.yaml` → `peer_mac`. Flash both again.
3. **Channel:** `wifi_channel` must match on both (default 6). ESP-NOW uses the maintenance AP's channel.
4. **Radio:** configure its serial module exactly as for the wired option ([meshtastic-setup.md](meshtastic-setup.md)), wired to the **hub**.
5. **Test:**
   - Press **"Ask garage for status"** on the hub's web page. A status line should appear on the mesh within a few seconds (`ev=ack_status`), and in the hub's **"Last line from garage"**.
   - Send `GDO1!status` from a phone app on the channel. The hub's **"Last line forwarded to garage"** should show it.

## Range and placement

ESP-NOW is ordinary 2.4 GHz Wi-Fi frames at a low rate, so expect about Wi-Fi range or a little more: tens of metres indoors, through a wall or two. The unicast frames are acknowledged and retried by the radio hardware, and a failed send is logged (`ESP-NOW send failed`).

- On a XIAO C6, switch to the **U.FL** connector with an external 2.4 GHz antenna (set `antenna_select` to `ALWAYS_ON` in `boards/xiao-esp32c6.yaml`) if the hop is long.
- A DevKit V1's PCB antenna is fine for one or two walls; keep it away from metal boxes and the opener's motor housing.
- The hub can sit right beside the radio and share its USB power supply.

## Failure behaviour

| What fails | What happens |
|---|---|
| Hub or radio down | Garage status stops reaching the houses; HA's `binary_sensor.garage_mesh_link_stale` turns on after 13 h (two missed 6 h heartbeats). The opener works normally. |
| Garage node down | Same: no status, stale after 13 h. Commands are sent but nothing answers. |
| ESP-NOW hop marginal | Garage → hub status lines retry at the link layer; failures are logged on the garage node. Commands that fail just get no `ack_…` status back. |
