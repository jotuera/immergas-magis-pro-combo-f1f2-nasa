# Samsung EHS error codes (F1/F2 / NASA side)

The Magis Pro / Combo V2 runs a Samsung EHS core inside its outdoor unit (ODU), and the F1/F2 bus is that
unit's NASA bus. When the Samsung side faults, an error code appears on the dedicated Samsung wired remote
(e.g. **MWR-WW10N**). Immergas reads the same side through the MIM, so these codes are useful when
diagnosing the F1/F2 bus.

The codes below are transcribed verbatim from the official **Samsung EHS** manual (indoor unit
AE090JNYDEH and relatives), "Troubleshooting" section. This is the user-facing subset (sensors,
communication, pump/flow); the full outdoor-unit service list is larger.

> Source: *Samsung EHS owner's manual (SPLIT), PL, EHS Indoor AE090JNYDEH_EU_IB_PL_05130A-00*,
> pages POLSKI-50…52. Descriptions translated from Polish.

## Temperature sensors

| Code | Meaning |
|------|---------|
| **E122** | Short or open circuit on the EVA inlet temperature sensor |
| **E123** | Short or open circuit on the EVA outlet temperature sensor |
| **E653** | Short or open circuit on the wired-remote temperature sensor |
| **E654** | Data error in the remote controller (memory read/write error) |
| **E901** | Short or open circuit on the water inlet temperature sensor |
| **E902** | Short or open circuit on the plate heat-exchanger outlet temperature sensor |
| **E903** | Short or open circuit on the water outlet temperature sensor (backup heater) |
| **E904** | Short or open circuit on the water-tank temperature sensor |
| **E916** | Short or open circuit on the mixing-valve temperature sensor (when FSV #4041 = "1") |

Sensor locations (from the manual diagram): **E901** water inlet, **E902** plate HX outlet (Tw1/Tw2),
**E903** water outlet / backup heater (Tw3), **E904** water tank, **E122** EVA inlet (reference inlet),
**E123** EVA outlet (reference outlet), **E653/E654** wired remote.

## Communication

| Code | Meaning |
|------|---------|
| **E601** | Communication error between the remote controller and the Hydro unit |
| **E604** | Monitoring error between the remote controller and the Hydro unit |
| **E653** | Remote-controller temperature sensor disconnected or damaged |
| **E654** | Data error in the remote controller (memory read/write error) — data transfer problem between the Micom module and IC07 (EEPROM) |

## Water pump and flow switch

| Code | Meaning |
|------|---------|
| **E911** | Flow-switch OFF error. Triggered when the flow switch turns off within 30 s of the water-pump signal going on (start-up), or within 15 s after start-up. Indicates insufficient water flow: < 7 l/min (✴✴090✴✴ models) / < 12 l/min (✴✴160✴✴ models). |
| **E912** | Flow-switch ON error. Triggered when the flow switch turns on within 10 min while the water-pump signal is off. Water flow > 7 l/min (✴✴090✴✴) / > 12 l/min (✴✴160✴✴). |

---

Polish version: [ERROR_CODES.pl.md](ERROR_CODES.pl.md). For FSV mapping (e.g. #4041) see
`components/f1f2_nasa/f1f2_fsv_catalog.yaml` and [FSV_MAP.md](FSV_MAP.md).
