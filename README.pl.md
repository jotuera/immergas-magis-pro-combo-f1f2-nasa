# Immergas Magis Pro / Combo — dane pompy ciepła z magistrali Samsung NASA (F1/F2), ESPHome

🇬🇧 [English](README.md) | 🇵🇱 Polski

Odczyt wszystkiego, co **jednostka zewnętrzna** Immergas Magis Pro / Combo V2 (Samsung EHS, np. **Audax Pro V2**) i **płytka Samsung MIM-B19N** w Magisie wymieniają po magistrali **F1/F2**. Dane trafiają do **Home Assistanta** przez **M5Stack Atom + RS485** z ESPHome. Bez chmury i bez cudzego komponentu NASA: projekt ma własny komponent `immergas_nasa`.

> Część rodziny [`jotuera/immergas-magis-pro-combo-*`](https://github.com/jotuera?tab=repositories): **dd-modbus** (D+/D-, emulator Dominusa i paneli), **tt-bms** (T-/T+, Modbus BMS) i **f1f2-nasa** (to repo).

> ⚠️ **Beta (v0.10.0).** Przetestowane na **Magis Combo 9 Plus V2 + Audax Pro 9 V2**. Opinie z innych instalacji mile widziane.

## Co dostajesz

- **Temperatury**: zasilanie i powrót, aktywna zadana wody, zewnętrzna, tłoczenie, głowica sprężarki, wymiennik zewnętrzny, faza ciekła, IPM.
- **Sprężarka**: częstotliwość rzeczywista, zlecona i docelowa, etap startu, blokada restartu, prąd, moc, energia (kWh), napięcie szyny DC i sieci.
- **Wentylator i zawory**: obroty zadane i rzeczywiste, pozycja EEV (0–2000), zawór 4-drogowy, zawór gorącego gazu, grzałka tacy, odszranianie i jego etap.
- **Stany**: tryb jednostki zewnętrznej (Postój / Tryb bezpieczny / Normalna praca / Odszranianie …), tryb jednostki wewnętrznej, pompa, grzałki.
- **Błędy**: kod Samsunga z opisem, np. `E101: Błąd komunikacji jednostka wewn. - zewn.`.
- **Firmware**: numery części i daty płyty głównej ODU, jej EEPROM, falownika i płytki MIM.
- **Wszystkie 94 parametry instalatora FSV**, odczytywane zapytaniami: krzywa grzewcza, limity CWU, grzałki itd. Widoczne jako zwykłe sensory tylko do odczytu.
- **Wartości pochodne**: ΔT obiegu, chwilowy i całkowity COP, energia dzienna.
- **Nieznane PDU** jako diagnostyczne encje `RAW`, plus opcjonalny **sniffer magistrali**, który pomaga je rozszyfrować (zob. [PDU_MAP.md](PDU_MAP.md)).
- **Wykrywanie adresów**: encja `NASA urządzenia na magistrali` pokazuje wszystkie urządzenia na magistrali, a log podpowiada, czy trzeba zmienić `indoor_address` / `outdoor_address` (zob. [Jak ustalić adresy](#jak-ustalić-adresy)).
- **Tłumaczenia**: nazwy encji, teksty stanów i opisy błędów są tłumaczone, a język zmienia **jedna linia** (`language: en | pl`). Nowy język to jeden plik (zob. [TRANSLATING.md](TRANSLATING.md)).

### Dlaczego tylko odczyt?

Na F1/F2 **rządzi sterownik Immergasa**. Sam liczy zadaną temperaturę wody ze swojej krzywej (menu Termoregulacja R02–R05 + offset U03) i steruje stroną Samsunga przez płytkę MIM. Zapisy do jednostki wewnętrznej po F1/F2 są ignorowane. Dlatego projekt **nigdy nie zapisuje**. Przy `transmit: true` wysyła tylko zapytania **READ** po wartości FSV, których nikt nie rozgłasza. `transmit: false` oznacza czysty nasłuch.

Do sterowania kotłem służą [dd-modbus](https://github.com/jotuera/immergas-magis-pro-combo-dd-modbus) i [tt-bms](https://github.com/jotuera/immergas-magis-pro-combo-tt-bms). Magistrala pilota F3/F4, na której zapisy działają, jest w planach.

## Sprzęt

- **M5Stack Atom Lite** (ESP32) + **Atomic RS485 Base** (automatyczny kierunek) albo dowolny ESP32 z transceiverem RS485. Przy transceiverze z pinem DE/RE ustaw `flow_control_pin`.
- Piny w przykładzie: **TX = GPIO19, RX = GPIO22**.
- Podłączenie: **F1 / F2** w skrzynce Magisa (zaciski płytki MIM-B19N) → RS485 **A / B**. Jeśli widzisz same błędne ramki, zamień A/B.
- Magistrala: **9600 bodów, 8E1**.
- **Przełącznik obrotowy na MIM-B19N musi stać na 1** (fabrycznie 0). Przy 0 płytka milczy i nic nie da się odczytać.

## Instalacja

1. Skopiuj `immergas-magis-pro-combo-f1f2-nasa.yaml` i `secrets.yaml.example` (zmień nazwę na `secrets.yaml`) do katalogu ESPHome, potem uzupełnij Wi-Fi, klucz API i hasło OTA.
2. Wybierz język: `substitutions: language: pl`.
3. Wgraj. Komponent pobiera się z tego repo przez `external_components`.
4. Po ok. 30 s odczytają się FSV; potem są odświeżane co `poll_interval` albo na żądanie przyciskiem **Odczytaj FSV teraz**.

### Wersje

Przykład przypina komponent do tagu wydania: `github://jotuera/immergas-magis-pro-combo-f1f2-nasa@v0.10.0`. Żeby zaktualizować, zmień `@v0.10.0` na nowszy tag; `@main` oznacza zawsze najnowszą wersję rozwojową. Z GitHuba pobiera się tylko komponent, Twój YAML zostaje u Ciebie.

Opcje komponentu i sposoby deklarowania encji (po `key`, po kodzie `fsv` albo jako surowy `pdu`) opisuje [README.md](README.md#component-options). Pełna lista kluczy jest w [PDU_MAP.md](PDU_MAP.md).

## Jak ustalić adresy

Każde urządzenie na magistrali NASA ma 3-bajtowy adres `klasa.kanał.adres`:

| Adres | Urządzenie | Opcja |
|---|---|---|
| `10.00.00` | jednostka zewnętrzna (Samsung EHS) | `outdoor_address` |
| `20.00.01` | jednostka wewnętrzna = płytka MIM-B19N w Magisie | `indoor_address` |
| `80.FF.00` | ten ESP (wysyła tylko zapytania READ) | `address` |

Wartości domyślne pasują do Magis Combo / Pro V2 z przełącznikiem obrotowym MIM na **1**. Ostatni bajt adresu jednostki wewnętrznej odpowiada pozycji przełącznika (1 → `20.00.01`); w typowych modułach hydro Samsunga bywa `20.00.00`.

Nie trzeba zgadywać, komponent sam znajduje urządzenia:

1. Wgraj z ustawieniami domyślnymi.
2. Zajrzyj do encji **NASA urządzenia na magistrali** (albo do logu: `Found device on the bus: ...`). Jest tam lista wszystkich urządzeń, które nadają, np. `10.00.00 jednostka zewnętrzna, 20.00.01 jednostka wewnętrzna`.
3. Po 60 s komponent porównuje listę z ustawieniami. Jeśli się różnią, w logu pojawia się ostrzeżenie z gotową linią do wpisania, np. `Found 20.00.00 instead - set 'indoor_address: 20.00.00'`.
4. Wpisz tę linię pod `immergas_nasa:` i wgraj ponownie.

Jeśli lista zostaje pusta, log mówi o tym po 60 s. Sprawdź podłączenie F1/F2 → A/B (zamień A/B), 9600 8E1 i przełącznik obrotowy MIM (przy 0 magistrala milczy).

## Sniffer

Włącz przełącznik **Sniffer NASA** (diagnostyczny, po restarcie wyłączony). Każda zmiana wartości jest wtedy logowana jako `10.00.00 8238: 0 -> 30` (tag `sniff`, poziom INFO), każda cała ramka na poziomie DEBUG (`logger: level: DEBUG`), a ostatnia ramka i zmiana pojawiają się w dwóch sensorach tekstowych. To najprostszy sposób, żeby pomóc w dekodowaniu: nagraj log podczas odszraniania, grzania CWU albo mrozu i załóż issue.

## Uwagi

- **Przejście z komponentu `samsung_nasa` (Beormund)**: angielskie nazwy sensorów są takie same jak w typowym configu z tamtym komponentem. Przy tej samej `esphome: name:` HA zachowa te encje i ich historię. Polski język tworzy nowe encje. Encji zapisywalnych (climate, number, select, switch) tu nie ma.
- Wartości, których Magis Combo nie wysyła (ciśnienia, ssanie, czujniki wody na ODU…), są pominięte. Zob. koniec [PDU_MAP.md](PDU_MAP.md).
- Własny adres komponentu to `80.FF.00`. Jeśli na magistrali już jest urządzenie z tym adresem, zmień `address:`.

## Zastrzeżenie

Niezależny projekt hobbystyczny, niezwiązany z Samsungiem ani Immergasem. Używasz na własną odpowiedzialność. Domyślna konfiguracja tylko słucha i wysyła zapytania READ, niczego w pompie nie zmienia.

## Licencja

MIT © 2026 JoTu. Zob. [LICENSE](LICENSE) i [NOTICE](NOTICE).
