#pragma once

#include <HalStorage.h>
#include <Logging.h>
#include <PendingBookList.h>

#include <string>
#include <vector>

// Fork: books whose last automatic KOSync upload did not finish (no Wi-Fi, network
// error, power press, crash), and finished books waiting for the next upload; see
// PendingBookList.h. Main loop only: these are unsynchronized read-modify-writes.
namespace kosyncpending {

constexpr char FILE_PATH[] = "/.crosspoint/kosync_pending.txt";

// False when the file exists but could not be read: writing then would lose it.
inline bool readList(std::string& list) {
  list.clear();
  if (!Storage.exists(FILE_PATH)) return true;
  if (Storage.readFileToString("KOSync", FILE_PATH, pendingbooks::MAX_BYTES + 1024, list)) return true;
  LOG_ERR("KOSync", "Could not read retry list");
  list.clear();
  return false;
}

inline void writeList(const std::string& list) {
  if (list.empty()) {
    Storage.remove(FILE_PATH);
  } else if (!Storage.writeFile(FILE_PATH, String(list.c_str()))) {
    LOG_ERR("KOSync", "Could not save retry marker");
  }
}

inline bool isPendingFor(const std::string& bookPath) {
  std::string list;
  return readList(list) && pendingbooks::contains(list, bookPath);
}

inline bool anyFinishedExcept(const std::string& bookPath) {
  std::string list;
  return readList(list) && pendingbooks::anyFinishedExcept(list, bookPath);
}

inline std::vector<pendingbooks::Book> books() {
  std::string list;
  readList(list);
  return pendingbooks::parse(list);
}

inline void mark(const std::string& bookPath, const std::string& finishedXpath = "") {
  std::string list;
  if (readList(list)) writeList(pendingbooks::added(list, bookPath, finishedXpath));
}

// For sleep paths, where the open-book path may still name the book just finished:
// an entry that already exists keeps its finished mark.
inline void markIfAbsent(const std::string& bookPath) {
  std::string list;
  if (readList(list) && !pendingbooks::contains(list, bookPath)) writeList(pendingbooks::added(list, bookPath));
}

inline void clearFor(const std::string& bookPath) {
  std::string list;
  if (readList(list) && pendingbooks::contains(list, bookPath)) writeList(pendingbooks::removed(list, bookPath));
}

// The book moved or was renamed: the waiting upload follows it.
inline void rename(const std::string& from, const std::string& to) {
  std::string list;
  if (readList(list) && pendingbooks::contains(list, from)) writeList(pendingbooks::renamed(list, from, to));
}

}  // namespace kosyncpending
