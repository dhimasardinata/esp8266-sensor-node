#ifndef I_CACHE_MANAGER_H
#define I_CACHE_MANAGER_H

#include <Arduino.h>

enum class CacheReadError { NONE, CACHE_EMPTY, FILE_READ_ERROR, OUT_OF_MEMORY, CORRUPT_DATA };

class ICacheManager {
public:
  virtual ~ICacheManager() = default;

  virtual void init() = 0;
  virtual void reset() = 0;
  [[nodiscard]] virtual bool write(const char* data, uint16_t len) = 0;
  [[nodiscard]] virtual CacheReadError read_one(char* out_buffer, size_t buffer_size, size_t& out_len) = 0;
  [[nodiscard]] virtual bool pop_one() = 0;
  virtual void get_status(uint32_t& size_bytes, uint32_t& head, uint32_t& tail) = 0;
  virtual uint32_t get_size() = 0;
};

#endif  // I_CACHE_MANAGER_H