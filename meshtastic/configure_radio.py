#!/usr/bin/env python3
"""Configure the garage's Meshtastic radio for the ratgdo bridge.

Enables the Serial module in TEXTMSG mode on two free GPIOs, so the XIAO's
status lines are broadcast as text messages on the primary channel, and text
messages received from the mesh are printed back to the XIAO as
"<short name>: <text>".

Usage:
    uvx --from meshtastic python configure_radio.py --port /dev/ttyACM0 \
        --rxd 48 --txd 47 [--baud 9600] [--short GDO1 --long "Garage Bridge"]

Close any phone app connected to the radio first; it holds the only client
slot and writes will silently not apply. The radio reboots after the module
config is written. Re-run with --show to read the settings back.
"""
import argparse
import time

import meshtastic.serial_interface
from meshtastic.protobuf import module_config_pb2

SerialConfig = module_config_pb2.ModuleConfig.SerialConfig
BAUDS = {9600: SerialConfig.BAUD_9600, 19200: SerialConfig.BAUD_19200,
         38400: SerialConfig.BAUD_38400, 57600: SerialConfig.BAUD_57600,
         115200: SerialConfig.BAUD_115200}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True, help="radio's USB serial port")
    ap.add_argument("--rxd", type=int, help="radio GPIO that receives from the XIAO (XIAO TX)")
    ap.add_argument("--txd", type=int, help="radio GPIO that sends to the XIAO (XIAO RX)")
    ap.add_argument("--baud", type=int, default=9600, choices=sorted(BAUDS))
    ap.add_argument("--short", help="short name for the radio (optional)")
    ap.add_argument("--long", help="long name for the radio (optional)")
    ap.add_argument("--show", action="store_true", help="only print current settings")
    args = ap.parse_args()

    iface = meshtastic.serial_interface.SerialInterface(args.port)
    time.sleep(3)
    node = iface.localNode
    s = node.moduleConfig.serial

    if not args.show:
        if args.rxd is None or args.txd is None:
            ap.error("--rxd and --txd are required unless --show")
        if args.short or args.long:
            node.setOwner(long_name=args.long, short_name=args.short)
            time.sleep(4)
        s.enabled = True
        s.mode = SerialConfig.TEXTMSG
        s.rxd = args.rxd
        s.txd = args.txd
        s.baud = BAUDS[args.baud]
        s.timeout = 250        # ms of silence that ends one message
        s.echo = False         # don't echo our own broadcasts back to the XIAO
        node.writeConfig("serial")
        print("serial module written; the radio will reboot")
        time.sleep(4)
    else:
        print(f"enabled={s.enabled} mode={SerialConfig.Serial_Mode.Name(s.mode)} "
              f"rxd={s.rxd} txd={s.txd} baud={SerialConfig.Serial_Baud.Name(s.baud)} "
              f"timeout={s.timeout} echo={s.echo}")
    iface.close()


if __name__ == "__main__":
    main()
