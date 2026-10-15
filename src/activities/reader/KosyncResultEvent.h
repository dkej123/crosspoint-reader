#pragma once

#include <cstdio>
#include <string>

#include "KOReaderDocumentId.h"
#include "util/PluginEvents.h"

// Fork: queues the "kosync.result" plugin event (docs/plugin-events.md) so Home
// Assistant learns what the automatic upload did instead of polling KOSync.
// percentages are 0..1; remotePercentage < 0 means unknown.
inline void emitKosyncResult(const std::string& bookPath, const char* result, const float localPercentage,
                             const float remotePercentage = -1.0f) {
  if (!pluginevents::anySubscriber(pluginevents::Event::KosyncResult)) return;

  // Same id as reader.session, so both events key the same book.
  const std::string document = KOReaderDocumentId::calculate(bookPath);
  char percent[12];
  char remotePercent[12] = "";
  snprintf(percent, sizeof(percent), "%.2f", static_cast<double>(localPercentage * 100.0f));
  if (remotePercentage >= 0.0f) {
    snprintf(remotePercent, sizeof(remotePercent), "%.2f", static_cast<double>(remotePercentage * 100.0f));
  }
  const pluginevents::Var vars[] = {{"book", bookPath.c_str()},
                                    {"document", document.c_str()},
                                    {"result", result},
                                    {"percent", percent},
                                    {"remote_percent", remotePercent}};
  pluginevents::emit(pluginevents::Event::KosyncResult, vars, sizeof(vars) / sizeof(vars[0]));
}
