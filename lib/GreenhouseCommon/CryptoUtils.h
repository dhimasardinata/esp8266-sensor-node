#ifndef CRYPTO_UTILS_H
#define CRYPTO_UTILS_H

#include <Arduino.h>
#include <bearssl/bearssl.h>

#include <optional>
#include <string_view>
#include <vector>

namespace CryptoUtils {

  // PLACEHOLDER: Replace with your own 32-byte AES-256 key
  // Must match the key in client-side JavaScript (data/crypto.js)
  // Generate with: python -c "import os; print([hex(b) for b in os.urandom(32)])"
  constexpr uint8_t AES_KEY[] = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
      0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
      0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
      0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
  };

  struct EncryptedPayload {
    std::vector<uint8_t> iv;
    std::vector<uint8_t> ciphertext;
  };

  class AES_CBC_Cipher {
  public:
    explicit AES_CBC_Cipher(std::string_view key);
    ~AES_CBC_Cipher();

    AES_CBC_Cipher(const AES_CBC_Cipher&) = delete;
    AES_CBC_Cipher& operator=(const AES_CBC_Cipher&) = delete;

    [[nodiscard]] std::optional<EncryptedPayload> encrypt(std::string_view plaintext) const;
    [[nodiscard]] std::optional<std::vector<uint8_t>> decrypt(const EncryptedPayload& payload) const;

  private:
    mutable br_aes_ct_cbcenc_keys m_enc_ctx;
    mutable br_aes_ct_cbcdec_keys m_dec_ctx;
  };

  String serialize_payload(const EncryptedPayload& payload);
  std::optional<EncryptedPayload> deserialize_payload(std::string_view serialized);

}  // namespace CryptoUtils

#endif  // CRYPTO_UTILS_H
