# Hardware

## The opener's wire bus (Security+ 2.0)

From the [rat-ratgdo FAQ](https://github.com/Kaldek/rat-ratgdo):

- **Red** is +12 V for the wall panel **and** the data line. It idles high; a device sends a bit by pulling it to ground. 9600 baud, 8N1, half-duplex.
- **White** is ground.
- **Black/grey** is the obstruction (IR) sensor line, which carries pulses.
- ⚠ Some openers have a red terminal that's only a dry-contact door button. Check your opener's terminal labels.

esphome-ratgdo opens its UART **inverted** (`begin(9600, 8N1, rx, tx, true)`). All circuits below produce that inverted logic, so no firmware change is needed.

## Pin maps

| Function | ESP32 DevKit V1 | XIAO ESP32-C6 |
|---|---|---|
| Bus TX (`uart_tx_pin`) | GPIO2 (D2) | GPIO1 (D1) |
| Bus RX (`uart_rx_pin`) | GPIO4 (D4) | GPIO2 (D2) |
| Obstruction (`input_obst_pin`) | GPIO15 (D15) | GPIO21 (D3) |
| To radio RXD (`mesh_tx_pin`, wired node or hub) | GPIO17 (TX2) | GPIO0 (D0) |
| From radio TXD (`mesh_rx_pin`, wired node or hub) | GPIO16 (RX2) | GPIO17 (D7) |

- **XIAO C6:** don't use D6/GPIO16 for the mesh TX. It's UART0 TX, and the boot ROM prints there at reset; the radio would broadcast that as a text message.
- **DevKit V1:** every pin is in one run on the 30-pin header: `3V3 | GND | D15 (OBST) | D2 (TX) | D4 (RX) | RX2 | TX2`. An ESP-NOW garage node uses the first five; TX2/RX2 are only for the radio link (wired node or hub). These differ from upstream ratgdo's ESP32 pins (GPIO22/21/23) on purpose.
- **DevKit V1 strapping pins:** GPIO2 and GPIO15 affect boot. TX's 10k base pull-down holds GPIO2 low at reset, which flashing needs; the board's blue LED on GPIO2 will flicker on transmit. OBST held low at reset only mutes the ROM boot log. Don't put RX or OBST on D2: the LED loads the 10k pull-up. ⚠ GPIO15 may toggle briefly during boot, before ratgdo starts.
- **DevKit V1:** the serial logger is turned off in firmware (see `boards/esp32-devkit-v1.yaml`), because ratgdo hard-codes UART1 and ESPHome would otherwise give the radio link UART1 too. Read logs from the web UI or with `esphome logs` over OTA.
- ⚠ Check your board's silkscreen against these GPIO numbers before wiring.

## Opener interface circuits

Pick one. Pin names below use the XIAO; substitute the DevKit pins from the table.

### Circuit A: MOSFET (the proven ratgdo v2.5 design)

```
 TX:  RED ──────────────┐
                        │ D
                     [AO3400A]  G ──[100R]──┬───── uart_tx_pin
                        │ S                 │
 WHITE ─────────────────┴───────────────────┴[10k]── GND        gate pull-down: stays off at boot

 RX:  RED ──[10k ⚠]──┐ G                    3V3 ──[10k]──┐
                     [2N7000]  D ────────────────────────┴─── uart_rx_pin
                        │ S
 WHITE ─────────────────┴────────────────────────────────── GND

 OBST: BLACK ──[10k]──┬──────────────────────────────────── input_obst_pin
                      [10k]
 WHITE ───────────────┴──────────────────────────────────── GND
```

- **TX:** use an **AO3400A** (fully on at about 2.5 V gate). A 2N7000 is marginal at 3.3 V.
- **RX:** the 2N7000 gate sees 12 V, so it's fine here; the GPIO never sees 12 V.
- **Obstruction:** ⚠ measure black–white first; the 10k/10k divider assumes about 6.6 V.
- ⚠ Compare against rat-ratgdo's `schematics/ratgdo open source ESP-32_schem_v6.png` before soldering.

### Circuit H: hybrid (PC817 RX + obstruction, AO3400A TX), recommended first build

```
 OPENER SIDE (red / white / black)                          ESP SIDE

 ── RX ───────────────────────────────────────────────────────────────────────
 RED ──[R1 4.7k]── U1 pin1 (A)                    3V3 ──[R2 3.3k]──┬── U1 pin4 (C)
                   U1 pin2 (K) ── WHITE                            └──── uart_rx_pin
                                                                   U1 pin3 (E) ── GND

 ── OBSTRUCTION ──────────────────────────────────────────────────────────────
 BLACK ──[R3 2.2k ⚠]── U2 pin1 (A)                3V3 ──[R4 4.7k]──┬── U2 pin4 (C)
                       U2 pin2 (K) ── WHITE                        └──── input_obst_pin
                                                                   U2 pin3 (E) ── GND
   ⚠ R3 ≈ (V_black − 1.2 V) / 2 mA, after measuring black–white

 ── TX option T1: AO3400A (default) ──────────────────────────────────────────
 RED ── Q1 Drain;  Q1 Gate ──[R5 100R]──┬── uart_tx_pin;  └──[R6 10k]── GND
        Q1 Source ── WHITE
 *** J1: WHITE ── ESP GND *** (required for T1)

 ── TX option T2: PC817 (fully isolated test) ────────────────────────────────
 RED ── U3 pin4 (C);  WHITE ── U3 pin3 (E)
 uart_tx_pin ──[R7 330R]── U3 pin1 (A);  U3 pin2 (K) ── GND      (remove J1 and Q1)
```

| Ref | Part | Start value | Notes |
|---|---|---|---|
| U1, U2, (U3) | PC817 | | |
| Q1 | AO3400A + SOT-23→DIP adapter | | |
| R1 | 4.7k | | RX LED ≈ 2.3 mA; try 10k (less bus load) or 3.3k (faster) |
| R2 | 3.3k | | RX pull-up; 2.2k–10k |
| R3 | 2.2k ⚠ | | set after measuring |
| R4 | 4.7k | | |
| R5 / R6 | 100R / 10k | | gate series / pull-down |
| R7 | 330R | | T2 only |

**PC817 risks:** it turns off slowly (tens of µs) against a 104 µs bit, its LED loads the red line, and as TX it may not sink enough current to pull red low. That's why TX defaults to the MOSFET.

## Bench test without an opener

`firmware/bench-loopback.yaml` sends `55 AA 0F F0` every 2 s out of TX and reads it back on RX through a fake bus:

```
 12 V supply (+) ──[1k]──┬── "RED" node (feeds R1 and Q1's drain)
 12 V supply (−) ── "WHITE"
```

Pass: every `>>> 55:AA:0F:F0` in the log is followed by `<<< 55:AA:0F:F0`. Garbage or missing bytes mean the edges are too slow: lower R2 or R1.

## Powering the garage node

| Source | Verdict |
|---|---|
| USB power supply on the opener's ceiling outlet | Simplest. Goes down with mains power. |
| **Opener's battery-backup connector** → LM2596 buck set to **5 V** → ESP32 5V/VIN | **Recommended** on openers with standby-battery support (built in or optional). A dedicated 12 V source separate from the data bus, and it keeps running during a power cut while the opener is on battery, which is when door state matters most. Set the buck to 5 V **before** connecting the ESP. ⚠ Check the connector's voltage and polarity with a meter first. |
| The red wire ("power stealing", like a Nest thermostat) | **Doesn't work.** Red is the data line *and* the wall panel's power. rat-ratgdo tested a buck converter plus ESP8266 on it, and the draw [broke the bus](https://github.com/Kaldek/rat-ratgdo/blob/main/Using%20red%20wire%20for%20power.md). An ESP32 needs a radio on all the time (about 0.3–0.8 W continuous, no sleep possible, since ratgdo must keep listening), roughly 10–100× a Nest's average draw. A hold-up capacitor doesn't help: the problem is the steady load, and a capacitor on the bus also swallows the data pulses. |
| A dry-contact wall button (older openers only) | Same problem. Riding out a 2 s button press at about 0.4 W would need about 15,000 µF (12 V → 6 V), and recharging it loads the line the opener is reading. |

The **hub** in option B sits beside the radio and shares its USB supply.

## Radio side

Use any two free GPIOs on the Meshtastic radio. Examples (⚠ verify against your board's pinout and Meshtastic variant file):

- **Heltec V3 / V4:** GPIO47 / GPIO48 on the header are not used by Meshtastic (the V4 uses 2, 5, 7 and 46 for its PA; 38–42 for GPS).
- **Heltec V4, via the GPS connector (to be explored, untested):** the V4's GPS port is already a UART plus power. Meshtastic's V4 variant puts the GPS serial on **GPIO38** (GPS TX → board) and **GPIO39** (board → GPS), with GPS power switched by **GPIO34**. If you don't need GPS (a fixed node can use a fixed position instead), the port could carry the link with a plug-in cable and no header soldering:
  1. Remove the GPS module, or leave it in. Set `position.gps_mode = NOT_PRESENT` so Meshtastic doesn't open its GPS UART on those pins.
  2. Configure the serial module with `--rxd 38 --txd 39`: the ESP32's `mesh_tx_pin` goes to the connector's former GPS-TX pin, and `mesh_rx_pin` to its former GPS-RX pin.
  3. ⚠ Two parts of the firmware are then configured for the same pins. If the GPS code still claims them, the link sees garbage or nothing; fall back to GPIO47/48.
  4. ⚠ The connector's power pin is probably switched by GPIO34 and may stay **off** when GPS isn't present. Meter it before relying on it; otherwise power the ESP32 from the V4's 5 V pin or the shared USB supply, and use the connector only for the two data lines and ground.
  5. ⚠ Check the connector's pin order against your board revision; the GPIO numbers above come from the firmware's variant file, not a physical pinout.

  Results welcome as an issue or PR.
- **M5Stack Unit C6L:** use the Grove port's two signal pins.

## PCB

Once the breadboard results are in, lay out a small 2-layer board in KiCad: a socket for the ESP32 board, a 3-way screw terminal for red/white/black, a 4-pin header for the radio (TX, RX, GND, 5 V), a solder jumper for J1, and footprints for both TX options. Keep 12 V traces away from GPIO traces.
