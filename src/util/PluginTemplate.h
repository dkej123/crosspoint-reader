#pragma once

#include <cstdio>
#include <string>

// Template variable substitution for plugin event requests (host-testable).
//
// "{NAME}" inserts the raw value. "{NAME|json}" inserts the value escaped for a
// JSON string literal (quotes, backslashes, control characters), so a body like
//   {"book":"{event.book|json}"}
// stays valid JSON whatever the book path contains. Unknown names are left
// as-is (plugin-events.md: unmatched variables stay literal).
namespace plugintemplate {

inline std::string jsonEscape(const std::string& value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (const char ch : value) {
    const auto c = static_cast<unsigned char>(ch);
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += ch;
        }
    }
  }
  return out;
}

inline void replaceAll(std::string& s, const std::string& key, const std::string& value) {
  size_t pos = 0;
  while ((pos = s.find(key, pos)) != std::string::npos) {
    s.replace(pos, key.size(), value);
    pos += value.size();
  }
}

// name without braces, e.g. "event.book".
inline void substituteVar(std::string& tpl, const std::string& name, const std::string& value) {
  replaceAll(tpl, "{" + name + "|json}", jsonEscape(value));
  replaceAll(tpl, "{" + name + "}", value);
}

}  // namespace plugintemplate
