#pragma once

#include <cstddef>
#include <cstdint>

struct br_aes_ct_cbcenc_keys {};
struct br_aes_ct_cbcdec_keys {};

inline void br_aes_ct_cbcenc_init(br_aes_ct_cbcenc_keys*, const void*, size_t) {}
inline void br_aes_ct_cbcdec_init(br_aes_ct_cbcdec_keys*, const void*, size_t) {}
inline void br_aes_ct_cbcenc_run(br_aes_ct_cbcenc_keys*, void*, void*, size_t) {}
inline void br_aes_ct_cbcdec_run(br_aes_ct_cbcdec_keys*, void*, void*, size_t) {}
