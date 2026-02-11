#include "CryptoUtils.h"

#include <ESP8266WiFi.h>
#include <libb64/cdecode.h>
#include <libb64/cencode.h>

#include <algorithm>
#include <memory>
#include <new>

#include "Logger.h"

namespace {
  // Strip CR/LF from base64 output in-place
  size_t strip_newlines(char* buf, size_t len) {
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
      if (buf[i] != '\n' && buf[i] != '\r')
        buf[j++] = buf[i];
    }
    buf[j] = '\0';
    return j;
  }

  size_t base64_encode_to_buffer(const uint8_t* data, size_t len, char* out_buffer, size_t out_max) {
    if (!data || !out_buffer || len == 0)
      return 0;

    size_t expected = ((len + 2) / 3) * 4;
    if (expected >= out_max)
      return 0;

    base64_encodestate state;
    base64_init_encodestate(&state);
    int len1 = base64_encode_block((const char*)data, len, out_buffer, &state);
    int len2 = base64_encode_blockend(&out_buffer[len1], &state);
    return strip_newlines(out_buffer, len1 + len2);
  }

  size_t base64_decode_to_buffer(std::string_view str, uint8_t* out, size_t out_max) {
    if (str.empty() || !out)
      return 0;
    base64_decodestate state;
    base64_init_decodestate(&state);
    int decoded_len = base64_decode_block(str.data(), str.length(), reinterpret_cast<char*>(out), &state);
    return (decoded_len >= 0 && (size_t)decoded_len <= out_max) ? decoded_len : 0;
  }

  size_t validate_pkcs7_padding(const uint8_t* data, size_t len) {
    if (len == 0)
      return 0;
    size_t pad = data[len - 1];
    if (pad == 0 || pad > 16 || pad > len)
      return 0;
    for (size_t i = 0; i < pad; i++) {
      if (data[len - 1 - i] != pad)
        return 0;
    }
    return pad;
  }

  uint32_t get_time_stamp() {
    return (uint32_t)time(nullptr);
  }

  std::unique_ptr<CryptoUtils::AES_CBC_Cipher> g_wsCipher;
}  // namespace

namespace CryptoUtils {

  AES_CBC_Cipher::AES_CBC_Cipher(std::string_view key) {
    if (key.size() != 32)
      return;

    // CHANGED: Allocate on Heap to prevent Stack Overflow
    m_enc_ctx.reset(new (std::nothrow) br_aes_ct_cbcenc_keys);
    m_dec_ctx.reset(new (std::nothrow) br_aes_ct_cbcdec_keys);

    if (m_enc_ctx)
      br_aes_ct_cbcenc_init(m_enc_ctx.get(), key.data(), key.size());
    if (m_dec_ctx)
      br_aes_ct_cbcdec_init(m_dec_ctx.get(), key.data(), key.size());
  }

  AES_CBC_Cipher::~AES_CBC_Cipher() {
    // Secure cleanup (zero out memory before delete)
    if (m_enc_ctx) {
      volatile uint8_t* p = reinterpret_cast<volatile uint8_t*>(m_enc_ctx.get());
      size_t n = sizeof(br_aes_ct_cbcenc_keys);
      while (n--)
        *p++ = 0;
    }
    if (m_dec_ctx) {
      volatile uint8_t* p = reinterpret_cast<volatile uint8_t*>(m_dec_ctx.get());
      size_t n = sizeof(br_aes_ct_cbcdec_keys);
      while (n--)
        *p++ = 0;
    }
  }

