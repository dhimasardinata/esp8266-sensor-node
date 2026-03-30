#pragma once

#include <cstddef>
#include <cstdint>

struct br_sha256_context {};

inline void br_sha256_init(br_sha256_context*) {}
inline void br_sha256_update(br_sha256_context*, const void*, size_t) {}
inline void br_sha256_out(br_sha256_context*, void*) {}
