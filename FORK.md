# dkej123/crosspoint-reader — fork pod Xteink X4 Pro

Fork [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)
(gałąź `develop`) pod mój Xteink X4 Pro. Gałąź robocza: **`dkej/x4pro`**.

Cel: synchronizacja pozycji czytania między Xteinkiem a telefonem (Readest)
bez ręcznego klikania, potwierdzenie synchronizacji w Home Assistant oraz pilot
BLE do kartkowania.

## Stan (14.10.2026)

### Zrobione
- [x] Fork `dkej123/crosspoint-reader`, gałąź `dkej/x4pro`, env `x4pro-dkej`
- [x] Automatyczny KOSync — scalony upstream PR #2760 (pobieranie przy
      otwarciu, wysyłanie przy wyjściu/uśpieniu, nigdy nie nadpisuje dalszej
      pozycji z serwera)
- [x] OTA z wydań forka (wersje `1.6.5.N`), skrypt `scripts/dkej_release.sh`
- [x] Pilot BLE do kartkowania (tylko przy otwartej książce + przełącznik +
      ekran parowania)
- [x] Wtyczka SD → webhook Home Assistant (`sd-card/plugins/homeassistant`),
      filtr `{event.NAME|json}` w firmware
- [x] Polskie tłumaczenia nowych opcji
- [x] Przegląd kodu — 5 błędów poprawionych (2 z PR #2760, 2 w pilocie BLE,
      1 we wtyczkach); szczegóły w „Przegląd i testy”
- [x] Testy z wymagań, całość 629/629; firmware się buduje
- [x] HA: statystyki czytania (import z CrossInk), widok „Czytanie”,
      dziennik sesji, potwierdzenie sync na Pixela
- [x] Pełna kopia karty SD z CrossInk (`~/Workspace/CrossPoint/backup-crossink-2026-10-13/`)
- [x] Wtyczka HA wgrana na kartę SD czytnika
- [x] Firmware na czytniku, test na sprzęcie 14.10: kosync przy uśpieniu, sesje
      i push w HA, cofanie (bez nadpisania), pilot BLE (C6 jako pilot testowy)
- [x] Niezawodność synchronizacji przy uśpieniu (sekcja 1a): unikalne id
      zdarzeń, ponawianie zapytań kosync, ponawianie Wi-Fi do ~2 min
- [x] HA: zapytanie kosync w integracji HTTP Requests (zamiast `rest_command`),
      opóźnienie 20 s i tolerancja 0,15 pkt w pushu

### Do zrobienia
- [ ] HA: usunąć encję `sensor.xteink_statystyki_test` i testowy wpis
      statystyk `sensor.xteink_postep`
- [ ] Usunąć z `/.crosspoint/wifi.json` na karcie SD sieć `Pixel_8598` (śmieć
      po CrossInku; nie widać jej w ustawieniach, bo nie jest w zasięgu)
- [ ] Pierwsze wydanie `1.6.5.1` na GitHubie forka (dla przyszłego OTA)
- [ ] Opcjonalnie: zgłosić błędy z przeglądu w upstream PR #2760
- [ ] Projekt dashboardu „Czytanie” (lista encji niżej)

### Encje HA dla dashboardu
Proste sensory: `sensor.xteink_ksiazka` (tytuł), `sensor.xteink_postep` (%),
`sensor.xteink_tempo_czytania` (%/h), `sensor.xteink_szacowany_czas_do_konca`
(h), `sensor.xteink_czas_czytania_dzis` (min), `sensor.xteink_czas_laczny` (h,
długoterminowe statystyki), `sensor.xteink_sesje`, `sensor.xteink_srednia_sesja`
(min), `sensor.xteink_seria_biezaca` (dni), `sensor.xteink_seria_najdluzsza` (dni).

`sensor.xteink_statystyki` (stan = minuty łącznie), atrybuty:
- `pory_dnia_s` — 4 × sekundy [rano 5–12, popołudnie 12–17, wieczór 17–21, noc 21–5]
- `dni_tygodnia_s` — 7 × sekundy [pon..nd]
- `dni_czytania` — daty `RRRR-MM-DD` (180 dni); `dni_czytania_lacznie`
- `dzienne_s` — `{RRRR-MM-DD: sekundy}` (35 dni)
- `ksiazki` — `{hash: {tytul, plik, czas_s, sesje, start, postep, ukonczona, ostatnio}}`; `biezaca_ksiazka` = hash
- `ostatnie_sesje` — do 50: `{ts, data, od, do, ksiazka, min, postep_od, postep_do}`
- `ostatnia_sesja`, `laczny_czas_s`, `sesje`, `seria_najdluzsza`

