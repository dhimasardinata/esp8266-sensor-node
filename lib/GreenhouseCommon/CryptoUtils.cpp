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

  // Strip CR/LF from base64 output in-place
  size_t strip_newlines(char* buf, size_t len) {
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
      if (buf[i] != '\n' && buf[i] != '\r') buf[j++] = buf[i];
    }
    buf[j] = '\0';
    return j;
  }

  size_t base64_encode_to_buffer(const uint8_t* data, size_t len, char* out_buffer) {
    if (!data || !out_buffer || len == 0) return 0;

    base64_encodestate state;
    base64_init_encodestate(&state);
    int len1 = base64_encode_block((const char*)data, len, out_buffer, &state);
    int len2 = base64_encode_blockend(&out_buffer[len1], &state);
    return strip_newlines(out_buffer, len1 + len2);
  }

  // Decode base64 to fixed-size output buffer, returns actual length
  size_t base64_decode_to_buffer(std::string_view str, uint8_t* out, size_t out_max) {
    if (str.empty() || !out) return 0;
    base64_decodestate state;
    base64_init_decodestate(&state);
    int decoded_len = base64_decode_block(str.data(), str.length(), reinterpret_cast<char*>(out), &state);
    return (decoded_len >= 0 && (size_t)decoded_len <= out_max) ? decoded_len : 0;
  }

  // Validate PKCS7 padding - returns padding length or 0 if invalid
  size_t validate_pkcs7_padding(const uint8_t* data, size_t len) {
    if (len == 0) return 0;
    size_t pad = data[len - 1];
    if (pad == 0 || pad > 16 || pad > len) return 0;
    for (size_t i = 0; i < pad; i++) {
      if (data[len - 1 - i] != pad) return 0;
    }
    return pad;
  }

}  // namespace

namespace CryptoUtils {

  // Shared singleton cipher - eliminates ~800 bytes of duplicate BearSSL contexts
  AES_CBC_Cipher& getSharedCipher() {
    static AES_CBC_Cipher instance(std::string_view(reinterpret_cast<const char*>(AES_KEY), 32));
    return instance;
  }
  
  // Static encryption buffer - avoids 768-byte stack allocation in deep call chains
  char* getEncryptionBuffer() {
    static char buffer[ENCRYPTION_BUFFER_SIZE];
    return buffer;
  }

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
    // Secure zeroing helper
    volatile uint8_t* p = reinterpret_cast<volatile uint8_t*>(&m_enc_ctx);
    size_t n = sizeof(m_enc_ctx);
    while(n--) *p++ = 0;
    
