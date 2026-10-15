# Home Assistant plugin (SD card)

Copy this folder to `/plugins/homeassistant/` on the reader's SD card and
create `config.json` next to `device.json` (see `config.example.json`). The
webhook id is a secret — `config.json` is git-ignored.

Events (see `docs/plugin-events.md`):

- `reader.session` — reading time, start/end progress and the KOSync document
  hash; brings Wi-Fi up at sleep entry to deliver.
- `sleep.enter` — book and whole-book percent, delivered on the same sleep.
- `kosync.result` (fork) — what the automatic KOSync upload did; see
  `HA_KOSYNC_RESULT.md` (Polish) for the Home Assistant changes.

The Home Assistant side is a webhook-triggered template sensor block
(`sensor.xteink_*`) plus an automation that pushes a sync confirmation to the
phone when `kosync.result` arrives (`sensor.xteink_kosync`). Reading the
document back from the KOSync server is no longer needed for that.
