# Home Assistant plugin (SD card)

Copy this folder to `/plugins/homeassistant/` on the reader's SD card and
create `config.json` next to `device.json` (see `config.example.json`). The
webhook id is a secret — `config.json` is git-ignored.

Events (see `docs/plugin-events.md`):

- `reader.session` — reading time, start/end progress and the KOSync document
  hash; brings Wi-Fi up at sleep entry to deliver.
- `sleep.enter` — book and whole-book percent, delivered on the same sleep.

The Home Assistant side is a webhook-triggered template sensor block
(`sensor.xteink_*`) plus an automation that reads the document back from the
KOSync server and pushes a confirmation to the phone.
