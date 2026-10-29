# Immergas Magis Pro / Combo — heat-pump data from the Samsung NASA bus (F1/F2), ESPHome

🇬🇧 English | 🇵🇱 [Polski](README.pl.md)

<p align="center">
  <img src="https://img.shields.io/github/v/tag/jotuera/immergas-magis-pro-combo-f1f2-nasa?label=version&color=blue" alt="Version">
  <img src="https://img.shields.io/badge/platform-ESP32-41BDF5.svg" alt="ESP32">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-green.svg" alt="License: MIT"></a>
</p>

Read everything the **outdoor unit** of an Immergas Magis Pro / Combo V2 (a Samsung EHS unit, e.g. **Audax Pro V2**) and the **Samsung MIM-B19N interface board** inside the Magis exchange on the **F1/F2** bus. You get it in **Home Assistant** through an **M5Stack Atom + RS485** running ESPHome. No cloud is involved and no third-party NASA component is needed: the project ships its own component, `f1f2_nasa`.

> Part of the family [`jotuera/immergas-magis-pro-combo-*`](https://github.com/jotuera?tab=repositories): **dd-modbus** (D+/D-, Dominus/panel emulator), **tt-bms** (T-/T+, BMS Modbus) and **f1f2-nasa** (this repo). All use the same layout (`packages/<bus>_*.yaml`, ids and substitutions with the bus prefix `dd_` / `tt_` / `f1f2_`), so they can run together on one device.
>
> The `f1f2_nasa` component **reads** the bus by default, and can also **write** a few whitelisted settings once you add the control packages and arm it (see [Control](#control-optional-readwrite)).

> **v2.5.2.** Tested on a **Magis Combo 9 Plus V2 + Audax Pro 9 V2**. Feedback from other Magis Pro / Combo installs is very welcome. Upgrading: see [CHANGELOG.md](CHANGELOG.md).

## What you get

- **Temperatures**: flow / return, active water target, outdoor, discharge, compressor top, condenser out, refrigerant liquid, IPM.
- **Compressor**: actual / ordered / target frequency, frequency limit (drops to 19 Hz during the Immergas night function U11), start sequence stage, restart inhibit, current, power, energy (kWh), DC-link and mains voltage.
- **Outdoor fan and valves**: target / actual RPM, main EEV position (0–2000), 4-way valve, hot-gas valve, base heater, defrost mode and stage.
- **State**: outdoor driving mode (Stop / Safety / Normal / Defrost …), indoor operation mode, pump, booster / backup heater.
- **Errors**: Samsung error code with a description, e.g. `E101: Indoor - outdoor communication error`.
- **Firmware**: part numbers and build dates of the outdoor main board, its EEPROM, the inverter board and the MIM board.
- **All 94 FSV installer settings**, read on demand. The water law, DHW limits, heater settings and so on show up as read-only sensors.
- **Derived values**: flow ΔT, energy consumed today.
- **Unknown PDUs** are exposed as diagnostic `RAW` entities, and an optional **bus sniffer** helps decode them (see [PDU_MAP.md](PDU_MAP.md)).
- **Address discovery**: a `NASA Devices` entity lists every unit on the bus, and the log tells you if the indoor / outdoor address needs changing (see [Finding the addresses](#finding-the-addresses)).
- **Translations**: entity names, state texts and error texts are translated; you pick the language with **one line** in the packages list (`f1f2_en` / `f1f2_pl`). Adding a language means adding one file (see [TRANSLATING.md](TRANSLATING.md)).

### Read-only by default

On F1/F2 the **Immergas controller is in charge**. It computes the water target from its own heating curve (menu Termoregulation R02–R05 + offset U03) and drives the Samsung side through the MIM board. The `f1f2_nasa` reader therefore **only reads**: it sends **READ** requests to fetch the FSV values that are never broadcast, and nothing else. Set `f1f2_transmit: "false"` for a pure listener (no READs either).

A few things on the Samsung side *can* be set — see [Control](#control-optional-readwrite) — but most writes are ignored or overridden by Immergas, which keeps the control loop. To control the boiler itself, use [dd-modbus](https://github.com/jotuera/immergas-magis-pro-combo-dd-modbus) or [tt-bms](https://github.com/jotuera/immergas-magis-pro-combo-tt-bms).

Which registers the Immergas controller commands (visible/settable on the D+/D- and T-/T+ Modbus buses) and which the Samsung outdoor unit owns on its own is documented in [register_control.md](register_control.md) (controlled by Immergas) and [registers_not_modbus.md](registers_not_modbus.md) (not controlled by Modbus). Based on a cross-check of all three buses.

## Hardware

- **M5Stack Atom Lite** (ESP32) + **Atomic RS485 Base** (auto direction), or any ESP32 with an RS485 transceiver. For a transceiver with a DE/RE pin, set `flow_control_pin` in `f1f2_core`.
- Pins in the example: **TX = GPIO19, RX = GPIO22** (substitutions `f1f2_uart_tx_pin` / `f1f2_uart_rx_pin`).
- Wiring: **F1 / F2** of the Magis indoor box (the MIM-B19N board terminals) → RS485 **A / B**. If you only see bad frames, swap A/B.
- Bus: **9600 baud, 8E1**.
- **The MIM-B19N rotary switch must be set to 1** (factory 0). At 0 the MIM stays silent and nothing can be read.

## Setup

1. Copy `immergas-magis-pro-combo-f1f2-nasa.yaml` and `secrets.yaml.example` (renamed to `secrets.yaml`) to your ESPHome folder, then fill in Wi-Fi, the API key and the OTA password.
2. Pick the language: in the packages list keep **one** of `packages/lang/f1f2_en.yaml` or `packages/lang/f1f2_pl.yaml`.
3. Install. The packages and the `f1f2_nasa` component are fetched from this repo.
4. After about 30 s the FSV values are read; after that they are re-read every `f1f2_poll_interval`, or on demand with the **Read FSV Now** button.

### Packages

| File | Content |
|---|---|
| `packages/f1f2_core.yaml` | UART, the `f1f2_nasa` hub, the component download, `NASA Devices` (**required**) |
| `packages/f1f2_indoor.yaml` | indoor unit: water temperatures, active water target, DHW, zone 1, pump, heaters, modes |
| `packages/f1f2_outdoor.yaml` | outdoor unit: compressor, frequency limit, fan, EEV, refrigerant temperatures, power, energy, errors |
| `packages/f1f2_info.yaml` | firmware versions, capacity class, linked indoor units |
| `packages/f1f2_fsv.yaml` | all 94 FSV installer settings + **Read FSV Now** button |
| `packages/f1f2_computed.yaml` | flow ΔT, energy consumed today |
| `packages/f1f2_diagnostics.yaml` | undecoded PDUs, bus statistics, sniffer |
| `packages/f1f2_ehs_extra.yaml` | optional: zone 2 (temps + power), flow rate, produced power / energy, COP, PV control, Smart Grid — **not on a Magis** (always OFF / not available), useful on other Samsung EHS installs |
| `packages/lang/f1f2_en.yaml`, `f1f2_pl.yaml` | language of the reader (pick one) |

**Control** (optional, see [Control](#control-optional-readwrite)); needs `f1f2_transmit: "true"`:

| File | Content |
|---|---|
| `packages/f1f2_control.yaml` | **writes**: NASA connection + Emulate EHS panel gates + Active ODU control arm + Outing mode (`0x406D`) + FR compressor limit (`0x40A7`) |
| `packages/f1f2_outing_targets.yaml` | the 9 Outing-mode targets (FSV 5011–5019) as **editable sliders** (`number`) |

### Versions

The example pins everything to a release tag (`ref: v2.5.2`; the cores download the components with the same tag). Change `ref:` to a newer tag to upgrade, or to `main` to follow the development version.

### Settings (substitutions)

Put any of these under `substitutions:` in your main YAML:

```yaml
substitutions:
  f1f2_uart_tx_pin: GPIO19
  f1f2_uart_rx_pin: GPIO22
  f1f2_transmit: "true"            # "false" = listen only (no FSV values)
  f1f2_poll_interval: 30min        # FSV re-read period
  f1f2_indoor_address: "20.00.01"  # see "Finding the addresses"
  f1f2_outdoor_address: "10.00.00"
  n_f1f2_flow_delta: "My delta T"  # names of the template entities (n_f1f2_*)
```

Your own entities use the platform `f1f2_nasa`, declared by **key** (the name, unit and decoding come from the catalog), by **FSV code**, or as a raw **PDU**:

```yaml
sensor:
  - platform: f1f2_nasa
    key: flow_temp_out                 # name from the selected language
  - platform: f1f2_nasa
    fsv: 2011                          # "FSV 2011 - Heating water law - outdoor temp point 1"
  - platform: f1f2_nasa
    pdu: 0x8032                        # anything else: raw value, diagnostic
    source: outdoor
    name: "NASA Outdoor PDU 8032"
```

You can override any field, e.g. `name:`, `filters:` or `entity_category:`. All keys are listed in [PDU_MAP.md](PDU_MAP.md).

### Running on one device with the other buses

All three projects use bus prefixes for every file, id and substitution, so their package lists can be put next to each other under one `packages:` with one device section (esp32, wifi, api, logger). Give each bus its own UART pins (an ESP32 has three hardware UARTs; set `logger: baud_rate: 0`).

## Finding the addresses

Every unit on the NASA bus has a 3-byte address `class.channel.address`:

| Address | Unit | Substitution |
|---|---|---|
| `10.00.00` | outdoor unit (Samsung EHS) | `f1f2_outdoor_address` |
| `20.00.01` | indoor unit = the MIM-B19N board in the Magis | `f1f2_indoor_address` |
| `80.FF.00` | this ESP (only sends READ requests) | `address:` of `f1f2_nasa` |

The defaults match a Magis Combo / Pro V2 with the MIM rotary switch at **1**. The last byte of the indoor address follows the rotary switch (rotary 1 → `20.00.01`); generic Samsung EHS hydro units often use `20.00.00`.

You don't have to guess. The component finds the units itself:

1. Install with the defaults.
2. Look at the **NASA Devices** entity (or the log line `Found device on the bus: ...`). It lists every unit that sends frames, e.g. `10.00.00 outdoor unit, 20.00.01 indoor unit`.
3. After 60 s the component compares the list with your settings. If they differ it logs a warning with the address it found, e.g. `indoor_address is 20.00.01, but that device is not on the bus. Found 20.00.00 instead - set 'indoor_address: 20.00.00'.`
4. Put that address in your substitutions (`f1f2_indoor_address: "20.00.00"`) and install again.

If the list stays empty, the log says so after 60 s. Check F1/F2 → A/B (swap A/B), 9600 8E1, and the MIM rotary switch (at 0 the bus is silent).

## Sniffer

Turn on the **NASA Sniffer** switch (diagnostic, off after reboot). Every value change is then logged as `10.00.00 8238: 0 -> 30` (tag `f1f2_sniff`, level INFO). At level DEBUG (`logger: logs: f1f2_sniff: DEBUG`) every frame is logged too, with the device time and the silence before it, e.g. `RX t=123456 gap=12 skip=5 [xxxxxxxxxx] dt=3 10.00.00 > B0.00.FF NOTIFY #42 ... | 3200...34` (`skip` = bytes seen between frames, their hex in brackets, `dt` = ms from the last of them to the frame) (also `BAD` frames with a wrong CRC and our own `TX` frames); while the sniffer is on, the main loop runs at full speed so the times are accurate to about 1 ms. The last frame and change are shown in two text sensors. This is the easiest way to contribute decodings: capture a log during a defrost, a DHW run or frost, then open an issue.

## Control (optional, read+write)

> **Read this first.** On a Magis the Immergas controller owns the loop: in testing almost every write was overridden or rejected. Two levers do work, and one read-only status is useful. The honest, per-register results are in [FINDINGS.md](FINDINGS.md) and [FSV_MAP.md](FSV_MAP.md). This is a research tool for your own unit, at your own risk — not a general control solution.

Control is **native** in `f1f2_nasa`: add **`packages/f1f2_control.yaml`** to the normal reader config and set `f1f2_transmit: "true"`. The one hub reads richly *and* writes, on one UART. What you get:

- **Active ODU control** (`switch`) — the master gate. Nothing is ever transmitted to the pump until this is ON (`allow_write`). Off after every reboot.
- **Outing mode** (`0x406D`) — activates Samsung away mode; the one lever that can take over the heating target (FSV 5011–5019 set its temperatures). On a Magis it may still be overridden by Immergas.
- **FR – limit compressor** (`0x40A7`) — **confirmed working**: caps the compressor at the built-in ~50 % ratio (verified 35 Hz → 30 Hz under load). An eco / quiet lever; it only bites while the compressor runs above ~32 Hz. The custom ratio value (`0x42F1`) is rejected by a Magis, so you get the fixed default.
- **Editable FSV** — add **`packages/f1f2_outing_targets.yaml`** for the 9 Outing-mode targets (FSV 5011–5019) as sliders, or declare your own `number:` with `platform: f1f2_nasa` and `fsv:` (see "Settings").
- The active Samsung **error code** is already in the reader (`system_error`, from `f1f2_outdoor`); descriptions in [ERROR_CODES.md](ERROR_CODES.md).

**Safety model** — three layered gates (switches), each needs the one above it:

1. **NASA connection** — master fuse. Off = the hub ignores the bus entirely (RX drained but not dispatched, no polls, no writes); **every** entity (sensors, binary sensors, text sensors, control) goes **unavailable** — the device is disconnected. Default on.
2. **Emulate EHS panel** — active readback. Off = no READ polling; the editable FSV sliders and the Outing/FR control switches go **unavailable**, the passive sensors stay live. Default on; turning it back on refreshes the readback right away.
3. **Active ODU control** — the write arm. Off after every reboot.

A WRITE leaves the device only if **all three** are on **and** the message is whitelisted (the two fixed levers, plus any id you expose as an editable `number`). On top of that a **hard block** (`is_hard_blocked_`) refuses the compressor, EEV, protection, water-flow, unit on/off and operation-mode registers no matter what, even a mistaken `number:` on one; the FR ratio is hard-capped at 100 % so it can only derate. With `f1f2_control.yaml` absent, none of these switches exist and the device cannot write.

One device does both — see `immergas-magis-pro-combo-f1f2-nasa-combined.yaml`.

## Notes

- **Migrating from the `samsung_nasa` (Beormund) component**: the English sensor names here match the usual names from that setup. Keep the same `esphome: name:` and Home Assistant keeps those entities and their history. Writable entities (climate, numbers, selects, switches) are not offered here.
- Values that a Magis Combo never sends (pressures, suction, the water sensors on the outdoor unit…) are left out. See the end of [PDU_MAP.md](PDU_MAP.md).
- The component's own address is `80.FF.00`. If your bus already has a device using it, set `address:` under `f1f2_nasa:`.

## Disclaimer

This is an independent hobby project, not affiliated with Samsung or Immergas. You use it at your own risk. The default (reader) configuration only listens and sends READ requests; it does not change any setting of your heat pump. The optional control path ([Control](#control-optional-readwrite)) can write a few whitelisted settings, but only after you add its packages and turn on the **Active ODU control** switch — review the safety model and [FINDINGS.md](FINDINGS.md) before using it.

## License

MIT © 2026 JoTu. See [LICENSE](LICENSE) and [NOTICE](NOTICE) (MIT projects consulted for protocol facts).
