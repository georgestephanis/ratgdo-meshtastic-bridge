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
| Bus TX (`uart_tx_pin`) | GPIO22 (D22) | GPIO1 (D1) |
| Bus RX (`uart_rx_pin`) | GPIO21 (D21) | GPIO2 (D2) |
| Obstruction (`input_obst_pin`) | GPIO23 (D23) | GPIO21 (D3) |
| To radio RXD (`mesh_tx_pin`) | GPIO17 (TX2) | GPIO0 (D0) |
| From radio TXD (`mesh_rx_pin`) | GPIO16 (RX2) | GPIO17 (D7) |

- **XIAO C6:** don't use D6/GPIO16 for the mesh TX. It's UART0 TX, and the boot ROM prints there at reset; the radio would broadcast that as a text message.
- **DevKit V1:** the serial logger is turned off in firmware (see `esp32-devkit-v1.yaml`), because ratgdo hard-codes UART1 and ESPHome would otherwise give the radio link UART1 too. Read logs from the web UI or with `esphome logs` over OTA.
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

## Radio side

Use any two free GPIOs on the Meshtastic radio. Examples (⚠ verify against your board's pinout and Meshtastic variant file):

- **Heltec V3 / V4:** GPIO47 / GPIO48 on the header are not used by Meshtastic (the V4 uses 2, 5, 7 and 46 for its PA; 38–42 for GPS).
- **M5Stack Unit C6L:** use the Grove port's two signal pins.

## PCB

Once the breadboard results are in, lay out a small 2-layer board in KiCad: a socket for the ESP32 board, a 3-way screw terminal for red/white/black, a 4-pin header for the radio (TX, RX, GND, 5 V), a solder jumper for J1, and footprints for both TX options. Keep 12 V traces away from GPIO traces.