Widok: dashboard „Dom” (`lovelace`) → `czytanie` (typ `sections`, karty
standardowe HA). `dzienne_s` i `ostatnie_sesje` są puste do pierwszych sesji z
nowego firmware.

## Sprzęt

| | X4 / X3 | **X4 Pro (mój)** |
|---|---|---|
| Układ | ESP32-C3 | **ESP32-S3R8** |
| RAM | ~380 KB, bez PSRAM | ~512 KB + **8 MB PSRAM** |
| Bufor ekranu | w RAM | w PSRAM (`FREEINK_FB_PSRAM`) |

Większość problemów z pamięcią opisywanych w społeczności (Wi-Fi + TLS + BLE
razem z otwartym EPUB-em) dotyczy X4/X3. Na X4 Pro jest dużo zapasu.

## Co dodaje fork

### 1. Automatyczny KOSync (upstream PR #2760)

Scalony [PR #2760](https://github.com/crosspoint-reader/crosspoint-reader/pull/2760)
(w upstreamie jeszcze niescalony). Ustawienia: **Settings → System → KOReader
Sync**, obie opcje domyślnie **wyłączone**:

- **Automatic progress upload** — przy wyjściu z książki albo uśpieniu czytnik
  sam łączy się z Wi-Fi, **najpierw pobiera pozycję z serwera** i wysyła swoją
  **tylko gdy lokalna jest dalej**. Jeśli na serwerze jest dalej (np. czytałem
  na telefonie), niczego nie nadpisuje.
- **Automatic progress check** — przy otwarciu książki albo wybudzeniu z
  otwartą książką sprawdza serwer (max ~6 s, w tle, bez komunikatów). Gdy
  serwer jest dalej, pyta **Resume / No**. Nigdy nie wysyła.

Pozycja idzie **prosto z czytnika do kosync** — Home Assistant nie uczestniczy.

Zmiany przy scalaniu z `develop`:
- menu KOSync: nazwane indeksy z `develop` + dwa nowe checkboxy (14 pozycji),
- usypianie: zdarzenia wtyczek podzielone na „kolejkuj” (zanim reader zniknie)
  i „dostarcz” (po odroczonym wysłaniu pozycji, bez rysowania na ekranie snu),
- `Activity::prepareForSleep(bool)` z PR → `prepareDeferredSleep(bool)`, żeby
  nie przeciążać `prepareForSleep()` z `develop`,
- automatyczny sync pomija synchronizację zakreśleń (nie ma limitu czasu).

Konfiguracja na czytniku:
- **Sync Server URL:** `https://kosync.haosdk.pl` (puste = serwer CrossPoint!)
- **Server type:** KOSync server
- **Document matching:** **Binary** (w Readest odpowiednik: „File Content”)
- login/hasło jak w Readest, potem **Authenticate**

Serwer `kosync.haosdk.pl` to zwykły koreader-sync-server — wymaga nagłówka
`Accept: application/vnd.koreader.v1+json` (inaczej HTTP 412) i nie ma API
Athene (`/athene/v1/*` → 404), dlatego aplikacja Athene się z nim nie loguje.

### 1a. Niezawodność synchronizacji przy uśpieniu (14.10.2026)

Znalezione na sprzęcie z logiem debug (`env:x4pro-dkej-debug`, `LOG_LEVEL=2`):

- **Sesje odrzucane przez HA jako duplikaty.** Id zdarzenia wtyczki było
  `<esp_random()>-<licznik>`; przed włączeniem radia `esp_random()` daje po
  każdym wybudzeniu tę samą liczbę, więc pierwsza sesja po każdym wybudzeniu
  miała id `3b9c2a9f-1` i HA ją pomijał. Teraz
  `<losowe>-<licznik rozruchów w NVS>-<licznik>` (`src/util/PluginEventId.h`,
  NVS `pevt/boot`).
- **Zgubione zapytanie kosync przerywało wysyłkę.** Raz drugie zapytanie
  (kontrolne, hash nazwy pliku) nie dotarło do nginx na RPi (zginęło między
  czytnikiem, Cloudflare i tunelem), czekało do końca limitu 15 s, a PR #2760
  wtedy rezygnuje z wysyłki. Teraz każde zapytanie automatyczne (odczyty i
  wysyłka) przy błędzie sieci/5xx jest ponawiane co 0,5 s, próba max 5 s,
  całość max 120 s (`lib/KOReaderSync/SyncRetryPolicy.h`). Błędy logowania,
  JSON, brak pamięci — bez ponawiania. Zabezpieczenie przed nadpisaniem
  dalszej pozycji bez zmian.
- **Wi-Fi poddawało się po 6 s.** Join z MikroTikiem trwa 4–5 s (−76 dBm), raz
  przekroczył 6 s. Teraz pierwsza sieć ma 10 s (ostatnia 15 s), a gdy wszystkie
  zapisane sieci zawiodą, czytnik zaczyna kolejną rundę (przerwa 1 s) aż do
  100 s (`AutomaticWifiConnectionPolicy.h`, `WifiSelectionActivity`). Koszt:
  poza zasięgiem znanych sieci uśpienie trwa ~2 min z włączonym radiem.
- **Wysyłka do HA po nieudanym Wi-Fi nie startowała** (`sta is connecting,
  cannot set config`) — przed joinem zdarzeń przy uśpieniu `WiFi.disconnect`.

Zachowanie, które jest celowe (nie błąd):
- różnica ±1 strony = ta sama pozycja → bez wysyłki do kosync,
- strona liczy się do sesji po ≥ 2 s, sesja ≥ 10 s; szybkie kartkowanie albo
  cofanie nie tworzy sesji, więc nie ma pusha,
- tryb „Ask” przy wyjściu z książki pyta, gdy serwer jest dalej.

**Pilot testowy:** ESP32-C6 jako klawiatura BLE HID
(`~/Workspace/CrossPoint/ble-remote-tester/`, NimBLE-Arduino), sterowany z
terminala poleceniem `pilot` (`pilot n|p|d|u|e|s|log`, bez argumentu tryb
interaktywny). macOS nie pozwala Macowi udawać urządzenia BLE HID
(`CBErrorDomain 8`). Sprawdzone 14.10: parowanie, radio tylko przy otwartej
książce, każde naciśnięcie = jedna strona.

### 2. OTA z własnych wydań

- `OtaUpdater` pobiera wydania z repo z flagi `CROSSPOINT_OTA_REPO`
  (domyślnie upstream). Env **`x4pro-dkej`** ustawia `dkej123/crosspoint-reader`,
  więc „Check for updates” **nie podmieni forka na oficjalny build**.
- Wersje 4-członowe: `1.6.5.1`, `1.6.5.2`, … (4. segment = numer buildu forka).
  Porównanie: `1.6.5.2 > 1.6.5.1 > 1.6.5`.
- Asset w wydaniu: `crosspoint-<wersja>-x4pro.bin`.
- Wydanie: podbić `[dkej] version` w `platformio.ini`, potem
  `scripts/dkej_release.sh` (build + `gh release create` na forku).
- GitHub Actions na forku są wyłączone (workflow upstreamu budowałby 7 urządzeń).

**Pierwsze wgranie musi być przez USB** (z CrossInk/oficjalnego firmware OTA
nie przejdzie na fork):

```bash
pio run -e x4pro-dkej -t upload
```

### 3. Status w Home Assistant (wtyczka SD + webhook)

Bez zmian w firmware — korzysta z systemu wtyczek SD CrossPoint
(`docs/sd-plugins.md`, `docs/plugin-events.md`, tylko w `develop`).

**Na karcie SD:** folder `sd-card/plugins/homeassistant/` → `/plugins/homeassistant/`
plus `config.json` z adresem HA i webhook ID (sekret, git-ignored; wzór w
`config.example.json`).

Zdarzenia:
- `reader.session` — czas czytania, postęp początek/koniec, hash dokumentu
  KOSync; `connect: true` → przy uśpieniu czytnik włącza Wi-Fi, by dostarczyć,
- `sleep.enter` — książka i procent.

Przy uśpieniu kolejność jest: wysłanie pozycji do kosync → zdarzenia do HA →
sen. Wi-Fi tylko gdy bateria ≥ 20%, join max 10 s; nieudane zdarzenia czekają w
kolejce na SD (dostarczanie „co najmniej raz”).

Wartości tekstowe w treści żądań używają filtra `{event.NAME|json}` (dodatek
forka w `src/util/PluginTemplate.h`), więc JSON jest poprawny dla każdej nazwy
pliku (cudzysłowy, backslashe, polskie znaki).

### 3a. Konfiguracja Home Assistant (statystyki czytania)

Wszystko w `/config` na HA (poza tym repo). Kopie robocze, skrypty importu i
testów: `~/Workspace/CrossPoint/ha-xteink/` (zawierają webhook ID — lokalnie,
nie commitować).

**`configuration.yaml` → `template:`**
- Blok z wyzwalaczem `webhook` (`local_only: true`, tylko POST) — webhook ID
  jest sekretem (w `config.json` na karcie SD i w HA).
- `sensor.xteink_statystyki` — jedno źródło prawdy; stan = minuty łącznie,
  atrybuty: `laczny_czas_s`, `sesje`, `pory_dnia_s` [rano 5–12, popołudnie
  12–17, wieczór 17–21, noc 21–5], `dni_tygodnia_s` [pon..nd], `dni_czytania`
  (180 dni), `dni_czytania_lacznie`, `dzienne_s` (35 dni), `seria_najdluzsza`,
  `ksiazki` {hash KOReader → tytul, plik, czas_s, sesje, start, postep,
  ukonczona, ostatnio}, `biezaca_ksiazka`, `ostatnia_sesja`, `ostatnie_sesje`
  (50), `przetworzone_id` (200, deduplikacja), `import_crossink`.
  - sesje dzielone między pory dnia / dni tygodnia / dni kalendarzowe według
    rzeczywistego czasu startu i końca (jak w CrossInk), także przez północ;
  - zdarzenia spóźnione liczone do dnia sesji, nie dostarczenia;
  - `sleep.enter` z prawdziwym procentem aktualizuje postęp książki;
  - limit rozmiaru atrybutów (przycinanie przy > 14 KB).
  - **dzień czytania kończy się o 05:00**, nie o północy — czytanie po północy
    liczy się do poprzedniego dnia (`dzienne_s`, `dni_czytania`,
    `dni_tygodnia_s`, serie, „dziś”, kalendarz); pory dnia bez zmian;
  - zdarzenie `korekta` (jednorazowe, idempotentne po `id`, ślad w atrybucie
    `korekty`): przesunięcia sekund między dniami tygodnia / porami dnia,
    dodanie / usunięcie dni czytania, `przeklucz_ksiazki` (scalenie wpisów
    książki spod starego hasha pod nowy).
- Sensory pochodne: `sensor.xteink_czas_laczny` (h, `total_increasing` →
  długoterminowe statystyki), `_sesje`, `_seria_biezaca`, `_seria_najdluzsza`,
  `_srednia_sesja`, `_czas_czytania_dzis`, `_ksiazka`, `_postep`,
  `_tempo_czytania` (%/h), `_szacowany_czas_do_konca`.

**`configuration.yaml` → `utility_meter:`** — `xteink_czytanie_dzien /
_tydzien / _miesiac` na `sensor.xteink_czas_laczny` (wymaga restartu HA).

**Integracja HTTP Requests (`dkej123/ha-http-requests` ≥ 0.8.0)** — zapytanie
„kosync progress” (sekcja Xteink, entry `01M4XJ9P7Y8GZPQ6XM1MRB6VJ0`): GET
`https://kosync.haosdk.pl/syncs/progress/{{ document }}`, nagłówki `Accept`,
`x-auth-user` oraz `x-auth-key` (MD5 hasła — wpisany ręcznie w panelu, nie w
YAML). Automatyzacja wywołuje je przez `http_requests.execute` z
`variables: {document: …}`. Zastąpiło wcześniejszy `rest_command` (usunięty
14.10, razem z `kosync_user`/`kosync_key` w `secrets.yaml`).

**`automations.yaml`**
- „Xteink – potwierdzenie synchronizacji (push na Pixel)” — po nowej,
  najnowszej sesji czeka 20 s (czytnik wysyła webhook zaraz po połączeniu z
  Wi-Fi, a pozycję do kosync chwilę później), odczytuje pozycję z kosync
  akcją `http_requests.execute` (integracja HTTP Requests, zapytanie
  „kosync progress” z `{{ document }}` w URL) i wysyła na
  `notify.mobile_app_pixel_9`: ✅ zsynchronizowane (różnica ≤ 0,15 pkt) /
  ℹ️ na serwerze dalej (nie nadpisano) / ⚠️ nie dotarło.
- „Xteink – sesja czytania w dzienniku” — wpis do logbooka, np.
  „Zmiana: 25 min, 31% → 34%” (spóźnione: „sesja z RRRR-MM-DD, dosłana później”).

**Dashboard** „Przegląd” (`lovelace`) → widok **Czytanie** (`/lovelace/czytanie`):
kafelki, wykres dzienny czasu czytania, kalendarz 5 tygodni, tabele pór dnia i
dni tygodnia, książki, ostatnie sesje, logbook.

**Import z CrossInk** (13.10.2026, jednorazowe zdarzenie `import.crossink`,
idempotentne): 608 min / 44 sesje, pory dnia i dni tygodnia z
`global_stats.bin`, 18 dni czytania, seria 15; książki kluczowane hashem
KOReadera (partial MD5, pierwszy fragment od offsetu 0 — jak w firmware): Silos
`8c71eec1cc13ddd041ba9a9d531617c4` (ukończona 5.10), Zmiana
`ee588842248b1d44e453836ae77b7a5d` (23,1%). Import pierwotnie policzył hashe od
offsetu 256 — poprawione 14.10 korektą `przeklucz_ksiazki`
(`ha-xteink/korekta_hashe_2026-10-14.py`), po pierwszej sesji z czytnika. Korekta 14.10.2026
(`ha-xteink/korekta_2026-10-10.py`): CrossInk liczył dni kalendarzowo, a 34,5 min
nocy przypisane niedzieli 11.10 to czytanie w sobotę 10.10 po północy — przeniesione
na sobotę; 10.10 dniem czytania, najdłuższa seria 19 dni (25.09–13.10). Przybliżone czasy
dzienne z kopii zapasowych CrossInk celowo pominięte. Pełna kopia karty SD z
CrossInk: `~/Workspace/CrossPoint/backup-crossink-2026-10-13/` (zawiera hasła —
nie commitować).

Kopie konfiguracji HA: `configuration.yaml.pre-xteink-20261013`,
`configuration.yaml.pre-xteink-stats-20261013`,
`automations.yaml.pre-xteink-20261013`,
`automations.yaml.pre-xteink-stats-20261013`,
`secrets.yaml.pre-kosync-20261013`,
`automations.yaml.pre-http-requests-kosync-20261014`,
`automations.yaml.pre-xteink-opoznienie-20261014`,
`automations.yaml.pre-xteink-tolerancja-20261014`.

### 4. Pilot BLE do kartkowania

- Host BLE HID z SDK (`BleKeyboardHost`, flaga `FREEINK_CAP_BLE_HID_HOST`) na
  NimBLE-Arduino — włączony **tylko w env `x4pro-dkej`**; inne buildy mają
  zaślepki (`src/ble/BleRemote.cpp`), więc kod upstreamu się nie zmienia.
- **Settings → Controls → Bluetooth Remote** — przełącznik (domyślnie wyłączony).
- **Settings → Controls → Pair Bluetooth Remote** — skan 10 s, lista urządzeń
  (`[HID]` = reklamuje usługę HID), wybór = połączenie i bonding (ew. kod do
  wpisania na pilocie). Adres zapisany w ustawieniach (`bleRemoteAddr`),
  bond w NVS. „Zapomnij pilota” usuwa bond i wyłącza funkcję.
- **Radio działa tylko przy otwartej książce** (`ReaderActivity::onEnter` →
  start, `onExit`/uśpienie → stop) i tylko gdy przełącznik jest włączony i
  pilot sparowany. Na ekranie parowania stos działa tylko na czas tego ekranu.
- Mapowanie: PageDown / → / ↓ / Enter / spacja → następna strona;
  PageUp / ← / ↑ / Backspace → poprzednia. Przyciski, których dekoder
  klawiatury nie rozpoznaje (np. tanie piloty „TikTok” wysyłające głośność),
  przewracają do przodu.
- Wpięte w `ReaderUtils::detectPageTurn`, więc działa w EPUB, TXT i XTC i w
  każdej orientacji (pion/poziom).
- Tylko BLE (ESP32-S3 nie ma klasycznego Bluetooth).
- Obejście linkera `_btLibraryInUse` (`src/ble/BtLibraryInUseShim.cpp`) z
  upstream PR #3197 (`feat-ble`) — tego PR nie scalamy (1779 linii, 217 commitów
  za `develop`, pisany pod pamięć X4).

