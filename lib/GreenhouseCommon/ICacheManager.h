#ifndef I_CACHE_MANAGER_H
#define I_CACHE_MANAGER_H

#include <Arduino.h>

enum class CacheReadError { NONE, CACHE_EMPTY, FILE_READ_ERROR, OUT_OF_MEMORY, CORRUPT_DATA, SCANNING };

// ============================================================================
// CRTP Base Class for Zero-Overhead Cache Interface
// ============================================================================
// CRTP provides compile-time polymorphism without virtual function overhead:
// - No vtable pointer (saves 4 bytes per object)
// - No indirect function calls (enables inlining)

template <typename Derived>
class ICacheManager {
public:
  void init() {
    static_cast<Derived*>(this)->initImpl();
  }
  
  void reset() {
    static_cast<Derived*>(this)->resetImpl();
  }
  
  [[nodiscard]] bool write(const char* data, uint16_t len) {
    return static_cast<Derived*>(this)->writeImpl(data, len);
  }
  
  [[nodiscard]] CacheReadError read_one(char* out_buffer, size_t buffer_size, size_t& out_len) {
    return static_cast<Derived*>(this)->read_oneImpl(out_buffer, buffer_size, out_len);
  }
  
  [[nodiscard]] bool pop_one() {
    return static_cast<Derived*>(this)->pop_oneImpl();
  }
  
  void get_status(uint32_t& size_bytes, uint32_t& head, uint32_t& tail) {
    static_cast<Derived*>(this)->get_statusImpl(size_bytes, head, tail);
  }
  
  uint32_t get_size() {
    return static_cast<Derived*>(this)->get_sizeImpl();
  }

protected:
  ICacheManager() = default;
  ~ICacheManager() = default;
  ICacheManager(const ICacheManager&) = default;
  ICacheManager& operator=(const ICacheManager&) = default;
};

#endif  // I_CACHE_MANAGER_H