  bool AES_CBC_Cipher::decrypt(
      const EncryptedPayload& payload, char* out_buf, size_t out_max, size_t& out_len, uint32_t* out_timestamp) const {
    out_len = 0;

    // Check allocation health
    if (!m_dec_ctx) {
      LOG_ERROR("CRYPTO", "Decryption failed: No Context");
      return false;
    }

    if (payload.ciphertextLen == 0 || (payload.ciphertextLen % 16) != 0) {
      LOG_ERROR("CRYPTO", "Decryption failed: Invalid length alignment (%u)", payload.ciphertextLen);
      return false;
    }

    // FIX: Minimum size is 1 block (16 bytes), not 20 bytes.
    if (payload.ciphertextLen < 16) {
      LOG_ERROR("CRYPTO", "Decryption failed: Length too short (%u)", payload.ciphertextLen);
      return false;
    }

    if (payload.ciphertextLen > out_max + EncryptedPayload::TS_SIZE + 16) {
      LOG_ERROR("CRYPTO", "Decryption failed: Output buffer risk");
      return false;
    }

    if (payload.ciphertextLen > EncryptedPayload::MAX_CIPHERTEXT_SIZE) {
      LOG_ERROR("CRYPTO", "Decryption failed: Oversized payload");
      return false;
    }

    auto unlock = [&]() {
      noInterrupts();
      m_busy = false;
      interrupts();
    };

    noInterrupts();
    if (m_busy) {
      interrupts();
      return false;
    }
    m_busy = true;
    interrupts();

    std::unique_ptr<uint8_t[]> work_buf(new (std::nothrow) uint8_t[EncryptedPayload::MAX_CIPHERTEXT_SIZE]);
    std::unique_ptr<uint8_t[]> iv_copy(new (std::nothrow) uint8_t[EncryptedPayload::IV_SIZE]);
    if (!work_buf || !iv_copy) {
      LOG_WARN("CRYPTO", "Decrypt skipped: low heap for scratch buffers");
      unlock();
      return false;
    }

    memcpy(work_buf.get(), payload.ciphertext.data(), payload.ciphertextLen);
    memcpy(iv_copy.get(), payload.iv.data(), EncryptedPayload::IV_SIZE);

    // Use pointer operator
    // Use pointer operator
    br_aes_ct_cbcdec_run(m_dec_ctx.get(), iv_copy.get(), work_buf.get(), payload.ciphertextLen);

    size_t pad = validate_pkcs7_padding(work_buf.get(), payload.ciphertextLen);
    if (pad == 0) {
      LOG_ERROR("CRYPTO", "Decryption failed: Invalid PKCS7 padding");
      unlock();
      return false;
    }

    size_t raw_len = payload.ciphertextLen - pad;
    if (raw_len < EncryptedPayload::TS_SIZE) {
      LOG_ERROR("CRYPTO", "Decryption failed: Payload too short (%u bytes)", raw_len);
      unlock();
      return false;
    }

    // Extract timestamp from first 4 bytes
    uint32_t msg_ts = (work_buf[0] << 24) | (work_buf[1] << 16) | (work_buf[2] << 8) | work_buf[3];
    uint32_t now = get_time_stamp();

    bool timeIsSynced = (now > 1704067200UL);

    if (timeIsSynced) {
      if (msg_ts > now + 30 || msg_ts < now - 30) {
        LOG_ERROR("CRYPTO", "Time skew failure: Msg=%u, Dev=%u, Diff=%d", msg_ts, now, (int)(msg_ts - now));
        unlock();
        return false;
      }
    } else {
      LOG_DEBUG("CRYPTO", F("Unsynced clock (Dev=%u): Bypassing skew check for Msg=%u"), now, msg_ts);
    }

    // Calculate actual text length (remove timestamp)
    size_t text_len = raw_len - EncryptedPayload::TS_SIZE;

    if (text_len > out_max) {
      LOG_ERROR("CRYPTO", "Output buffer too small (%u vs %u)", text_len, out_max);
      unlock();
      return false;
    }

    // Copy text data (skip first 4 bytes timestamp)
    memcpy(out_buf, work_buf.get() + EncryptedPayload::TS_SIZE, text_len);
    out_buf[text_len] = '\0';  // Null terminate
    out_len = text_len;

    if (out_timestamp)
      *out_timestamp = msg_ts;

    #if APP_LOG_LEVEL >= LOG_LEVEL_DEBUG
    LOG_DEBUG("CRYPTO", F("Decrypt ok. TS=%u, Len=%u"), msg_ts, out_len);
    #endif
    unlock();
    return true;
  }

