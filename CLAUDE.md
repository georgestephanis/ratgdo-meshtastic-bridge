# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESPHome firmware plus glue that lets a ratgdo-controlled garage door opener (Security+ 2.0) report status and take
commands over a Meshtastic LoRa mesh, so Home Assistant installs in other buildings can see and control it without
Wi-Fi or internet at the garage.

**Status: proposal, untested on hardware.** All firmwares compile for both supported boards, but nothing has run
against a real opener. Don't describe anything as verified on hardware unless the user says it was.

The Security+ 2.0 work is done by the upstream [ratgdo/esphome-ratgdo](https://github.com/ratgdo/esphome-ratgdo)
external component (GPL-3.0, pulled at build time, pinned by commit in `firmware/common.yaml`). This repo (MIT) only
adds the mesh link. Don't vendor upstream code.

## Topologies

- **Option A, wired (2 devices):** garage node ESP32 ←UART→ Meshtastic radio. Entry point `firmware/garage-wired.yaml`.
- **Option B, ESP-NOW hub (3 devices):** garage node ESP32 ←ESP-NOW→ hub ESP32 ←UART→ radio. Entry points
  `firmware/garage-espnow.yaml` (garage) and `firmware/mesh-hub.yaml` (hub). Used when the radio can't sit in the garage.
- **Option C (future, not built):** ESP-NOW handled inside a custom Meshtastic module, no hub. See
  `docs/future-meshtastic-espnow-module.md`.

## Firmware layout

Top-level role files (`garage-wired.yaml`, `garage-espnow.yaml`, `mesh-hub.yaml`) only set `substitutions:` and
assemble `packages:`. Logic lives in the included packages:

- `boards/*.yaml` — per-board pins, `esp32:` target, logger, RF switch. Selected by the `board:` package line.
  Supported: ESP32 DevKit V1 and XIAO ESP32-C6.
- `common.yaml` — garage node logic: ratgdo entities, `publish_status`, `handle_mesh_line` (command parsing and
  authorization), heartbeat. **Transport-neutral**: it calls `script.mesh_send` and expects a transport to call
  `handle_mesh_line`. Not compiled directly.
- `transport-uart.yaml` / `transport-espnow.yaml` — implement `mesh_send` and feed received lines in.
- `hub.yaml` — hub logic: radio UART ↔ ESP-NOW relay. The hub does **not** authorize commands; the garage node does.
- `espnow-link.yaml` + `mesh_auth.h` — sealed ESP-NOW link: counter + truncated HMAC-SHA256 with a shared key,
  replay rejection (counter must exceed last accepted). TX counter jumps +1000 on boot so values are never reused.
- `maintenance.yaml` — shared Wi-Fi AP, web UI, OTA. Nothing in normal operation depends on it.
- `bench-loopback.yaml` — opener-free test of the level-shifting circuit.

Keep new behavior in the right layer: protocol/door logic in `common.yaml`, wire details in a transport, pins in
`boards/`. Any new substitution a package relies on must be defined in every role file that includes it.

## Wire protocol

```
status:  GDO1|door=<open|closed|opening|closing|stopped>|light=<on|off>|obst=<clear|blocked>|ev=<event>
command: GDO1!<close|open|stop|light_on|light_off|status>
ev:      boot, change, heartbeat, manual, sync_failed, ack_<cmd>, denied_open, unknown_cmd
```

`GDO1` is the `gdo_tag` substitution. The protocol is duplicated in three places that must stay in sync:
`common.yaml` (producer/parser), `homeassistant/packages/garage_mesh.yaml` (consumer), and the README.

Behavioral invariants:

- Messages are short on purpose (LoRa airtime). Don't add fields casually.
- `status` is accepted from any sender; every other command requires the sender's short name in `allowed_senders`.
  This only prevents mistakes; anyone with the channel PSK can spoof a short name. Channel membership = may operate door.
- `open` is refused unless `allow_open: "true"` (default `"false"`). Keep that default.
- `publish_status` uses `mode: restart` + 3 s delay to coalesce bursts into one message.

## Hardware/radio gotchas encoded in the config

- The Meshtastic serial module (TEXTMSG mode) ends a message on **silence** (~250 ms read timeout), not on newline.
  Never append a newline when writing; `mesh_send` keeps a 1 s tail so consecutive lines don't merge.
- Received texts arrive as `\r\n<short name>: <text>\r\n`.
- ratgdo hard-codes UART1 for the opener bus. On the wired DevKit node the serial logger is disabled
  (`baud_rate: 0`) so the radio link gets UART0 instead of colliding on UART1. Read logs via the web UI or OTA.
- ESP-NOW shares the radio and channel with the maintenance AP, so hub and garage `wifi_channel` must match.
- The ratgdo `rolling_code_counter` and `client_id` numbers must stay; without them the opener stops accepting
  commands after reboot.
- `mesh_baud` must match the radio's `serial.baud`.

## Commands

ESPHome is run through `uvx`, from `firmware/`:

```bash
cd firmware
cp secrets.yaml.example secrets.yaml      # first time; set passwords (+ espnow_link_key for option B)
uvx esphome config garage-wired.yaml      # validate config
uvx esphome compile garage-wired.yaml     # build without flashing
uvx esphome run garage-wired.yaml         # build + flash
uvx esphome logs garage-wired.yaml        # logs over OTA
```

`garage-wired.yaml` and `garage-espnow.yaml` share the device name, so they share `.esphome/build/garage-mesh-bridge/`.
`esphome upload` flashes whatever was compiled last, which may be the other role. Flash with `esphome run` (it
recompiles), or compile the same file immediately before `upload`.

After firmware changes, compile **all three role files** (`garage-wired.yaml`, `garage-espnow.yaml`,
`mesh-hub.yaml`), and if board-relevant, for both boards by switching the `board:` package line (don't commit the
switch). There are no automated tests.

Radio setup (close any phone app connected to the radio first; it holds the only client slot):

```bash
uvx --from meshtastic python meshtastic/configure_radio.py --port /dev/ttyACM0 --rxd 48 --txd 47 \
    --short GDO1 --long "Garage Bridge"     # --show to read settings back
```

## Secrets

`firmware/secrets.yaml` is gitignored; never commit it or put real keys in `secrets.yaml.example`.
`firmware/.esphome/` is build output, also ignored.

## Docs

`docs/hardware.md` (wiring, level shifting, power), `docs/meshtastic-setup.md`, `docs/espnow-hub.md`,
`docs/home-assistant.md`, `docs/future-meshtastic-espnow-module.md`. When behavior, pins, or the protocol change,
update the README and the relevant doc in the same change. Mark untested assumptions with ⚠, matching the README.
