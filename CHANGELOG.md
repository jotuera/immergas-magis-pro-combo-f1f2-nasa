# Changelog

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
