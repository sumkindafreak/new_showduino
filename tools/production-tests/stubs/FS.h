#ifndef SHOWDUINO_HOST_TEST_FS_H
#define SHOWDUINO_HOST_TEST_FS_H

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#define FILE_READ "r"

namespace fs { class FS; }

class File {
public:
  File() = default;

  explicit operator bool() const { return state_ != nullptr; }
  bool isDirectory() const { return state_ && state_->directory; }

  const char *name() const {
    return state_ ? state_->name.c_str() : "";
  }

  size_t size() const {
    if (!state_ || state_->directory) return 0;
    std::error_code ec;
    const auto bytes = std::filesystem::file_size(state_->path, ec);
    return ec ? 0 : static_cast<size_t>(bytes);
  }

  size_t readBytes(char *buffer, size_t length) {
    if (!state_ || state_->directory || !buffer) return 0;
    if (!state_->stream.is_open()) {
      state_->stream.open(state_->path, std::ios::binary);
    }
    state_->stream.read(buffer, static_cast<std::streamsize>(length));
    return static_cast<size_t>(state_->stream.gcount());
  }

  File openNextFile() {
    if (!state_ || !state_->directory || state_->next >= state_->entries.size()) {
      return File();
    }
    return File(state_->entries[state_->next++]);
  }

  void close() { state_.reset(); }

private:
  struct State {
    explicit State(const std::filesystem::path &source)
        : path(source), directory(std::filesystem::is_directory(source)),
          name(source.generic_string()) {
      if (directory) {
        for (const auto &entry : std::filesystem::directory_iterator(source)) {
          entries.push_back(entry.path());
        }
      }
    }

    std::filesystem::path path;
    bool directory = false;
    std::string name;
    std::vector<std::filesystem::path> entries;
    size_t next = 0;
    std::ifstream stream;
  };

  explicit File(const std::filesystem::path &path)
      : state_(std::make_shared<State>(path)) {}

  std::shared_ptr<State> state_;
  friend class fs::FS;
};

namespace fs {

class FS {
public:
  explicit FS(const std::filesystem::path &root) : root_(root) {}

  bool exists(const char *path) const {
    return path && std::filesystem::exists(resolve(path));
  }

  bool mkdir(const char *path) {
    if (!path) return false;
    std::error_code ec;
    const auto resolved = resolve(path);
    return std::filesystem::create_directories(resolved, ec) ||
           (!ec && std::filesystem::is_directory(resolved));
  }

  File open(const char *path, const char * = FILE_READ) const {
    if (!path) return File();
    const auto resolved = resolve(path);
    return std::filesystem::exists(resolved) ? File(resolved) : File();
  }

private:
  std::filesystem::path resolve(const char *path) const {
    std::string relative = path ? path : "";
    while (!relative.empty() && (relative.front() == '/' || relative.front() == '\\')) {
      relative.erase(relative.begin());
    }
    return root_ / std::filesystem::path(relative);
  }

  std::filesystem::path root_;
};

} // namespace fs

#endif
