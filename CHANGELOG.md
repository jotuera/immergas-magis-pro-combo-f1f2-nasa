# Changelog

## v2.2.0

- **Editable FSV** via a new `number` platform: `- platform: f1f2_nasa` with `fsv:` (or `pdu:`) under `number:` creates a slider that reads the value and, on change, **writes it back** to the indoor unit. Writing goes through the same gate (`allow_write` / Arm) and is allowed only for ids you expose this way (plus the two fixed levers); everything else stays refused.
- New package **`packages/f1f2_outing_targets.yaml`**: the 9 Outing-mode targets (FSV 5011–5019) as ready-made sliders.
- **Hard-coded OFF-LIMITS list** in the hub: compressor frequency, EEV position, flow sensor / flow-protection chain, 4-way / hot-gas valve, defrost, base heater and the 2-/3-way valve (see FINDINGS.md) can now never be made writable through `number:`, even by mistake — `register_writable()` refuses and logs them, independently of what any `number:` entity declares. Closes a gap where the "never writable" claim for those registers depended only on the shipped packages not exposing them, not on the component enforcing it.

## v2.1.1

- **Fixed the combined example** (`immergas-magis-pro-combo-f1f2-nasa-combined.yaml`): it now uses a **single** `f1f2_nasa` component with the `f1f2_control` package (the earlier version wrongly put two NASA components on one shared UART, which cannot work — ESPHome UART reads are destructive). The full `f1f2_fsv` package is commented out; only the 9 Outing-mode settings (FSV 5011–5019) that drive the working lever are exposed. README "one device" guidance corrected to match.

## v2.1.0

- **Control is now native in the `f1f2_nasa` reader** — one component that reads *and* writes, so a single device can do both on one UART (two NASA components cannot share a UART in ESPHome). New package **`packages/f1f2_control.yaml`**: **Arm write** gate (`allow_write`, off by default), **Outing mode** (`0x406D`) and **FR – limit compressor** (`0x40A7`). Writes are gated by the arm switch **and** a whitelist in the hub (only `0x406D` / `0x40A7`); compressor / EEV / protection / water-flow / setpoint are refused. The hub reuses its existing silence-window transmit path; `f1f2_transmit: "true"` is required.
- This is the recommended single-device read+write path. The separate `nasa_bridge` component stays for a dedicated control node, but do not run both NASA components on one shared UART.

## v2.0.0

The repo now ships **two components**. `f1f2_nasa` stays the read-only reader (the default, unchanged); a second component **`nasa_bridge`** adds an **optional, gated control path** (parallel tap + inject). The reader's entities and names are unchanged, so existing installs keep their history.

- **Control path merged in** (component `nasa_bridge`, packages `bridge_core` / `bridge_control` / `bridge_status_*` / `bridge_fsv_*` / `bridge_panel_*` / `bridge_diagnostics`, language `lang/bridge_*`). Working levers on a Magis: **Outing mode** (`0x406D`) and **FR – limit compressor** (`0x40A7`, confirmed 35→30 Hz); plus a read-only **Samsung error code**. Safety: writes need the **Arm write** gate (`allow_write`, off by default) **and** a short whitelist; compressor / EEV / protection / flow can never be written; the FR ratio is capped at 100 % (derate only). See README "Control", `FINDINGS.md` and `FSV_MAP.md`.
- **One device, reader + control**: both sit on the same F1/F2 bus and **share one UART** (only the bridge transmits). Example: `immergas-magis-pro-combo-f1f2-nasa-combined.yaml`.

**Upgrading from v1.x:** change `ref:` to `v2.0.0`. A reader-only install needs nothing else; the control path is only created if you add its packages.

## v1.3.0

- **Ids for derived values**: `outdoor_current` (0x8217) and `mains_voltage` (0x82ED) now carry ids (`f1f2_outdoor_current`, `f1f2_mains_voltage`), so you can build your own template sensors from them (e.g. electrical power = current × voltage). No entities change.

## v1.2.0

- **Sniffer**: the frame log also shows the bytes seen between frames: their hex in brackets (first 32), and `dt=<ms>` from the last of them to the start of the frame. On a Magis 4-5 such bytes precede every frame.

## v1.1.0

- **Sniffer timing**: with the sniffer on and `f1f2_sniff` at DEBUG, every frame is logged with the device time, the silence before it and the number of bytes skipped between frames: `RX t=<ms> gap=<ms> skip=<bytes> <decoded> | <hex>`. Frames with a wrong CRC (`BAD`) and our own READ requests (`TX t=…`) are logged too. While the sniffer is on, the main loop runs at full speed, so the times are accurate to about 1 ms. Useful for measuring the bus timing and for complete frame captures.

## v1.0.0

One-application layout shared with [dd-modbus](https://github.com/jotuera/immergas-magis-pro-combo-dd-modbus) (`dd_`) and [tt-bms](https://github.com/jotuera/immergas-magis-pro-combo-tt-bms) (`tt_`): every file, id and substitution of this repo starts with `f1f2_`, so the three buses can later run on one device.

- **Configuration split into packages**: `packages/f1f2_core`, `f1f2_indoor`, `f1f2_outdoor`, `f1f2_info`, `f1f2_fsv`, `f1f2_computed`, `f1f2_diagnostics`, plus the optional `f1f2_ehs_extra`. The main YAML holds only the device section and the packages list, which is fetched from GitHub.
- **Language is picked in the packages list**: `packages/lang/f1f2_en.yaml` or `f1f2_pl.yaml`.
- **Component renamed** `immergas_nasa` → **`f1f2_nasa`** (YAML key and `platform: f1f2_nasa`). Its files are renamed with the `f1f2_` prefix as well: `f1f2_frame`, `f1f2_hub`, `f1f2_catalog.yaml`, `f1f2_fsv_catalog.yaml`, `translations/f1f2_<code>.yaml`. The C++ namespace is now `f1f2_nasa`, and the log tags are `f1f2_nasa` / `f1f2_sniff`.
- **Settings are substitutions**: `f1f2_uart_tx_pin`, `f1f2_uart_rx_pin`, `f1f2_transmit`, `f1f2_poll_interval`, `f1f2_indoor_address`, `f1f2_outdoor_address`, and the names of the template entities `n_f1f2_*`.
- **Values a Magis never provides** moved to the optional `f1f2_ehs_extra` package: zone 2 room temp / target, flow rate, produced power / energy, COP instant / total. A READ returns "not available" for them on a Magis. The deprecated `raw_8239` was removed from the default packages; use `frequency_limit`.
- **Entity names are unchanged.** Home Assistant keeps the entities and their history when you upgrade with the same device name and language.

**Upgrading from v0.x:** replace your YAML with the new main file. Copy over your device name, Wi-Fi / static IP, secrets and language. If you used your own `platform: immergas_nasa` entities, rename them to `platform: f1f2_nasa`, and rename `immergas_nasa_id:` to `f1f2_nasa_id:`.

## v0.11.0

- `0x8239` decoded as the compressor frequency limit (`frequency_limit`). It drops to 19 Hz during the Immergas night function U11.
- New raw candidates `0x80B1` and `0x428D`.

## v0.10.0

- Address discovery: the `NASA Devices` entity lists the units on the bus. After 60 s the log warns if the indoor / outdoor address does not match.

## v0.9.0

- First public beta: own Samsung NASA component, 94 FSV, translations EN / PL.
