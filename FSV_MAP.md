# Installer FSV map — Immergas Magis Combo V2 + Samsung NASA

🇬🇧 English | 🇵🇱 [Polski](FSV_MAP.pl.md)

A full map of **all 94 FSV installer settings** on the Samsung NASA bus (F1/F2) and their status **on this particular install** (Magis Combo V2 Plus + Audax Pro, the Immergas controller owns the loop). The project has been slimmed down: **only what actually works is exposed**; the rest is **commented out** in the packages (not deleted — kept with a note for reference / to restore). See also [FINDINGS.md](FINDINGS.md).

> **Why is so much disabled?** On a Magis the **Immergas** controller computes the setpoint (`0x4247`) and owns the loop, pump and DHW; the Samsung side is blind to the DHW tank (`0x4237`=0) and to the pump (`0x4089`=OFF while water flows). Most FSVs are either **overwritten**, **inert** (the feature's element is missing), or belong to **hardware absent** on this install.

## Summary

- ✅ **Exposed and working: 9** — the outing-mode targets 5011–5019 (Configuration). Plus the **non-FSV** levers in Controls: outing mode `0x406D` and FR control `0x40A7`.

- → **Moved to Controls: 1** (FR 5051).

- 🔇 **Disabled — hardware absent: 17** (heaters, mixing valve, zones, thermostats).

- 🔇 **Disabled — inert/overwritten: 67** (curve, limits, DHW, modes needing contacts).


**94 FSV total. Active entities: 9.**

## Full map (all 94)

| FSV | Code | Name | Group | Unit | Range | Status | Reason |
|---|---|---|---|---|---|---|---|
| 1011 | 0x424A | Water out max (cooling) | 1xxx Temperature limits | °C | 18…25 (step 1) | 🔇 Disabled — inert | temperature limit: ignored by Immergas (own setpoint; 1031/1032 tests confirmed) |
| 1012 | 0x424B | Water out min (cooling) | 1xxx Temperature limits | °C | 5…18 (step 1) | 🔇 Disabled — inert | temperature limit: ignored by Immergas (own setpoint; 1031/1032 tests confirmed) |
| 1021 | 0x424C | Room temp max (cooling) | 1xxx Temperature limits | °C | 28…30 (step 1) | 🔇 Disabled — inert | temperature limit: ignored by Immergas (own setpoint) |
| 1022 | 0x424D | Room temp min (cooling) | 1xxx Temperature limits | °C | 18…28 (step 1) | 🔇 Disabled — inert | temperature limit: ignored by Immergas (own setpoint) |
| 1031 | 0x424E | Water out max (heating) | 1xxx Temperature limits | °C | 37…55 (step 1) | 🔇 Disabled — inert | water-out max overwritten by Immergas (setpoint wins) |
| 1032 | 0x424F | Water out min (heating) | 1xxx Temperature limits | °C | 15…37 (step 1) | 🔇 Disabled — inert | water-out min heating: ignored - 1032=37 did not raise the setpoint (stayed 24 below the Samsung min) |
| 1041 | 0x4250 | Room temp max (heating) | 1xxx Temperature limits | °C | 18…30 (step 1) | 🔇 Disabled — inert | temperature limit: ignored by Immergas (own setpoint) |
| 1042 | 0x4251 | Room temp min (heating) | 1xxx Temperature limits | °C | 16…18 (step 1) | 🔇 Disabled — inert | temperature limit: ignored by Immergas (own setpoint) |
| 1051 | 0x4252 | DHW tank max | 1xxx Temperature limits | °C | 50…70 (step 1) | 🔇 Disabled — inert | DHW tank limit: ignored (Immergas owns setpoints; Samsung blind to tank 0x4237=0) |
| 1052 | 0x4253 | DHW tank min | 1xxx Temperature limits | °C | 30…40 (step 1) | 🔇 Disabled — inert | DHW tank limit: ignored (Immergas owns setpoints; Samsung blind to tank 0x4237=0) |
| 2011 | 0x4254 | Heating WL - outdoor temp point 1 | 2xxx Water law (heating/cooling curve) | °C | -20…5 (step 1) | 🔇 Disabled — inert | heating curve: whole water-law ignored by Immergas (WL2 switch test confirmed) |
| 2012 | 0x4255 | Heating WL - outdoor temp point 2 | 2xxx Water law (heating/cooling curve) | °C | 10…20 (step 1) | 🔇 Disabled — inert | heating curve: whole water-law ignored by Immergas (WL2 switch test confirmed) |
| 2021 | 0x4256 | Heating WL1 - water out point 1 | 2xxx Water law (heating/cooling curve) | °C | 17…55 (step 1) | 🔇 Disabled — inert | heating curve WL1 ignored by Immergas (own setpoint) |
| 2022 | 0x4257 | Heating WL1 - water out point 2 | 2xxx Water law (heating/cooling curve) | °C | 17…55 (step 1) | 🔇 Disabled — inert | heating curve WL1 ignored by Immergas (own setpoint) |
| 2031 | 0x4258 | Heating WL2 - water out point 1 | 2xxx Water law (heating/cooling curve) | °C | 17…55 (step 1) | 🔇 Disabled — inert | heating curve WL2: ignored by Immergas |
| 2032 | 0x4259 | Heating WL2 - water out point 2 | 2xxx Water law (heating/cooling curve) | °C | 17…55 (step 1) | 🔇 Disabled — inert | heating curve WL2: ignored by Immergas |
| 2041 | 0x4093 | Heating water law type | 2xxx Water law (heating/cooling curve) | — | WL1 / WL2 | 🔇 Disabled — inert | water-law type select: ignored - WL1->WL2 switch did not change the setpoint |
| 2051 | 0x425A | Cooling WL - outdoor temp point 1 | 2xxx Water law (heating/cooling curve) | °C | 25…35 (step 1) | 🔇 Disabled — inert | cooling curve: ignored (Immergas owns the setpoint, same mechanism) |
| 2052 | 0x425B | Cooling WL - outdoor temp point 2 | 2xxx Water law (heating/cooling curve) | °C | 35…45 (step 1) | 🔇 Disabled — inert | cooling curve: ignored (Immergas owns the setpoint, same mechanism) |
| 2061 | 0x425C | Cooling WL1 - water out point 1 | 2xxx Water law (heating/cooling curve) | °C | 5…25 (step 1) | 🔇 Disabled — inert | cooling curve WL1: ignored (same mechanism) |
| 2062 | 0x425D | Cooling WL1 - water out point 2 | 2xxx Water law (heating/cooling curve) | °C | 5…25 (step 1) | 🔇 Disabled — inert | cooling curve WL1: ignored (same mechanism) |
| 2071 | 0x425E | Cooling WL2 - water out point 1 | 2xxx Water law (heating/cooling curve) | °C | 5…25 (step 1) | 🔇 Disabled — inert | cooling curve WL2: ignored (same mechanism) |
| 2072 | 0x425F | Cooling WL2 - water out point 2 | 2xxx Water law (heating/cooling curve) | °C | 5…25 (step 1) | 🔇 Disabled — inert | cooling curve WL2: ignored (same mechanism) |
| 2081 | 0x4094 | Cooling water law type | 2xxx Water law (heating/cooling curve) | — | WL1 / WL2 | 🔇 Disabled — inert | cooling water-law type select: ignored (same mechanism) |
| 2091 | 0x4095 | External thermostat #1 (UFH) | 2xxx Water law (heating/cooling curve) | — | 0 / 1 | 🔇 Disabled — hardware absent | external thermostat #1 (UFH): no thermostat wired to the Samsung side (room control is Immergas/Salus) - inert here |
| 2092 | 0x4096 | External thermostat #2 (FCU) | 2xxx Water law (heating/cooling curve) | — | 0 / 1 | 🔇 Disabled — hardware absent | external thermostat #2 (FCU): no thermostat wired to the Samsung side (room control is Immergas/Salus) - inert here |
| 2093 | 0x4127 | Remote controller room temp control | 2xxx Water law (heating/cooling curve) | — | Ambient only / Water law + thermo-off -> pump off / Water law + thermo-off -> pump on / Water law + thermo-off -> pump 30% | 🔇 Disabled — hardware absent | room temp control from the Samsung remote: no Samsung remote on this install (panel emulated, room control is Immergas) - inert here |
| 3011 | 0x4097 | DHW tank application | 3xxx DHW & disinfection | — | 0 / 1 | 🔇 Disabled — inert | DHW: inert - CWU is Immergas's domain (Samsung blind to tank, 0x4237=0) |
| 3021 | 0x4260 | DHW max temp by heat pump | 3xxx DHW & disinfection | °C | 45…50 (step 1) | 🔇 Disabled — inert | DHW max temp by HP: write holds but inert - Samsung blind to tank |
| 3022 | 0x4261 | DHW heat pump stop difference | 3xxx DHW & disinfection | °C | 2…10 (step 1) | 🔇 Disabled — inert | DHW: inert - CWU is Immergas's domain (Samsung blind to tank) |
| 3023 | 0x4262 | DHW heat pump start difference | 3xxx DHW & disinfection | °C | 5…20 (step 1) | 🔇 Disabled — inert | DHW: inert - CWU is Immergas's domain (Samsung blind to tank) |
| 3024 | 0x4263 | Min space heating time | 3xxx DHW & disinfection | min | 1…20 (step 1) | 🔇 Disabled — inert | DHW/heating timer: inert - CWU is Immergas's domain |
| 3025 | 0x4264 | Max DHW time | 3xxx DHW & disinfection | min | 5…95 (step 5) | 🔇 Disabled — inert | DHW: inert - CWU is Immergas's domain (Samsung blind to tank) |
| 3026 | 0x4265 | Max space heating time | 3xxx DHW & disinfection | h | 0.5…10 (step 0.5) | 🔇 Disabled — inert | DHW/heating timer: inert - CWU is Immergas's domain |
| 3031 | 0x4098 | Booster heater | 3xxx DHW & disinfection | — | 0 / 1 | 🔇 Disabled — inert | backup heater: inert - heaters planned on the Immergas indoor unit, not Samsung (re-test only if wired to the Samsung/MIM side) |
| 3032 | 0x4266 | Booster heater delay | 3xxx DHW & disinfection | min | 20…95 (step 5) | 🔇 Disabled — inert | backup heater: inert - heaters planned on the Immergas indoor unit, not Samsung (re-test only if wired to the Samsung/MIM side) |
| 3033 | 0x4267 | Booster heater overshoot | 3xxx DHW & disinfection | °C | 0…4 (step 1) | 🔇 Disabled — inert | backup heater: inert - heaters planned on the Immergas indoor unit, not Samsung (re-test only if wired to the Samsung/MIM side) |
| 3041 | 0x4099 | Disinfection | 3xxx DHW & disinfection | — | 0 / 1 | 🔇 Disabled — inert | disinfection: inert - Samsung blind to tank; disinfection is Immergas P15 |
| 3042 | 0x409A | Disinfection day | 3xxx DHW & disinfection | — | Sunday / Monday / Tuesday / Wednesday / Thursday / Friday / Saturday / All days | 🔇 Disabled — inert | disinfection group (inert, see 3041) |
| 3043 | 0x4269 | Disinfection start hour | 3xxx DHW & disinfection | h | 0…23 (step 1) | 🔇 Disabled — inert | disinfection group (inert, see 3041) |
| 3044 | 0x426A | Disinfection target temp | 3xxx DHW & disinfection | °C | 40…70 (step 5) | 🔇 Disabled — inert | disinfection group (inert, see 3041) |
| 3045 | 0x426B | Disinfection duration | 3xxx DHW & disinfection | min | 5…60 (step 5) | 🔇 Disabled — inert | disinfection group (inert, see 3041) |
| 3046 | 0x42CE | Disinfection max time | 3xxx DHW & disinfection | h | 0…24 (step 1) | 🔇 Disabled — inert | disinfection group (inert, see 3041) |
| 3051 | 0x409B | Forced DHW timer | 3xxx DHW & disinfection | — | 0 / 1 | 🔇 Disabled — inert | forced DHW: inert - CWU is Immergas's domain (Samsung blind to tank) |
| 3052 | 0x426C | Forced DHW duration | 3xxx DHW & disinfection | min | 30…300 (step 10) | 🔇 Disabled — inert | forced DHW: inert - CWU is Immergas's domain |
| 3061 | 0x409C | Solar panel or DHW thermostat | 3xxx DHW & disinfection | — | none / solar panel / DHW thermostat | 🔇 Disabled — inert | DHW solar/thermostat: inert - CWU is Immergas's domain |
| 3071 | 0x409D | 3-way valve direction | 3xxx DHW & disinfection | — | room / tank | 🔇 Disabled — inert | DHW 3-way valve: inert - the valve is on the Immergas side |
| 3081 | 0x42ED | Booster heater 1st step capacity | 3xxx DHW & disinfection | kW | 1…6 (step 1) | 🔇 Disabled — inert | backup heater power: inert - heaters on the Immergas side (re-test only if Samsung/MIM) |
| 3082 | 0x42EE | Booster heater 2nd step capacity | 3xxx DHW & disinfection | kW | 1…6 (step 1) | 🔇 Disabled — inert | backup heater power: inert - heaters on the Immergas side (re-test only if Samsung/MIM) |
| 3083 | 0x42EF | Booster heater (BSH) capacity | 3xxx DHW & disinfection | kW | 1…6 (step 1) | 🔇 Disabled — inert | tank (BSH) heater power: inert - heaters on the Immergas side (re-test only if Samsung/MIM) |
| 4011 | 0x409E | Heating or DHW priority | 4xxx Heating & zones | — | DHW / heating | 🔇 Disabled — inert | heating/DHW priority: inert - Immergas owns the control loop and CWU is 100% its domain (Samsung blind to the tank); same mechanism as the tested pump/curve/limits (not independently tested) |
| 4012 | 0x426D | Outdoor temp for priority | 4xxx Heating & zones | °C | -15…20 (step 1) | 🔇 Disabled — inert | outdoor temp for priority: inert - priority is decided by Immergas (see 4011) |
| 4013 | 0x426E | Heating off outdoor temp | 4xxx Heating & zones | °C | 14…35 (step 1) | 🔇 Disabled — inert | heating-off outdoor temp: inert - Immergas owns the setpoint and decides when to heat; not testable here (outdoor 8.5C is below the 14C min threshold) |
| 4021 | 0x409F | Backup heater application | 4xxx Heating & zones | — | 0 / 1 | 🔇 Disabled — hardware absent | backup heater application: not on the Samsung side (heaters go to the Immergas indoor unit) - FSV holds but inert here |
| 4022 | 0x40A0 | Backup or booster heater priority | 4xxx Heating & zones | — | both / backup heater / booster heater | 🔇 Disabled — hardware absent | backup/booster heater priority: heaters on the Immergas side - FSV holds but inert here |
| 4023 | 0x40A1 | Cold weather compensation | 4xxx Heating & zones | — | 0 / 1 | 🔇 Disabled — inert | cold-weather compensation (backup heater): inert - heaters on the Immergas side |
| 4024 | 0x4270 | Backup heater threshold temp | 4xxx Heating & zones | °C | -15…35 (step 1) | 🔇 Disabled — hardware absent | backup heater threshold temp: heaters on the Immergas side - FSV holds but inert here |
| 4025 | 0x4271 | Defrost backup temp | 4xxx Heating & zones | °C | 10…55 (step 5) | 🔇 Disabled — hardware absent | defrost backup temp: tied to the backup heater (Immergas side) - FSV holds but inert here |
| 4031 | 0x40A2 | Backup boiler | 4xxx Heating & zones | — | 0 / 1 | 🔇 Disabled — hardware absent | backup boiler application: not fitted on this install - FSV holds but inert here |
| 4032 | 0x40A3 | Backup boiler priority | 4xxx Heating & zones | — | 0 / 1 | 🔇 Disabled — hardware absent | backup boiler priority: not fitted on this install - FSV holds but inert here |
| 4033 | 0x4272 | Backup boiler threshold temp | 4xxx Heating & zones | °C | -20…5 (step 1) | 🔇 Disabled — hardware absent | backup boiler threshold temp: not fitted on this install - FSV holds but inert here |
| 4041 | 0x40C0 | Mixing valve | 4xxx Heating & zones | — | 0 / 1 | 🔇 Disabled — hardware absent | mixing valve application: not fitted on this install - FSV holds but inert here |
| 4042 | 0x4286 | Mixing valve target delta T (heating) | 4xxx Heating & zones | °C | 5…15 (step 1) | 🔇 Disabled — hardware absent | mixing valve target delta T (heating): no mixing valve on this install - FSV holds but inert here |
| 4043 | 0x4287 | Mixing valve target delta T (cooling) | 4xxx Heating & zones | °C | 5…15 (step 1) | 🔇 Disabled — hardware absent | mixing valve target delta T (cooling): no mixing valve on this install - FSV holds but inert here |
| 4044 | 0x40C1 | Mixing valve control factor | 4xxx Heating & zones | — | 1…5 (step 1) | 🔇 Disabled — hardware absent | mixing valve control factor: no mixing valve on this install - FSV holds but inert here |
| 4045 | 0x4288 | Mixing valve control interval | 4xxx Heating & zones | min | 1…30 (step 1) | 🔇 Disabled — hardware absent | mixing valve control interval: no mixing valve on this install - FSV holds but inert here |
| 4046 | 0x4289 | Mixing valve running time | 4xxx Heating & zones | s | 60…240 (step 30) | 🔇 Disabled — hardware absent | mixing valve running time: no mixing valve on this install - FSV holds but inert here |
| 4051 | 0x40C2 | Inverter pump control | 4xxx Heating & zones | — | 0 / 1 | 🔇 Disabled — inert | inverter pump application: inert - the circulation pump is Immergas-controlled (confirmed by the 4052 test) |
| 4052 | 0x428A | Inverter pump target delta T | 4xxx Heating & zones | °C | 2…8 (step 1) | 🔇 Disabled — inert | inverter pump target delta T: TESTED inert - 4052 set 5->8 did not change flow (stayed ~1550 l/h) or delta T (~1.3C); pump runs at a fixed speed, Immergas controls circulation, Samsung is blind (0x4089 reads OFF while 1550 l/h flows) |
| 4053 | 0x40C3 | Inverter pump control factor | 4xxx Heating & zones | — | 1…3 (step 1) | 🔇 Disabled — inert | inverter pump control factor: inert - pump is Immergas-controlled (confirmed by the 4052 test) |
| 4061 | 0x411A | Zone control | 4xxx Heating & zones | — | 0 / 1 | 🔇 Disabled — hardware absent | zone control: write holds (tested 2026-10-28, ON held then reverted OFF) but zones are done by the Immergas hydraulics (KLF08/Salus), not the Samsung side - inert here |
| 5011 | 0x4273 | Outing - cooling water out | 5xxx Special modes (outing, derating, PV, Smart Grid) | °C | 5…25 (step 1) | ✅ Exposed — works | outing mode takes over the setpoint (confirmed) |
| 5012 | 0x4274 | Outing - cooling room | 5xxx Special modes (outing, derating, PV, Smart Grid) | °C | 18…30 (step 1) | ✅ Exposed — works | outing mode takes over the setpoint (confirmed) |
| 5013 | 0x4275 | Outing - heating water out | 5xxx Special modes (outing, derating, PV, Smart Grid) | °C | 15…55 (step 1) | ✅ Exposed — works | outing mode takes over the setpoint (confirmed) |
| 5014 | 0x4276 | Outing - heating room | 5xxx Special modes (outing, derating, PV, Smart Grid) | °C | 16…30 (step 1) | ✅ Exposed — works | outing mode takes over the setpoint (confirmed) |
| 5015 | 0x4277 | Outing - auto cooling WL1 water | 5xxx Special modes (outing, derating, PV, Smart Grid) | °C | 5…25 (step 1) | ✅ Exposed — works | outing mode takes over the setpoint (confirmed) |
| 5016 | 0x4278 | Outing - auto cooling WL2 water | 5xxx Special modes (outing, derating, PV, Smart Grid) | °C | 5…25 (step 1) | ✅ Exposed — works | outing mode takes over the setpoint (confirmed) |
| 5017 | 0x4279 | Outing - auto heating WL1 water | 5xxx Special modes (outing, derating, PV, Smart Grid) | °C | 15…55 (step 1) | ✅ Exposed — works | outing mode takes over the setpoint (confirmed) |
| 5018 | 0x427A | Outing - auto heating WL2 water | 5xxx Special modes (outing, derating, PV, Smart Grid) | °C | 15…55 (step 1) | ✅ Exposed — works | outing mode takes over the setpoint (confirmed) |
| 5019 | 0x427B | Outing - DHW tank target | 5xxx Special modes (outing, derating, PV, Smart Grid) | °C | 30…70 (step 1) | ✅ Exposed — works | outing mode takes over the setpoint (confirmed) |
| 5021 | 0x427C | DHW saving temp difference | 5xxx Special modes (outing, derating, PV, Smart Grid) | °C | 0…40 (step 1) | 🔇 Disabled — inert | DHW saving temp difference: inert - CWU is Immergas's domain (parent 5022 inert, Samsung blind to tank) |
| 5022 | 0x4128 | DHW saving mode | 5xxx Special modes (outing, derating, PV, Smart Grid) | — | 0 / 1 | 🔇 Disabled — inert | DHW saving: inert - Samsung blind to tank (0x4237=0), DHW is Immergas's domain |
| 5023 | 0x42F0 | DHW saving thermo-off difference | 5xxx Special modes (outing, derating, PV, Smart Grid) | °C | 0…20 (step 1) | 🔇 Disabled — inert | DHW saving thermo-off difference: inert - CWU is Immergas's domain (parent 5022 inert) |
| 5041 | 0x40A4 | Power peak control | 5xxx Special modes (outing, derating, PV, Smart Grid) | — | 0 / 1 | 🔇 Disabled — inert | peak-power control: write holds but inert (needs external peak contact) |
| 5042 | 0x40A5 | Power peak - forced off parts | 5xxx Special modes (outing, derating, PV, Smart Grid) | — | All thermal off / Additional heater only / Backup heater only | 🔇 Disabled — inert | peak-power forced-off parts: inert - needs external peak contact (parent 5041 inert) |
| 5043 | 0x40A6 | Power peak - input voltage | 5xxx Special modes (outing, derating, PV, Smart Grid) | — | low / high | 🔇 Disabled — inert | peak-power input voltage: inert - needs external peak contact (parent 5041 inert) |
| 5051 | 0x40A7 | Frequency ratio control | 5xxx Special modes (outing, derating, PV, Smart Grid) | — | 0 / 1 | → Controls (works) | FR control enable - WORKS (caps compressor ~50%), the FR switch in f1f2_control |
| 5081 | 0x411B | PV control | 5xxx Special modes (outing, derating, PV, Smart Grid) | — | 0 / 1 | 🔇 Disabled — inert | PV control: write holds but inert (needs external PV contact) |
| 5082 | 0x42DB | PV control - cooling shift | 5xxx Special modes (outing, derating, PV, Smart Grid) | °C | 0…20 (step 1) | 🔇 Disabled — inert | PV cooling shift: inert - needs external PV contact (parent 5081 inert) |
| 5083 | 0x42DC | PV control - heating shift | 5xxx Special modes (outing, derating, PV, Smart Grid) | °C | 0…20 (step 1) | 🔇 Disabled — inert | PV heating shift: inert - needs external PV contact (parent 5081 inert) |
| 5091 | 0x411C | Smart Grid control | 5xxx Special modes (outing, derating, PV, Smart Grid) | — | 0 / 1 | 🔇 Disabled — inert | Smart Grid: write holds but inert (needs external SG contact) |
| 5092 | 0x42DD | Smart Grid - heating shift | 5xxx Special modes (outing, derating, PV, Smart Grid) | °C | 0…20 (step 1) | 🔇 Disabled — inert | Smart Grid heating shift: inert - needs external SG contact (parent 5091 inert) |
| 5093 | 0x42DE | Smart Grid - DHW shift | 5xxx Special modes (outing, derating, PV, Smart Grid) | °C | 0…20 (step 1) | 🔇 Disabled — inert | Smart Grid DHW shift: inert - needs external SG contact (parent 5091 inert) |
| 5094 | 0x411D | Smart Grid - DHW mode | 5xxx Special modes (outing, derating, PV, Smart Grid) | — | 0 / 1 | 🔇 Disabled — inert | Smart Grid DHW mode: inert - needs external SG contact (parent 5091 inert) |

---

*Generated from the project's FSV catalog (`components/f1f2_nasa/f1f2_fsv_catalog.yaml`). The working levers (outing, FR) live in `f1f2_control`, outside the FSV family; editable FSV use the `number` platform (see `f1f2_outing_targets`).*