    p = reinterpret_cast<volatile uint8_t*>(&m_dec_ctx);
    n = sizeof(m_dec_ctx);
    while(n--) *p++ = 0;
  }

  std::optional<EncryptedPayload> AES_CBC_Cipher::encrypt(std::string_view plaintext) const {
    size_t original_len = plaintext.size();
    if (original_len > EncryptedPayload::MAX_CIPHERTEXT_SIZE) return std::nullopt;

    size_t padding_len = 16 - (original_len % 16);
    size_t total_len = original_len + padding_len;
    
    if (total_len > EncryptedPayload::MAX_CIPHERTEXT_SIZE) return std::nullopt;

    EncryptedPayload payload;
    os_get_random(payload.iv.data(), EncryptedPayload::IV_SIZE);
    
    // Bounds check already done above (total_len <= MAX_CIPHERTEXT)
    // payload.ciphertext is std::array, no resize needed
    
    memcpy(payload.ciphertext.data(), plaintext.data(), original_len);
    // Fill padding
    std::fill(payload.ciphertext.begin() + original_len, payload.ciphertext.begin() + total_len, (uint8_t)padding_len);
    
    payload.ciphertextLen = total_len;

    std::array<uint8_t, EncryptedPayload::IV_SIZE> iv_copy = payload.iv;
    br_aes_ct_cbcenc_run(&m_enc_ctx, iv_copy.data(), payload.ciphertext.data(), payload.ciphertextLen);

    return payload;
  }

  std::optional<std::vector<uint8_t>> AES_CBC_Cipher::decrypt(const EncryptedPayload& payload) const {
    if (payload.ciphertextLen == 0 || (payload.ciphertextLen % 16) != 0)
      return std::nullopt;

    std::array<uint8_t, EncryptedPayload::IV_SIZE> iv_copy = payload.iv;
    std::vector<uint8_t> plaintext(payload.ciphertext.begin(), payload.ciphertext.begin() + payload.ciphertextLen);

    br_aes_ct_cbcdec_run(&m_dec_ctx, iv_copy.data(), plaintext.data(), plaintext.size());

    size_t pad = validate_pkcs7_padding(plaintext.data(), plaintext.size());
    if (pad == 0) return std::nullopt;

    plaintext.resize(plaintext.size() - pad);
    return plaintext;
  }

  size_t encrypt_and_serialize(std::string_view plaintext, char* out_buf, size_t out_len, const br_aes_ct_cbcenc_keys* enc_ctx) {
    if (!out_buf || out_len < 32) return 0; // Minimum safety

    // 1. Generate & Encode IV (16 bytes -> ~24 bytes b64)
    uint8_t iv[16];
    os_get_random(iv, 16);
    size_t iv_b64_len = base64_encode_to_buffer(iv, 16, out_buf);
    
    if (iv_b64_len == 0 || iv_b64_len + 2 >= out_len) return 0;
    
    char* ptr = out_buf + iv_b64_len;
    *ptr++ = ':';

    // 2. Padding & Encryption (In-place if possible, but let's use a small stack buffer)
    // MAX_PAYLOAD_SIZE is usually 384. 512 is safe for stack.
    size_t original_len = plaintext.size();
    size_t padding_len = 16 - (original_len % 16);
    size_t total_len = original_len + padding_len;
    
    if (total_len > 512) return 0; // Guard against stack overflow
    if (original_len > 512) return 0;

    uint8_t work_buf[512];
    // Bounds checked above, but adding explicit min for static analysis
    size_t copy_len = std::min(original_len, sizeof(work_buf));
    memcpy(work_buf, plaintext.data(), copy_len);
    // Fill padding safely
    for(size_t i=0; i<padding_len; i++) work_buf[original_len + i] = (uint8_t)padding_len;
    
    // BearSSL needs a mutable IV copy
    uint8_t iv_temp[16];
    memcpy(iv_temp, iv, 16);
    
    // Cast away const because BearSSL context is opaque and manipulated internally
    br_aes_ct_cbcenc_run(const_cast<br_aes_ct_cbcenc_keys*>(enc_ctx), iv_temp, work_buf, total_len);
    
    // 3. Encode Ciphertext
    size_t cipher_b64_len = base64_encode_to_buffer(work_buf, total_len, ptr);
    
    return iv_b64_len + 1 + cipher_b64_len;
  }

  size_t fast_serialize_encrypted(std::string_view plaintext, char* out_buf, size_t out_len, const AES_CBC_Cipher& cipher) {
    return encrypt_and_serialize(plaintext, out_buf, out_len, cipher.get_enc_ctx());
  }

  String serialize_payload(const EncryptedPayload& payload) {
    size_t iv_len = base64_encode_expected_len(EncryptedPayload::IV_SIZE) + 4;
    size_t cipher_len = base64_encode_expected_len(payload.ciphertextLen) + 4;
    size_t total_len = iv_len + 1 + cipher_len + 1;
    std::unique_ptr<char[]> buffer(new (std::nothrow) char[total_len]);
    if (!buffer) {
      LOG_ERROR("CRYPTO", F("OOM during serialization"));
      return String();
    }

    char* ptr = buffer.get();
    size_t written1 = base64_encode_to_buffer(payload.iv.data(), EncryptedPayload::IV_SIZE, ptr);
    ptr += written1;
    *ptr++ = ':';
    size_t written2 = base64_encode_to_buffer(payload.ciphertext.data(), payload.ciphertextLen, ptr);
    ptr += written2;
    *ptr = '\0';
    return String(buffer.get());
  }

  std::optional<EncryptedPayload> deserialize_payload(std::string_view serialized) {
    size_t separator = serialized.find(':');
    if (separator == std::string_view::npos) return std::nullopt;
    
    EncryptedPayload payload;
    size_t ivLen = base64_decode_to_buffer(serialized.substr(0, separator), 
                                            payload.iv.data(), EncryptedPayload::IV_SIZE);
    size_t ctLen = base64_decode_to_buffer(serialized.substr(separator + 1),
                                            payload.ciphertext.data(), EncryptedPayload::MAX_CIPHERTEXT_SIZE);
    
    if (ivLen != EncryptedPayload::IV_SIZE || ctLen == 0) return std::nullopt;
    payload.ciphertextLen = ctLen;
    return payload;
  }

}  // namespace CryptoUtils