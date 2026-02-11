#ifndef CACHE_MANAGER_H
#define CACHE_MANAGER_H

#include "ICacheManager.h"
#include <FS.h>

// ============================================================================
// CacheManager with CRTP (Zero Virtual Overhead)
// ============================================================================

class CacheManager final : public ICacheManager<CacheManager> {
  friend class ICacheManager<CacheManager>;
  
public:
  CacheManager();

  // Deleted copy constructor and assignment operator
  CacheManager(const CacheManager&) = delete;
  CacheManager& operator=(const CacheManager&) = delete;

  // CRTP implementation methods
  void initImpl();
  void resetImpl();
  [[nodiscard]] bool writeImpl(const char* data, uint16_t len);
  CacheReadError read_oneImpl(char* out_buffer, size_t buffer_size, size_t& out_len);
  [[nodiscard]] bool pop_oneImpl();
  void get_statusImpl(uint32_t& size_bytes, uint32_t& head, uint32_t& tail);
  [[nodiscard]] uint32_t get_sizeImpl();
  
  // Custom method (not in ICacheManager for now) to reduce write amplification
  void flush();

private:
  bool m_dirty = false;
  fs::File m_file;
};

#endif  // CACHE_MANAGER_H