# Home Assistant (receiving side)

Each house runs Home Assistant with the [Meshtastic integration](https://github.com/meshtastic/home-assistant), connected to a radio on the same channel as the garage bridge.

## Pick a package

| Package | Use it when |
|---|---|
| `homeassistant/packages/garage_mesh.yaml` | This HA should be able to operate the door. Adds a `cover`, a command script and an obstruction alert. The sender's radio short name must be in the bridge's `allowed_senders`. |
| `homeassistant/packages/garage_mesh_display.yaml` | This HA should only see the door, for example with a report-only bridge (`allowed_senders: ""`). Same sensors, but no cover or alert, and `script.garage_mesh_command` only sends `status`. |

Install one or the other, not both: they define the same entities.

## Install the package

1. Enable packages in `configuration.yaml` if you haven't:
   ```yaml
   homeassistant:
     packages: !include_dir_named packages
   ```
2. Copy the package you picked to `/config/packages/`.
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
| `cover.garage_door_mesh` | *(Full package only.)* A normal garage door entity; open/close/stop send commands over the mesh. Unavailable while the link is stale. |
| `script.garage_mesh_command` | Send a command: in the full package any of `close`, `open`, `stop`, `light_on`, `light_off`, `status`; in the display package only `status`. |
| `automation.garage_mesh_obstruction_alert` | *(Full package only.)* Persistent notification on obstruction. |

The state is whatever the last message said, so after an HA restart it stays `unknown` until the next message. Call `script.garage_mesh_command` with `status` to ask for one.

## Dashboard

`homeassistant/dashboards/garage_mesh.yaml` is a ready-made view for either package: door, light, obstruction and link tiles, when the last message arrived and why (`heartbeat`, `change`, `ack_status`, ...), a "request status" button with a confirmation, a 48 h history graph and a 7-day logbook. Create a new dashboard, open its raw configuration editor and paste the file in. With the full package, add a tile for `cover.garage_door_mesh` to get open/close buttons.

The entities come from a template package, so they have no device and land in no area. To have them show up as a room (in the default dashboard, voice assistants and area targeting), create an area such as "Garage (remote)" and assign the four sensors and `script.garage_mesh_command` to it under Settings > Entities. If the house has its own garage area, use a separate one so the two don't mix.

The "last message" time is the useful health signal: the bridge sends a heartbeat every 6 h even when nothing changes, so anything older than that means messages aren't reaching this HA (see [Checking the link both ways](meshtastic-setup.md#checking-the-link-both-ways)).

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
