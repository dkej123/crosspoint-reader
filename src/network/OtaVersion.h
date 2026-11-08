#pragma once

#include <cstdio>
#include <cstring>

namespace ota {

// Whether `latest` (a release tag) should be offered as an update over
// `current` (the running CROSSPOINT_VERSION). Versions are MAJOR.MINOR.PATCH
// with an optional 4th fork build number (1.6.5.2 > 1.6.5.1 > 1.6.5); any
// suffix after the numbers ("-x4pro", "-rc+abc") is ignored for ordering. An
// unparsable `latest` is never offered. A running "-rc" build is replaced by
// the final release of the same version.
inline bool isVersionNewer(const char* latest, const char* current) {
  if (!latest || !current || !latest[0]) return false;
  if (std::strcmp(latest, current) == 0) return false;

  int l[4] = {0, 0, 0, 0};
  int c[4] = {0, 0, 0, 0};
  if (std::sscanf(latest, "%d.%d.%d.%d", &l[0], &l[1], &l[2], &l[3]) < 3) return false;
  std::sscanf(current, "%d.%d.%d.%d", &c[0], &c[1], &c[2], &c[3]);

  for (int i = 0; i < 4; i++) {
    if (l[i] != c[i]) return l[i] > c[i];
  }
  return std::strstr(current, "-rc") != nullptr && std::strstr(latest, "-rc") == nullptr;
}

}  // namespace ota
