#ifndef CACHE_MANAGER_H
#define CACHE_MANAGER_H

#include "ICacheManager.h"

class CacheManager : public ICacheManager {
public:
  CacheManager();

  // Deleted copy constructor and assignment operator to satisfy -Weffc++
  CacheManager(const CacheManager&) = delete;
  CacheManager& operator=(const CacheManager&) = delete;

  void init() override;
  void reset() override;
  [[nodiscard]] bool write(const char* data, uint16_t len) override;
  CacheReadError read_one(char* out_buffer, size_t buffer_size, size_t& out_len) override;
  [[nodiscard]] bool pop_one() override;
  void get_status(uint32_t& size_bytes, uint32_t& head, uint32_t& tail) override;
  [[nodiscard]] uint32_t get_size() override;
};

#endif  // CACHE_MANAGER_H