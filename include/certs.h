#ifndef CERTS_H
#define CERTS_H

#include <Arduino.h>

// PLACEHOLDER: Generate your own SSL certificate and key
// Use: openssl req -x509 -newkey rsa:2048 -keyout key.pem -out cert.pem -days 3650 -nodes
// Then run: python scripts/convert_certs.py

const uint8_t server_cert[] PROGMEM = {
    // YOUR CERTIFICATE DATA HERE
    0x00
};
const size_t server_cert_len = sizeof(server_cert);

const uint8_t server_key[] PROGMEM = {
    // YOUR PRIVATE KEY DATA HERE  
    0x00
};
const size_t server_key_len = sizeof(server_key);

#endif  // CERTS_H
