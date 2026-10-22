# F1/F2 registers NOT controlled by the Immergas Modbus buses

🇬🇧 English | 🇵🇱 [Polski](./registers_not_modbus.pl.md)

Registers of the F1/F2 bus (`f1f2_nasa` catalog) that the **Immergas Modbus buses (D+/D- and T-/T+)
do NOT control** — they belong to the Samsung outdoor unit and the MIM board. Immergas only sends a
demand (heat / don't heat, a setpoint); the values below are set by the ODU / MIM firmware itself.
Companion to [register_control.md](register_control.md).

Basis: cross-bus audit (2026-10-01), U11 test (2026-10-13), write test (2026-10-22).

Columns:
- **source** — where the value appears on F1/F2: `ODU` = broadcast 10.00.00→B0.00.FF; `MIM` =
  broadcast 20.00.01→B3.00.FF; `request` = only in reply to our READ (FSV).
- **kind** — `telemetry` (measurement, read-only), `actuator/state` (ODU drives an element),
  `parameter` (a setting), `info` (static).

## 1. Outdoor unit (ODU, 0x8xxx) — telemetry, read-only

| Entity | PDU | source | kind |
|---|---|---|---|
| Discharge temperature | 0x820A | ODU | telemetry |
| Condenser temperature (outdoor coil) | 0x8218 | ODU | telemetry |
| Compressor top temperature | 0x8280 | ODU | telemetry |
| IPM module temperature | 0x8254 | ODU | telemetry |
| Target discharge temperature | 0x8223 | ODU | telemetry (target computed by the ODU) |
| Outdoor unit current | 0x8217 | ODU | telemetry |
| Heat pump voltage | 0x24FC | ODU | telemetry |
| Mains voltage | 0x82ED | ODU | telemetry |
| DC-link voltage | 0x823B | ODU | telemetry |
| Power consumed / outdoor power | 0x8413 / 0x8411 | ODU | telemetry |
| Energy consumed (total) | 0x8414 | ODU | telemetry |
| Outdoor unit capacity | 0x82E3 | ODU | info |
| Linked indoor units | 0x0207 | ODU | info |

## 2. Outdoor unit — elements and states driven by the ODU (not Modbus)

| Entity | PDU | source | kind |
|---|---|---|---|
| Compressor frequency (actual) | 0x8238 | ODU | actuator (ODU picks the Hz) |
| Ordered / target frequency | 0x8236 / 0x8237 | ODU | actuator |
| Frequency limit 64/95 (natural) | 0x8239 | ODU | ODU control (NOT U11; 19 = U11, see register_control.md) |
| Fan speed (target / actual) | 0x823D / 0x82D9 | ODU | actuator |
| Main EEV position | 0x8229 | ODU | actuator |
| 4-way valve | 0x801A | ODU | actuator |
| Hot-gas valve | 0x8017 | ODU | actuator |
| Base (drain-pan) heater | 0x80AF | ODU | actuator (frost protection) |
| Defrost | 0x402E | ODU | state (ODU decides) |
| Defrost stage | 0x8061 | ODU | state |
| Compressor start sequence | 0x8248 | ODU | state (start-up logic) |
| Restart inhibit | 0x8249 | ODU | state (protection) |
| Compressor running | 0x8010 | ODU | state |
| ODU driving mode | 0x8001 | ODU | state |
| ODU operation state (heat/cool) | 0x8003 | ODU | state |
| Fan operation state | 0x800D | ODU | state |
| Error code / system error | 0x8235 | ODU | state |
| "Running under night limit" flag | 0x80B1 | ODU | state (effect of U11 — triggered by Immergas, set by the ODU) |

## 3. Indoor unit / MIM (0x40xx / 0x42xx) not controlled by Modbus

| Entity | PDU | source | kind |
|---|---|---|---|
| Indoor EEV position | 0x4217 | MIM | actuator (ODU/MIM) |
| 2-way valve | 0x408A | MIM | actuator |
| 3-way valve + position | 0x4067 | MIM | actuator (follows DHW demand, but the MIM drives it) |
| Indoor capacity request | 0x4211 | MIM | telemetry |
| Indoor absolute capacity | 0x4212 | MIM | info |
| 0x428D (U11 carrier) | 0x428D | MIM | parameter (value comes from Immergas U11, but the MIM broadcasts it) |

## 4. FSV installer settings (94)

**Samsung** settings, read only on our READ (`request`). Immergas does not set them over Modbus;
moreover the water-law FSV (2011–2022) is inactive on a Magis, because the Immergas curve computes the
water target. Full list in `components/f1f2_nasa/f1f2_fsv_catalog.yaml` and `PDU_MAP.md`.

| Range | PDU | meaning |
|---|---|---|
| Temperature limits 1011–1052 | 0x424A–0x4253 | water & room min/max (cool / heat / DHW) |
| Water law 2011–2093 | 0x4254…/0x4093… | heating/cooling curve (overridden by Immergas here) |
| DHW 3011–3083 | 0x4097…/0x4260… | DHW mode, hysteresis, heater, disinfection, solar |
| Heaters / boiler 4011–4061 | 0x409E…/0x426D… | backup heater, backup boiler, mixing valve, pump |
| Modes 5011–5094 | 0x4273…/0x411B… | outing mode, saving, power peak, PV, Smart Grid |

## 5. Firmware / static info

| Entity | PDU | source |
|---|---|---|
| Outdoor unit firmware | 0x0608 (ODU) | ODU |
| Outdoor unit EEPROM | 0x060C | ODU |
| Inverter firmware | 0x8601 | ODU |
| Indoor unit (MIM) firmware | 0x0608 (indoor) | MIM |

## 6. RAW — undecoded, unknown kind

0x0402, 0x0411–0x0415, 0x4006, 0x440A, 0x4604 (indoor); 0x041B, 0x8201, 0x823F, 0x8243, 0x824B,
0x824C, 0x82E1, 0x840F, 0x8417 (outdoor). They do not come from Immergas (no match on D+/D- or T-/T+);
to be decoded with the sniffer. Details: `NASA_PDU_crossref.md`, `PDU_MAP.md` (in the research repo).

---

**Note on writing:** the telemetry registers (kind "telemetry/info") are read-only — a write does
nothing. The ODU-side "actuator/state" registers are the running parameters of an inverter compressor
(frequency, EEV, valves, protections) — writing a wrong value risks damage or unsafe operation. That
is why none of them is nominated here as a "safe write target".
