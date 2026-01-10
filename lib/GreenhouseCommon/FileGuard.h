#ifndef FILE_GUARD_H
#define FILE_GUARD_H

#include <FS.h>

class FileGuard {
public:
  explicit FileGuard(File file) : m_file(file) {}

  ~FileGuard() {
    if (m_file) {
      m_file.close();
    }
  }

  FileGuard(const FileGuard&) = delete;
  FileGuard& operator=(const FileGuard&) = delete;

  operator File&() {
    return m_file;
  }
  File* operator->() {
    return &m_file;
  }
  explicit operator bool() const {
    return static_cast<bool>(m_file);
  }

  size_t write(const uint8_t* buf, size_t size) {
    if (!m_file)
      return 0;
    return m_file.write(buf, size);
  }

  size_t write(uint8_t data) {
    if (!m_file)
      return 0;
    return m_file.write(data);
  }

  int read() {
    if (!m_file)
      return -1;
    return m_file.read();
  }

  // --- FIX: Added buffer read method ---
  size_t read(uint8_t* buf, size_t size) {
    if (!m_file)
      return 0;
    return m_file.read(buf, size);
  }

  void flush() {
    if (m_file)
      m_file.flush();
  }

private:
  File m_file;
};

#endif  // FILE_GUARD_H