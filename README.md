# SlimeVR Tracker nRF Receiver AFH — Quoeskeni fork

Firmware for Nordic nRF52833 / nRF52840 receiver boards, with AFH support for the StackedSmol AFH tracker set.

This repository is the receiver-side fork used by `Quoeskeni/SmolSlimeConfigurator-AFH` and `Quoeskeni/SlimeNRF-Firmware-CI`. Upstream/original repositories are read-only context; changes here are intended only for this Quoeskeni fork.

## AFH pairing contract

The configurator's receiver **Start AFH Pairing** button sends:

```text
afh_set_channel 100
afh_info
pair
```

Receiver firmware therefore guarantees:

- `afh_set_channel 100` moves the runtime ESB channel back to the AFH discovery/pairing channel.
- `pair` enters pairing mode on the shared discovery ESB address, matching unpaired trackers.
- Pairing starts on AFH default channel `100`, then normal paired traffic can use the derived paired address and negotiated AFH channel.
- `exit` leaves pairing mode and returns the receiver to normal HID dongle operation.
- Serial logs include `Pairing state: started` and `Pairing state: stopped` for configurator diagnostics.

## SlimeVR server compatibility

No SlimeVR server patch is required. The receiver still presents as the normal SlimeVR HID dongle; AFH remains radio/firmware-side.

## License

Unless otherwise specified, all code in this repository is dual-licensed under either:

- MIT License ([LICENSE-MIT](LICENSE-MIT) or https://opensource.org/license/mit/)
- Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or https://opensource.org/license/apache-2-0/)

at your option.
