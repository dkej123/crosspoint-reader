# Home Assistant: przejście na zdarzenie `kosync.result`

Od poprawek z 14.10.2026 (FORK.md, sekcja 1b) czytnik sam mówi HA, co zrobiła
automatyczna wysyłka pozycji do kosync. Automatyzacja nie musi już czekać 20 s
i odpytywać serwera kosync ani porównywać procentów z tolerancją 0,15 pkt.

Zmienia się też kolejność: zdarzenia wtyczki (`reader.session`, `sleep.enter`,
`kosync.result`) idą teraz **po** zapisie w kosync, na tym samym połączeniu.

Wszystkie zmiany są w `/config` na HA. Webhook ID to ten sam sekret co w
`config.json` na karcie SD — poniżej `TWOJ_WEBHOOK_ID`. Nie commitować go.

## 0. Kopie zapasowe

```sh
cp configuration.yaml configuration.yaml.pre-kosync-result-20261015
cp automations.yaml automations.yaml.pre-kosync-result-20261015
```

## 1. Co wysyła czytnik

POST na `/api/webhook/<id>`, JSON:

```json
{
  "event": "kosync.result",
  "id": "3fa9c21b-42-7",
  "ts": 1792050000,
  "book": "/Ksiazki/Zmiana.epub",
  "document": "ee588842248b1d44e453836ae77b7a5d",
  "result": "uploaded",
  "percent": "34.12",
  "remote_percent": "33.80"
}
```

| `result`       | Znaczenie                                                                    |
|----------------|------------------------------------------------------------------------------|
| `uploaded`     | pozycja wysłana do kosync                                                    |
| `in_sync`      | serwer już ma tę stronę, nic nie trzeba było wysyłać                         |
| `remote_ahead` | na serwerze jest dalej (np. czytane w Readest) — **nie nadpisano**           |
| `failed`       | błąd sieci/serwera po ponowieniach — następne wyjście z **tej samej** książki ponowi, nawet bez przewracania strony |
| `no_wifi`      | nie udało się połączyć z Wi-Fi — pozycja pójdzie przy następnym przewróceniu strony i wyjściu |
| `error`        | błąd, którego ponawianie nie naprawi (logowanie, hash, pozycja z serwera nie pasuje do książki) |

Wyjście z książki bez przewrócenia strony nie wysyła nic do kosync i nie
tworzy zdarzenia.

- `percent` i `remote_percent` to 0–100 z dwoma miejscami po przecinku.
  `remote_percent` jest pusty, gdy czytnik nie zna pozycji z serwera.
- `ts` to czas Unix powstania zdarzenia, a 0, gdy zegar nie był ustawiony.
  Przy `no_wifi` zdarzenie czeka na karcie i dochodzi przy następnym
  połączeniu, więc może przyjść z opóźnieniem.
- Dostarczanie działa w trybie „co najmniej raz”, więc to samo `id` może
  przyjść dwa razy. Deduplikuj po `id`.

## 2. Istniejący blok statystyk (`configuration.yaml` → `template:`)

Blok z wyzwalaczem `webhook`, który liczy `sensor.xteink_statystyki`, będzie
teraz dostawał także `kosync.result` — przy prawie każdym wyjściu z książki i
uśpieniu po czytaniu. To nie jest opcjonalne; sprawdź dwie rzeczy:

- Czy dla nieznanego `event` zostawia stan i atrybuty bez zmian. Sprawdzić
  trzeba gałąź „else” po `trigger.json.event`.
- Czy dopisuje `id` do `przetworzone_id` tylko dla zdarzeń, które przetwarza.
  W przeciwnym razie `kosync.result` będzie wypychać z listy 200 ostatnich id
  identyfikatory sesji.

Jeśli którykolwiek warunek nie jest spełniony, najprościej dodać do tego bloku
warunek `conditions:` (jak w punkcie 3) ograniczony do jego zdarzeń. Albo na
początku obsługi coś takiego:

```jinja
{% if trigger.json.event not in ['reader.session', 'sleep.enter', 'korekta', 'import.crossink'] %}
  {{ this.state }}   {# atrybuty analogicznie: this.attributes.<nazwa> #}
{% else %} ... {% endif %}
```

## 3. Nowy sensor `sensor.xteink_kosync` (`configuration.yaml` → `template:`)

Dodaj osobny blok. HA pozwala na kilka wyzwalaczy `webhook` z tym samym ID,
ale `local_only` i `allowed_methods` muszą być takie same jak w istniejącym
bloku.

```yaml
template:
  # ... istniejące bloki bez zmian ...

  - triggers:
      - trigger: webhook
        webhook_id: TWOJ_WEBHOOK_ID
        allowed_methods: [POST]
        local_only: true
    conditions:
      - condition: template
        value_template: >-
          {{ trigger.json.event == 'kosync.result'
             and trigger.json.id != state_attr('sensor.xteink_kosync', 'id') }}
    sensor:
      - name: "Xteink kosync"
        unique_id: xteink_kosync
        icon: mdi:book-sync
        state: "{{ trigger.json.result }}"
        attributes:
          id: "{{ trigger.json.id }}"
          ts: "{{ trigger.json.ts | int(0) }}"
          ksiazka: "{{ trigger.json.book | regex_replace('^.*/', '') | regex_replace('\\.epub$', '', ignorecase=True) }}"
          plik: "{{ trigger.json.book }}"
          dokument: "{{ trigger.json.document }}"
          procent: "{{ trigger.json.percent | float(none) }}"
          procent_serwer: "{{ trigger.json.remote_percent | float(none) if trigger.json.remote_percent else none }}"
```

