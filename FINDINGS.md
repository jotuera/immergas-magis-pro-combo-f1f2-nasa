# Findings — what the bridge proved on a Magis

🇬🇧 English | 🇵🇱 [Polski](FINDINGS.pl.md)

The goal of this project was to drive **Samsung outdoor-unit functions that the Immergas Modbus does not
expose** (quiet mode, an ODU-side heating curve, an ODU-side maximum heating temperature), by injecting NASA
write frames on the F1/F2 bus, **without touching the traffic Immergas controls**. The honest result, on a
**Magis Combo 9 Plus V2 + Audax Pro 9 V2**, is **negative but thorough**. This file documents it so no one has
to repeat the dead ends.

## The write path works (for "soft" registers)

Writing an FSV / enum to the indoor unit (`20.00.01`) **reaches it and persists**. Proven rigorously: a WRITE
(`type 0x12`) is answered by the MIM (`src 20.00.01`, `type 0x15` response) carrying the stored value, and an
independent READ confirms the new value afterwards. Verified on:

- **FSV 1012** `0x424B` (cooling min temp, inert) — write persists.
- **FSV 1031** `0x424E` (water-out max heating) — write persists.
- **FR control enable** `0x40A7` — enum `0 → 1` written and read back as `1`.

The parser only trusts frames from the devices (`20.00.01` / `10.00.00`), never its own echo (`80.FF.00`), so
these confirmations are real, not a mirror of what was sent.

## …but every functional lever is overwritten or rejected

| Lever | Register | Result |
|---|---|---|
| Water-out **max** heating (FSV 1031) | `0x424E` | Write persists, but the **Immergas setpoint `0x4247` overrides it**: with demand 31.7 °C the flow `0x4238` climbed past the 22 °C max toward the demand. Temperature *limits* do nothing. |
| **FR ratio** (FSV 5051) | `0x42F1` | Write **rejected** — stays `0xFFFF`. Enable (`0x40A7`) flips to 1, but the ratio value never takes, and the compressor `0x8238` does not change. Matches the manual: *"Only tank-integrated hydro units."* |
| **Quiet mode** | `0x428D` | Driven by the **Immergas night function U11** (D+/D- → MIM broadcasts `0x428D` → ODU drops `0x8239` 64→19). Not a Samsung-only lever; already available through Immergas. |
| **Power peak** (5041) | `0x40A4` | Enable only; acts only on an external peak-demand signal (not wired). Write holds, does nothing. |
| **Outing / Away** | `0x406D` | Needs the away-mode trigger; not present. Write holds, does nothing. |
| **Smart Grid** (5091) | `0x411C` | Needs an SG signal; not wired. Write holds, does nothing. |

**Conclusion:** on F1/F2 the **Immergas controller owns the loop**. It computes the water target from its own
heating curve and drives the Samsung side through the MIM. Anything it does not control either needs an
external trigger that is not present, or is rejected by the unit. There is no safe *and* effective Samsung-only
write lever on this installation. The quiet mode you *can* get is the Immergas U11 night function.

## OFF-LIMITS — never write these

The component's whitelist permits only FR control (`0x40A7` / `0x42F1`) and the one configured FSV. It
deliberately refuses everything below, and so should you:

- **Compressor frequency** registers `0x8236`–`0x8239`, **EEV position** `0x8229` — writing these commands the
  compressor / expansion valve directly. Risk of hardware damage.
- **Flow sensor** `0x42E9` (`VAR_IN_FLOW_SENSOR_CALC`) — it **feeds flow protection** (related `0x42E8`,
  `0x4102`, ODU `0x803B`, errors E911/E912/E913). A fake flow could wake the Samsung flow-check (now idle at
  the `65535` "no sensor" sentinel) and let it make protection decisions on an invented number → compressor
  running dry → destroyed heat exchanger. Irreversible, cosmetic upside. Immergas does not send `0x42E9`.
- 4-way / hot-gas valves, defrost, base heater and any other ODU actuator / protection register.

## The useful path instead: COP, read-only

The constructive goal (knowing the heat pump's efficiency) is better met **without any write**, by computing
COP in Home Assistant from values already on the buses:

```
thermal_power_W = (flow_l_per_h / 3600) * 4186 * (0x4238 - 0x4236)
COP            = thermal_power_W / 0x8413
```

(flow from the T-/T+ bus, ΔT and electrical power from the F1/F2 reader.) This is more accurate than the
outdoor unit's own internal meter. The read-only reader for that is
[f1f2-nasa](https://github.com/jotuera/immergas-magis-pro-combo-f1f2-nasa).
