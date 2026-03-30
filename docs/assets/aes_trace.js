(function (root, factory) {
  if (typeof module === "object" && module.exports) {
    module.exports = factory();
  } else {
    root.AesTrace = factory();
  }
})(typeof globalThis !== "undefined" ? globalThis : this, function () {
  "use strict";

  var BLOCK_SIZE = 16;
  var AES_ROUNDS = 14;
  var MAX_PLAINTEXT_SIZE = 144;
  var WS_CHUNK_SIZE = MAX_PLAINTEXT_SIZE;

  var WS_REPLAY_SKEW_SEC_STRICT = 30;
  var WS_REPLAY_SKEW_SEC_SOFT = 300;
  var WS_REPLAY_SKEW_SEC_MAX = 900;

  var DEMO_VECTOR = {
    key: "visual-aes-demo-key-32-byte-seed",
    ivHex: "00112233445566778899aabbccddeeff",
    timestamp: 1735689600,
    plaintext: "setwifi greenhouse-lab supersecret",
    expectedCiphertextHex:
      "9c90c033268577c7b941141c5f72092c513ea5ed327a3e00493ab0c1c7da7cae80c121aaae9e42bf5c17dba32e40f180",
    expectedTerminalPayload:
      "ABEiM0RVZneImaq7zN3u/w==:nJDAMyaFd8e5QRQcX3IJLFE+pe0yej4ASTqwwcfafK6AwSGqrp5Cv1wX26MuQPGA",
    expectedPortalPayload:
      "ENC:ABEiM0RVZneImaq7zN3u/w==:nJDAMyaFd8e5QRQcX3IJLFE+pe0yej4ASTqwwcfafK6AwSGqrp5Cv1wX26MuQPGA",
  };

  var LONG_PRESETS = {
    statusJson:
      '{"type":"status","fw":"v2.7.4","uptimeMs":42888342,"health":{"score":89,"heap":90,"fragmentation":82,"cpu":88,"wifi":84,"sensor":92},"memory":{"free":25120,"maxBlock":11264,"minFree":17888,"minBlock":6912,"fragmentationPercent":24},"cpu":{"loopAvgUs":1421,"loopMaxUs":7843,"slowLoopPercent":1,"slowLoopCount":28},"wifi":{"state":"Connected","ssid":"GH-LAB-STA","rssi":-67,"ip":"192.168.1.100"},"time":{"synced":true,"source":"NTP","syncAge":"12s"},"api":{"lastSuccessAge":"9s","qosActive":false},"sensors":{"sht":"OK","bh1750":"OK","temperature":26.48,"humidity":69.21,"lux":401.7},"boot":{"reason":"Health Check","crashCount":0},"notes":"generated for docs long payload scenario"}',
    wsStatusText:
      "========== SYSTEM STATUS ==========\n" +
      "FW: 2.7.4 | Uptime: 11d 22h 03m\n" +
      "\n" +
      "[HEALTH] Score: 89/100 (A)\n" +
      "  Heap:90 Frag:82 CPU:88 WiFi:84 Sensor:92\n" +
      "\n" +
      "[MEMORY]\n" +
      "  Free: 25120 bytes | MaxBlock: 11264 bytes\n" +
      "  MinFree: 17888 bytes | MinBlock: 6912 bytes\n" +
      "  Fragmentation: 24%\n" +
      "\n" +
      "[CPU]\n" +
      "  Loop avg: 1421 us | max: 7843 us\n" +
      "  Slow loops: 1% (28 total)\n" +
      "\n" +
      "REDACTED" +
      "  SSID: REDACTED
      "  IP: 192.168.1.100\n" +
      "\n" +
      "[TIME] 14:38:22 (sync: 12s ago, src: NTP)\n" +
      "[API] Last success: 9s ago\n" +
      "[SENSORS] SHT: OK | BH1750: OK\n" +
      "[BOOT] Reason: Health Check | Crash Count: 0\n" +
      "====================================\n",
  };

  var SBOX = [
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b,
    0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
    0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26,
    0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2,
    0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
    0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed,
    0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f,
    0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
    0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec,
    0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14,
    0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
    0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d,
    0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f,
    0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
    0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11,
    0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f,
    0xb0, 0x54, 0xbb, 0x16,
  ];

  var INV_SBOX = [
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e,
    0x81, 0xf3, 0xd7, 0xfb, 0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87,
    0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb, 0x54, 0x7b, 0x94, 0x32,
    0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49,
    0x6d, 0x8b, 0xd1, 0x25, 0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16,
    0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92, 0x6c, 0x70, 0x48, 0x50,
    0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05,
    0xb8, 0xb3, 0x45, 0x06, 0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02,
    0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b, 0x3a, 0x91, 0x11, 0x41,
    0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8,
    0x1c, 0x75, 0xdf, 0x6e, 0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89,
    0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b, 0xfc, 0x56, 0x3e, 0x4b,
    0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59,
    0x27, 0x80, 0xec, 0x5f, 0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d,
    0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef, 0xa0, 0xe0, 0x3b, 0x4d,
    0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63,
    0x55, 0x21, 0x0c, 0x7d,
  ];

  var RCON = [0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40];

  function hasTextEncoder() {
    return typeof TextEncoder !== "undefined" && typeof TextDecoder !== "undefined";
  }

  function utf8ToBytes(text) {
    if (hasTextEncoder()) {
      return new Uint8Array(new TextEncoder().encode(text));
    }
    if (typeof Buffer !== "undefined") {
      return Uint8Array.from(Buffer.from(text, "utf8"));
    }
    var escaped = unescape(encodeURIComponent(text));
    var out = new Uint8Array(escaped.length);
    for (var i = 0; i < escaped.length; i++) {
      out[i] = escaped.charCodeAt(i) & 0xff;
    }
    return out;
  }

  function bytesToUtf8(bytes) {
    if (hasTextEncoder()) {
      return new TextDecoder("utf-8", { fatal: false }).decode(bytes);
    }
    if (typeof Buffer !== "undefined") {
      return Buffer.from(bytes).toString("utf8");
    }
    var latin1 = "";
    for (var i = 0; i < bytes.length; i++) {
      latin1 += String.fromCharCode(bytes[i]);
    }
    try {
      return decodeURIComponent(escape(latin1));
    } catch (err) {
      return latin1;
    }
  }

  function bytesToHex(bytes) {
    var out = "";
    for (var i = 0; i < bytes.length; i++) {
      var v = bytes[i].toString(16);
      if (v.length < 2) {
        v = "0" + v;
      }
      out += v;
    }
    return out;
  }

  function hexToBytes(hex) {
    var clean = String(hex || "").trim().toLowerCase();
    clean = clean.replace(/\s+/g, "");
    if (clean.length % 2 !== 0) {
      throw new Error("Hex length must be even");
    }
    var out = new Uint8Array(clean.length / 2);
    for (var i = 0; i < clean.length; i += 2) {
      var byte = parseInt(clean.slice(i, i + 2), 16);
      if (Number.isNaN(byte)) {
        throw new Error("Invalid hex input");
      }
      out[i / 2] = byte;
    }
    return out;
  }

  function bytesToBase64(bytes) {
    if (typeof Buffer !== "undefined") {
      return Buffer.from(bytes).toString("base64");
    }
    var binary = "";
    for (var i = 0; i < bytes.length; i++) {
      binary += String.fromCharCode(bytes[i]);
    }
    return btoa(binary);
  }

  function base64ToBytes(base64Text) {
    var input = String(base64Text || "").trim();
    if (typeof Buffer !== "undefined") {
      return Uint8Array.from(Buffer.from(input, "base64"));
    }
    var binary = atob(input);
    var out = new Uint8Array(binary.length);
    for (var i = 0; i < binary.length; i++) {
      out[i] = binary.charCodeAt(i) & 0xff;
    }
    return out;
  }

  function concatBytes(chunks) {
    var total = REDACTED
    var i;
    for (i = 0; i < chunks.length; i++) {
      total += REDACTED
    }
    var out = new Uint8Array(total);
    var offset = 0;
    for (i = 0; i < chunks.length; i++) {
      out.set(chunks[i], offset);
      offset += chunks[i].length;
    }
    return out;
  }

  function u32ToBytesBE(value) {
    var v = value >>> 0;
    return new Uint8Array([
      (v >>> 24) & 0xff,
      (v >>> 16) & 0xff,
      (v >>> 8) & 0xff,
      v & 0xff,
    ]);
  }

  function bytesToU32BE(bytes, offset) {
    var i = offset || 0;
    if (bytes.length < i + 4) {
      throw new Error("Not enough bytes for u32");
    }
    return (
      ((bytes[i] << 24) >>> 0) |
      (bytes[i + 1] << 16) |
      (bytes[i + 2] << 8) |
      bytes[i + 3]
    ) >>> 0;
  }

  function xorBytes(a, b) {
    if (a.length !== b.length) {
      throw new Error("XOR length mismatch");
    }
    var out = new Uint8Array(a.length);
    for (var i = 0; i < a.length; i++) {
      out[i] = a[i] ^ b[i];
    }
    return out;
  }

  function chunkBytes(bytes, size) {
    var chunks = [];
    for (var i = 0; i < bytes.length; i += size) {
      chunks.push(bytes.slice(i, i + size));
    }
    return chunks;
  }

  function clampNumber(value, min, max) {
    var v = Number(value);
    if (!Number.isFinite(v)) {
      return min;
    }
    if (v < min) {
      return min;
    }
    if (v > max) {
      return max;
    }
    return v;
  }

  function clampInt32(value) {
    var n = Number(value);
    if (!Number.isFinite(n)) {
      return 0;
    }
    if (n < -2147483648) {
      return -2147483648;
    }
    if (n > 2147483647) {
      return 2147483647;
    }
    return n | 0;
  }

  function formatRelativeSeconds(deltaSec) {
    var abs = Math.abs(deltaSec);
    var sign = deltaSec >= 0 ? "+" : "-";
    if (abs < 60) {
      return sign + abs + "s";
    }
    if (abs < 3600) {
      return sign + Math.floor(abs / 60) + "m " + (abs % 60) + "s";
    }
    if (abs < 86400) {
      return (
        sign +
        Math.floor(abs / 3600) +
        "h " +
        Math.floor((abs % 3600) / 60) +
        "m"
      );
    }
    return (
      sign +
      Math.floor(abs / 86400) +
      "d " +
      Math.floor((abs % 86400) / 3600) +
      "h"
    );
  }

  function formatEpochContext(epoch, nowEpoch) {
    var raw = Number(epoch);
    if (!Number.isFinite(raw) || raw < 0 || raw > 0xffffffff) {
      return {
        epoch: raw,
        valid: false,
        utcIso: "invalid",
        localString: "invalid",
        relativeToNow: "invalid",
        relativeSeconds: null,
      };
    }

    var sec = raw >>> 0;
    var date = new Date(sec * 1000);
    var nowSec = Number.isFinite(Number(nowEpoch))
      ? Number(nowEpoch)
      : Math.floor(Date.now() / 1000);
    var delta = sec - nowSec;

    return {
      epoch: sec,
      valid: true,
      utcIso: date.toISOString(),
      localString: date.toLocaleString(),
      relativeToNow: formatRelativeSeconds(delta),
      relativeSeconds: delta,
    };
  }

  function pkcs7Pad(data) {
    var padLength = BLOCK_SIZE - (data.length % BLOCK_SIZE);
    if (padLength === 0) {
      padLength = BLOCK_SIZE;
    }
    var padding = new Uint8Array(padLength);
    padding.fill(padLength);
    return {
      padded: concatBytes([data, padding]),
      padLength: padLength,
    };
  }

  function pkcs7Unpad(data) {
    if (data.length === 0 || data.length % BLOCK_SIZE !== 0) {
      return {
        ok: false,
        error: "Ciphertext length is invalid for PKCS7",
      };
    }
    var padLength = data[data.length - 1];
    if (padLength === 0 || padLength > BLOCK_SIZE || padLength > data.length) {
      return {
        ok: false,
        error: "Padding byte out of range",
      };
    }
    var i;
    for (i = 0; i < padLength; i++) {
      if (data[data.length - 1 - i] !== padLength) {
        return {
          ok: false,
          error: "PKCS7 tail bytes mismatch",
        };
      }
    }
    return {
      ok: true,
      padLength: padLength,
      unpadded: data.slice(0, data.length - padLength),
    };
  }

  function rotWord(word) {
    return [word[1], word[2], word[3], word[0]];
  }

  function subWord(word) {
    return [
      SBOX[word[0]],
      SBOX[word[1]],
      SBOX[word[2]],
      SBOX[word[3]],
    ];
  }

  function keyExpansion256(keyBytes) {
    if (keyBytes.length !== 32) {
      throw new Error("AES-256 key must be exactly 32 bytes");
    }

    var Nk = 8;
    var Nb = 4;
    var Nr = 14;
    var totalWords = REDACTED
    var words = new Array(totalWords);
    var i;

    for (i = 0; i < Nk; i++) {
      words[i] = [
        keyBytes[i * 4],
        keyBytes[i * 4 + 1],
        keyBytes[i * 4 + 2],
        keyBytes[i * 4 + 3],
      ];
    }

    for (i = Nk; i < totalWords; i++) {
      var temp = words[i - 1].slice();
      if (i % Nk === 0) {
        temp = subWord(rotWord(temp));
        temp[0] ^= RCON[i / Nk - 1];
      } else if (i % Nk === 4) {
        temp = subWord(temp);
      }

      words[i] = [
        words[i - Nk][0] ^ temp[0],
        words[i - Nk][1] ^ temp[1],
        words[i - Nk][2] ^ temp[2],
        words[i - Nk][3] ^ temp[3],
      ];
    }

    var roundKeys = [];
    for (var round = 0; round <= Nr; round++) {
      var rk = new Uint8Array(16);
      for (var col = 0; col < 4; col++) {
        var w = words[round * 4 + col];
        rk[col * 4] = w[0];
        rk[col * 4 + 1] = w[1];
        rk[col * 4 + 2] = w[2];
        rk[col * 4 + 3] = w[3];
      }
      roundKeys.push(rk);
    }
    return roundKeys;
  }

  function xtime(value) {
    var shifted = (value << 1) & 0xff;
    if (value & 0x80) {
      shifted ^= 0x1b;
    }
    return shifted;
  }

  function gmul(a, b) {
    var x = a & 0xff;
    var y = b & 0xff;
    var p = 0;
    for (var i = 0; i < 8; i++) {
      if (y & 1) {
        p ^= x;
      }
      var hiBitSet = x & 0x80;
      x = (x << 1) & 0xff;
      if (hiBitSet) {
        x ^= 0x1b;
      }
      y >>= 1;
    }
    return p & 0xff;
  }

  function addRoundKey(state, roundKey) {
    for (var i = 0; i < 16; i++) {
      state[i] ^= roundKey[i];
    }
  }

  function subBytes(state) {
    for (var i = 0; i < 16; i++) {
      state[i] = SBOX[state[i]];
    }
  }

  function invSubBytes(state) {
    for (var i = 0; i < 16; i++) {
      state[i] = INV_SBOX[state[i]];
    }
  }

  function shiftRows(state) {
    var out = new Uint8Array(16);
    for (var row = 0; row < 4; row++) {
      for (var col = 0; col < 4; col++) {
        out[col * 4 + row] = state[((col + row) % 4) * 4 + row];
      }
    }
    state.set(out);
  }

  function invShiftRows(state) {
    var out = new Uint8Array(16);
    for (var row = 0; row < 4; row++) {
      for (var col = 0; col < 4; col++) {
        out[col * 4 + row] = state[((col - row + 4) % 4) * 4 + row];
      }
    }
    state.set(out);
  }

  function mixColumns(state) {
    for (var col = 0; col < 4; col++) {
      var i = col * 4;
      var a0 = state[i];
      var a1 = state[i + 1];
      var a2 = state[i + 2];
      var a3 = state[i + 3];

      state[i] = xtime(a0) ^ (xtime(a1) ^ a1) ^ a2 ^ a3;
      state[i + 1] = a0 ^ xtime(a1) ^ (xtime(a2) ^ a2) ^ a3;
      state[i + 2] = a0 ^ a1 ^ xtime(a2) ^ (xtime(a3) ^ a3);
      state[i + 3] = (xtime(a0) ^ a0) ^ a1 ^ a2 ^ xtime(a3);
    }
  }

  function invMixColumns(state) {
    for (var col = 0; col < 4; col++) {
      var i = col * 4;
      var a0 = state[i];
      var a1 = state[i + 1];
      var a2 = state[i + 2];
      var a3 = state[i + 3];

      state[i] =
        gmul(a0, 14) ^ gmul(a1, 11) ^ gmul(a2, 13) ^ gmul(a3, 9);
      state[i + 1] =
        gmul(a0, 9) ^ gmul(a1, 14) ^ gmul(a2, 11) ^ gmul(a3, 13);
      state[i + 2] =
        gmul(a0, 13) ^ gmul(a1, 9) ^ gmul(a2, 14) ^ gmul(a3, 11);
      state[i + 3] =
        gmul(a0, 11) ^ gmul(a1, 13) ^ gmul(a2, 9) ^ gmul(a3, 14);
    }
  }

  function encryptBlockWithTrace(block, cbcSource, roundKeys) {
    var cbcXor = xorBytes(block, cbcSource);
    var state = cbcXor.slice();
    var rounds = [];

    var round0Input = state.slice();
    addRoundKey(state, roundKeys[0]);
    rounds.push({
      round: 0,
      inputHex: bytesToHex(round0Input),
      addRoundKeyHex: bytesToHex(state),
    });

    for (var round = 1; round < AES_ROUNDS; round++) {
      var roundInput = state.slice();
      subBytes(state);
      var subBytesState = state.slice();

      shiftRows(state);
      var shiftRowsState = state.slice();

      mixColumns(state);
      var mixColumnsState = state.slice();

      addRoundKey(state, roundKeys[round]);
      var addRoundKeyState = state.slice();

      rounds.push({
        round: round,
        inputHex: bytesToHex(roundInput),
        subBytesHex: bytesToHex(subBytesState),
        shiftRowsHex: bytesToHex(shiftRowsState),
        mixColumnsHex: bytesToHex(mixColumnsState),
        addRoundKeyHex: bytesToHex(addRoundKeyState),
      });
    }

    var lastRoundInput = state.slice();
    subBytes(state);
    var lastSubBytes = state.slice();

    shiftRows(state);
    var lastShiftRows = state.slice();

    addRoundKey(state, roundKeys[AES_ROUNDS]);
    var lastAddRoundKey = state.slice();

    rounds.push({
      round: AES_ROUNDS,
      inputHex: bytesToHex(lastRoundInput),
      subBytesHex: bytesToHex(lastSubBytes),
      shiftRowsHex: bytesToHex(lastShiftRows),
      addRoundKeyHex: bytesToHex(lastAddRoundKey),
    });

    return {
      cbcXor: cbcXor,
      ciphertext: state,
      rounds: rounds,
    };
  }

  function decryptBlockWithTrace(cipherBlock, cbcSource, roundKeys) {
    var state = cipherBlock.slice();
    var rounds = [];

    var round14Input = state.slice();
    addRoundKey(state, roundKeys[AES_ROUNDS]);
    var round14AddRoundKey = state.slice();

    rounds.push({
      round: AES_ROUNDS,
      inputHex: bytesToHex(round14Input),
      addRoundKeyHex: bytesToHex(round14AddRoundKey),
    });

    for (var round = AES_ROUNDS - 1; round >= 1; round--) {
      var roundInput = state.slice();
      invShiftRows(state);
      var roundInvShiftRows = state.slice();
      invSubBytes(state);
      var roundInvSubBytes = state.slice();
      addRoundKey(state, roundKeys[round]);
      var roundAddRoundKey = state.slice();
      invMixColumns(state);
      var roundInvMixColumns = state.slice();

      rounds.push({
        round: round,
        inputHex: bytesToHex(roundInput),
        invShiftRowsHex: bytesToHex(roundInvShiftRows),
        invSubBytesHex: bytesToHex(roundInvSubBytes),
        addRoundKeyHex: bytesToHex(roundAddRoundKey),
        invMixColumnsHex: bytesToHex(roundInvMixColumns),
      });
    }

    var round0Input = state.slice();
    invShiftRows(state);
    var round0InvShiftRows = state.slice();
    invSubBytes(state);
    var round0InvSubBytes = state.slice();
    addRoundKey(state, roundKeys[0]);
    var round0AddRoundKey = state.slice();

    var aesOutput = state.slice();
    var plaintext = xorBytes(aesOutput, cbcSource);

    rounds.push({
      round: 0,
      inputHex: bytesToHex(round0Input),
      invShiftRowsHex: bytesToHex(round0InvShiftRows),
      invSubBytesHex: bytesToHex(round0InvSubBytes),
      addRoundKeyHex: bytesToHex(round0AddRoundKey),
      cbcXorHex: bytesToHex(plaintext),
    });

    return {
      aesOutput: aesOutput,
      plaintext: plaintext,
      rounds: rounds,
    };
  }

  function decryptBlock(cipherBlock, roundKeys) {
    var zeroCbc = new Uint8Array(BLOCK_SIZE);
    return decryptBlockWithTrace(cipherBlock, zeroCbc, roundKeys).aesOutput;
  }

  function maskKey(keyText) {
    if (keyText.length <= 8) {
      return "********";
    }
    return keyText.slice(0, 6) + "..." + keyText.slice(-4);
  }

  function serializePayload(ivBytes, ciphertextBytes, mode) {
    var ivB64 = bytesToBase64(ivBytes);
    var ctB64 = bytesToBase64(ciphertextBytes);
    var core = ivB64 + ":" + ctB64;
    if (mode === "portal") {
      return "ENC:" + core;
    }
    return core;
  }

  function parseSerializedPayload(serialized, mode) {
    var raw = String(serialized || "").trim();
    var effectiveMode = mode;

    if (!effectiveMode) {
      effectiveMode = raw.indexOf("ENC:") === 0 ? "portal" : "terminal";
    }

    if (effectiveMode === "portal") {
      if (raw.indexOf("ENC:") !== 0) {
        throw new Error("Payload portal harus berprefix ENC:");
      }
      raw = raw.slice(4);
    } else if (raw.indexOf("ENC:") === 0) {
      throw new Error("Mode terminal tidak menerima prefix ENC:");
    }

    var parts = raw.split(":");
    if (parts.length !== 2 || !parts[0] || !parts[1]) {
      throw new Error("Format payload harus iv_b64:cipher_b64");
    }

    var ivBytes = base64ToBytes(parts[0]);
    var ciphertextBytes = base64ToBytes(parts[1]);

    return {
      mode: effectiveMode,
      ivB64: parts[0],
      ciphertextB64: parts[1],
      ivBytes: ivBytes,
      ciphertextBytes: ciphertextBytes,
      normalized: raw,
    };
  }

  function encryptTrace(options) {
    var opts = options || {};
    var keyText =
      typeof opts.keyText === "string" && opts.keyText.length > 0
        ? opts.keyText
        : DEMO_VECTOR.key;
    var ivHex =
      typeof opts.ivHex === "string" && opts.ivHex.length > 0
        ? opts.ivHex
        : DEMO_VECTOR.ivHex;
    var plaintext =
      typeof opts.plaintext === "string" ? opts.plaintext : DEMO_VECTOR.plaintext;

    var timestamp =
      opts.timestamp === undefined || opts.timestamp === null
        ? DEMO_VECTOR.timestamp
        : Number(opts.timestamp);

    if (!Number.isFinite(timestamp) || timestamp < 0 || timestamp > 0xffffffff) {
      throw new Error("Timestamp harus 0..4294967295");
    }

    var keyBytes = utf8ToBytes(keyText);
    if (keyBytes.length !== 32) {
      throw new Error("Demo key harus tepat 32 byte UTF-8");
    }

    var ivBytes = hexToBytes(ivHex);
    if (ivBytes.length !== BLOCK_SIZE) {
      throw new Error("IV harus 16 byte (32 karakter hex)");
    }

    var plaintextBytes = utf8ToBytes(plaintext);
    var timestampBytes = u32ToBytesBE(timestamp >>> 0);
    var payloadBytes = concatBytes([timestampBytes, plaintextBytes]);
    var paddedInfo = pkcs7Pad(payloadBytes);

    var blocks = chunkBytes(paddedInfo.padded, BLOCK_SIZE);
    var roundKeys = keyExpansion256(keyBytes);

    var blockTraces = [];
    var ciphertextBlocks = [];
    var previous = ivBytes;

    for (var i = 0; i < blocks.length; i++) {
      var encrypted = encryptBlockWithTrace(blocks[i], previous, roundKeys);
      blockTraces.push({
        index: i,
        plaintextBlockHex: bytesToHex(blocks[i]),
        cbcSourceHex: bytesToHex(previous),
        cbcXorHex: bytesToHex(encrypted.cbcXor),
        ciphertextBlockHex: bytesToHex(encrypted.ciphertext),
        rounds: encrypted.rounds,
      });
      ciphertextBlocks.push(encrypted.ciphertext);
      previous = encrypted.ciphertext;
    }

    var ciphertextBytes = concatBytes(ciphertextBlocks);
    var ivB64 = bytesToBase64(ivBytes);
    var ciphertextB64 = bytesToBase64(ciphertextBytes);

    var warnings = [];
    if (plaintextBytes.length > MAX_PLAINTEXT_SIZE) {
      warnings.push(
        "Panjang plaintext melebihi batas firmware (" +
          MAX_PLAINTEXT_SIZE +
          " byte). Dokumentasi tetap bisa memproses, firmware asli akan menolak buffer oversized."
      );
    }

    return {
      warnings: warnings,
      inputs: {
        plaintext: plaintext,
        plaintextLength: plaintextBytes.length,
        timestamp: timestamp >>> 0,
        timestampIso: new Date((timestamp >>> 0) * 1000).toISOString(),
        timestampContext: formatEpochContext(timestamp >>> 0),
        ivHex: bytesToHex(ivBytes),
        keyLength: keyBytes.length,
        keyMasked: maskKey(keyText),
      },
      stage: {
        plaintextHex: bytesToHex(plaintextBytes),
        timestampHex: bytesToHex(timestampBytes),
        payloadHex: bytesToHex(payloadBytes),
        payloadLength: payloadBytes.length,
        paddedHex: bytesToHex(paddedInfo.padded),
        paddedLength: paddedInfo.padded.length,
        padLength: paddedInfo.padLength,
      },
      blocks: blockTraces,
      outputs: {
        ciphertextHex: bytesToHex(ciphertextBytes),
        ivB64: ivB64,
        ciphertextB64: ciphertextB64,
        terminalPayload: ivB64 + ":" + ciphertextB64,
        portalPayload: "ENC:" + ivB64 + ":" + ciphertextB64,
      },
    };
  }

  function chunkWsPlaintext(plaintext, maxBytes) {
    var text = String(plaintext || "");
    var limit = Number(maxBytes || WS_CHUNK_SIZE);
    if (!Number.isFinite(limit) || limit < 1) {
      limit = WS_CHUNK_SIZE;
    }

    var chunks = [];
    var currentText = "";
    var currentBytes = 0;
    var byteOffset = 0;
    var chars = Array.from(text);

    for (var i = 0; i < chars.length; i++) {
      var ch = chars[i];
      var chBytes = utf8ToBytes(ch).length;
      if (currentBytes > 0 && currentBytes + chBytes > limit) {
        chunks.push({
          text: currentText,
          byteLength: currentBytes,
          chunkOffsetStart: byteOffset,
          chunkOffsetEnd: byteOffset + currentBytes,
        });
        byteOffset += currentBytes;
        currentText = "";
        currentBytes = 0;
      }
      currentText += ch;
      currentBytes += chBytes;
    }

    if (currentBytes > 0 || text.length === 0) {
      chunks.push({
        text: currentText,
        byteLength: currentBytes,
        chunkOffsetStart: byteOffset,
        chunkOffsetEnd: byteOffset + currentBytes,
      });
    }

    return chunks;
  }

  function frameIvFromBase(baseIvBytes, frameIndex) {
    var out = baseIvBytes.slice();
    var idx = frameIndex >>> 0;
    out[14] ^= idx & 0xff;
    out[15] ^= (idx >>> 8) & 0xff;
    return out;
  }

  function encryptWsFramesTrace(options) {
    var opts = options || {};
    var plaintext = String(opts.plaintext || "");
    var baseTimestamp =
      opts.baseTimestamp === undefined || opts.baseTimestamp === null
        ? DEMO_VECTOR.timestamp
        : Number(opts.baseTimestamp);
    var mode = opts.mode === "portal" ? "portal" : "terminal";
    var keyText =
      typeof opts.keyText === "string" && opts.keyText.length > 0
        ? opts.keyText
        : DEMO_VECTOR.key;

    if (
      !Number.isFinite(baseTimestamp) ||
      baseTimestamp < 0 ||
      baseTimestamp > 0xffffffff
    ) {
      throw new Error("baseTimestamp tidak valid");
    }

    var baseIv = hexToBytes(opts.baseIvHex || DEMO_VECTOR.ivHex);
    if (baseIv.length !== BLOCK_SIZE) {
      throw new Error("baseIvHex harus 16 byte");
    }

    var chunks = chunkWsPlaintext(plaintext, WS_CHUNK_SIZE);
    var frames = [];
    var totalBlocks = REDACTED
    var totalRounds = REDACTED

    for (var i = 0; i < chunks.length; i++) {
      var chunk = chunks[i];
      var ivBytes = frameIvFromBase(baseIv, i);
      var timestamp = (baseTimestamp + i) >>> 0;
      var trace = encryptTrace({
        plaintext: chunk.text,
        timestamp: timestamp,
        ivHex: bytesToHex(ivBytes),
        keyText: keyText,
      });
      var serialized =
        mode === "portal"
          ? trace.outputs.portalPayload
          : trace.outputs.terminalPayload;
      var frameRounds = trace.blocks.length * (AES_ROUNDS + 1);
      totalBlocks += REDACTED
      totalRounds += REDACTED
      frames.push({
        frameIndex: i,
        chunkOffsetStart: chunk.chunkOffsetStart,
        chunkOffsetEnd: chunk.chunkOffsetEnd,
        chunkPlaintext: chunk.text,
        chunkLength: chunk.byteLength,
        timestamp: timestamp,
        timestampContext: formatEpochContext(timestamp),
        ivHex: trace.inputs.ivHex,
        ivB64: trace.outputs.ivB64,
        payloadHex: trace.stage.payloadHex,
        paddedHex: trace.stage.paddedHex,
        blockCount: trace.blocks.length,
        roundCountTotal: REDACTED
        serializedPayload: serialized,
        trace: trace,
      });
    }

    return {
      mode: mode,
      chunkSize: WS_CHUNK_SIZE,
      sourceLengthBytes: utf8ToBytes(plaintext).length,
      frameCount: frames.length,
      totalBlocks: REDACTED
      totalRounds: REDACTED
      frames: frames,
    };
  }

  function deriveFirmwareIv(options) {
    var opts = options || {};
    var rawIv = hexToBytes(opts.rawIvHex || DEMO_VECTOR.ivHex);
    if (rawIv.length !== BLOCK_SIZE) {
      throw new Error("rawIvHex harus 16 byte (32 hex)");
    }

    var micros = clampNumber(opts.micros, 0, 0xffffffff) >>> 0;
    var rssi = clampInt32(opts.rssi);
    var rssiMask = rssi & 0xff;

    var masks = [
      micros & 0xff,
      (micros >>> 8) & 0xff,
      (micros >>> 16) & 0xff,
      rssiMask,
    ];
    var sources = [
      "micros low byte",
      "micros byte[1]",
      "micros byte[2]",
      "rssi byte",
    ];

    var finalIv = rawIv.slice();
    for (var i = 0; i < 4; i++) {
      finalIv[i] ^= masks[i];
    }

    var byteDiff = [];
    for (var b = 0; b < BLOCK_SIZE; b++) {
      var before = rawIv[b];
      var mask = b < 4 ? masks[b] : 0;
      var after = finalIv[b];
      byteDiff.push({
        index: b,
        beforeHex: (before < 16 ? "0" : "") + before.toString(16),
        maskHex: (mask < 16 ? "0" : "") + mask.toString(16),
        afterHex: (after < 16 ? "0" : "") + after.toString(16),
        source: b < 4 ? sources[b] : "unchanged",
      });
    }

    return {
      rawIvHex: bytesToHex(rawIv),
      rawIvB64: bytesToBase64(rawIv),
      finalIvHex: bytesToHex(finalIv),
      finalIvB64: bytesToBase64(finalIv),
      micros: micros,
      rssi: rssi,
      rssiMaskHex: (rssiMask < 16 ? "0" : "") + rssiMask.toString(16),
      masksHex: masks.map(function (v) {
        return (v < 16 ? "0" : "") + v.toString(16);
      }),
      byteDiff: byteDiff,
    };
  }

  function validateSerialized(serialized, options) {
    var opts = options || {};
    var steps = [];
    var parsed = null;
    var blocks = [];
    var decryptBlocks = [];
    var decryptedPadded = null;

    function pushStep(name, ok, detail) {
      steps.push({
        name: name,
        ok: !!ok,
        detail: detail,
      });
    }

    try {
      parsed = parseSerializedPayload(serialized, opts.mode);
      pushStep("Split payload", true, "Format iv_b64:cipher_b64 valid");

      if (parsed.ivBytes.length !== BLOCK_SIZE) {
        pushStep("Decode base64 IV", false, "IV length harus 16 byte");
        return {
          ok: false,
          steps: steps,
          mode: parsed.mode,
          ivHex: bytesToHex(parsed.ivBytes),
          ciphertextHex: bytesToHex(parsed.ciphertextBytes),
        };
      }
      pushStep("Decode base64 IV", true, "IV = 16 byte");

      if (
        parsed.ciphertextBytes.length === 0 ||
        parsed.ciphertextBytes.length % BLOCK_SIZE !== 0
      ) {
        pushStep(
          "Decode base64 ciphertext",
          false,
          "Ciphertext harus >0 dan kelipatan 16 byte"
        );
        return {
          ok: false,
          steps: steps,
          mode: parsed.mode,
          ivHex: bytesToHex(parsed.ivBytes),
          ciphertextHex: bytesToHex(parsed.ciphertextBytes),
        };
      }
      pushStep(
        "Decode base64 ciphertext",
        true,
        "Ciphertext length = " + parsed.ciphertextBytes.length + " byte"
      );

      var keyText =
        typeof opts.keyText === "string" && opts.keyText.length > 0
          ? opts.keyText
          : DEMO_VECTOR.key;
      var keyBytes = utf8ToBytes(keyText);
      if (keyBytes.length !== 32) {
        pushStep("Load key", false, "Key harus 32 byte");
        return {
          ok: false,
          steps: steps,
          mode: parsed.mode,
          ivHex: bytesToHex(parsed.ivBytes),
          ciphertextHex: bytesToHex(parsed.ciphertextBytes),
        };
      }
      pushStep("Load key", true, "AES-256 key loaded");

      var roundKeys = keyExpansion256(keyBytes);
      blocks = chunkBytes(parsed.ciphertextBytes, BLOCK_SIZE);
      var decryptedBlocks = [];
      var previous = parsed.ivBytes;

      for (var i = 0; i < blocks.length; i++) {
        var decTrace = decryptBlockWithTrace(blocks[i], previous, roundKeys);
        decryptedBlocks.push(decTrace.plaintext);
        decryptBlocks.push({
          blockIndex: i,
          ciphertextBlockHex: bytesToHex(blocks[i]),
          cbcSourceHex: bytesToHex(previous),
          aesOutputHex: bytesToHex(decTrace.aesOutput),
          plaintextBlockHex: bytesToHex(decTrace.plaintext),
          rounds: decTrace.rounds,
        });
        previous = blocks[i];
      }

      decryptedPadded = concatBytes(decryptedBlocks);
      pushStep("AES-CBC decrypt", true, "Decrypt " + blocks.length + " block selesai");

      var unpad = pkcs7Unpad(decryptedPadded);
      if (!unpad.ok) {
        pushStep("PKCS7 validate", false, unpad.error);
        return {
          ok: false,
          steps: steps,
          mode: parsed.mode,
          ivHex: bytesToHex(parsed.ivBytes),
          ciphertextHex: bytesToHex(parsed.ciphertextBytes),
          decryptBlocks: decryptBlocks,
          decryptedPaddedHex: bytesToHex(decryptedPadded),
          pkcs7Error: unpad.error,
        };
      }
      pushStep("PKCS7 validate", true, "Padding = " + unpad.padLength + " byte");

      if (unpad.unpadded.length < 4) {
        pushStep("Extract timestamp", false, "Payload dekripsi terlalu pendek");
        return {
          ok: false,
          steps: steps,
          mode: parsed.mode,
          ivHex: bytesToHex(parsed.ivBytes),
          ciphertextHex: bytesToHex(parsed.ciphertextBytes),
          decryptBlocks: decryptBlocks,
          decryptedPaddedHex: bytesToHex(decryptedPadded),
          unpaddedHex: bytesToHex(unpad.unpadded),
        };
      }

      var msgTimestamp = bytesToU32BE(unpad.unpadded, 0);
      var plaintextBytes = unpad.unpadded.slice(4);
      var plaintext = bytesToUtf8(plaintextBytes);
      pushStep("Extract timestamp", true, "Timestamp = " + msgTimestamp);

      var timeSynced = opts.timeSynced !== false;
      var rawWindow =
        opts.skewWindowSec === undefined || opts.skewWindowSec === null
          ? WS_REPLAY_SKEW_SEC_STRICT
          : Number(opts.skewWindowSec);
      var window = clampNumber(rawWindow, WS_REPLAY_SKEW_SEC_STRICT, WS_REPLAY_SKEW_SEC_MAX);

      var deviceEpoch =
        opts.deviceEpoch === undefined || opts.deviceEpoch === null
          ? msgTimestamp
          : Number(opts.deviceEpoch);

      if (!Number.isFinite(deviceEpoch) || deviceEpoch < 0 || deviceEpoch > 0xffffffff) {
        pushStep("Replay skew check", false, "Device epoch tidak valid");
        return {
          ok: false,
          steps: steps,
          mode: parsed.mode,
          ivHex: bytesToHex(parsed.ivBytes),
          ciphertextHex: bytesToHex(parsed.ciphertextBytes),
          decryptBlocks: decryptBlocks,
          decryptedPaddedHex: bytesToHex(decryptedPadded),
          unpaddedHex: bytesToHex(unpad.unpadded),
          payloadHex: bytesToHex(unpad.unpadded),
          timestamp: msgTimestamp,
          timestampHex: bytesToHex(unpad.unpadded.slice(0, 4)),
          plaintextHex: bytesToHex(plaintextBytes),
          plaintext: plaintext,
        };
      }

      if (timeSynced) {
        var now = deviceEpoch >>> 0;
        var tooFuture = msgTimestamp > now + window;
        var tooPast = msgTimestamp + window < now;
        if (tooFuture || tooPast) {
          pushStep(
            "Replay skew check",
            false,
            "Timestamp di luar window +/-" + window + " detik"
          );
          return {
            ok: false,
            steps: steps,
            mode: parsed.mode,
            ivHex: bytesToHex(parsed.ivBytes),
            ciphertextHex: bytesToHex(parsed.ciphertextBytes),
            decryptBlocks: decryptBlocks,
            decryptedPaddedHex: bytesToHex(decryptedPadded),
            unpaddedHex: bytesToHex(unpad.unpadded),
            payloadHex: bytesToHex(unpad.unpadded),
            timestamp: msgTimestamp,
            timestampHex: bytesToHex(unpad.unpadded.slice(0, 4)),
            plaintextHex: bytesToHex(plaintextBytes),
            plaintext: plaintext,
            timestampContext: formatEpochContext(msgTimestamp, now),
            deviceEpoch: now,
            deviceEpochContext: formatEpochContext(now, now),
            skewWindowSec: window,
          };
        }
        pushStep(
          "Replay skew check",
          true,
          "Valid dalam window +/-" + window + " detik"
        );
      } else {
        pushStep(
          "Replay skew check",
          true,
          "Bypass: REDACTED
        );
      }

      return {
        ok: true,
        steps: steps,
        mode: parsed.mode,
        ivHex: bytesToHex(parsed.ivBytes),
        ciphertextHex: bytesToHex(parsed.ciphertextBytes),
        decryptBlocks: decryptBlocks,
        decryptedPaddedHex: bytesToHex(decryptedPadded),
        unpaddedHex: bytesToHex(unpad.unpadded),
        timestampHex: bytesToHex(unpad.unpadded.slice(0, 4)),
        plaintextHex: bytesToHex(plaintextBytes),
        payloadHex: bytesToHex(unpad.unpadded),
        timestamp: msgTimestamp,
        timestampContext: formatEpochContext(msgTimestamp, deviceEpoch),
        plaintext: plaintext,
      };
    } catch (error) {
      pushStep("Parse payload", false, error.message || String(error));
      return {
        ok: false,
        steps: steps,
        error: error.message || String(error),
        mode: parsed ? parsed.mode : undefined,
        ivHex: parsed ? bytesToHex(parsed.ivBytes) : undefined,
        ciphertextHex: parsed ? bytesToHex(parsed.ciphertextBytes) : undefined,
        decryptBlocks: decryptBlocks,
        decryptedPaddedHex: decryptedPadded ? bytesToHex(decryptedPadded) : undefined,
      };
    }
  }

  function toStateMatrixLines(hexOrBytes) {
    var bytes;
    if (typeof hexOrBytes === "string") {
      bytes = hexToBytes(hexOrBytes);
    } else {
      bytes = hexOrBytes;
    }
    if (!bytes || bytes.length !== 16) {
      return ["state harus 16 byte"];
    }

    var lines = [];
    for (var row = 0; row < 4; row++) {
      var line = [];
      for (var col = 0; col < 4; col++) {
        var idx = col * 4 + row;
        var v = bytes[idx].toString(16);
        if (v.length < 2) {
          v = "0" + v;
        }
        line.push(v);
      }
      lines.push(line.join(" "));
    }
    return lines;
  }

  return {
    BLOCK_SIZE: BLOCK_SIZE,
    AES_ROUNDS: AES_ROUNDS,
    MAX_PLAINTEXT_SIZE: MAX_PLAINTEXT_SIZE,
    WS_CHUNK_SIZE: WS_CHUNK_SIZE,
    WS_REPLAY_SKEW_SEC_STRICT: WS_REPLAY_SKEW_SEC_STRICT,
    WS_REPLAY_SKEW_SEC_SOFT: WS_REPLAY_SKEW_SEC_SOFT,
    WS_REPLAY_SKEW_SEC_MAX: WS_REPLAY_SKEW_SEC_MAX,
    DEMO_VECTOR: DEMO_VECTOR,
    LONG_PRESETS: LONG_PRESETS,
    encryptTrace: encryptTrace,
    encryptWsFramesTrace: encryptWsFramesTrace,
    decryptBlockWithTrace: decryptBlockWithTrace,
    deriveFirmwareIv: deriveFirmwareIv,
    formatEpochContext: formatEpochContext,
    validateSerialized: validateSerialized,
    serializePayload: serializePayload,
    parseSerializedPayload: parseSerializedPayload,
    chunkWsPlaintext: chunkWsPlaintext,
    utf8ToBytes: utf8ToBytes,
    bytesToUtf8: bytesToUtf8,
    hexToBytes: hexToBytes,
    bytesToHex: bytesToHex,
    bytesToBase64: bytesToBase64,
    base64ToBytes: base64ToBytes,
    toStateMatrixLines: toStateMatrixLines,
  };
});
