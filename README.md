# Immergas Magis Pro / Combo — heat-pump data from the Samsung NASA bus (F1/F2), ESPHome

🇬🇧 English | 🇵🇱 [Polski](README.pl.md)

Read everything the **outdoor unit** of an Immergas Magis Pro / Combo V2 (a Samsung EHS unit, e.g. **Audax Pro V2**) and the **Samsung MIM-B19N interface board** inside the Magis exchange on the **F1/F2** bus. You get it in **Home Assistant** through an **M5Stack Atom + RS485** running ESPHome. No cloud is involved and no third-party NASA component is needed: the project ships its own component, `immergas_nasa`.

> Part of the family [`jotuera/immergas-magis-pro-combo-*`](https://github.com/jotuera?tab=repositories): **dd-modbus** (D+/D-, Dominus/panel emulator), **tt-bms** (T-/T+, BMS Modbus) and **f1f2-nasa** (this repo).

> ⚠️ **Beta (v0.9.0).** Tested on a **Magis Combo 9 Plus V2 + Audax Pro 9 V2**. Feedback from other Magis Pro / Combo installs is very welcome.

## What you get

- **Temperatures**: flow / return, active water target, outdoor, discharge, compressor top, condenser out, refrigerant liquid, IPM.
- **Compressor**: actual / ordered / target frequency, start sequence stage, restart inhibit, current, power, energy (kWh), DC-link and mains voltage.
- **Outdoor fan and valves**: target / actual RPM, main EEV position (0–2000), 4-way valve, hot-gas valve, base heater, defrost mode and stage.
- **State**: outdoor driving mode (Stop / Safety / Normal / Defrost …), indoor operation mode, pump, booster / backup heater.
- **Errors**: Samsung error code with a description, e.g. `E101: Indoor - outdoor communication error`.
- **Firmware**: part numbers and build dates of the outdoor main board, its EEPROM, the inverter board and the MIM board.
- **All 94 FSV installer settings**, read on demand. The water law, DHW limits, heater settings and so on show up as read-only diagnostic entities.
- **Derived values**: flow ΔT, instantaneous and total COP, daily energy.
- **Unknown PDUs** are exposed as diagnostic `RAW` entities, and an optional **bus sniffer** helps decode them (see [PDU_MAP.md](PDU_MAP.md)).
- **Translations**: entity names, state texts and error texts are translated, and you switch language with **one line** (`language: en | pl`). Adding a language means adding one file (see [TRANSLATING.md](TRANSLATING.md)).

### Why read-only?

On F1/F2 the **Immergas controller is in charge**. It computes the water target from its own heating curve (menu Termoregulation R02–R05 + offset U03) and drives the Samsung side through the MIM board. Writes to the indoor unit on F1/F2 are simply ignored. This project therefore **never writes**. With `transmit: true` it only sends **READ** requests, to fetch the FSV values that are never broadcast. Set `transmit: false` for a pure listener.

To control the boiler, use [dd-modbus](https://github.com/jotuera/immergas-magis-pro-combo-dd-modbus) or [tt-bms](https://github.com/jotuera/immergas-magis-pro-combo-tt-bms). The F3/F4 remote-controller bus, where writes do work, is on the roadmap.

## Hardware

- **M5Stack Atom Lite** (ESP32) + **Atomic RS485 Base** (auto direction), or any ESP32 with an RS485 transceiver. For a transceiver with a DE/RE pin, set `flow_control_pin`.
- Pins in the example: **TX = GPIO19, RX = GPIO22**.
- Wiring: **F1 / F2** of the Magis indoor box (the MIM-B19N board terminals) → RS485 **A / B**. If you only see bad frames, swap A/B.
- Bus: **9600 baud, 8E1**.
- **The MIM-B19N rotary switch must be set to 1** (factory 0). At 0 the MIM stays silent and nothing can be read.

## Setup

1. Copy `immergas-magis-pro-combo-f1f2-nasa.yaml` and `secrets.yaml.example` (renamed to `secrets.yaml`) to your ESPHome folder, then fill in Wi-Fi, the API key and the OTA password.
2. Pick the language: `substitutions: language: en` or `pl`.
3. Install. The component is fetched from this repo by `external_components`.
4. After about 30 s the FSV values are read; after that they are re-read every `poll_interval`, or on demand with the **Read FSV Now** button.

### Component options

```yaml
immergas_nasa:
  uart_id: nasa_uart
  language: en               # en | pl | ... (translations/<code>.yaml)
  transmit: true             # false = listen only
  poll_interval: 30min       # FSV re-read period
  republish_interval: 60s    # unchanged values are re-sent to HA at most this often
  # address: 80.FF.00        # own NASA address (JIG tester class)
  # indoor_address: 20.00.01
  # outdoor_address: 10.00.00
  # read_address: B2.00.20   # destination of READ requests
  # flow_control_pin: GPIO23
```

Entities are declared by **key** (the name, unit and decoding come from the catalog), by **FSV code**, or as a raw **PDU**:

```yaml
sensor:
  - platform: immergas_nasa
    key: flow_temp_out                 # name from the selected language
  - platform: immergas_nasa
    fsv: 2011                          # "FSV 2011 - Heating water law - outdoor temp point 1"
  - platform: immergas_nasa
    pdu: 0x8032                        # anything else: raw value, diagnostic
    source: outdoor
    name: "NASA Outdoor PDU 8032"
```

You can override any field, e.g. `name:`, `filters:` or `entity_category:`. All keys are listed in [PDU_MAP.md](PDU_MAP.md).

## Sniffer

Turn on the **NASA Sniffer** switch (diagnostic, off after reboot). Every value change is then logged as `10.00.00 8238: 0 -> 30` (tag `sniff`, level INFO), every whole frame at level DEBUG (`logger: level: DEBUG`), and the last frame and change are shown in two text sensors. This is the easiest way to contribute decodings: capture a log during a defrost, a DHW run or frost, then open an issue.

## Notes

- **Migrating from the `samsung_nasa` (Beormund) component**: the English sensor names here match the usual names from that setup. Keep the same `esphome: name:` and Home Assistant keeps those entities and their history. Writable entities (climate, numbers, selects, switches) are not offered here.
- Values that a Magis Combo never sends (pressures, suction, the water sensors on the outdoor unit…) are left out. See the end of [PDU_MAP.md](PDU_MAP.md).
- The component's own address is `80.FF.00`. If your bus already has a device using it, change `address:`.

## Disclaimer

This is an independent hobby project, not affiliated with Samsung or Immergas. You use it at your own risk. The default configuration only listens and sends READ requests; it does not change any setting of your heat pump.

## License

MIT © 2026 JoTu. See [LICENSE](LICENSE) and [NOTICE](NOTICE) (MIT projects consulted for protocol facts).