Ustawienia KOSync i teksty mają też polskie tłumaczenia (`polish.yaml`).

## Przegląd i testy

Błędy znalezione w przeglądzie (poprawione):
- **Quick Resume po odroczonym śnie** (z PR #2760) — zapisana klatka ekranu
  była od razu kasowana → `SleepFramePolicy.h`.
- **Zadanie automatycznego sprawdzania zabijane `vTaskDelete`** (z PR #2760)
  przy wyjściu z książki/uśpieniu — mogło zostawić zajęte blokady Wi-Fi/TLS/SD
  i ścigało się z samousunięciem zadania. Teraz zadanie współdzieli
  `AutomaticProgressCheckJob` i jest tylko porzucane; porzucone nie rusza już
  Wi-Fi.
- **NimBLE włączony w chwili głębokiego snu** — blokujące, ograniczone
  `bleremote::shutdownForSleep()` w `commitDeepSleep`.
- **Podwójne kartkowanie** — klawisz i surowe zdarzenie z tego samego
  naciśnięcia w różnych klatkach.
- **Niepoprawny JSON we wtyczce** przy nazwie pliku z cudzysłowem — filtr `|json`.

Testy (GoogleTest, `test/`, pisane z wymagań, nie z kodu): `ble_remote`,
`ota_version`, `sleep_frame_policy`, `ha_plugin_manifest` (prawdziwy
`device.json`, ArduinoJson 7.4.2 jak w firmware), `automatic_check_abandon`,
`sleep_wifi_policy`, `orientation_rotation`, `plugin_event_id`, `sync_retry`
(+ przepisane `AutomaticWifiConnectionPolicyTest`).
Każdy poprawiony błąd został celowo przywrócony, by potwierdzić, że test go
wykrywa. Całość: **629/629**.

```bash
cmake -S test -B build/test && cmake --build build/test -j && ctest --test-dir build/test
```

## Odrzucone pomysły (i dlaczego)

- **ESP-NOW + pośrednik w domu** — CrossInk ma „Nearby Position Sync” po
  ESP-NOW (nagłówek `CIBP`, kanał 1 na sztywno, hash KOSync), więc ESP w domu
  mógłby udawać drugi czytnik. Ale: nadal ręczne uruchamianie z menu, router
  musiałby stać na kanale 1, protokół nieudokumentowany, a pobieranie pozycji
  wymaga znajomości książki. Auto-KOSync rozwiązuje problem prościej.
- **Status przez BLE advertising do skanerów ESPHome** — energetycznie tańsze
  per zdarzenie, ale Wi-Fi i tak włącza się przy uśpieniu dla kosync, więc
  status po webhooku jest praktycznie darmowy. BLE wymagałby też kodu w forku i
  przeflashowania skanerów, od których zależy obecność/alarm. Do rozważenia
  tylko dla statusu „na żywo” w trakcie czytania.
- **Athene (com.aditya.athene)** — zamknięta aplikacja, wymaga serwera
  athene-sync, nie zwykłego kosync. Zamiast niej **Readest** (AGPL, KOSync z
  własnym URL; pobiera przy otwarciu/powrocie, wysyła 5 s po zmianie strony i
  przy zamknięciu; strategie silent/prompt/send/receive).

## Utrzymanie

```bash
git fetch upstream
git switch dkej/x4pro
git merge upstream/develop      # konflikty najczęściej: sleep path w main.cpp,
                                # KOReaderSettingsActivity, KOReaderSyncActivity
pio run -e x4pro-dkej
```

Gdy PR #2760 zostanie scalony w upstreamie, merge powinien się „wchłonąć”.

Diagnoza na sprzęcie: `pio run -e x4pro-dkej-debug` (to samo co `x4pro-dkej`,
wersja `…-debug`, `LOG_LEVEL=2`), wgranie tylko aplikacji
`esptool --chip esp32s3 write-flash 0x10000 .pio/build/x4pro-dkej-debug/firmware.bin`
i log z USB (115200). Czytnik musi być wybudzony przez całe wgrywanie — gdy
zaśnie w trakcie, aplikacja jest uszkodzona do czasu ponownego wgrania
(bootloader i partycje zostają, więc to zawsze da się naprawić). Po diagnozie
wrócić na `x4pro-dkej`.