Warunek `id != …` odcina powtórzone dostarczenia, więc każde nowe `id` zmienia
atrybut dokładnie raz. Po zapisaniu zrób **Narzędzia deweloperskie → YAML →
Encje szablonów** (przeładowanie) albo restart HA.

## 4. Automatyzacja potwierdzenia (`automations.yaml`)

**Usuń albo wyłącz** starą automatyzację „Xteink – potwierdzenie
synchronizacji (push na Pixel)”: tę z `delay: 20 s`, `http_requests.execute`
i tolerancją 0,15. Zastąp ją tą:

```yaml
- id: xteink_kosync_potwierdzenie
  alias: "Xteink – potwierdzenie synchronizacji (push na Pixel)"
  description: "Push z wynikiem automatycznej wysyłki pozycji do kosync (zdarzenie kosync.result)."
  mode: queued
  triggers:
    - trigger: state
      entity_id: sensor.xteink_kosync
      attribute: id
  conditions:
    # pomija start HA / przeładowanie szablonów i wpisy bez wyniku
    - condition: template
      value_template: >-
        {{ trigger.from_state is not none and trigger.to_state is not none
           and trigger.to_state.state not in ['unknown', 'unavailable'] }}
  actions:
    - variables:
        a: "{{ trigger.to_state.attributes }}"
        wynik: "{{ trigger.to_state.state }}"
        lokalnie: "{{ '%.1f' | format(a.procent | float(0)) }}"
        serwer: "{{ '%.1f' | format(a.procent_serwer | float(0)) if a.procent_serwer is number else '?' }}"
        opoznienie_min: "{{ ((now().timestamp() - a.ts) / 60) | int if a.ts | int(0) > 0 else 0 }}"
        tytul: >-
          {{ {'uploaded': '✅ Zsynchronizowano',
              'in_sync': '✅ Już zsynchronizowane',
              'remote_ahead': 'ℹ️ Na serwerze dalej',
              'failed': '⚠️ Nie wysłano do kosync',
              'no_wifi': '⚠️ Brak Wi-Fi przy wysyłce',
              'error': '❌ Błąd synchronizacji'}.get(wynik, wynik) }}
        tresc: >-
          {{ a.ksiazka }}: {% if wynik in ['uploaded', 'in_sync'] %}{{ lokalnie }}%
          {%- elif wynik == 'remote_ahead' %}serwer {{ serwer }}%, czytnik {{ lokalnie }}% — nie nadpisano
          {%- elif wynik == 'failed' %}{{ lokalnie }}% — czytnik ponowi przy następnym wyjściu z tej książki
          {%- elif wynik == 'no_wifi' %}{{ lokalnie }}% — pójdzie po następnym przewróceniu strony
          {%- else %}sprawdź ustawienia KOSync na czytniku (logowanie, dopasowanie dokumentu){% endif %}
          {%- if opoznienie_min > 10 %} (zdarzenie sprzed {{ opoznienie_min }} min){% endif %}
    - action: notify.mobile_app_pixel_9
      data:
        title: "{{ tytul }}"
        message: "{{ tresc }}"
        data:
          tag: xteink-kosync   # nowy push zastępuje poprzedni
```

Jeśli nie chcesz pushów przy każdym sukcesie, dodaj `uploaded` i `in_sync` do
listy wykluczeń w `conditions`. Wtedy powiadomienie przyjdzie tylko przy
problemach i przy „na serwerze dalej”.

## 5. Co można usunąć (opcjonalnie)

- Zapytanie „kosync progress” w integracji HTTP Requests nie jest już
  potrzebne do potwierdzeń. Można je zostawić do ręcznego sprawdzania.
- Automatyzacja „Xteink – sesja czytania w dzienniku” zostaje bez zmian.

## 6. Kolejność wdrożenia

1. HA: punkty 2–4, a potem przeładowanie szablonów i automatyzacji.
2. Test z sieci lokalnej (`local_only`):

   ```sh
   curl -X POST -H 'Content-Type: application/json' \
     -d '{"event":"kosync.result","id":"test-1","ts":0,"book":"/Test.epub","document":"0","result":"uploaded","percent":"12.34","remote_percent":"10.00"}' \
     http://<adres-HA>:8123/api/webhook/TWOJ_WEBHOOK_ID
   ```

   Sprawdź, że `sensor.xteink_kosync` = `uploaded`, że przyszedł push i że
   statystyki się **nie** zmieniły. Drugie wywołanie z tym samym `id` nie
   powinno wysłać pushu. Jeśli przeładowanie szablonów zgłosi błąd webhooka
   (dwa bloki z tym samym ID), przenieś sensor do istniejącego bloku.
3. Karta SD: skopiuj nowy `device.json` do `/plugins/homeassistant/`.
   `config.json` się nie zmienia.
4. Wgraj firmware (`pio run -e x4pro-dkej -t upload`), otwórz książkę,
   przewróć stronę i wyjdź albo uśpij czytnik. Powinien przyjść push
   „✅ Zsynchronizowano”. Wyjście bez przewracania strony nie daje pushu.
