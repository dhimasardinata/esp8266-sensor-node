#pragma once

#include <cstddef>
#include <cstdint>

struct br_hmac_key_context {};
struct br_hmac_context {};

inline void br_hmac_key_init(br_hmac_key_context*, const void*, const void*, size_t) {}
inline void br_hmac_init(br_hmac_context*, const br_hmac_key_context*, size_t) {}
inline void br_hmac_update(br_hmac_context*, const void*, size_t) {}
inline void br_hmac_out(br_hmac_context*, void*) {}
