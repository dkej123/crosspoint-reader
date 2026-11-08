#pragma once

#include <cstddef>
#include <string>
#include <vector>

// Books whose automatic KOSync upload did not finish, one per line, oldest first:
// "<path>" retries when that book is next left; "<path>\t<xpath>" is a finished
// book that any later upload can send without opening it. The upload of another
// book must not drop a book that is waiting.
namespace pendingbooks {

constexpr size_t MAX_BOOKS = 8;
constexpr size_t MAX_BYTES = 4096;

struct Book {
  std::string path;
  std::string xpath;  // empty: needs the book open
};

inline std::vector<Book> parse(const std::string& list) {
  std::vector<Book> books;
  size_t start = 0;
  while (start < list.size()) {
    size_t end = list.find('\n', start);
    if (end == std::string::npos) end = list.size();
    if (end > start) {
      const std::string line = list.substr(start, end - start);
      const size_t tab = line.find('\t');
      books.push_back(tab == std::string::npos ? Book{line, ""} : Book{line.substr(0, tab), line.substr(tab + 1)});
    }
    start = end + 1;
  }
  return books;
}

inline std::string join(const std::vector<Book>& books) {
  std::string list;
  for (const auto& book : books) {
    list += book.path;
    if (!book.xpath.empty()) {
      list += '\t';
      list += book.xpath;
    }
    list += '\n';
  }
  return list;
}

inline bool contains(const std::string& list, const std::string& path) {
  for (const auto& book : parse(list)) {
    if (book.path == path) return true;
  }
  return false;
}

inline std::string removed(const std::string& list, const std::string& path) {
  std::vector<Book> kept;
  for (auto& book : parse(list)) {
    if (book.path != path) kept.push_back(std::move(book));
  }
  return join(kept);
}

// The last write decides: a retry of a reopened book replaces its finished mark,
// so going back to re-read never sends a stale 100%. Past the limit, retries go
// first: they can start again when their book is next read.
inline std::string added(const std::string& list, const std::string& path, const std::string& xpath = "") {
  std::vector<Book> books = parse(removed(list, path));
  books.push_back({path, xpath});
  while (books.size() > MAX_BOOKS || (books.size() > 1 && join(books).size() > MAX_BYTES)) {
    auto drop = books.begin();
    for (auto it = books.begin(); it != books.end(); ++it) {
      if (it->xpath.empty()) {
        drop = it;
        break;
      }
    }
    books.erase(drop);
  }
  return join(books);
}

inline std::string renamed(const std::string& list, const std::string& from, const std::string& to) {
  auto books = parse(list);
  for (auto& book : books) {
    if (book.path == from) book.path = to;
  }
  return join(books);
}

inline bool anyFinishedExcept(const std::string& list, const std::string& path) {
  for (const auto& book : parse(list)) {
    if (!book.xpath.empty() && book.path != path) return true;
  }
  return false;
}

}  // namespace pendingbooks
