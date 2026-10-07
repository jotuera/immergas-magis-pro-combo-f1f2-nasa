# Adding a language

All user-visible texts of the `immergas_nasa` component live in one file per language:
`components/immergas_nasa/translations/<code>.yaml`. The available languages are `en` (default) and `pl`.

1. Copy `translations/en.yaml` to `translations/<code>.yaml`, where `<code>` is the ISO 639-1 code, e.g. `de`, `it` or `fr`.
2. Translate the **values** only. Never change the keys (`flow_temp_out:`, `1011:`, `0:`…).
3. You may leave keys out: anything missing is taken from English.
4. Keep the placeholders exactly: `{value}`, `{code}`, `{text}` and `{name}`.
5. Test with `language: <code>` in your config (`esphome config your.yaml` lists the resulting names).
6. Open a pull request.

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
