#include "CryptoUtils.h"

#include <ESP8266WiFi.h>  // Untuk os_get_random
#include <libb64/cdecode.h>
#include <libb64/cencode.h>

#include <algorithm>
#include <memory>
#include <new>
#include <string>
#include <vector>
#include "Logger.h"

namespace {

  // --- HELPER BASE64 MANUAL (Zero Allocation) ---
  size_t base64_encode_to_buffer(const uint8_t* data, size_t len, char* out_buffer) {
    if (!data || !out_buffer || len == 0)
      return 0;

    base64_encodestate state;
    base64_init_encodestate(&state);

    int len1 = base64_encode_block((const char*)data, len, out_buffer, &state);
    int len2 = base64_encode_blockend(&out_buffer[len1], &state);

    size_t total_len = len1 + len2;

    // Hapus newline yang ditambahkan libb64
    // Kita lakukan in-place compaction
    size_t final_len = 0;
    for (size_t i = 0; i < total_len; i++) {
      if (out_buffer[i] != '\n' && out_buffer[i] != '\r') {
        out_buffer[final_len++] = out_buffer[i];
      }
    }
    out_buffer[final_len] = '\0';  // Null terminate
    return final_len;
  }

  std::vector<uint8_t> base64_decode_helper(std::string_view str) {
    if (str.empty())
      return {};

    size_t expected_len = base64_decode_expected_len(str.length());
    std::vector<uint8_t> decoded(expected_len);

    base64_decodestate state;
    base64_init_decodestate(&state);

    int decoded_len = base64_decode_block(str.data(), str.length(), reinterpret_cast<char*>(decoded.data()), &state);

    if (decoded_len >= 0 && (size_t)decoded_len <= expected_len) {
      decoded.resize(decoded_len);
    } else {
      return {};
    }

    return decoded;
  }

}  // namespace

namespace CryptoUtils {

  AES_CBC_Cipher::AES_CBC_Cipher(std::string_view key) {
    if (key.size() != 32) {
      LOG_ERROR("CRYPTO", F("FATAL: AES key must be 32 bytes for AES-256."));
      return;
    }
    br_aes_ct_cbcenc_init(&m_enc_ctx, key.data(), key.size());
    br_aes_ct_cbcdec_init(&m_dec_ctx, key.data(), key.size());
  }

  AES_CBC_Cipher::~AES_CBC_Cipher() {
    // HARDENING: Securely wipe key contexts from memory
    // Prevents potential key extraction from RAM after use
    memset(&m_enc_ctx, 0, sizeof(m_enc_ctx));
    memset(&m_dec_ctx, 0, sizeof(m_dec_ctx));
  }

  std::optional<EncryptedPayload> AES_CBC_Cipher::encrypt(std::string_view plaintext) const {
    EncryptedPayload payload;

    // 1. Generate IV
    payload.iv.resize(16);
    os_get_random(payload.iv.data(), payload.iv.size());

    // 2. PKCS7 Padding
    size_t original_len = plaintext.size();
    size_t padding_len = 16 - (original_len % 16);
    size_t total_len = original_len + padding_len;

    // Alokasi ciphertext
    payload.ciphertext.resize(total_len);

    // Copy plaintext
    memcpy(payload.ciphertext.data(), plaintext.data(), original_len);

    // Isi padding
    uint8_t pad_byte = (uint8_t)padding_len;
    memset(payload.ciphertext.data() + original_len, pad_byte, padding_len);

    // 3. Enkripsi (Gunakan copy IV karena BearSSL mengubahnya in-place)
    std::vector<uint8_t> iv_working_copy = payload.iv;
    br_aes_ct_cbcenc_run(&m_enc_ctx, iv_working_copy.data(), payload.ciphertext.data(), payload.ciphertext.size());

    return payload;
  }

  std::optional<std::vector<uint8_t>> AES_CBC_Cipher::decrypt(const EncryptedPayload& payload) const {
    if (payload.iv.size() != 16 || payload.ciphertext.empty() || (payload.ciphertext.size() % 16) != 0) {
      return std::nullopt;
    }

    std::vector<uint8_t> iv_copy = payload.iv;
    std::vector<uint8_t> plaintext = payload.ciphertext;

    br_aes_ct_cbcdec_run(&m_dec_ctx, iv_copy.data(), plaintext.data(), plaintext.size());

    size_t plaintext_len = plaintext.size();
    if (plaintext_len == 0)
      return std::nullopt;

    size_t padding_len = plaintext.back();
    if (padding_len == 0 || padding_len > 16 || padding_len > plaintext_len) {
      return std::nullopt;
    }

    for (size_t i = 0; i < padding_len; ++i) {
      if (plaintext[plaintext_len - 1 - i] != padding_len) {
        return std::nullopt;
      }
    }

    plaintext.resize(plaintext_len - padding_len);
    return plaintext;
  }

  String serialize_payload(const EncryptedPayload& payload) {
    size_t iv_len = base64_encode_expected_len(payload.iv.size()) + 4;  // +4 safety
    size_t cipher_len = base64_encode_expected_len(payload.ciphertext.size()) + 4;
    size_t total_len = iv_len + 1 + cipher_len + 1;  // +1 separator, +1 null
    std::unique_ptr<char[]> buffer(new (std::nothrow) char[total_len]);
    if (!buffer) {
      LOG_ERROR("CRYPTO", F("OOM during serialization"));
      return String();
    }

    char* ptr = buffer.get();
    size_t written1 = base64_encode_to_buffer(payload.iv.data(), payload.iv.size(), ptr);
    ptr += written1;
    *ptr++ = ':';
    size_t written2 = base64_encode_to_buffer(payload.ciphertext.data(), payload.ciphertext.size(), ptr);
    ptr += written2;
    *ptr = '\0';

    return String(buffer.get());
  }

  std::optional<EncryptedPayload> deserialize_payload(std::string_view serialized) {
    size_t separator = serialized.find(':');
    if (separator == std::string_view::npos) {
      return std::nullopt;
    }
    EncryptedPayload payload;
    // string_view::substr is efficient (pointer arithmetic)
    payload.iv = base64_decode_helper(serialized.substr(0, separator));
    payload.ciphertext = base64_decode_helper(serialized.substr(separator + 1));

    if (payload.iv.empty() || payload.ciphertext.empty()) {
      return std::nullopt;
    }
    return payload;
  }

}  // namespace CryptoUtils