# F1/F2 registers — what the Immergas controller commands vs. the Samsung outdoor unit

🇬🇧 English | 🇵🇱 [Polski](./register_control.pl.md)

Classification of every F1/F2 register read by the `f1f2_nasa` component, based on this project's
config and logs: cross-bus audit of all three buses (2026-10-01), night-function (U11) test
(2026-10-13), active water-target analysis, and the 0x428D write test (2026-10-22).

**"Controlled by Immergas"** = the command originates from the Immergas controller and is
visible/settable on the **D+/D-** or **T-/T+** Modbus bus. Writing it on F1/F2 would fight the boiler
(proven: writes to the indoor unit "go into the void", a directed 0x428D write to the ODU was ignored).

## A. Commanded by the Immergas controller (D+/D-, T-/T+)

| Entity | PDU | What Immergas commands / evidence |
|---|---|---|
| Water outlet target (active) | 0x4247 | Immergas curve R02–R05 + offset U03; = D04 on D+/D- and T-/T+ |
| DHW target temperature | 0x4235 | boiler DHW setpoint (D05 on D+/D-) |
| Zone 1 target temperature | 0x4201 | Immergas zone setpoint |
| Zone 2 target temperature | 0x42D6 | same (not broadcast here) |
| Operation mode (4 = heat) | 0x4001 | Immergas system mode (Winter CH+DHW, register 2000 on D+/D-) |
| DHW mode | 0x4066 | Immergas DHW mode |
| DHW on | 0x4065 | DHW demand (Dominus / panel / thermostat) |
| Zone 1 / 2 on | 0x4000 / 0x411E | zone heat demand (Salus / panel → Immergas) |
| Status bits (0x403 = demand) | 0x440E | Immergas heat-demand flag |
| Control reference (water out) | 0x406F | Immergas configuration |
| Circulation pump | 0x4089 | the pump is driven by the Immergas board (A03/A04, flow D14 on T-/T+) |
| **Night function:** limit = 19 Hz | 0x8239 (=19) | Immergas U11: the boiler writes 0x1E W2 = 30 % on D+/D- |
| **Night function:** limit flag | 0x80B1 | effect of U11 (while the compressor runs under the limit) |
| **Night function:** carrier | 0x428D (=0x011E) | the MIM broadcasts the U11 value in its own cycle |

## B. The Samsung outdoor unit's own — NOT controlled by Immergas

Immergas only sends a **demand** (heat / don't heat, a setpoint); the outdoor unit decides how to meet
it. Immergas does not set the registers below.

| Entity | PDU | Decided by |
|---|---|---|
| Compressor frequency (actual / ordered / target) | 0x8238 / 0x8236 / 0x8237 | ODU picks the Hz for the demand |
| Frequency limit 64 / 95 (natural) | 0x8239 | ODU regulation in Normal mode (NOT U11) |
| Fan speed (target / actual) | 0x823D / 0x82D9 | ODU |
| EEV position (outdoor / indoor) | 0x8229 / 0x4217 | ODU |
| 4-way / hot-gas valve | 0x801A / 0x8017 | ODU |
| Base (drain-pan) heater | 0x80AF | ODU (frost protection) |
| Defrost + stage | 0x402E / 0x8061 | ODU decides |
| Start sequence / restart inhibit | 0x8248 / 0x8249 | ODU protection logic |
| Compressor running | 0x8010 | ODU state |
| ODU driving / operation / fan state | 0x8001 / 0x8003 / 0x800D | ODU status |
| Error code / system error | 0x8235 | ODU |
| ODU temperatures: discharge, condenser, comp top, IPM, target discharge | 0x820A / 0x8218 / 0x8280 / 0x8254 / 0x8223 | ODU telemetry |
| Voltages, current, power, energy, capacity | 0x82ED / 0x823B / 0x24FC / 0x8217 / 0x8411 / 0x8413 / 0x8414 / 0x82E3 | ODU telemetry |
| Indoor capacity request / absolute | 0x4211 / 0x4212 | ODU / MIM |
| 2-way / 3-way valve | 0x408A / 0x4067 | MIM actuator; position follows DHW demand but the MIM drives it |
| **94 FSV installer settings** | 0x424A… | Samsung installer settings (MIM-E03 remote / defaults); not via Immergas Modbus, and the water-law FSV is overridden by the Immergas curve anyway |
| Firmware ODU / EEPROM / inverter / MIM | 0x0608 / 0x060C / 0x8601 | static info |

## C. Shared measurement (neither side "controls" it)

| Entity | PDU | Note |
|---|---|---|
| Outdoor temperature (+ copy) | 0x8204 / 0x420C | the same physical sensor as D06 / D79 — a reading, not control |
| Refrigerant inlet temperature | 0x4205 | = D24 on D+/D- and T-/T+ (reading) |
| Water outlet TW2 temperature | 0x4204 | reading |

## D. Not broadcast here / no data

Zone 2 (0x42D4 / 0x42D6), flow rate (0x42E9), produced power and energy (0x4426 / 0x4427) — the MIM
returns 65535 to a READ. Electric backup / booster heaters (0x406C / 0x4087) — a Magis uses the gas
boiler, not heaters. Plus a dozen undecoded raw PDUs (RAW, diagnostic).

---

**The gist:** Immergas controls **what and when** the pump should do — setpoints, modes, heat demand,
even the quiet mode (U11). The outdoor unit decides **how** to do it — frequency, fan, EEV, defrost —
and Immergas does not touch that. That is why the boiler is controlled over D+/D- and T-/T+, not by
writing on F1/F2.
