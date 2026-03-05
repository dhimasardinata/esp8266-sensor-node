#ifndef CRC32_H
#define CRC32_H

#include <Arduino.h>

namespace Crc32 {

// IEEE CRC32 (polynomial 0xEDB88320), supports incremental chaining via
// initial_crc = previous final CRC.
uint32_t compute(const void* data, size_t length, uint32_t initial_crc = 0);

}  // namespace Crc32

#endif  // CRC32_H
