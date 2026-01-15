#pragma once
#include <stdint.h>
#include <stddef.h>

namespace BearSSL {
    class AES {
    public:
         virtual ~AES() {}
    };
    
    class Cipher {
    public:
        virtual ~Cipher() {}
        virtual bool encrypt(void* dest, const void* src, size_t len) { return true; }
    };
    
    class WiFiClientSecure {
    public:
        void setInsecure() {}
        void setBufferSizes(int, int) {}
    };
}
