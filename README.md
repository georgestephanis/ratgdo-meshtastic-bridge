# ratgdo-meshtastic-bridge

Garage door status and control over a private **Meshtastic** LoRa mesh, for a garage that has **no internet, no Wi-Fi and no Home Assistant** of its own.

A small ESP32 runs [`esphome-ratgdo`](https://github.com/ratgdo/esphome-ratgdo) to talk to a Security+ 2.0 opener (LiftMaster/Chamberlain), and reaches a Meshtastic radio either by three wires or through a small ESP-NOW hub. Status changes go out on the mesh as short text messages; commands come back the same way. Any Home Assistant with its own Meshtastic radio on the channel turns those messages into a normal garage door entity.

> **Status: proposal, untested on hardware.** All three firmwares (wired garage node, ESP-NOW garage node, hub) compile for both boards below. Nothing here has been run against a real opener yet. Treat every ⚠ as something to check on the bench.

## Two ways to connect the radio

Pick whichever fits the building. The mesh protocol, the Home Assistant side and the opener circuit are identical.

| | **A. Wired** (two devices) | **B. ESP-NOW hub** (three devices) |
|---|---|---|
| Layout | garage node ── 3 wires ── radio | garage node ))) ESP-NOW ((( hub ── 3 wires ── radio |
| Use when | the radio can sit near the opener, or you can run a cable to it | the radio needs a window or wall for LoRa range, away from the opener |
| Firmware | `garage-wired.yaml` | `garage-espnow.yaml` + `mesh-hub.yaml` |
| Extra parts | a cable | a second ESP32 and its power |
| Docs | [meshtastic-setup.md](docs/meshtastic-setup.md) | [espnow-hub.md](docs/espnow-hub.md) |

```
 A. Wired
   opener ── garage node ESP32 ── UART ── Meshtastic radio  ))))  mesh  ((((  radio ── HA (each house)

 B. ESP-NOW hub
   opener ── garage node ESP32  ))) ESP-NOW (((  hub ESP32 ── UART ── Meshtastic radio  ))))  mesh  ((((  radio ── HA
                                  HMAC-sealed, anti-replay
```

A third layout, ESP-NOW handled inside a custom Meshtastic build with no hub, is documented as a possible future option, but deliberately not built: [docs/future-meshtastic-espnow-module.md](docs/future-meshtastic-espnow-module.md).

The ESP-NOW link is authenticated: frames carry a counter and an HMAC tag with a shared key, so frames can't be forged or replayed from outside the garage (ESPHome's ESP-NOW itself is unencrypted). Details in [docs/espnow-hub.md](docs/espnow-hub.md).

## Why not Home Assistant at the garage?

A full HA install at a remote, offline building is a lot of upkeep for one door: an SBC, an SD card, a Wi-Fi access point for the ratgdo, clock drift without NTP, and carrying it somewhere with internet for updates. This replaces all of that with two or three microcontrollers and no network at all at the garage. The houses already have HA, so the logic lives there.

Trade-offs: no local dashboard or logging at the garage (the houses log everything), and everything travels at LoRa speed (a few seconds per message, and limited airtime).

## How it works

| Direction | What happens |
|---|---|
| Garage → mesh | On any change of door, light or obstruction (plus every 6 h as a heartbeat), the ESP32 writes one line to the radio's serial port, e.g. `GDO1\|door=closed\|light=off\|obst=clear\|ev=change`. The radio's Serial module in **TEXTMSG** mode broadcasts it as a text message on its primary channel. Bursts (`opening` → `open`) are folded into one message after a 3 s pause. |
| Mesh → garage | The radio prints every text message it receives as `<short name>: <text>` (in option B the hub forwards just the `GDO1!` lines over ESP-NOW). The garage node acts on lines like `HOMA: GDO1!close` if the sender's short name is in `allowed_senders`, then replies with a fresh status (`ev=ack_close`). |
| Mesh → HA | The [Meshtastic HA integration](https://github.com/meshtastic/home-assistant) fires `meshtastic_api_text_message` for each text. The package in `homeassistant/packages/` parses `GDO1\|…` messages into a sensor, binary sensors and a template **cover** whose open/close/stop buttons send `GDO1!…` commands. |

### Message format

```
status:   GDO1|door=<open|closed|opening|closing|stopped>|light=<on|off>|obst=<clear|blocked>|ev=<event>
          ev: boot, change, heartbeat, manual, sync_failed, ack_<cmd>, denied_open, unknown_cmd
command:  GDO1!<close|open|stop|light_on|light_off|status>
```

`GDO1` is a substitution (`gdo_tag`); use a different tag per opener if you have more than one.

### Security

- Everything rides on the channel's **PSK** (encryption key). Anyone with the channel key can read the status and could set their short name to an allowed one, so `allowed_senders` stops mistakes, not attackers. Treat channel membership as "may operate the door".
- `status` is accepted from any sender (it only reads); every other command needs a short name in `allowed_senders`.
- **Report-only:** set `allowed_senders: ""`. The bridge still broadcasts every state change and heartbeat, and answers `status`, but refuses every other command (logged as denied). This is the safest way to run it.
- **`open` is refused by default** (`allow_open: "false"`). Closing remotely is the useful, low-risk direction. Turn opening on only if you understand the risk.
- Status lines are visible to every member of the channel, including in phone apps. For a quieter setup, give the garage radio a dedicated primary channel and add that channel as a secondary on the house radios only. Relay nodes still forward it: with the default `rebroadcast_mode: ALL`, Meshtastic relays packets it can't decrypt.

## Hardware

| Part | Notes |
|---|---|
| ESP32 board(s) | **ESP32 DevKit V1** (ESP32-WROOM-32, same chip as upstream ratgdo v2.5) or **Seeed XIAO ESP32-C6**, one per device, in any mix. Board files: `firmware/boards/`. |
| Opener interface | ratgdo v2.5-style level shifting: MOSFET, PC817 optocouplers, or a hybrid. See [docs/hardware.md](docs/hardware.md). |
| Meshtastic radio | Any ESP32-S3 or ESP32-C6 Meshtastic node with two free GPIOs (Heltec V3/V4, M5Stack C6L, etc.). It can also be your existing relay node. |
| Power | USB brick on the opener's ceiling outlet powers both. |

Wiring between the ESP32 that touches the radio (garage node in A, hub in B) and the radio (cross TX/RX, share ground):

```
 ESP32                     Meshtastic radio
   mesh_tx_pin  ─────────▶  serial RXD (--rxd)
   mesh_rx_pin  ◀─────────  serial TXD (--txd)
   GND          ──────────  GND
```

## Setup

1. **Radio:** put it on your private channel, then enable the serial module ([docs/meshtastic-setup.md](docs/meshtastic-setup.md)):
   ```
   uvx --from meshtastic python meshtastic/configure_radio.py --port /dev/ttyACM0 --rxd 48 --txd 47 --short GDO1 --long "Garage Bridge"
   ```
2. **Firmware:** copy `firmware/secrets.yaml.example` to `firmware/secrets.yaml` and set the passwords (and `espnow_link_key` for option B). In the top-level file, set the `board:` line and `allowed_senders` (your house radios' short names), then flash:
   ```
   cd firmware
   uvx esphome run garage-wired.yaml                 # option A
   uvx esphome run garage-espnow.yaml                # option B, garage node
   uvx esphome run mesh-hub.yaml                     # option B, hub (then exchange MACs: docs/espnow-hub.md)
   ```
3. **Home Assistant** (each house): copy `homeassistant/packages/garage_mesh.yaml` into `/config/packages/`, change the `CHANGE ME` values (channel notify entity, tag), restart. See [docs/home-assistant.md](docs/home-assistant.md).
4. **Test:** press "Send status to mesh" on the garage node's web page (maintenance AP), check `sensor.garage_door_mesh` updates in HA, then send `GDO1!status` from a phone app.

## Repository layout

```
firmware/
  garage-wired.yaml          option A: garage node wired to the radio
  garage-espnow.yaml         option B: garage node, ESP-NOW to the hub
  mesh-hub.yaml              option B: hub wired to the radio
  boards/                    esp32-devkit-v1.yaml, xiao-esp32c6.yaml (pins, logger, RF switch)
  common.yaml                garage node logic: ratgdo + GDO1 protocol (transport-neutral)
  transport-uart.yaml        garage node <-> radio over UART
  transport-espnow.yaml      garage node <-> hub over ESP-NOW
  hub.yaml                   hub logic: radio UART <-> ESP-NOW
  espnow-link.yaml, mesh_auth.h   sealed ESP-NOW link (HMAC + replay counter)
  maintenance.yaml           maintenance AP, web UI, OTA (all devices)
  bench-loopback.yaml        opener-free test of the level-shifting circuit
  secrets.yaml.example
meshtastic/configure_radio.py   enables the radio's serial module (TEXTMSG)
homeassistant/packages/garage_mesh.yaml   receiving side: sensors, cover, command script
docs/                        hardware, radio, ESP-NOW hub, HA, and the future in-radio ESP-NOW idea
```

## Credits

- [ratgdo/esphome-ratgdo](https://github.com/ratgdo/esphome-ratgdo) does all the Security+ 2.0 work; this repo only adds the mesh link.
- [Kaldek/rat-ratgdo](https://github.com/Kaldek/rat-ratgdo) for open ratgdo v2.5 schematics.
- Meshtastic's [Serial module](https://meshtastic.org/docs/configuration/module/serial/).

MIT licensed (see `LICENSE`). esphome-ratgdo is GPL-3.0 and is pulled in at build time as an external component, not redistributed here.
