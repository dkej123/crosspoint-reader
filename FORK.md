# dkej123/crosspoint-reader — fork pod Xteink X4 Pro

Fork [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)
(gałąź `develop`) pod mój Xteink X4 Pro. Gałąź robocza: **`dkej/x4pro`**.

Cel: synchronizacja pozycji czytania między Xteinkiem a telefonem (Readest)
bez ręcznego klikania, potwierdzenie synchronizacji w Home Assistant oraz pilot
BLE do kartkowania.

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

**W Home Assistant** (`/config`, poza tym repo):
- `configuration.yaml` → `template:` blok z webhookiem (`local_only`) i
  sensorami `sensor.xteink_ksiazka`, `sensor.xteink_postep`,
  `sensor.xteink_czas_czytania_dzis` (deduplikacja sesji po `event.id`),
- `configuration.yaml` → `rest_command.kosync_get_progress` (GET
  `/syncs/progress/<hash>` z `!secret kosync_user` / `kosync_key`),
- `secrets.yaml` → `kosync_user`, `kosync_key` (= MD5 hasła),
- `automations.yaml` → **„Xteink – potwierdzenie synchronizacji (push na
  Pixel)”**: po każdej sesji czyta pozycję z kosync i wysyła na Pixela
  ✅ zsynchronizowane / ℹ️ na serwerze dalej / ⚠️ nie dotarło.

Kopie zapasowe w HA: `configuration.yaml.pre-xteink-20261013`,
`automations.yaml.pre-xteink-20261013`, `secrets.yaml.pre-kosync-20261013`.

### 4. Pilot BLE do kartkowania (w toku)

- SDK `freeink-sdk` ma gotowy host BLE HID (`BleKeyboardHost`) za flagą
  `FREEINK_CAP_BLE_HID_HOST` — włączany tylko w env forka.
- **Działa wyłącznie przy otwartej książce** i tylko gdy włączony jest
  **przełącznik w ustawieniach**.
- Tylko BLE (ESP32-S3 nie ma klasycznego Bluetooth) — piloty/gamepady BLE HID.
- Inspiracja: fork CrumBLE (CrossInk) — tam ekran parowania i mapowanie klawiszy.

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
