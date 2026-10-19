# Home Assistant (receiving side)

Each house runs Home Assistant with the [Meshtastic integration](https://github.com/meshtastic/home-assistant), connected to a radio on the same channel as the garage bridge.

## Install the package

1. Enable packages in `configuration.yaml` if you haven't:
   ```yaml
   homeassistant:
     packages: !include_dir_named packages
   ```
2. Copy `homeassistant/packages/garage_mesh.yaml` to `/config/packages/garage_mesh.yaml`.
3. Edit the `CHANGE ME` lines:
   - `notify.mesh_channel_examplemesh` → your channel's notify entity (the integration creates `notify.mesh_channel_<channel name>`).
   - `GDO1` → your `gdo_tag`, if you changed it (three places).
4. Check the config and restart HA.

## What you get

| Entity | Meaning |
|---|---|
| `sensor.garage_door_mesh` | `open`, `closed`, `opening`, `closing`, `stopped` or `unknown`. Attributes: `light`, `obstruction`, `event`, `from_node`, `received`. |
| `binary_sensor.garage_obstruction_mesh` | On when the safety beam is blocked. |
| `binary_sensor.garage_light_mesh` | The opener's light. |
| `binary_sensor.garage_mesh_link_stale` | On when no status has arrived in 13 h (two missed heartbeats). |
| `cover.garage_door_mesh` | A normal garage door entity; open/close/stop send commands over the mesh. Unavailable while the link is stale. |
| `script.garage_mesh_command` | Send any command: `close`, `open`, `stop`, `light_on`, `light_off`, `status`. |
| `automation.garage_mesh_obstruction_alert` | Persistent notification on obstruction. |

The state is whatever the last message said, so after an HA restart it stays `unknown` until the next message. Call `script.garage_mesh_command` with `status` to ask for one.

## Example: alert if the door is left open

```yaml
automation:
  - alias: "Garage left open"
    triggers:
      - trigger: state
        entity_id: sensor.garage_door_mesh
        to: open
        for: "00:30:00"
    actions:
      - action: notify.notify
        data:
          message: "The garage door has been open for 30 minutes."
```

## Without Home Assistant

Any Meshtastic app on the channel shows the status lines as chat messages, and you can type `GDO1!close` or `GDO1!status` to it, provided your short name is in `allowed_senders`.