  size_t AES_CBC_Cipher::encrypt(std::string_view plaintext, char* out_buf, size_t out_len) const {
    if (!out_buf || out_len < 32 || !m_enc_ctx)
      return 0;

    auto unlock = [&]() {
      noInterrupts();
      m_busy = false;
      interrupts();
    };

    noInterrupts();
    if (m_busy) {
      interrupts();
      return 0;
    }
    m_busy = true;
    interrupts();

    std::unique_ptr<uint8_t[]> work_buf(new (std::nothrow) uint8_t[EncryptedPayload::MAX_CIPHERTEXT_SIZE]);
    std::unique_ptr<uint8_t[]> iv(new (std::nothrow) uint8_t[EncryptedPayload::IV_SIZE]);
    if (!work_buf || !iv) {
      LOG_WARN("CRYPTO", "Encrypt skipped: low heap for scratch buffers");
      unlock();
      return 0;
    }

    os_get_random(iv.get(), EncryptedPayload::IV_SIZE);

    // CHANGED: Mix in additional entropy (Micros + RSSI)
    uint32_t t = micros();
    int32_t r = WiFi.RSSI();
    iv[0] ^= (uint8_t)t;
    iv[1] ^= (uint8_t)(t >> 8);
    iv[2] ^= (uint8_t)(t >> 16);
    iv[3] ^= (uint8_t)r;

    size_t iv_b64_len = base64_encode_to_buffer(iv.get(), EncryptedPayload::IV_SIZE, out_buf, out_len);
    if (iv_b64_len == 0 || iv_b64_len + 2 >= out_len) {
      unlock();
      return 0;
    }

    char* ptr = out_buf + iv_b64_len;
    *ptr++ = ':';

    size_t data_len = plaintext.size() + EncryptedPayload::TS_SIZE;
    size_t padding_len = 16 - (data_len % 16);
    size_t total_len = REDACTED

    if (total_len > EncryptedPayload:REDACTED
      unlock();
      return 0;
    }

    uint32_t now = get_time_stamp();
    work_buf[0] = (uint8_t)(now >> 24);
    work_buf[1] = (uint8_t)(now >> 16);
    work_buf[2] = (uint8_t)(now >> 8);
    work_buf[3] = (uint8_t)(now);

    memcpy(work_buf.get() + 4, plaintext.data(), plaintext.size());

    for (size_t i = 0; i < padding_len; i++)
      work_buf[data_len + i] = (uint8_t)padding_len;

    // Use pointer
    br_aes_ct_cbcenc_run(const_cast<br_aes_ct_cbcenc_keys*>(m_enc_ctx.get()),
                         iv.get(),
                         work_buf.get(),
                         total_len);

    size_t remaining_out = out_len - (iv_b64_len + 1);
    size_t cipher_b64_len = base64_encode_to_buffer(work_buf.get(), total_len, ptr, remaining_out);

    if (cipher_b64_len == 0) {
      unlock();
      return 0;
    }

    size_t total_written = REDACTED
    out_buf[total_written] = REDACTED

    unlock();
    return total_written;
  }

  size_t fast_serialize_encrypted(std::string_view plaintext,
                                  char* out_buf,
                                  size_t out_len,
                                  const AES_CBC_Cipher& cipher) {
    if (!cipher.get_enc_ctx())
      return 0;
    return cipher.encrypt(plaintext, out_buf, out_len);
  }

  size_t fast_serialize_encrypted_main(std::string_view plaintext, char* out_buf, size_t out_len) {
    return fast_serialize_encrypted(plaintext, out_buf, out_len, sharedCipher());
  }

  size_t fast_serialize_encrypted_ws(std::string_view plaintext, char* out_buf, size_t out_len) {
    return fast_serialize_encrypted(plaintext, out_buf, out_len, sharedCipherWs());
  }

  const AES_CBC_Cipher& sharedCipher() {
    static AES_CBC_Cipher cipher(std::string_view(reinterpret_cast<const char*>(AES_KEY), 32));
    return cipher;
  }

  const AES_CBC_Cipher& sharedCipherWs() {
    if (!g_wsCipher) {
      std::unique_ptr<AES_CBC_Cipher> cipher(
          new (std::nothrow) AES_CBC_Cipher(std::string_view(reinterpret_cast<const char*>(AES_KEY), 32)));
      if (cipher) {
        g_wsCipher.swap(cipher);
      } else {
        // Fallback to shared cipher to avoid hard failure on low memory.
        return sharedCipher();
      }
    }
    return *g_wsCipher;
  }

  void releaseWsCipher() {
    g_wsCipher.reset();
  }

  std::optional<EncryptedPayload> deserialize_payload(std::string_view serialized) {
    size_t separator = serialized.find(':');
    if (separator == std::string_view::npos)
      return std::nullopt;

    EncryptedPayload payload;
    size_t ivLen =
        base64_decode_to_buffer(serialized.substr(0, separator), payload.iv.data(), EncryptedPayload::IV_SIZE);

    size_t ctLen = base64_decode_to_buffer(
        serialized.substr(separator + 1), payload.ciphertext.data(), EncryptedPayload::MAX_CIPHERTEXT_SIZE);

    if (ivLen != EncryptedPayload::IV_SIZE || ctLen == 0)
      return std::nullopt;
    payload.ciphertextLen = ctLen;
    return payload;
  }

}  // namespace CryptoUtils
