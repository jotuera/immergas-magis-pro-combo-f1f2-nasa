# Adding a language

A language consists of two files, both named with the bus prefix `f1f2_` and the ISO 639-1 code (`de`, `it`, `fr`…):

| File | Content |
|---|---|
| `components/f1f2_nasa/translations/f1f2_<code>.yaml` | everything the component shows: entity names, state texts, error texts, FSV names |
| `packages/lang/f1f2_<code>.yaml` | selects the language (`f1f2_language: <code>`) and names the few template entities (`n_f1f2_*`) |

The available languages are `en` (default) and `pl`.

1. Copy `components/f1f2_nasa/translations/f1f2_en.yaml` to `f1f2_<code>.yaml` and `packages/lang/f1f2_en.yaml` to `f1f2_<code>.yaml`.
2. Translate the **values** only. Never change the keys (`flow_temp_out:`, `1011:`, `0:`, `n_f1f2_flow_delta:`…). In the package file set `f1f2_language: <code>`.
3. You may leave keys out of the component file: anything missing is taken from English.
4. Keep the placeholders exactly: `{value}`, `{code}`, `{text}` and `{name}`.
5. Test with `packages/lang/f1f2_<code>.yaml` in your packages list (`esphome config your.yaml` lists the resulting names).
6. Open a pull request.

The component file has these sections:
The file has these sections:

| Section | What it translates |
|---|---|
| `sensors`, `binary_sensors`, `text_sensors` | entity names (by catalog key) |
| `switches`, `buttons` | sniffer switch / "Read FSV now" button |
| `fsv` + `fsv_format` | FSV parameter names, e.g. `FSV {code} - {name}` |
| `maps` | state texts (driving mode, defrost stage, DHW mode…) |
| `errors` | error descriptions, by Samsung error number |
| `texts` | small strings: unknown value, no error, error formats |

**Changing the language changes the entity names**, so Home Assistant will create new entities. Pick the language once, before the first install.

For entity names, prefer the wording of the **official Immergas / Samsung manual in your language**, so that users recognise them.

> Do not use `/` in entity or FSV names - ESPHome reserves it for URLs. Use "or", "-" or "," instead.
