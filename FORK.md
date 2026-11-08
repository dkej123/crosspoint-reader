# dkej123/crosspoint-reader — fork pod Xteink X4 Pro

Fork [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)
(gałąź `develop`). Gałąź robocza: **`dkej/x4pro-2.0`**, env **`x4pro-dkej`**.
Poprzednia wersja forka (1.6.5.x, z integracją Home Assistant i własnym pilotem
BLE): tag `x4pro-1.6.5-final`, gałąź `dkej/x4pro`.

## Co dodaje fork

### 1. Automatyczny KOSync (upstream PR #2760 + poprawki forka)

Ustawienia: **Settings → Connectivity → CrossPoint Sync**, obie opcje domyślnie
wyłączone.

- **Automatic progress upload** — przy wyjściu z książki albo uśpieniu czytnik
  łączy się z Wi-Fi, pobiera pozycję z serwera i wysyła swoją tylko wtedy, gdy
  lokalna jest dalej. Bez przewrócenia strony nic nie wysyła, chyba że poprzednia
  wysyłka nie doszła do skutku, także z braku Wi-Fi, po naciśnięciu zasilania albo
  awarii (`/.crosspoint/kosync_pending.txt`, do 8 książek). Koszt: po wyjściu poza
  zasięg kolejne uśpienie z tą książką znów szuka sieci do 100 s. Poniżej 20% baterii
  przy uśpieniu nie wysyła, tylko zapisuje znacznik.
  Ukończona książka idzie na serwer jako 100% i początek ostatniego rozdziału. Wybór
  kolejnej książki z menu końcowego otwiera ją od razu, a ukończoną zapisuje na liście.
  Najbliższa wysyłka innej książki wysyła też książki z listy, bez ich otwierania, i
  uśpienie z książką łączy się z Wi-Fi nawet bez przewrócenia strony, gdy coś czeka.
  Ponowne otwarcie ukończonej książki anuluje wpis i idzie ona zwykłą wysyłką. Z ekranu
  domowego czytnik nic nie wysyła. Serwera z 100% nie nadpisuje niższą pozycją.
  Przeniesienie książki przez stronę WWW lub WebDAV usuwa ją z listy.
- **Automatic progress check** — przy otwarciu książki albo wybudzeniu sprawdza
  serwer w tle. Gdy serwer jest dalej, pyta **Resume / No**. Nigdy nie wysyła.

Limity czasu:

| Sytuacja | Wi-Fi | Całość | Co widać |
|---|---|---|---|
| Uśpienie | do 100 s, kolejne rundy po zapisanych sieciach | do 120 s | ekran snu, światło zgaszone; przycisk zasilania wraca do książki; po ~150 s czytnik zasypia mimo wszystko |
| Wyjście z książki | do 10 s | do 30 s | strona książki |
| Sprawdzenie w tle | do 15 s, do 10 s na sieć | + do 15 s na zapytanie | można czytać dalej |

Współpraca z pilotem BLE z upstreamu (`lib/BlePageTurner`): moduł nie pilnuje Wi-Fi,
więc radio jest zatrzymywane przed każdym użyciem Wi-Fi przez KOSync, a na czas
sprawdzenia w tle `main.cpp` nie pozwala mu wystartować. Pilot rozłącza się więc na
chwilę po otwarciu książki albo wybudzeniu, gdy sprawdzanie jest włączone.

Konfiguracja czytnika:

- **Sync Server URL:** `https://kosync.haosdk.pl` (puste = serwer CrossPoint)
- **Server type:** KOSync server
- **Document matching:** Binary (w Readest: „File Content”)

### 2. Obrót przytrzymaniem przycisku

Włączane ustawieniem **Controls → Long-Press Button Behavior → Orientation Change**.
Prawy przycisk (6, `Side Down`) zawsze obraca czytnik o ćwierć obrotu w prawo,
lewy (3, `Side Up`) w lewo, niezależnie od orientacji (`OrientationRotation.h`).
Po obrocie przyciski stron są ignorowane do ich puszczenia. Upstream: issue #3571,
PR #3982.

### 3. OTA z wydań forka

`OtaUpdater` pobiera wydania z repo z flagi `CROSSPOINT_OTA_REPO`, więc „Check for
updates” nie podmieni forka na oficjalny build. Wersje są 4-członowe: `2.0.0.1`,
`2.0.0.2`, … Wydanie: podbić `[dkej] version` w `platformio.ini`, potem
`scripts/dkej_release.sh`. Wgranie przez USB: `pio run -e x4pro-dkej -t upload`.
GitHub Actions na forku są włączone: wypchnięcie brancha i opublikowanie wydania mogą
uruchomić workflow upstreamu.

## Testy

```bash
cmake -S test -B build/test && cmake --build build/test -j && ctest --test-dir build/test
```

Diagnoza na sprzęcie: `pio run -e x4pro-dkej-debug` (`LOG_LEVEL=2`) i log z USB
(115200). Czytnik musi być wybudzony przez całe wgrywanie.

## Utrzymanie

Fork trzyma się jako krótka lista commitów nad `upstream/develop`, po jednym na
funkcję:

```bash
git fetch upstream
git switch dkej/x4pro-2.0
git rebase upstream/develop
ctest --test-dir build/test && pio run -e x4pro-dkej
```

Gdy #2760 albo #3982 wejdą do upstreamu, wystarczy usunąć odpowiedni commit.
