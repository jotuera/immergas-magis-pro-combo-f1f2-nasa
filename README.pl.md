# Immergas Magis Pro / Combo — dane pompy ciepła z magistrali Samsung NASA (F1/F2), ESPHome

🇬🇧 [English](README.md) | 🇵🇱 Polski

<p align="center">
  <img src="https://img.shields.io/github/v/tag/jotuera/immergas-magis-pro-combo-f1f2-nasa?label=version&color=blue" alt="Wersja">
  <img src="https://img.shields.io/badge/platform-ESP32-41BDF5.svg" alt="ESP32">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-green.svg" alt="Licencja: MIT"></a>
</p>

Odczyt wszystkiego, co **jednostka zewnętrzna** Immergas Magis Pro / Combo V2 (Samsung EHS, np. **Audax Pro V2**) i **płytka Samsung MIM-B19N** w Magisie wymieniają po magistrali **F1/F2**. Dane trafiają do **Home Assistanta** przez **M5Stack Atom + RS485** z ESPHome. Bez chmury i bez cudzego komponentu NASA: projekt ma własny komponent `f1f2_nasa`.

> Część rodziny [`jotuera/immergas-magis-pro-combo-*`](https://github.com/jotuera?tab=repositories): **dd-modbus** (D+/D-, emulator Dominusa i paneli), **tt-bms** (T-/T+, Modbus BMS) i **f1f2-nasa** (to repo). Wszystkie trzy mają ten sam układ (`packages/<magistrala>_*.yaml`, identyfikatory i podstawienia z prefiksem `dd_` / `tt_` / `f1f2_`), więc mogą działać razem na jednym urządzeniu.

> **v1.2.0.** Przetestowane na **Magis Combo 9 Plus V2 + Audax Pro 9 V2**. Opinie z innych instalacji mile widziane. Aktualizacja z v0.x: zob. [CHANGELOG.md](CHANGELOG.md).

## Co dostajesz

- **Temperatury**: zasilanie i powrót, aktywna zadana wody, zewnętrzna, tłoczenie, głowica sprężarki, wymiennik zewnętrzny, faza ciekła, IPM.
- **Sprężarka**: częstotliwość rzeczywista, zlecona i docelowa, limit częstotliwości (spada do 19 Hz w funkcji nocnej Immergasa U11), etap startu, blokada restartu, prąd, moc, energia (kWh), napięcie szyny DC i sieci.
- **Wentylator i zawory**: obroty zadane i rzeczywiste, pozycja EEV (0–2000), zawór 4-drogowy, zawór gorącego gazu, grzałka tacy, odszranianie i jego etap.
- **Stany**: tryb jednostki zewnętrznej (Postój / Tryb bezpieczny / Normalna praca / Odszranianie …), tryb jednostki wewnętrznej, pompa, grzałki.
- **Błędy**: kod Samsunga z opisem, np. `E101: Błąd komunikacji jednostka wewn. - zewn.`.
- **Firmware**: numery części i daty płyty głównej ODU, jej EEPROM, falownika i płytki MIM.
- **Wszystkie 94 parametry instalatora FSV**, odczytywane zapytaniami: krzywa grzewcza, limity CWU, grzałki itd. Widoczne jako zwykłe sensory tylko do odczytu.
- **Wartości pochodne**: ΔT obiegu, energia pobrana dziś.
- **Nieznane PDU** jako diagnostyczne encje `RAW`, plus opcjonalny **sniffer magistrali**, który pomaga je rozszyfrować (zob. [PDU_MAP.md](PDU_MAP.md)).
- **Wykrywanie adresów**: encja `NASA urządzenia na magistrali` pokazuje wszystkie urządzenia na magistrali, a log podpowiada, czy trzeba zmienić adres jednostki wewnętrznej lub zewnętrznej (zob. [Jak ustalić adresy](#jak-ustalić-adresy)).
- **Tłumaczenia**: nazwy encji, teksty stanów i opisy błędów są tłumaczone, a język wybiera **jedna linia** na liście pakietów (`f1f2_en` / `f1f2_pl`). Nowy język to jeden plik (zob. [TRANSLATING.md](TRANSLATING.md)).

### Dlaczego tylko odczyt?

Na F1/F2 **rządzi sterownik Immergasa**. Sam liczy zadaną temperaturę wody ze swojej krzywej (menu Termoregulacja R02–R05 + offset U03) i steruje stroną Samsunga przez płytkę MIM. Zapisy do jednostki wewnętrznej po F1/F2 są ignorowane. Dlatego projekt **nigdy nie zapisuje**. Wysyła tylko zapytania **READ** po wartości FSV, których nikt nie rozgłasza. `f1f2_transmit: "false"` oznacza czysty nasłuch.

Do sterowania kotłem służą [dd-modbus](https://github.com/jotuera/immergas-magis-pro-combo-dd-modbus) i [tt-bms](https://github.com/jotuera/immergas-magis-pro-combo-tt-bms).

Które rejestry steruje sterownik Immergasa (widoczne/ustawialne na magistralach Modbus D+/D- i T-/T+), a które są własne jednostki zewnętrznej Samsunga, opisują [register_control.pl.md](register_control.pl.md) (sterowane przez Immergasa) i [registers_not_modbus.pl.md](registers_not_modbus.pl.md) (niesterowane przez Modbus). Na podstawie audytu krzyżowego trzech magistral.

## Sprzęt

- **M5Stack Atom Lite** (ESP32) + **Atomic RS485 Base** (automatyczny kierunek) albo dowolny ESP32 z transceiverem RS485. Przy transceiverze z pinem DE/RE ustaw `flow_control_pin` w `f1f2_core`.
- Piny w przykładzie: **TX = GPIO19, RX = GPIO22** (podstawienia `f1f2_uart_tx_pin` / `f1f2_uart_rx_pin`).
- Podłączenie: **F1 / F2** w skrzynce Magisa (zaciski płytki MIM-B19N) → RS485 **A / B**. Jeśli widzisz same błędne ramki, zamień A/B.
- Magistrala: **9600 bodów, 8E1**.
- **Przełącznik obrotowy na MIM-B19N musi stać na 1** (fabrycznie 0). Przy 0 płytka milczy i nic nie da się odczytać.

## Instalacja

1. Skopiuj `immergas-magis-pro-combo-f1f2-nasa.yaml` i `secrets.yaml.example` (zmień nazwę na `secrets.yaml`) do katalogu ESPHome, potem uzupełnij Wi-Fi, klucz API i hasło OTA.
2. Wybierz język: na liście pakietów zostaw **jeden** z plików `packages/lang/f1f2_en.yaml` lub `packages/lang/f1f2_pl.yaml`.
3. Wgraj. Pakiety i komponent `f1f2_nasa` pobierają się z tego repo.
4. Po ok. 30 s odczytają się FSV; potem są odświeżane co `f1f2_poll_interval` albo na żądanie przyciskiem **Odczytaj FSV teraz**.

### Wersje

Przykład przypina wszystko do tagu wydania (`ref: v1.2.0`; `f1f2_core` pobiera komponent z tym samym tagiem). Żeby zaktualizować, zmień `ref:` na nowszy tag; `main` oznacza zawsze najnowszą wersję rozwojową.

### Pakiety

| Plik | Zawartość |
|---|---|
| `packages/f1f2_core.yaml` | UART, hub `f1f2_nasa`, pobranie komponentu, `NASA urządzenia na magistrali` (**wymagany**) |
| `packages/f1f2_indoor.yaml` | jednostka wewnętrzna: temperatury wody, aktywna zadana, CWU, strefa 1, pompa, grzałki, tryby |
| `packages/f1f2_outdoor.yaml` | jednostka zewnętrzna: sprężarka, limit częstotliwości, wentylator, EEV, temperatury czynnika, moc, energia, błędy |
| `packages/f1f2_info.yaml` | wersje firmware, klasa mocy, liczba jednostek wewnętrznych |
| `packages/f1f2_fsv.yaml` | 94 parametry FSV + przycisk **Odczytaj FSV teraz** |
| `packages/f1f2_computed.yaml` | ΔT obiegu, energia pobrana dziś |
| `packages/f1f2_diagnostics.yaml` | nieznane PDU, statystyki magistrali, sniffer |
| `packages/f1f2_ehs_extra.yaml` | opcjonalnie: strefa 2, przepływ, moc i energia wyprodukowana, COP — **Magis ich nie wysyła** (MIM odpowiada „brak danych”), przydatne w innych instalacjach Samsung EHS |
| `packages/lang/f1f2_en.yaml`, `f1f2_pl.yaml` | język (wybierz jeden) |

Ustawienia (podstawienia `f1f2_*`, np. piny, adresy, `f1f2_transmit`, `f1f2_poll_interval`, nazwy `n_f1f2_*`) i sposoby deklarowania własnych encji (`platform: f1f2_nasa` po `key`, kodzie `fsv` albo surowym `pdu`) opisuje [README.md](README.md#settings-substitutions). Pełna lista kluczy jest w [PDU_MAP.md](PDU_MAP.md).

### Jedno urządzenie na kilka magistral

Wszystkie trzy projekty mają prefiksy magistrali w nazwach plików, identyfikatorów i podstawień, więc ich listy pakietów można postawić obok siebie pod jednym `packages:` z jedną sekcją urządzenia (esp32, wifi, api, logger). Każda magistrala potrzebuje własnych pinów UART (ESP32 ma trzy sprzętowe UART-y; ustaw `logger: baud_rate: 0`).

## Jak ustalić adresy

Każde urządzenie na magistrali NASA ma 3-bajtowy adres `klasa.kanał.adres`:

| Adres | Urządzenie | Podstawienie |
|---|---|---|
| `10.00.00` | jednostka zewnętrzna (Samsung EHS) | `f1f2_outdoor_address` |
| `20.00.01` | jednostka wewnętrzna = płytka MIM-B19N w Magisie | `f1f2_indoor_address` |
| `80.FF.00` | ten ESP (wysyła tylko zapytania READ) | `address:` w `f1f2_nasa` |

Wartości domyślne pasują do Magis Combo / Pro V2 z przełącznikiem obrotowym MIM na **1**. Ostatni bajt adresu jednostki wewnętrznej odpowiada pozycji przełącznika (1 → `20.00.01`); w typowych modułach hydro Samsunga bywa `20.00.00`.

Nie trzeba zgadywać, komponent sam znajduje urządzenia:

1. Wgraj z ustawieniami domyślnymi.
2. Zajrzyj do encji **NASA urządzenia na magistrali** (albo do logu: `Found device on the bus: ...`). Jest tam lista wszystkich urządzeń, które nadają, np. `10.00.00 jednostka zewnętrzna, 20.00.01 jednostka wewnętrzna`.
3. Po 60 s komponent porównuje listę z ustawieniami. Jeśli się różnią, w logu pojawia się ostrzeżenie ze znalezionym adresem, np. `Found 20.00.00 instead - set 'indoor_address: 20.00.00'`.
4. Wpisz ten adres w podstawieniach (`f1f2_indoor_address: "20.00.00"`) i wgraj ponownie.

Jeśli lista zostaje pusta, log mówi o tym po 60 s. Sprawdź podłączenie F1/F2 → A/B (zamień A/B), 9600 8E1 i przełącznik obrotowy MIM (przy 0 magistrala milczy).

## Sniffer

Włącz przełącznik **Sniffer NASA** (diagnostyczny, po restarcie wyłączony). Każda zmiana wartości jest wtedy logowana jako `10.00.00 8238: 0 -> 30` (tag `f1f2_sniff`, poziom INFO), każda cała ramka na poziomie DEBUG (`logger: logs: f1f2_sniff: DEBUG`) razem z czasem z urządzenia i ciszą przed nią (`RX t=… gap=… skip=…`, także ramki `BAD` z błędnym CRC i nasze `TX`; dokładność ok. 1 ms), a ostatnia ramka i zmiana pojawiają się w dwóch sensorach tekstowych. To najprostszy sposób, żeby pomóc w dekodowaniu: nagraj log podczas odszraniania, grzania CWU albo mrozu i załóż issue.

## Uwagi

- **Przejście z komponentu `samsung_nasa` (Beormund)**: angielskie nazwy sensorów są takie same jak w typowym configu z tamtym komponentem. Przy tej samej `esphome: name:` HA zachowa te encje i ich historię. Polski język tworzy nowe encje. Encji zapisywalnych (climate, number, select, switch) tu nie ma.
- Wartości, których Magis Combo nie wysyła (ciśnienia, ssanie, czujniki wody na ODU…), są pominięte. Zob. koniec [PDU_MAP.md](PDU_MAP.md).
- Własny adres komponentu to `80.FF.00`. Jeśli na magistrali już jest urządzenie z tym adresem, ustaw `address:` pod `f1f2_nasa:`.

## Zastrzeżenie

Niezależny projekt hobbystyczny, niezwiązany z Samsungiem ani Immergasem. Używasz na własną odpowiedzialność. Domyślna konfiguracja tylko słucha i wysyła zapytania READ, niczego w pompie nie zmienia.

## Licencja

MIT © 2026 JoTu. Zob. [LICENSE](LICENSE) i [NOTICE](NOTICE).
