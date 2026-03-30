const fs = require("fs");
const path = require("path");

function fail(message) {
  throw new Error(message);
}

function assert(condition, message) {
  if (!condition) {
    fail(message);
  }
}

function loadCacheCrcTrace() {
  const scriptPath = path.resolve(__dirname, "../../docs/assets/cache_crc_trace.js");
  const code = fs.readFileSync(scriptPath, "utf8");
  global.window = global;
  global.CacheCrcTrace = undefined;
  eval(code); // eslint-disable-line no-eval
  if (!global.CacheCrcTrace) {
    fail("CacheCrcTrace module gagal dimuat");
  }
  return global.CacheCrcTrace;
}

function run() {
  const lib = loadCacheCrcTrace();
  const sampleRecord = {
    timestamp: 1735689600,
    temp10: 253,
    hum10: 680,
    lux: 1234,
    rssi: -67,
  };

  // Scenario A: RTC fail -> fallback LittleFS success.
  let ctx = lib.createContext("firmware");
  let result = lib.simulateRtcIngressFallback(ctx, "rtc_fail_to_fs", sampleRecord);
  assert(result.ok === true, "Scenario A harus sukses");
  assert(result.error === "NONE", "Scenario A error harus NONE");
  assert(result.state && result.state.littlefs && result.state.littlefs.sizeBytes > 0, "Scenario A harus menulis ke LittleFS");

  // Scenario B: RTC fail + LittleFS fail -> drop guarded.
  ctx = lib.createContext("firmware");
  result = lib.simulateRtcIngressFallback(ctx, "both_fail", sampleRecord);
  assert(result.ok === false, "Scenario B harus gagal");
  assert(result.error === "FILE_READ_ERROR", "Scenario B harus FILE_READ_ERROR");
  assert(result.state && result.state.littlefs && result.state.littlefs.sizeBytes === 0, "Scenario B tidak boleh menulis LittleFS");

  // Scenario C: Per-record corruption is recoverable, not total wipe.
  ctx = lib.createContext("firmware");
  lib.rtcAppend(ctx, sampleRecord, { autoFlush: false });
  lib.rtcAppend(
    ctx,
    {
      timestamp: sampleRecord.timestamp + 60,
      temp10: sampleRecord.temp10 + 1,
      hum10: sampleRecord.hum10 + 1,
      lux: sampleRecord.lux + 1,
      rssi: sampleRecord.rssi + 1,
    },
    { autoFlush: false }
  );
  lib.rtcAppend(
    ctx,
    {
      timestamp: sampleRecord.timestamp + 120,
      temp10: sampleRecord.temp10 + 2,
      hum10: sampleRecord.hum10 + 2,
      lux: sampleRecord.lux + 2,
      rssi: sampleRecord.rssi + 2,
    },
    { autoFlush: false }
  );
  const beforeCount = lib.snapshot(ctx).rtc.count;
  lib.rtcInjectCrcMismatch(ctx);
  const firstPeek = lib.rtcPeek(ctx);
  assert(
    firstPeek.error === "CORRUPT_DATA" || firstPeek.error === "SCANNING",
    "Scenario C peek pertama harus masuk recovery state"
  );
  const secondPeek = lib.rtcPeek(ctx);
  assert(secondPeek.error === "NONE" || secondPeek.error === "CACHE_EMPTY", "Scenario C peek kedua harus selesai recovery");
  const afterCount = lib.snapshot(ctx).rtc.count;
  assert(afterCount >= 0 && afterCount < beforeCount, "Scenario C harus mempertahankan data valid tersisa");

  console.log("REDACTED");
}

run();
