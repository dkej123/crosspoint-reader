#!/usr/bin/env bash
# Build the X4 Pro fork firmware and publish it as a GitHub release on the fork,
# where on-device "Check for updates" (CROSSPOINT_OTA_REPO) picks it up.
# Usage: scripts/dkej_release.sh            (uses [dkej] version from platformio.ini)
set -euo pipefail
cd "$(dirname "$0")/.."
version=$(awk -F' *= *' '/^\[dkej\]/{s=1;next} /^\[/{s=0} s&&$1=="version"{print $2}' platformio.ini)
repo=$(awk -F' *= *' '/^\[dkej\]/{s=1;next} /^\[/{s=0} s&&$1=="ota_repo"{print $2}' platformio.ini)
[[ -n "$version" && -n "$repo" ]] || { echo "missing [dkej] version/ota_repo" >&2; exit 1; }
if gh release view "$version" --repo "$repo" >/dev/null 2>&1; then
  echo "release $version already exists on $repo — bump [dkej] version first" >&2; exit 1
fi
pio run -e x4pro-dkej
asset="crosspoint-${version}-x4pro.bin"
# The device looks the asset up by file name; gh's "file#label" only sets the
# display label, so upload a file that already has the exact name.
dist=$(mktemp -d)
trap 'rm -rf "$dist"' EXIT
cp .pio/build/x4pro-dkej/firmware.bin "${dist}/${asset}"
git push origin HEAD
gh release create "$version" --repo "$repo" --target "$(git rev-parse HEAD)" \
  --title "$version (X4 Pro fork)" --notes "Fork build of $(git rev-parse --short HEAD)." \
  "${dist}/${asset}"
echo "published $asset to $repo"
