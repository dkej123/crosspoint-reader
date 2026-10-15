#pragma once

#include <HalStorage.h>
#include <Logging.h>

#include <string>

// Fork: remembers that the last automatic KOSync upload for a book failed after
// joining Wi-Fi, so the next exit/sleep with that book uploads again even when
// no page was turned in between (the "position unchanged" skip would drop it).
// One slot: a failure in another book replaces it.
namespace kosyncpending {

constexpr char FILE_PATH[] = "/.crosspoint/kosync_pending.txt";

inline bool isPendingFor(const std::string& bookPath) {
  std::string stored;
  return Storage.exists(FILE_PATH) && Storage.readFileToString("KOSync", FILE_PATH, 512, stored) && stored == bookPath;
}

inline void mark(const std::string& bookPath) {
  if (!Storage.writeFile(FILE_PATH, String(bookPath.c_str()))) LOG_ERR("KOSync", "Could not save retry marker");
}

// Another book's outcome must not drop this book's pending retry.
inline void clearFor(const std::string& bookPath) {
  if (isPendingFor(bookPath)) Storage.remove(FILE_PATH);
}

}  // namespace kosyncpending
