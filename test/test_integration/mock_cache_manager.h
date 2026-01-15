#ifndef MOCK_CACHE_MANAGER_H
#define MOCK_CACHE_MANAGER_H

#include <ICacheManager.h>

#include <string>
#include <vector>

class MockCacheManager : public ICacheManager {
public:
  // --- Test control variables ---
  std::vector<std::string> stored_data;
  bool init_called = false;
  bool reset_called = false;
  uint32_t pop_count = 0;

  // --- Interface implementation ---
  void init() override {
    init_called = true;
  }
  void reset() override {
    reset_called = true;
    stored_data.clear();
  }

  bool write(const char* data, uint16_t len) override {
    stored_data.push_back(std::string(data, len));
    return true;
  }

  CacheReadError read_one(char* out_buffer, size_t buffer_size, size_t& out_len) override {
    if (stored_data.empty()) {
      out_len = 0;
      return CacheReadError::CACHE_EMPTY;
    }
    const std::string& first = stored_data.front();
    if (first.length() > buffer_size) {
      return CacheReadError::OUT_OF_MEMORY;
    }
    memcpy(out_buffer, first.c_str(), first.length());
    out_len = first.length();
    return CacheReadError::NONE;
  }

  bool pop_one() override {
    if (!stored_data.empty()) {
      pop_count++;
      stored_data.erase(stored_data.begin());
      return true;
    }
    return false;
  }

  uint32_t get_size() override {
    uint32_t total_size = 0;
    for (const auto& s : stored_data) {
      total_size += s.length() + sizeof(uint16_t) + sizeof(uint32_t);
    }
    return total_size;
  }

  void get_status(uint32_t& size_bytes, uint32_t& head, uint32_t& tail) override {
    size_bytes = get_size();
    head = 0;
    tail = 0;
  }
};

#endif  // MOCK_CACHE_MANAGER_H