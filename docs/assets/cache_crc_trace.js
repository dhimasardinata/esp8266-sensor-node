(function () {
  "use strict";

  var CACHE_ERROR = {
    NONE: "NONE",
    CACHE_EMPTY: "CACHE_EMPTY",
    FILE_READ_ERROR: "FILE_READ_ERROR",
    OUT_OF_MEMORY: "OUT_OF_MEMORY",
    CORRUPT_DATA: "CORRUPT_DATA",
    SCANNING: "SCANNING",
  };

  var CONSTANTS = {
    RTC_SENSOR_MAGIC: 0xcafebabe >>> 0,
    RTC_RECORD_MAGIC: 0xbeef,
    RTC_LAYOUT_VERSION: 2,
    RTC_RECOVERY_BUDGET_SLOTS: 4,
    FS_HEADER_MAGIC: 0xdeadbeef >>> 0,
    FS_RECORD_MAGIC: 0xa55a,
    FS_VERSION: 4,
    FS_DATA_START: 24,
    SYNC_SCAN_BUDGET_BYTES: 1024,
    TRIM_BUDGET_BYTES: 2048,
    RECORD_LEN_TOLERANCE: 100,
    MAX_PAYLOAD_SIZE: 256,
    EMERGENCY_QUEUE_CAPACITY: 16,
  };

  var SCALE_CONFIG = {
    visual: {
      id: "visual",
      label: "visual",
      rtcMaxRecords: 8,
      fsMaxBytes: 4096,
      maxPayloadSize: CONSTANTS.MAX_PAYLOAD_SIZE,
    },
    firmware: {
      id: "firmware",
      label: "firmware-real",
      rtcMaxRecords: 17,
      fsMaxBytes: 100 * 1024,
      maxPayloadSize: CONSTANTS.MAX_PAYLOAD_SIZE,
    },
  };

  var CACHE_PRESETS = {
    normal_ok: {
      label: "normal_ok",
      note: "Kondisi sehat: record RTC dan LittleFS valid, read_one harus sukses.",
    },
    crc_mismatch: {
      label: "crc_mismatch",
      note: "Record LittleFS valid lalu CRC diubah untuk simulasi data korup.",
    },
    magic_corrupt_salvage: {
      label: "magic_corrupt_salvage",
      note: "Magic record rusak, tapi payload dan CRC valid sehingga bisa diselamatkan.",
    },
    sync_loss_rescan: {
      label: "sync_loss_rescan",
      note:
        "Awal record diberi noise, read pertama mengembalikan SCANNING lalu resync ke record berikutnya.",
    },
  };

  var UTF8_ENCODER = typeof TextEncoder !== "undefined" ? new TextEncoder() : null;
  var UTF8_DECODER = typeof TextDecoder !== "undefined" ? new TextDecoder("utf-8", { fatal: false }) : null;

  var CRC_TABLE = (function buildCrcTable() {
    var table = new Uint32Array(256);
    for (var i = 0; i < 256; i++) {
      var c = i;
      for (var j = 0; j < 8; j++) {
        c = (c & 1) ? ((0xedb88320 ^ (c >>> 1)) >>> 0) : (c >>> 1);
      }
      table[i] = c >>> 0;
    }
    return table;
  })();

  function clampU16(value) {
    var n = Number(value);
    if (!Number.isFinite(n)) return 0;
    return (n & 0xffff) >>> 0;
  }

  function clampS16(value) {
    var n = Number(value);
    if (!Number.isFinite(n)) return 0;
    var v = (n & 0xffff) >>> 0;
    return v > 0x7fff ? v - 0x10000 : v;
  }

  function clampU32(value) {
    var n = Number(value);
    if (!Number.isFinite(n)) return 0;
    return n >>> 0;
  }

  function hex(value, width) {
    var h = (value >>> 0).toString(16).toUpperCase();
    while (h.length < (width || 8)) h = "0" + h;
    return "0x" + h;
  }

  function utf8ToBytes(text) {
    var value = String(text == null ? "" : text);
    if (UTF8_ENCODER) return UTF8_ENCODER.encode(value);
    var out = [];
    for (var i = 0; i < value.length; i++) {
      out.push(value.charCodeAt(i) & 0xff);
    }
    return Uint8Array.from(out);
  }

  function bytesToUtf8(bytes) {
    if (!bytes || bytes.length === 0) return "";
    if (UTF8_DECODER) {
      try {
        return UTF8_DECODER.decode(bytes);
      } catch (error) {
        // fall through
      }
    }
    var out = "";
    for (var i = 0; i < bytes.length; i++) {
      var b = bytes[i];
      out += b >= 32 && b <= 126 ? String.fromCharCode(b) : ".";
    }
    return out;
  }

  function bytesToHex(bytes, limit) {
    if (!bytes || bytes.length === 0) return "";
    var max = Number.isFinite(limit) && limit > 0 ? Math.min(limit, bytes.length) : bytes.length;
    var out = [];
    for (var i = 0; i < max; i++) {
      var h = bytes[i].toString(16).toUpperCase();
      out.push(h.length < 2 ? "0" + h : h);
    }
    if (max < bytes.length) out.push("...");
    return out.join("");
  }

  function u16ToBytesLE(value) {
    var v = clampU16(value);
    return [v & 0xff, (v >>> 8) & 0xff];
  }

  function s16ToBytesLE(value) {
    var v = clampS16(value);
    return u16ToBytesLE(v < 0 ? 0x10000 + v : v);
  }

  function u32ToBytesLE(value) {
    var v = clampU32(value);
    return [v & 0xff, (v >>> 8) & 0xff, (v >>> 16) & 0xff, (v >>> 24) & 0xff];
  }

  function crc32Compute(bytes, initialCrc) {
    if (!bytes || bytes.length === 0) {
      return clampU32(initialCrc || 0);
    }
    var crc = (clampU32(initialCrc || 0) ^ 0xffffffff) >>> 0;
    for (var i = 0; i < bytes.length; i++) {
      crc = (CRC_TABLE[(crc ^ bytes[i]) & 0xff] ^ (crc >>> 8)) >>> 0;
    }
    return (crc ^ 0xffffffff) >>> 0;
  }

  function pushStatus(arr, status, title, detail) {
    arr.push({
      status: status,
      title: title,
      detail: detail || "-",
    });
  }

  function cloneRecord(record) {
    return {
      timestamp: record.timestamp >>> 0,
      temp10: clampS16(record.temp10),
      hum10: clampS16(record.hum10),
      lux: clampU16(record.lux),
      rssi: clampS16(record.rssi),
    };
  }

  function createEmptyRecord() {
    return {
      timestamp: 0,
      temp10: 0,
      hum10: 0,
      lux: 0,
      rssi: 0,
    };
  }

  function createEmptyRtcSlot() {
    return {
      magic: 0,
      seq: 0,
      timestamp: 0,
      temp10: 0,
      hum10: 0,
      lux: 0,
      rssi: 0,
      crc: 0,
    };
  }

  function cloneRtcSlot(slot) {
    return {
      magic: clampU16(slot.magic),
      seq: clampU16(slot.seq),
      timestamp: clampU32(slot.timestamp),
      temp10: clampS16(slot.temp10),
      hum10: clampS16(slot.hum10),
      lux: clampU16(slot.lux),
      rssi: clampS16(slot.rssi),
      crc: clampU32(slot.crc),
    };
  }

  function normalizeScale(scaleMode) {
    return SCALE_CONFIG[scaleMode] || SCALE_CONFIG.visual;
  }

  function createRtcState(config) {
    var slots = [];
    for (var i = 0; i < config.rtcMaxRecords; i++) {
      slots.push(createEmptyRtcSlot());
    }
    var header = {
      blockMagic: CONSTANTS.RTC_SENSOR_MAGIC,
      version: CONSTANTS.RTC_LAYOUT_VERSION,
      maxRecords: clampU16(config.rtcMaxRecords),
      head: 0,
      tail: 0,
      count: 0,
      nextSeq: 0,
      headerCrc: 0,
    };
    header.headerCrc = rtcCalculateHeaderCrc(header);
    var state = {
      header: header,
      slots: slots,
      legacyV1: null,
    };
    return state;
  }

  function createFsState(config) {
    var state = {
      header: {
        magic: CONSTANTS.FS_HEADER_MAGIC,
        head: CONSTANTS.FS_DATA_START,
        tail: CONSTANTS.FS_DATA_START,
        size: 0,
        version: CONSTANTS.FS_VERSION,
        crc: 0,
      },
      data: new Uint8Array(config.fsMaxBytes),
      pendingScanningTicks: 0,
      lastRead: {
        error: CACHE_ERROR.NONE,
        payloadText: "",
        payloadHex: "",
        storedCrc: null,
        calculatedCrc: null,
        salvaged: false,
        len: 0,
      },
    };
    state.header.crc = fsCalculateHeaderCrc(state.header);
    return state;
  }

  function createContext(scaleMode) {
    var config = normalizeScale(scaleMode);
    var emergencyItems = new Array(CONSTANTS.EMERGENCY_QUEUE_CAPACITY);
    for (var i = 0; i < emergencyItems.length; i++) {
      emergencyItems[i] = null;
    }
    return {
      scaleMode: config.id,
      config: {
        scaleId: config.id,
        rtcMaxRecords: config.rtcMaxRecords,
        fsMaxBytes: config.fsMaxBytes,
        maxPayloadSize: config.maxPayloadSize,
      },
      rtc: createRtcState(config),
      fs: createFsState(config),
      emergency: {
        capacity: CONSTANTS.EMERGENCY_QUEUE_CAPACITY,
        items: emergencyItems,
        head: 0,
        tail: 0,
        count: 0,
        backpressure: false,
        enqueueTotal: REDACTED
        drainedTotal: REDACTED
        blockedSamples: 0,
      },
      lastCrcDetail: "",
      lastResult: null,
    };
  }

  function resetContext(target, scaleMode) {
    var next = createContext(scaleMode || (target && target.scaleMode));
    target.scaleMode = next.scaleMode;
    target.config = next.config;
    target.rtc = next.rtc;
    target.fs = next.fs;
    target.lastCrcDetail = "";
    target.lastResult = null;
    return target;
  }

  function emergencyPeek(ctx) {
    if (!ctx || !ctx.emergency || ctx.emergency.count <= 0) {
      return null;
    }
    return ctx.emergency.items[ctx.emergency.tail] || null;
  }

  function emergencyPop(ctx) {
    if (!ctx || !ctx.emergency || ctx.emergency.count <= 0) {
      return null;
    }
    var emergency = ctx.emergency;
    var item = emergency.items[emergency.tail];
    emergency.items[emergency.tail] = null;
    emergency.tail = (emergency.tail + 1) % emergency.capacity;
    emergency.count--;
    if (emergency.count <= 0) {
      emergency.count = 0;
      emergency.head = 0;
      emergency.tail = 0;
    }
    return item || null;
  }

  function emergencyEnqueue(ctx, record, steps, reasonLabel) {
    if (!ctx || !ctx.emergency || !record) {
      return false;
    }
    var emergency = ctx.emergency;
    if (emergency.count >= emergency.capacity) {
      emergency.backpressure = true;
      emergency.blockedSamples++;
      pushStatus(
        steps,
        "fail",
        "Emergency queue penuh",
        "Backpressure aktif. Sample baru ditahan untuk menjaga data lama tetap utuh."
      );
      return false;
    }
    emergency.items[emergency.head] = {
      timestamp: clampU32(record.timestamp),
      temp10: clampS16(record.temp10),
      hum10: clampS16(record.hum10),
      lux: clampU16(record.lux),
      rssi: clampS16(record.rssi),
    };
    emergency.head = (emergency.head + 1) % emergency.capacity;
    emergency.count++;
    emergency.enqueueTotal++;
    emergency.backpressure = emergency.count >= emergency.capacity;
    pushStatus(
      steps,
      emergency.backpressure ? "warn" : "info",
      "Emergency enqueue",
      "Record masuk queue (" +
        emergency.count +
        "/" +
        emergency.capacity +
        ")" +
        (reasonLabel ? " karena " + reasonLabel : ".")
    );
    return true;
  }

  function rtcRecordToBytes(record) {
    var bytes = [];
    bytes.push.apply(bytes, u32ToBytesLE(record.timestamp));
    bytes.push.apply(bytes, s16ToBytesLE(record.temp10));
    bytes.push.apply(bytes, s16ToBytesLE(record.hum10));
    bytes.push.apply(bytes, u16ToBytesLE(record.lux));
    bytes.push.apply(bytes, s16ToBytesLE(record.rssi));
    return Uint8Array.from(bytes);
  }

  function rtcSlotToBytes(slot) {
    var bytes = [];
    bytes.push.apply(bytes, u16ToBytesLE(slot.magic));
    bytes.push.apply(bytes, u16ToBytesLE(slot.seq));
    bytes.push.apply(bytes, u32ToBytesLE(slot.timestamp));
    bytes.push.apply(bytes, s16ToBytesLE(slot.temp10));
    bytes.push.apply(bytes, s16ToBytesLE(slot.hum10));
    bytes.push.apply(bytes, u16ToBytesLE(slot.lux));
    bytes.push.apply(bytes, s16ToBytesLE(slot.rssi));
    return Uint8Array.from(bytes);
  }

  function rtcCalculateSlotCrc(slot) {
    return crc32Compute(rtcSlotToBytes(slot), 0);
  }

  function rtcHeaderToBytes(header) {
    var bytes = [];
    bytes.push.apply(bytes, u32ToBytesLE(header.blockMagic));
    bytes.push.apply(bytes, u16ToBytesLE(header.version));
    bytes.push.apply(bytes, u16ToBytesLE(header.maxRecords));
    bytes.push.apply(bytes, u16ToBytesLE(header.head));
    bytes.push.apply(bytes, u16ToBytesLE(header.tail));
    bytes.push.apply(bytes, u16ToBytesLE(header.count));
    bytes.push.apply(bytes, u16ToBytesLE(header.nextSeq));
    return Uint8Array.from(bytes);
  }

  function rtcCalculateHeaderCrc(header) {
    return crc32Compute(rtcHeaderToBytes(header), 0);
  }

  function rtcSyncHeaderCrc(rtc) {
    rtc.header.headerCrc = rtcCalculateHeaderCrc(rtc.header);
  }

  function rtcSetSlotFromRecord(slot, record, seq) {
    slot.magic = CONSTANTS.RTC_RECORD_MAGIC;
    slot.seq = clampU16(seq);
    slot.timestamp = clampU32(record.timestamp);
    slot.temp10 = clampS16(record.temp10);
    slot.hum10 = clampS16(record.hum10);
    slot.lux = clampU16(record.lux);
    slot.rssi = clampS16(record.rssi);
    slot.crc = rtcCalculateSlotCrc(slot);
  }

  function rtcRecordFromSlot(slot) {
    return cloneRecord(slot);
  }

  function rtcIsSlotValid(slot) {
    if (!slot) return false;
    if (clampU16(slot.magic) !== CONSTANTS.RTC_RECORD_MAGIC) return false;
    return (clampU32(slot.crc) >>> 0) === (rtcCalculateSlotCrc(slot) >>> 0);
  }

  function rtcCountSlotStatus(rtc) {
    var valid = 0;
    var corrupt = 0;
    var total = REDACTED
    for (var i = 0; i < total; i++) {
      var slot = rtc.slots[i];
      if ((slot.magic >>> 0) === 0 && (slot.timestamp >>> 0) === 0 && (slot.crc >>> 0) === 0) {
        continue;
      }
      if (rtcIsSlotValid(slot)) {
        valid++;
      } else {
        corrupt++;
      }
    }
    return { valid: valid, corrupt: corrupt };
  }

  function rtcSeqBefore(a, b) {
    if ((a & 0xffff) === (b & 0xffff)) return false;
    return (((b - a) & 0xffff) >>> 0) < 0x8000;
  }

  function rtcSortBySeq(a, b) {
    if ((a.seq & 0xffff) === (b.seq & 0xffff)) return 0;
    return rtcSeqBefore(a.seq, b.seq) ? -1 : 1;
  }

  function rtcBoundaryValid(header, config) {
    return !(
      header.count < 0 ||
      header.count > config.rtcMaxRecords ||
      header.head < 0 ||
      header.head >= config.rtcMaxRecords ||
      header.tail < 0 ||
      header.tail >= config.rtcMaxRecords
    );
  }

  function rtcLegacyCrcValid(legacy) {
    if (!legacy || !legacy.records) return false;
    var crc = 0;
    crc = crc32Compute(Uint8Array.from(u32ToBytesLE(legacy.magic)), crc);
    crc = crc32Compute(Uint8Array.from(u16ToBytesLE(legacy.head)), crc);
    crc = crc32Compute(Uint8Array.from(u16ToBytesLE(legacy.tail)), crc);
    crc = crc32Compute(Uint8Array.from(u16ToBytesLE(legacy.count)), crc);
    crc = crc32Compute(Uint8Array.from(u16ToBytesLE(legacy.padding || 0)), crc);
    for (var i = 0; i < legacy.records.length; i++) {
      crc = crc32Compute(rtcRecordToBytes(legacy.records[i]), crc);
    }
    return (crc >>> 0) === (clampU32(legacy.crc) >>> 0);
  }

  function rtcTryMigrateLegacy(ctx, steps) {
    var legacy = ctx && ctx.rtc ? ctx.rtc.legacyV1 : null;
    if (!legacy) return false;
    if ((legacy.magic >>> 0) !== CONSTANTS.RTC_SENSOR_MAGIC) return false;
    if (!Array.isArray(legacy.records) || legacy.records.length === 0) return false;
    var legacyMax = legacy.records.length;
    if (
      legacy.count < 0 ||
      legacy.count > legacyMax ||
      legacy.head < 0 ||
      legacy.head >= legacyMax ||
      legacy.tail < 0 ||
      legacy.tail >= legacyMax
    ) {
      return false;
    }
    if (!rtcLegacyCrcValid(legacy)) {
      return false;
    }

    var available = Math.min(legacy.count, legacyMax);
    var importCount = Math.min(available, ctx.config.rtcMaxRecords);
    var startOffset = available > importCount ? available - importCount : 0;
    rtcClearInternal(ctx);
    for (var i = 0; i < importCount; i++) {
      var srcIndex = (legacy.tail + startOffset + i) % legacyMax;
      rtcSetSlotFromRecord(ctx.rtc.slots[i], legacy.records[srcIndex], i);
    }
    ctx.rtc.header.count = importCount;
    ctx.rtc.header.tail = 0;
    ctx.rtc.header.head = importCount % ctx.config.rtcMaxRecords;
    ctx.rtc.header.nextSeq = importCount & 0xffff;
    rtcSyncHeaderCrc(ctx.rtc);
    ctx.rtc.legacyV1 = null;
    if (steps) {
      pushStatus(
        steps,
        "warn",
        "Migrasi RTC v1 -> v2",
        "Legacy valid terdeteksi, diimpor " + importCount + " record."
      );
    }
    return true;
  }

  function rtcSalvageFromCurrentSlots(ctx, steps) {
    var slots = ctx.rtc.slots || [];
    var valid = [];
    for (var i = 0; i < slots.length; i++) {
      if (!rtcIsSlotValid(slots[i])) continue;
      valid.push(cloneRtcSlot(slots[i]));
    }

    if (valid.length === 0) {
      rtcClearInternal(ctx);
      if (steps) {
        pushStatus(steps, "warn", "Salvage RTC", "Tidak ada slot valid, RTC di-reset fresh.");
      }
      return true;
    }

    valid.sort(rtcSortBySeq);
    var dedup = [];
    for (var j = 0; j < valid.length; j++) {
      if (dedup.length > 0 && (dedup[dedup.length - 1].seq & 0xffff) === (valid[j].seq & 0xffff)) {
        continue;
      }
      dedup.push(valid[j]);
    }
    valid = dedup;

    var keep = Math.min(valid.length, ctx.config.rtcMaxRecords);
    rtcClearInternal(ctx);
    for (var k = 0; k < keep; k++) {
      var src = valid[k];
      rtcSetSlotFromRecord(ctx.rtc.slots[k], src, src.seq);
    }
    ctx.rtc.header.count = keep;
    ctx.rtc.header.tail = 0;
    ctx.rtc.header.head = keep % ctx.config.rtcMaxRecords;
    ctx.rtc.header.nextSeq = keep > 0 ? ((valid[keep - 1].seq + 1) & 0xffff) : 0;
    rtcSyncHeaderCrc(ctx.rtc);
    if (steps) {
      pushStatus(
        steps,
        "warn",
        "Salvage RTC",
        "Header rusak, " + keep + " slot valid berhasil direbuild by-seq."
      );
    }
    return true;
  }

  function rtcSanitizeFront(ctx, budgetSlots, steps) {
    var header = ctx.rtc.header;
    if (header.count <= 0) {
      header.head = 0;
      header.tail = 0;
      rtcSyncHeaderCrc(ctx.rtc);
      return CACHE_ERROR.NONE;
    }

    var removed = 0;
    while (header.count > 0 && removed < budgetSlots) {
      var tail = header.tail;
      var slot = ctx.rtc.slots[tail];
      if (rtcIsSlotValid(slot)) {
        break;
      }
      if (steps) {
        pushStatus(
          steps,
          "warn",
          "Drop slot korup",
          "slot[" + tail + "] invalid (magic/crc), dibuang dari antrian RTC."
        );
      }
      ctx.rtc.slots[tail] = createEmptyRtcSlot();
      header.tail = (header.tail + 1) % ctx.config.rtcMaxRecords;
      header.count--;
      removed++;
    }

    if (header.count === 0) {
      header.head = 0;
      header.tail = 0;
    }
    rtcSyncHeaderCrc(ctx.rtc);

    if (removed > 0) {
      if (header.count > 0 && !rtcIsSlotValid(ctx.rtc.slots[header.tail])) {
        return CACHE_ERROR.SCANNING;
      }
      return CACHE_ERROR.CORRUPT_DATA;
    }

    if (header.count > 0 && !rtcIsSlotValid(ctx.rtc.slots[header.tail])) {
      return CACHE_ERROR.SCANNING;
    }

    return CACHE_ERROR.NONE;
  }

  function rtcLoadAndHeal(ctx, steps) {
    var validation = rtcValidate(ctx.rtc, ctx.config);
    if (validation.ok) {
      return rtcSanitizeFront(ctx, CONSTANTS.RTC_RECOVERY_BUDGET_SLOTS, steps);
    }

    if (rtcTryMigrateLegacy(ctx, steps)) {
      return CACHE_ERROR.CORRUPT_DATA;
    }

    if (rtcSalvageFromCurrentSlots(ctx, steps)) {
      return CACHE_ERROR.CORRUPT_DATA;
    }

    rtcClearInternal(ctx);
    if (steps) {
      pushStatus(steps, "fail", "Recovery RTC", "Recovery gagal, fallback reset fresh.");
    }
    return CACHE_ERROR.CORRUPT_DATA;
  }

  function rtcValidate(rtc, config) {
    var header = rtc.header || {};
    var calc = rtcCalculateHeaderCrc(header);
    if ((header.blockMagic >>> 0) !== CONSTANTS.RTC_SENSOR_MAGIC) {
      return {
        ok: false,
        reason: "MAGIC_INVALID",
        detail:
          "Header magic RTC tidak valid: expected " +
          hex(CONSTANTS.RTC_SENSOR_MAGIC, 8) +
          ", got " +
          hex(header.blockMagic >>> 0, 8),
        calcCrc: calc,
      };
    }
    if ((header.version >>> 0) !== CONSTANTS.RTC_LAYOUT_VERSION) {
      return {
        ok: false,
        reason: "VERSION_MISMATCH",
        detail:
          "Version header RTC invalid: expected " +
          CONSTANTS.RTC_LAYOUT_VERSION +
          ", got " +
          (header.version >>> 0),
        calcCrc: calc,
      };
    }
    if ((header.maxRecords >>> 0) !== (config.rtcMaxRecords >>> 0)) {
      return {
        ok: false,
        reason: "MAX_RECORD_MISMATCH",
        detail:
          "maxRecords mismatch: expected " +
          config.rtcMaxRecords +
          ", got " +
          (header.maxRecords >>> 0),
        calcCrc: calc,
      };
    }
    if (!rtcBoundaryValid(header, config)) {
      return {
        ok: false,
        reason: "BOUNDARY_CORRUPT",
        detail:
          "Boundary rusak: head=" +
          header.head +
          ", tail=" +
          header.tail +
          ", count=" +
          header.count +
          ", max=" +
          config.rtcMaxRecords,
        calcCrc: calc,
      };
    }
    if ((header.headerCrc >>> 0) !== (calc >>> 0)) {
      return {
        ok: false,
        reason: "HEADER_CRC_MISMATCH",
        detail:
          "Header CRC mismatch: stored=" +
          hex(header.headerCrc >>> 0, 8) +
          ", calc=" +
          hex(calc >>> 0, 8),
        calcCrc: calc,
      };
    }
    return {
      ok: true,
      reason: "OK",
      detail: "RTC header valid (magic + version + boundary + header CRC).",
      calcCrc: calc,
    };
  }

  function rtcClearInternal(ctx) {
    var max = ctx.config.rtcMaxRecords;
    ctx.rtc.header.blockMagic = CONSTANTS.RTC_SENSOR_MAGIC;
    ctx.rtc.header.version = CONSTANTS.RTC_LAYOUT_VERSION;
    ctx.rtc.header.maxRecords = max;
    ctx.rtc.header.head = 0;
    ctx.rtc.header.tail = 0;
    ctx.rtc.header.count = 0;
    ctx.rtc.header.nextSeq = 0;
    ctx.rtc.legacyV1 = null;
    for (var i = 0; i < ctx.config.rtcMaxRecords; i++) {
      ctx.rtc.slots[i] = createEmptyRtcSlot();
    }
    rtcSyncHeaderCrc(ctx.rtc);
  }

  function rtcPeekRecord(ctx) {
    var header = ctx.rtc.header;
    if (header.count <= 0) return null;
    var slot = ctx.rtc.slots[header.tail];
    if (!rtcIsSlotValid(slot)) return null;
    return rtcRecordFromSlot(slot);
  }

  function rtcPopRecordInternal(ctx) {
    var header = ctx.rtc.header;
    if (header.count <= 0) return null;
    var tail = header.tail;
    var slot = ctx.rtc.slots[tail];
    if (!rtcIsSlotValid(slot)) return null;
    var out = rtcRecordFromSlot(slot);
    ctx.rtc.slots[tail] = createEmptyRtcSlot();
    header.tail = (header.tail + 1) % ctx.config.rtcMaxRecords;
    header.count--;
    if (header.count === 0) {
      header.head = 0;
      header.tail = 0;
    }
    rtcSyncHeaderCrc(ctx.rtc);
    return out;
  }

  function fsHeaderToBytes(header) {
    var bytes = [];
    bytes.push.apply(bytes, u32ToBytesLE(header.magic));
    bytes.push.apply(bytes, u32ToBytesLE(header.head));
    bytes.push.apply(bytes, u32ToBytesLE(header.tail));
    bytes.push.apply(bytes, u32ToBytesLE(header.size));
    bytes.push.apply(bytes, u16ToBytesLE(header.version));
    return Uint8Array.from(bytes);
  }

  function fsCalculateHeaderCrc(header) {
    return crc32Compute(fsHeaderToBytes(header), 0);
  }

  function fsNormalizePos(pos, config) {
    var start = CONSTANTS.FS_DATA_START;
    var size = config.fsMaxBytes;
    if (!Number.isFinite(pos)) return start;
    if (pos < start) pos = start;
    return start + ((pos - start) % size);
  }

  function fsPosToIndex(pos, config) {
    return fsNormalizePos(pos, config) - CONSTANTS.FS_DATA_START;
  }

  function fsReadBytes(fsState, pos, length, config) {
    var out = new Uint8Array(Math.max(0, length));
    for (var i = 0; i < out.length; i++) {
      var idx = fsPosToIndex(pos + i, config);
      out[i] = fsState.data[idx];
    }
    return out;
  }

  function fsWriteBytes(fsState, pos, bytes, config) {
    for (var i = 0; i < bytes.length; i++) {
      var idx = fsPosToIndex(pos + i, config);
      fsState.data[idx] = bytes[i] & 0xff;
    }
  }

  function fsReadU16(fsState, pos, config) {
    var b = fsReadBytes(fsState, pos, 2, config);
    return (b[0] | (b[1] << 8)) >>> 0;
  }

  function fsReadU32(fsState, pos, config) {
    var b = fsReadBytes(fsState, pos, 4, config);
    return (b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24)) >>> 0;
  }

  function fsWriteU16(fsState, pos, value, config) {
    fsWriteBytes(fsState, pos, Uint8Array.from(u16ToBytesLE(value)), config);
  }

  function fsWriteU32(fsState, pos, value, config) {
    fsWriteBytes(fsState, pos, Uint8Array.from(u32ToBytesLE(value)), config);
  }

  function fsAdvanceTail(fsState, amount, config) {
    var move = Math.max(0, Math.min(fsState.header.size, amount | 0));
    if (move === 0) return;
    fsState.header.tail = fsNormalizePos(fsState.header.tail + move, config);
    fsState.header.size -= move;
    if (fsState.header.size === 0) {
      fsState.header.head = CONSTANTS.FS_DATA_START;
      fsState.header.tail = CONSTANTS.FS_DATA_START;
    }
    fsState.header.crc = fsCalculateHeaderCrc(fsState.header);
  }

  function fsAdvanceHead(fsState, amount, config) {
    var move = Math.max(0, amount | 0);
    if (move === 0) return;
    fsState.header.head = fsNormalizePos(fsState.header.head + move, config);
    fsState.header.size += move;
    if (fsState.header.size > config.fsMaxBytes) {
      fsState.header.size = config.fsMaxBytes;
    }
    fsState.header.crc = fsCalculateHeaderCrc(fsState.header);
  }

  function fsRecordTotalSize(length) {
    return 2 + 2 + length + 4;
  }

  function fsPerformSyncScan(ctx, budgetBytes, steps) {
    var fsState = ctx.fs;
    var config = ctx.config;
    var scanned = 0;
    var unlimited = budgetBytes === 0;
    var low = CONSTANTS.FS_RECORD_MAGIC & 0xff;
    var high = (CONSTANTS.FS_RECORD_MAGIC >>> 8) & 0xff;

    while (fsState.header.size > 2) {
      if (!unlimited && scanned >= budgetBytes) {
        return { result: "NEED_MORE", scanned: scanned };
      }
      var chunk = Math.min(fsState.header.size, 64);
      if (!unlimited) {
        var remaining = budgetBytes - scanned;
        if (remaining < 2) {
          return { result: "NEED_MORE", scanned: scanned };
        }
        var maxChunk = remaining + 1;
        if (chunk > maxChunk) chunk = maxChunk;
      }

      var buf = fsReadBytes(fsState, fsState.header.tail, chunk, config);
      if (buf.length < 2) {
        return { result: "EMPTY", scanned: scanned };
      }

      var foundOffset = -1;
      for (var i = 0; i < buf.length - 1; i++) {
        if (buf[i] === low && buf[i + 1] === high) {
          foundOffset = i;
          break;
        }
      }

      if (foundOffset >= 0) {
        if (foundOffset > 0) {
          fsAdvanceTail(fsState, foundOffset, config);
        }
        if (steps) {
          pushStatus(
            steps,
            "info",
            "Sync scan menemukan magic",
            "Offset +" + foundOffset + " byte dari tail sebelumnya."
          );
        }
        return { result: "FOUND", scanned: scanned + foundOffset, offset: foundOffset };
      }

      var step = Math.max(1, buf.length - 1);
      fsAdvanceTail(fsState, step, config);
      scanned += step;
    }

    if (fsState.header.size > 0) {
      fsAdvanceTail(fsState, fsState.header.size, config);
    }
    return { result: "EMPTY", scanned: scanned };
  }

  function fsTrimForWrite(ctx, totalLen, steps) {
    var fsState = ctx.fs;
    var config = ctx.config;
    if (fsState.header.size + totalLen <= REDACTED
      return true;
    }

    var bytesNeeded = fsState.header.size + totalLen - config.fsMaxBytes;
    var bytesTrimmed = 0;

    while (bytesNeeded > 0 && fsState.header.size > 0) {
      var magic = fsReadU16(fsState, fsState.header.tail, config);
      if (magic !== CONSTANTS.FS_RECORD_MAGIC) {
        pushStatus(
          steps,
          "info",
          "Trim mendeteksi sync loss",
          "Magic " + hex(magic, 4) + " tidak valid. Menjalankan sync scan."
        );
        var scan = fsPerformSyncScan(ctx, CONSTANTS.SYNC_SCAN_BUDGET_BYTES, steps);
        if (scan.result === "NEED_MORE") {
          pushStatus(steps, "fail", "Trim gagal", "Budget scan habis saat trim.");
          return false;
        }
        if (scan.result !== "FOUND") {
          continue;
        }
        bytesNeeded = Math.max(0, fsState.header.size + totalLen - config.fsMaxBytes);
        continue;
      }

      var recordLen = fsReadU16(fsState, fsState.header.tail + 2, config);
      if (recordLen === 0 || recordLen > config.maxPayloadSize) {
        fsAdvanceTail(fsState, 1, config);
        bytesTrimmed += 1;
        bytesNeeded = Math.max(0, bytesNeeded - 1);
        pushStatus(
          steps,
          "info",
          "Trim skip byte korup",
          "Length record tidak valid (" + recordLen + "), tail digeser 1 byte."
        );
        if (bytesTrimmed >= CONSTANTS.TRIM_BUDGET_BYTES) {
          pushStatus(steps, "fail", "Trim gagal", "Melewati batas trim budget.");
          return false;
        }
        continue;
      }

      var totalRecord = REDACTED
      fsAdvanceTail(fsState, totalRecord, config);
      bytesTrimmed += totalRecord;
      bytesNeeded = Math.max(0, bytesNeeded - totalRecord);
      pushStatus(
        steps,
        "info",
        "Trim record lama",
        "REDACTED" + totalRecord + "REDACTED"
      );
      if (bytesTrimmed >= CONSTANTS.TRIM_BUDGET_BYTES && bytesNeeded > 0) {
        pushStatus(steps, "fail", "Trim gagal", "Melewati batas trim budget.");
        return false;
      }
    }

    return bytesNeeded <= 0;
  }

  function buildItem(status, title, detail) {
    return {
      status: status,
      title: title,
      detail: detail,
    };
  }

  function fsWriteRecordInternal(ctx, payloadBytes, steps) {
    var fsState = ctx.fs;
    var config = ctx.config;
    var length = payloadBytes.length;
    var total = REDACTED

    if (length > config.maxPayloadSize) {
      pushStatus(
        steps,
        "fail",
        "Write LittleFS ditolak",
        "Payload " + length + " byte melebihi MAX_PAYLOAD_SIZE=" + config.maxPayloadSize + "."
      );
      return {
        ok: false,
        error: CACHE_ERROR.OUT_OF_MEMORY,
      };
    }

    if (!fsTrimForWrite(ctx, total, steps)) {
      return {
        ok: false,
        error: CACHE_ERROR.OUT_OF_MEMORY,
      };
    }

    var payloadCrc = crc32Compute(payloadBytes, 0);
    var cursor = fsState.header.head;
    fsWriteU16(fsState, cursor, CONSTANTS.FS_RECORD_MAGIC, config);
    cursor += 2;
    fsWriteU16(fsState, cursor, length, config);
    cursor += 2;
    fsWriteBytes(fsState, cursor, payloadBytes, config);
    cursor += length;
    fsWriteU32(fsState, cursor, payloadCrc, config);

    fsAdvanceHead(fsState, total, config);
    pushStatus(
      steps,
      "REDACTED",
      "Write record LittleFS",
      "len=" + length + " byte | crc=" + hex(payloadCrc, 8) + " | total=" + total + " byte."
    );

    ctx.lastCrcDetail =
      "Write record\n" +
      "- payload_hex: " +
      bytesToHex(payloadBytes, 160) +
      "\n" +
      "- payload_text: " +
      JSON.stringify(bytesToUtf8(payloadBytes)) +
      "\n" +
      "- stored_crc: " +
      hex(payloadCrc, 8) +
      "\n" +
      "- record_layout: magic(0xA55A) + len(u16) + payload + crc32";

    return {
      ok: true,
      error: CACHE_ERROR.NONE,
      storedCrc: payloadCrc,
      length: length,
      totalLen: REDACTED
    };
  }

  function parseRecordAtTail(ctx) {
    var fsState = ctx.fs;
    var config = ctx.config;
    if (fsState.header.size < 8) {
      return null;
    }
    var magic = fsReadU16(fsState, fsState.header.tail, config);
    var length = fsReadU16(fsState, fsState.header.tail + 2, config);
    var payload = fsReadBytes(fsState, fsState.header.tail + 4, length, config);
    var storedCrc = fsReadU32(fsState, fsState.header.tail + 4 + length, config);
    var calc = crc32Compute(payload, 0);
    return {
      magic: magic,
      length: length,
      payload: payload,
      storedCrc: storedCrc,
      calculatedCrc: calc,
      total: REDACTED
    };
  }

  function fsPopOneInternal(ctx, steps, fromReadFlow) {
    var fsState = ctx.fs;
    var config = ctx.config;

    if (fsState.header.size === 0) {
      if (!fromReadFlow) {
        pushStatus(steps, "info", "Pop LittleFS", "Cache sudah kosong.");
      }
      return true;
    }

    var magic = fsReadU16(fsState, fsState.header.tail, config);
    if (magic !== CONSTANTS.FS_RECORD_MAGIC) {
      pushStatus(
        steps,
        "info",
        "Pop mendeteksi sync loss",
        "Magic tail=" + hex(magic, 4) + ", mencoba scan sinkronisasi."
      );
      var scan = fsPerformSyncScan(ctx, CONSTANTS.SYNC_SCAN_BUDGET_BYTES, steps);
      if (scan.result !== "FOUND") {
        return true;
      }
      magic = fsReadU16(fsState, fsState.header.tail, config);
      if (magic !== CONSTANTS.FS_RECORD_MAGIC) {
        return true;
      }
    }

    var recordLen = fsReadU16(fsState, fsState.header.tail + 2, config);
    if (recordLen > config.maxPayloadSize + CONSTANTS.RECORD_LEN_TOLERANCE) {
      fsAdvanceTail(fsState, 1, config);
      pushStatus(
        steps,
        "info",
        "Pop skip 1 byte",
        "record_len tidak masuk akal (" + recordLen + "), tail digeser 1 byte."
      );
      return true;
    }

    var total = REDACTED
    fsAdvanceTail(fsState, total, config);
    if (!fromReadFlow) {
      pushStatus(steps, "REDACTED", "REDACTED", "REDACTED" + total + "REDACTED");
    }
    return true;
  }

  function fsReadOneInternal(ctx, bufferSize, steps) {
    var fsState = ctx.fs;
    var config = ctx.config;

    if (fsState.pendingScanningTicks > 0) {
      fsState.pendingScanningTicks--;
      fsState.lastRead = {
        error: CACHE_ERROR.SCANNING,
        payloadText: "",
        payloadHex: "",
        storedCrc: null,
        calculatedCrc: null,
        salvaged: false,
        len: 0,
      };
      pushStatus(
        steps,
        "info",
        "Read LittleFS",
        "Sync-loss aktif. Sistem masih scanning pointer (SCANNING)."
      );
      return {
        ok: false,
        error: CACHE_ERROR.SCANNING,
      };
    }

    if (fsState.header.size === 0) {
      fsState.lastRead = {
        error: CACHE_ERROR.CACHE_EMPTY,
        payloadText: "",
        payloadHex: "",
        storedCrc: null,
        calculatedCrc: null,
        salvaged: false,
        len: 0,
      };
      pushStatus(steps, "info", "Read LittleFS", "Cache kosong.");
      return {
        ok: false,
        error: CACHE_ERROR.CACHE_EMPTY,
      };
    }

    var magic = fsReadU16(fsState, fsState.header.tail, config);
    if (magic !== CONSTANTS.FS_RECORD_MAGIC) {
      pushStatus(
        steps,
        "fail",
        "Magic record invalid",
        "Magic tail=" + hex(magic, 4) + ", expected=" + hex(CONSTANTS.FS_RECORD_MAGIC, 4) + "."
      );

      var presumedLen = fsReadU16(fsState, fsState.header.tail + 2, config);
      if (presumedLen > 0 && presumedLen <= config.maxPayloadSize && presumedLen <= bufferSize) {
        var payloadCandidate = fsReadBytes(fsState, fsState.header.tail + 4, presumedLen, config);
        var storedCandidate = fsReadU32(fsState, fsState.header.tail + 4 + presumedLen, config);
        var calcCandidate = crc32Compute(payloadCandidate, 0);
        if ((storedCandidate >>> 0) === (calcCandidate >>> 0)) {
          fsState.lastRead = {
            error: CACHE_ERROR.NONE,
            payloadText: bytesToUtf8(payloadCandidate),
            payloadHex: bytesToHex(payloadCandidate, 240),
            storedCrc: storedCandidate >>> 0,
            calculatedCrc: calcCandidate >>> 0,
            salvaged: true,
            len: presumedLen,
          };
          pushStatus(
            steps,
            "REDACTED",
            "Deep recovery salvage",
            "Magic rusak tapi CRC payload cocok. Record tetap terbaca."
          );
          ctx.lastCrcDetail =
            "Read salvage\n" +
            "- reason: magic_corrupt\n" +
            "- len: " +
            presumedLen +
            "\n" +
            "- payload_hex: " +
            bytesToHex(payloadCandidate, 160) +
            "\n" +
            "- stored_crc: " +
            hex(storedCandidate, 8) +
            "\n" +
            "- calculated_crc: " +
            hex(calcCandidate, 8);
          return {
            ok: true,
            error: CACHE_ERROR.NONE,
            salvaged: true,
          };
        }
      }

      var scan = fsPerformSyncScan(ctx, CONSTANTS.SYNC_SCAN_BUDGET_BYTES, steps);
      if (scan.result === "FOUND") {
        fsState.lastRead = {
          error: CACHE_ERROR.CORRUPT_DATA,
          payloadText: "",
          payloadHex: "",
          storedCrc: null,
          calculatedCrc: null,
          salvaged: false,
          len: 0,
        };
        pushStatus(steps, "fail", "Read gagal", "Data korup, pointer dipindah ke magic valid berikutnya.");
        return {
          ok: false,
          error: CACHE_ERROR.CORRUPT_DATA,
        };
      }
      if (scan.result === "NEED_MORE") {
        fsState.lastRead = {
          error: CACHE_ERROR.SCANNING,
          payloadText: "",
          payloadHex: "",
          storedCrc: null,
          calculatedCrc: null,
          salvaged: false,
          len: 0,
        };
        pushStatus(steps, "info", "Read menunggu", "Scan budget belum cukup, status SCANNING.");
        return {
          ok: false,
          error: CACHE_ERROR.SCANNING,
        };
      }
      fsState.lastRead = {
        error: CACHE_ERROR.CACHE_EMPTY,
        payloadText: "",
        payloadHex: "",
        storedCrc: null,
        calculatedCrc: null,
        salvaged: false,
        len: 0,
      };
      pushStatus(steps, "info", "Read selesai", "Tidak ada record valid setelah scanning.");
      return {
        ok: false,
        error: CACHE_ERROR.CACHE_EMPTY,
      };
    }

    var recordLen = fsReadU16(fsState, fsState.header.tail + 2, config);
    if (recordLen === 0 || recordLen > config.maxPayloadSize) {
      pushStatus(
        steps,
        "fail",
        "Length record invalid",
        "record_len=" + recordLen + ", record dibuang via pop_one."
      );
      fsPopOneInternal(ctx, steps, true);
      fsState.lastRead = {
        error: CACHE_ERROR.CORRUPT_DATA,
        payloadText: "",
        payloadHex: "",
        storedCrc: null,
        calculatedCrc: null,
        salvaged: false,
        len: 0,
      };
      return {
        ok: false,
        error: CACHE_ERROR.CORRUPT_DATA,
      };
    }

    if (recordLen > bufferSize) {
      pushStatus(
        steps,
        "fail",
        "Buffer read terlalu kecil",
        "Butuh " + recordLen + " byte, tersedia " + bufferSize + " byte."
      );
      fsState.lastRead = {
        error: CACHE_ERROR.OUT_OF_MEMORY,
        payloadText: "",
        payloadHex: "",
        storedCrc: null,
        calculatedCrc: null,
        salvaged: false,
        len: 0,
      };
      return {
        ok: false,
        error: CACHE_ERROR.OUT_OF_MEMORY,
      };
    }

    var payload = fsReadBytes(fsState, fsState.header.tail + 4, recordLen, config);
    var stored = fsReadU32(fsState, fsState.header.tail + 4 + recordLen, config);
    var calc = crc32Compute(payload, 0);
    if ((stored >>> 0) !== (calc >>> 0)) {
      pushStatus(
        steps,
        "fail",
        "CRC mismatch",
        "stored=" + hex(stored, 8) + ", calc=" + hex(calc, 8) + ". Record dipop."
      );
      fsPopOneInternal(ctx, steps, true);
      fsState.lastRead = {
        error: CACHE_ERROR.CORRUPT_DATA,
        payloadText: "",
        payloadHex: bytesToHex(payload, 240),
        storedCrc: stored >>> 0,
        calculatedCrc: calc >>> 0,
        salvaged: false,
        len: recordLen,
      };
      ctx.lastCrcDetail =
        "Read CRC mismatch\n" +
        "- payload_hex: " +
        bytesToHex(payload, 160) +
        "\n" +
        "- stored_crc: " +
        hex(stored, 8) +
        "\n" +
        "- calculated_crc: " +
        hex(calc, 8);
      return {
        ok: false,
        error: CACHE_ERROR.CORRUPT_DATA,
      };
    }

    fsState.lastRead = {
      error: CACHE_ERROR.NONE,
      payloadText: bytesToUtf8(payload),
      payloadHex: bytesToHex(payload, 240),
      storedCrc: stored >>> 0,
      calculatedCrc: calc >>> 0,
      salvaged: false,
      len: recordLen,
    };
    ctx.lastCrcDetail =
      "Read record\n" +
      "- payload_hex: " +
      bytesToHex(payload, 160) +
      "\n" +
      "- payload_text: " +
      JSON.stringify(fsState.lastRead.payloadText) +
      "\n" +
      "- stored_crc: " +
      hex(stored, 8) +
      "\n" +
      "- calculated_crc: " +
      hex(calc, 8);
    pushStatus(
      steps,
      "REDACTED",
      "Read record berhasil",
      "len=" + recordLen + " byte | crc=" + hex(calc, 8) + "."
    );
    return {
      ok: true,
      error: CACHE_ERROR.NONE,
    };
  }

  function formatRtcRecordText(record) {
    return (
      "ts=" +
      (record.timestamp >>> 0) +
      "|temp10=" +
      clampS16(record.temp10) +
      "|hum10=" +
      clampS16(record.hum10) +
      "|lux=" +
      clampU16(record.lux) +
      "|rssi=" +
      clampS16(record.rssi)
    );
  }

  function persistRecordRtcThenFs(ctx, record, steps, options) {
    var opts = options || {};
    var rtcRetries = Number.isFinite(opts.rtcRetries) ? Math.max(1, opts.rtcRetries | 0) : 3;
    var fsRetries = Number.isFinite(opts.fsRetries) ? Math.max(1, opts.fsRetries | 0) : 2;
    var forceRtcFail = !!opts.forceRtcFail;
    var forceFsFail = !!opts.forceFsFail;

    for (var i = 1; i <= rtcRetries; i++) {
      pushStatus(steps, "info", "RTC append retry", "Percobaan ke-" + i + " dari " + rtcRetries + ".");
      if (forceRtcFail) {
        pushStatus(steps, "fail", "RTC append gagal (simulasi)", "I/O RTC dipaksa gagal.");
        continue;
      }
      var rtcAttempt = rtcAppendOne(ctx, record, steps);
      if (rtcAttempt.ok) {
        pushStatus(steps, "REDACTED", "REDACTED", "REDACTED");
        return { ok: true, path: "rtc", error: CACHE_ERROR.NONE };
      }
      pushStatus(
        steps,
        rtcAttempt.error === CACHE_ERROR.SCANNING ? "info" : "fail",
        "RTC append gagal",
        "Status: " + (rtcAttempt.error || CACHE_ERROR.FILE_READ_ERROR) + "."
      );
    }

    pushStatus(steps, "info", "Fallback ke LittleFS", "RTC gagal, mencoba simpan payload ke LittleFS.");
    var payloadText = formatRtcRecordText(record);
    var payloadBytes = utf8ToBytes(payloadText);
    for (var fsAttempt = 1; fsAttempt <= fsRetries; fsAttempt++) {
      pushStatus(steps, "info", "LittleFS write retry", "Percobaan ke-" + fsAttempt + " dari " + fsRetries + ".");
      if (forceFsFail) {
        pushStatus(steps, "fail", "LittleFS write gagal (simulasi)", "I/O flash dipaksa gagal.");
        continue;
      }
      var write = fsWriteRecordInternal(ctx, payloadBytes, steps);
      if (write.ok) {
        ctx.fs.lastRead = {
          error: CACHE_ERROR.NONE,
          payloadText: payloadText,
          payloadHex: bytesToHex(payloadBytes, 240),
          storedCrc: write.storedCrc >>> 0,
          calculatedCrc: write.storedCrc >>> 0,
          salvaged: false,
          len: payloadBytes.length,
        };
        pushStatus(steps, "REDACTED", "REDACTED", "REDACTED");
        return { ok: true, path: "littlefs", error: CACHE_ERROR.NONE };
      }
      pushStatus(
        steps,
        "fail",
        "LittleFS write gagal",
        "Status: " + (write.error || CACHE_ERROR.FILE_READ_ERROR) + "."
      );
    }

    return { ok: false, path: "none", error: CACHE_ERROR.FILE_READ_ERROR };
  }

  function drainEmergencyQueue(ctx, steps, maxRecords, options) {
    if (!ctx || !ctx.emergency || ctx.emergency.count <= 0) {
      return { drained: 0, blocked: false };
    }
    var limit = Number.isFinite(maxRecords) ? Math.max(1, maxRecords | 0) : 2;
    var drained = 0;

    pushStatus(
      steps,
      "info",
      "Drain emergency queue",
      "Mencoba persist record lama terlebih dahulu (" +
        ctx.emergency.count +
        "/" +
        ctx.emergency.capacity +
        ")."
    );

    while (drained < limit && ctx.emergency.count > 0) {
      var item = emergencyPeek(ctx);
      if (!item) {
        break;
      }
      var persisted = persistRecordRtcThenFs(ctx, item, steps, options || {});
      if (!persisted.ok) {
        pushStatus(steps, "warn", "Drain tertunda", "Storage belum siap, record emergency tetap disimpan.");
        break;
      }
      emergencyPop(ctx);
      drained++;
      ctx.emergency.drainedTotal++;
      pushStatus(
        steps,
        "REDACTED",
        "Drain sukses",
        "1 record emergency dipersist via " + persisted.path + ". Sisa: " + ctx.emergency.count + "."
      );
    }

    if (ctx.emergency.backpressure && ctx.emergency.count < ctx.emergency.capacity) {
      ctx.emergency.backpressure = false;
      pushStatus(steps, "REDACTED", "REDACTED", "REDACTED");
    } else if (ctx.emergency.count >= ctx.emergency.capacity) {
      ctx.emergency.backpressure = true;
    }

    return {
      drained: drained,
      blocked: ctx.emergency.count > 0,
    };
  }

  function flushRtcToLittleFsInternal(ctx, steps, reasonLabel) {
    var moved = 0;
    var fail = false;
    var reason = reasonLabel || "flush";
    var attempts = 0;
    var guard = Math.max(1, ctx.config.rtcMaxRecords * 4);
    while (attempts < guard) {
      attempts++;
      var status = rtcLoadAndHeal(ctx, steps);
      if (status === CACHE_ERROR.SCANNING) {
        pushStatus(steps, "info", "RTC recovery", "Masih SCANNING, flush akan retry.");
        continue;
      }
      if (status === CACHE_ERROR.CORRUPT_DATA) {
        pushStatus(steps, "warn", "RTC recovery", "Korup ditemukan dan dikoreksi, flush retry.");
        continue;
      }
      if (ctx.rtc.header.count <= 0) {
        break;
      }
      var peek = rtcPeekRecord(ctx);
      if (!peek) {
        fail = true;
        pushStatus(steps, "fail", "Flush RTC->LittleFS berhenti", "Record tail invalid saat peek.");
        break;
      }
      var payload = formatRtcRecordText(peek);
      var write = fsWriteRecordInternal(ctx, utf8ToBytes(payload), steps);
      if (!write.ok) {
        fail = true;
        pushStatus(
          steps,
          "fail",
          "Flush RTC->LittleFS berhenti",
          "Write gagal pada record ke-" + (moved + 1) + "."
        );
        break;
      }
      if (!rtcPopRecordInternal(ctx)) {
        pushStatus(steps, "warn", "Pop RTC tertunda", "Tail berubah saat recovery, flush akan retry.");
        continue;
      }
      moved++;
      pushStatus(
        steps,
        "REDACTED",
        "Flush pindah record",
        "Record RTC tail dipindahkan ke LittleFS (reason=" + reason + ")."
      );
    }
    if (attempts >= guard && ctx.rtc.header.count > 0) {
      fail = true;
      pushStatus(steps, "fail", "Guard flush aktif", "Flush dihentikan untuk mencegah loop tanpa akhir.");
    }
    return {
      moved: moved,
      failed: fail,
    };
  }

  function snapshot(ctx) {
    var rtcValidation = rtcValidate(ctx.rtc, ctx.config);
    var rtcSlotStatus = rtcCountSlotStatus(ctx.rtc);
    var fsHeaderCalc = fsCalculateHeaderCrc(ctx.fs.header);

    return {
      scaleMode: ctx.scaleMode,
      scaleLabel: ctx.config.scaleId === "firmware" ? "firmware-real" : "visual",
      rtc: {
        version: ctx.rtc.header.version,
        head: ctx.rtc.header.head,
        tail: ctx.rtc.header.tail,
        count: ctx.rtc.header.count,
        maxRecords: ctx.config.rtcMaxRecords,
        nextSeq: ctx.rtc.header.nextSeq >>> 0,
        recordMagic: CONSTANTS.RTC_RECORD_MAGIC,
        recordMagicHex: hex(CONSTANTS.RTC_RECORD_MAGIC, 4),
        magic: ctx.rtc.header.blockMagic >>> 0,
        magicHex: hex(ctx.rtc.header.blockMagic >>> 0, 8),
        headerMagic: ctx.rtc.header.blockMagic >>> 0,
        headerMagicHex: hex(ctx.rtc.header.blockMagic >>> 0, 8),
        crc: ctx.rtc.header.headerCrc >>> 0,
        crcHex: hex(ctx.rtc.header.headerCrc >>> 0, 8),
        headerCrc: ctx.rtc.header.headerCrc >>> 0,
        headerCrcHex: hex(ctx.rtc.header.headerCrc >>> 0, 8),
        calculatedCrc: rtcValidation.calcCrc >>> 0,
        calculatedCrcHex: hex(rtcValidation.calcCrc >>> 0, 8),
        headerCalculatedCrc: rtcValidation.calcCrc >>> 0,
        headerCalculatedCrcHex: hex(rtcValidation.calcCrc >>> 0, 8),
        valid: rtcValidation.ok,
        validReason: rtcValidation.reason,
        headerValid: rtcValidation.ok,
        validSlots: rtcSlotStatus.valid,
        corruptSlots: rtcSlotStatus.corrupt,
        occupancyRatio:
          ctx.config.rtcMaxRecords <= 0
            ? 0
            : Math.min(1, Math.max(0, ctx.rtc.header.count / ctx.config.rtcMaxRecords)),
        occupancyText: ctx.rtc.header.count + " / " + ctx.config.rtcMaxRecords + " record",
      },
      littlefs: {
        sizeBytes: ctx.fs.header.size,
        maxBytes: ctx.config.fsMaxBytes,
        head: ctx.fs.header.head,
        tail: ctx.fs.header.tail,
        magic: ctx.fs.header.magic >>> 0,
        magicHex: hex(ctx.fs.header.magic >>> 0, 8),
        version: ctx.fs.header.version,
        headerCrc: ctx.fs.header.crc >>> 0,
        headerCrcHex: hex(ctx.fs.header.crc >>> 0, 8),
        headerCalculatedCrc: fsHeaderCalc >>> 0,
        headerCalculatedCrcHex: hex(fsHeaderCalc >>> 0, 8),
        headerValid:
          (ctx.fs.header.magic >>> 0) === CONSTANTS.FS_HEADER_MAGIC &&
          (ctx.fs.header.crc >>> 0) === (fsHeaderCalc >>> 0),
        occupancyRatio:
          ctx.config.fsMaxBytes <= 0
            ? 0
            : Math.min(1, Math.max(0, ctx.fs.header.size / ctx.config.fsMaxBytes)),
        occupancyText: ctx.fs.header.size + " / " + ctx.config.fsMaxBytes + " byte",
      },
      emergency: {
        depth: ctx.emergency.count,
        capacity: ctx.emergency.capacity,
        head: ctx.emergency.head,
        tail: ctx.emergency.tail,
        backpressure: !!ctx.emergency.backpressure,
        enqueueTotal: REDACTED
        drainedTotal: REDACTED
        blockedSamples: ctx.emergency.blockedSamples >>> 0,
        occupancyRatio:
          ctx.emergency.capacity <= 0
            ? 0
            : Math.min(1, Math.max(0, ctx.emergency.count / ctx.emergency.capacity)),
        occupancyText: ctx.emergency.count + " / " + ctx.emergency.capacity + " record",
      },
      lastRead: {
        error: ctx.fs.lastRead.error || CACHE_ERROR.NONE,
        payloadText: ctx.fs.lastRead.payloadText || "",
        payloadHex: ctx.fs.lastRead.payloadHex || "",
        storedCrc:
          ctx.fs.lastRead.storedCrc === null || ctx.fs.lastRead.storedCrc === undefined
            ? null
            : ctx.fs.lastRead.storedCrc >>> 0,
        calculatedCrc:
          ctx.fs.lastRead.calculatedCrc === null || ctx.fs.lastRead.calculatedCrc === undefined
            ? null
            : ctx.fs.lastRead.calculatedCrc >>> 0,
        storedCrcHex:
          ctx.fs.lastRead.storedCrc === null || ctx.fs.lastRead.storedCrc === undefined
            ? "-"
            : hex(ctx.fs.lastRead.storedCrc >>> 0, 8),
        calculatedCrcHex:
          ctx.fs.lastRead.calculatedCrc === null || ctx.fs.lastRead.calculatedCrc === undefined
            ? "-"
            : hex(ctx.fs.lastRead.calculatedCrc >>> 0, 8),
        salvaged: !!ctx.fs.lastRead.salvaged,
        len: ctx.fs.lastRead.len || 0,
      },
    };
  }

  function buildResult(ctx, ok, summary, steps, overrideError) {
    var state = snapshot(ctx);
    var outputError = overrideError || (state.lastRead && state.lastRead.error) || CACHE_ERROR.NONE;
    var result = {
      ok: !!ok,
      summary: summary,
      steps: steps,
      state: state,
      error: outputError,
      crcDetail: ctx.lastCrcDetail || "-",
      presetNote: "",
    };
    ctx.lastResult = result;
    return result;
  }

  function rtcAppendOne(ctx, record, steps) {
    var loadStatus = rtcLoadAndHeal(ctx, steps);
    if (loadStatus === CACHE_ERROR.SCANNING) {
      pushStatus(steps, "info", "Recovery RTC", "RTC masih SCANNING, append ditunda.");
      return { ok: false, error: CACHE_ERROR.SCANNING };
    }
    if (loadStatus === CACHE_ERROR.CORRUPT_DATA) {
      pushStatus(steps, "info", "Recovery RTC", "Korup terdeteksi dan dikoreksi sebelum append.");
    } else {
      pushStatus(steps, "REDACTED", "REDACTED", "REDACTED");
    }

    if (ctx.rtc.header.count >= ctx.config.rtcMaxRecords) {
      pushStatus(
        steps,
        "info",
        "RTC penuh",
        "Kebijakan firmware: overwrite oldest (tail maju 1)."
      );
      var dropIdx = ctx.rtc.header.tail;
      ctx.rtc.slots[dropIdx] = createEmptyRtcSlot();
      ctx.rtc.header.tail = (ctx.rtc.header.tail + 1) % ctx.config.rtcMaxRecords;
      ctx.rtc.header.count--;
    }

    var writeIndex = ctx.rtc.header.head;
    rtcSetSlotFromRecord(ctx.rtc.slots[writeIndex], record, ctx.rtc.header.nextSeq);
    ctx.rtc.header.head = (ctx.rtc.header.head + 1) % ctx.config.rtcMaxRecords;
    ctx.rtc.header.count++;
    ctx.rtc.header.nextSeq = (ctx.rtc.header.nextSeq + 1) & 0xffff;
    rtcSyncHeaderCrc(ctx.rtc);
    var lastSeq = (ctx.rtc.header.nextSeq - 1) & 0xffff;
    pushStatus(
      steps,
      "REDACTED",
      "Append RTC",
      "Record ditulis di slot[" +
        writeIndex +
        "] seq=" +
        lastSeq +
        ". head=" +
        ctx.rtc.header.head +
        ", tail=" +
        ctx.rtc.header.tail +
        ", count=" +
        ctx.rtc.header.count +
        "."
    );
    return {
      ok: true,
      error: CACHE_ERROR.NONE,
      slot: writeIndex,
      seq: lastSeq,
    };
  }

  function actionRtcAppend(ctx, recordInput, options) {
    var steps = [];
    var record = {
      timestamp: clampU32(recordInput && recordInput.timestamp),
      temp10: clampS16(recordInput && recordInput.temp10),
      hum10: clampS16(recordInput && recordInput.hum10),
      lux: clampU16(recordInput && recordInput.lux),
      rssi: clampS16(recordInput && recordInput.rssi),
    };

    pushStatus(
      steps,
      "info",
      "Input record RTC",
      "ts=" +
        record.timestamp +
        ", temp10=" +
        record.temp10 +
        ", hum10=" +
        record.hum10 +
        ", lux=" +
        record.lux +
        ", rssi=" +
        record.rssi
    );

    drainEmergencyQueue(ctx, steps, 2, {});
    if (ctx.emergency.count >= ctx.emergency.capacity) {
      ctx.emergency.backpressure = true;
      ctx.emergency.blockedSamples++;
      pushStatus(
        steps,
        "warn",
        "Backpressure aktif",
        "Queue emergency penuh, ingest baru ditahan agar data lama tidak hilang."
      );
      return buildResult(ctx, true, "Backpressure aktif: sample baru ditahan.", steps, CACHE_ERROR.NONE);
    }

    var persisted = persistRecordRtcThenFs(ctx, record, steps, {});
    if (!persisted.ok) {
      if (emergencyEnqueue(ctx, record, steps, "jalur RTC/LittleFS gagal")) {
        return buildResult(
          ctx,
          true,
          "RTC/LittleFS gagal, record masuk Emergency Queue (no-drop).",
          steps,
          CACHE_ERROR.NONE
        );
      }
      return buildResult(
        ctx,
        true,
        "Emergency Queue penuh, backpressure aktif. Record baru ditahan.",
        steps,
        CACHE_ERROR.NONE
      );
    }

    var autoFlush = !!(options && options.autoFlush);
    if (autoFlush && ctx.rtc.header.count >= ctx.config.rtcMaxRecords) {
      pushStatus(
        steps,
        "info",
        "Auto flush aktif",
        "RTC mencapai batas penuh, memulai handoff RTC -> LittleFS."
      );
      var flush = flushRtcToLittleFsInternal(ctx, steps, "auto_full");
      pushStatus(
        steps,
        flush.failed ? "fail" : "pass",
        "Handoff selesai",
        "Record dipindah: " + flush.moved + (flush.failed ? " (dengan gagal write)." : ".")
      );
      return buildResult(
        ctx,
        !flush.failed,
        flush.failed
          ? "Persist sukses, tapi auto flush RTC->LittleFS gagal sebagian."
          : "Append RTC sukses. Auto flush memindahkan " + flush.moved + " record.",
        steps
      );
    }

    return buildResult(
      ctx,
      true,
      persisted.path === "rtc" ? "Persist sukses via RTC." : "Persist sukses via LittleFS fallback.",
      steps
    );
  }

  function actionRtcSimulateIngressFallback(ctx, scenario, recordInput) {
    var steps = [];
    var record = {
      timestamp: clampU32(recordInput && recordInput.timestamp),
      temp10: clampS16(recordInput && recordInput.temp10),
      hum10: clampS16(recordInput && recordInput.hum10),
      lux: clampU16(recordInput && recordInput.lux),
      rssi: clampS16(recordInput && recordInput.rssi),
    };

    var mode = String(scenario || "rtc_fail_to_fs");
    var forceRtcFail = mode === "rtc_fail_to_fs" || mode === "both_fail";
    var forceFsFail = mode === "both_fail";
    var rtcRetries = 3;
    var fsRetries = 2;

    pushStatus(
      steps,
      "info",
      "Simulasi ingress",
      "Mode=" +
        mode +
        ", policy: RTC retry=" +
        rtcRetries +
        ", fallback FS retry=" +
        fsRetries +
        ", ts=" +
        record.timestamp +
        "."
    );

    drainEmergencyQueue(ctx, steps, 2, {
      forceRtcFail: forceRtcFail,
      forceFsFail: forceFsFail,
      rtcRetries: rtcRetries,
      fsRetries: fsRetries,
    });

    if (ctx.emergency.count >= ctx.emergency.capacity) {
      ctx.emergency.backpressure = true;
      ctx.emergency.blockedSamples++;
      pushStatus(
        steps,
        "warn",
        "Backpressure aktif",
        "Emergency queue penuh sebelum ingest, sample baru ditahan."
      );
      return buildResult(ctx, true, "Backpressure aktif: sample baru ditahan.", steps, CACHE_ERROR.NONE);
    }

    var persisted = persistRecordRtcThenFs(ctx, record, steps, {
      forceRtcFail: forceRtcFail,
      forceFsFail: forceFsFail,
      rtcRetries: rtcRetries,
      fsRetries: fsRetries,
    });
    if (persisted.ok) {
      return buildResult(
        ctx,
        true,
        persisted.path === "rtc"
          ? "Ingress sukses via RTC setelah retry guard."
          : "RTC gagal, fallback LittleFS sukses.",
        steps,
        CACHE_ERROR.NONE
      );
    }

    pushStatus(
      steps,
      "warn",
      "No-drop path",
      "RTC dan LittleFS gagal. Record dialihkan ke Emergency Queue."
    );
    if (emergencyEnqueue(ctx, record, steps, "REDACTED")) {
      ctx.fs.lastRead = {
        error: CACHE_ERROR.NONE,
        payloadText: formatRtcRecordText(record),
        payloadHex: bytesToHex(utf8ToBytes(formatRtcRecordText(record)), 240),
        storedCrc: null,
        calculatedCrc: null,
        salvaged: false,
        len: formatRtcRecordText(record).length,
      };
      return buildResult(
        ctx,
        true,
        "RTC + LittleFS gagal. Record masuk Emergency Queue (no-drop).",
        steps,
        CACHE_ERROR.NONE
      );
    }

    ctx.fs.lastRead = {
      error: CACHE_ERROR.NONE,
      payloadText: formatRtcRecordText(record),
      payloadHex: bytesToHex(utf8ToBytes(formatRtcRecordText(record)), 240),
      storedCrc: null,
      calculatedCrc: null,
      salvaged: false,
      len: formatRtcRecordText(record).length,
    };
    return buildResult(
      ctx,
      true,
      "Emergency Queue penuh. Backpressure aktif, sample baru ditahan.",
      steps,
      CACHE_ERROR.NONE
    );
  }

  function actionRtcPeek(ctx) {
    var steps = [];
    var loadStatus = rtcLoadAndHeal(ctx, steps);
    if (loadStatus === CACHE_ERROR.SCANNING) {
      pushStatus(steps, "info", "Peek RTC", "Recovery masih berjalan (SCANNING).");
      return buildResult(ctx, false, "Peek menunggu recovery RTC.", steps, CACHE_ERROR.SCANNING);
    }
    if (loadStatus === CACHE_ERROR.CORRUPT_DATA) {
      pushStatus(steps, "warn", "Peek RTC", "Korup dikoreksi, ulangi peek untuk membaca data terbaru.");
      return buildResult(ctx, false, "Recovery RTC selesai sebagian. Ulangi peek.", steps, CACHE_ERROR.CORRUPT_DATA);
    }
    if (ctx.rtc.header.count === 0) {
      pushStatus(steps, "info", "Peek RTC", "RTC kosong.");
      ctx.fs.lastRead = {
        error: CACHE_ERROR.CACHE_EMPTY,
        payloadText: "",
        payloadHex: "",
        storedCrc: null,
        calculatedCrc: null,
        salvaged: false,
        len: 0,
      };
      return buildResult(ctx, true, "RTC kosong.", steps, CACHE_ERROR.CACHE_EMPTY);
    }
    var record = rtcPeekRecord(ctx);
    if (!record) {
      pushStatus(steps, "fail", "Peek RTC", "Tail slot invalid setelah recovery.");
      return buildResult(ctx, false, "Peek gagal: tail slot invalid.", steps, CACHE_ERROR.CORRUPT_DATA);
    }
    var payload = formatRtcRecordText(record);
    ctx.fs.lastRead = {
      error: CACHE_ERROR.NONE,
      payloadText: payload,
      payloadHex: bytesToHex(utf8ToBytes(payload), 240),
      storedCrc: null,
      calculatedCrc: null,
      salvaged: false,
      len: payload.length,
    };
    pushStatus(steps, "REDACTED", "REDACTED", payload);
    return buildResult(ctx, true, "Peek RTC berhasil.", steps);
  }

  function actionRtcPop(ctx) {
    var steps = [];
    var loadStatus = rtcLoadAndHeal(ctx, steps);
    if (loadStatus === CACHE_ERROR.SCANNING) {
      pushStatus(steps, "info", "Pop RTC", "Recovery masih berjalan (SCANNING).");
      return buildResult(ctx, false, "Pop menunggu recovery RTC.", steps, CACHE_ERROR.SCANNING);
    }
    if (loadStatus === CACHE_ERROR.CORRUPT_DATA) {
      pushStatus(steps, "warn", "Pop RTC", "Korup dikoreksi, ulangi pop.");
      return buildResult(ctx, false, "Recovery RTC selesai sebagian. Ulangi pop.", steps, CACHE_ERROR.CORRUPT_DATA);
    }
    var record = rtcPopRecordInternal(ctx);
    if (!record) {
      pushStatus(steps, "info", "Pop RTC", "RTC kosong.");
      return buildResult(ctx, true, "RTC kosong, tidak ada yang dipop.", steps, CACHE_ERROR.CACHE_EMPTY);
    }
    pushStatus(steps, "REDACTED", "REDACTED", formatRtcRecordText(record));
    return buildResult(ctx, true, "Pop RTC berhasil.", steps);
  }

  function actionRtcClear(ctx) {
    var steps = [];
    rtcClearInternal(ctx);
    pushStatus(steps, "REDACTED", "REDACTED", "REDACTED");
    return buildResult(ctx, true, "RTC dibersihkan.", steps);
  }

  function actionRtcInjectMagicInvalid(ctx) {
    var steps = [];
    if (ctx.rtc.header.count === 0) {
      rtcSetSlotFromRecord(ctx.rtc.slots[0], { timestamp: 1735689600, temp10: 250, hum10: 650, lux: 1000, rssi: -70 }, 1);
      ctx.rtc.header.tail = 0;
      ctx.rtc.header.head = 1;
      ctx.rtc.header.count = 1;
      ctx.rtc.header.nextSeq = 2;
      rtcSyncHeaderCrc(ctx.rtc);
    }
    var tail = ctx.rtc.header.tail;
    ctx.rtc.slots[tail].magic = 0x1337;
    pushStatus(
      steps,
      "info",
      "Inject RTC magic invalid",
      "magic slot tail[" + tail + "] diubah menjadi " + hex(ctx.rtc.slots[tail].magic, 4) + "."
    );
    return buildResult(ctx, true, "Inject error RTC (magic invalid) diterapkan.", steps);
  }

  function actionRtcInjectCrcMismatch(ctx) {
    var steps = [];
    if (ctx.rtc.header.count === 0) {
      rtcSetSlotFromRecord(ctx.rtc.slots[0], { timestamp: 1735689600, temp10: 251, hum10: 651, lux: 1001, rssi: -69 }, 2);
      ctx.rtc.header.tail = 0;
      ctx.rtc.header.head = 1;
      ctx.rtc.header.count = 1;
      ctx.rtc.header.nextSeq = 3;
      rtcSyncHeaderCrc(ctx.rtc);
    }
    var tail = ctx.rtc.header.tail;
    ctx.rtc.slots[tail].crc = (ctx.rtc.slots[tail].crc ^ 0x0000ffff) >>> 0;
    pushStatus(
      steps,
      "info",
      "Inject RTC CRC mismatch",
      "CRC slot tail[" + tail + "] diubah manual menjadi " + hex(ctx.rtc.slots[tail].crc, 8) + "."
    );
    return buildResult(ctx, true, "Inject error RTC (CRC mismatch) diterapkan.", steps);
  }

  function actionRtcInjectBoundaryCorrupt(ctx) {
    var steps = [];
    ctx.rtc.header.head = ctx.config.rtcMaxRecords + 3;
    ctx.rtc.header.tail = 0;
    ctx.rtc.header.count = 1;
    ctx.rtc.header.headerCrc = rtcCalculateHeaderCrc(ctx.rtc.header);
    pushStatus(
      steps,
      "info",
      "Inject RTC boundary corrupt",
      "header.head dipaksa out-of-range: " + ctx.rtc.header.head + "."
    );
    return buildResult(ctx, true, "Inject error RTC (boundary corrupt) diterapkan.", steps);
  }

  function actionFsWrite(ctx, payloadText) {
    var steps = [];
    var payload = utf8ToBytes(payloadText);
    pushStatus(steps, "info", "Input write", "payload_len=" + payload.length + " byte.");
    var result = fsWriteRecordInternal(ctx, payload, steps);
    return buildResult(
      ctx,
      result.ok,
      result.ok ? "Write LittleFS berhasil." : "Write LittleFS gagal.",
      steps,
      result.error
    );
  }

  function actionFsReadOne(ctx, bufferSize) {
    var steps = [];
    var size = Math.max(1, Number(bufferSize) || 0);
    pushStatus(steps, "info", "Read request", "buffer_size=" + size + " byte.");
    var result = fsReadOneInternal(ctx, size, steps);
    var ok = result.error === CACHE_ERROR.NONE;
    var summary = ok
      ? "Read One berhasil."
      : "Read One selesai dengan status " + result.error + ".";
    return buildResult(ctx, ok, summary, steps, result.error);
  }

  function actionFsPopOne(ctx) {
    var steps = [];
    fsPopOneInternal(ctx, steps, false);
    return buildResult(ctx, true, "Pop One LittleFS selesai.", steps);
  }

  function actionFsClear(ctx) {
    var steps = [];
    ctx.fs = createFsState(ctx.config);
    ctx.lastCrcDetail = "";
    pushStatus(steps, "REDACTED", "REDACTED", "REDACTED");
    return buildResult(ctx, true, "LittleFS dibersihkan.", steps);
  }

  function actionFsInjectCrcMismatch(ctx) {
    var steps = [];
    if (ctx.fs.header.size === 0) {
      pushStatus(
        steps,
        "info",
        "Inject CRC mismatch",
        "Cache kosong, membuat 1 record dummy terlebih dahulu."
      );
      fsWriteRecordInternal(ctx, utf8ToBytes("preset|crc_mismatch|dummy"), steps);
    }
    var parsed = parseRecordAtTail(ctx);
    if (!parsed || parsed.length <= 0 || parsed.length > ctx.config.maxPayloadSize) {
      pushStatus(steps, "fail", "Inject CRC mismatch gagal", "Record tail tidak valid untuk diubah.");
      return buildResult(ctx, false, "Inject CRC mismatch gagal.", steps, CACHE_ERROR.CORRUPT_DATA);
    }
    var crcPos = ctx.fs.header.tail + 4 + parsed.length;
    var mutated = (parsed.storedCrc ^ 0x0000ffff) >>> 0;
    fsWriteU32(ctx.fs, crcPos, mutated, ctx.config);
    pushStatus(
      steps,
      "info",
      "Inject CRC mismatch",
      "CRC record tail diubah dari " + hex(parsed.storedCrc, 8) + " menjadi " + hex(mutated, 8) + "."
    );
    return buildResult(ctx, true, "Inject CRC mismatch diterapkan.", steps);
  }

  function actionFsInjectMagicCorrupt(ctx) {
    var steps = [];
    if (ctx.fs.header.size === 0) {
      pushStatus(
        steps,
        "info",
        "Inject magic corrupt",
        "Cache kosong, membuat 1 record dummy terlebih dahulu."
      );
      fsWriteRecordInternal(ctx, utf8ToBytes("preset|magic_corrupt|dummy"), steps);
    }
    var magic = fsReadU16(ctx.fs, ctx.fs.header.tail, ctx.config);
    var mutated = (magic ^ 0x000f) & 0xffff;
    fsWriteU16(ctx.fs, ctx.fs.header.tail, mutated, ctx.config);
    pushStatus(
      steps,
      "info",
      "Inject magic corrupt",
      "Magic tail diubah dari " + hex(magic, 4) + " menjadi " + hex(mutated, 4) + "."
    );
    return buildResult(ctx, true, "Inject magic corrupt diterapkan.", steps);
  }

  function actionFsInjectSyncLoss(ctx) {
    var steps = [];
    if (ctx.fs.header.size === 0) {
      fsWriteRecordInternal(ctx, utf8ToBytes("sync-loss-1"), steps);
      fsWriteRecordInternal(ctx, utf8ToBytes("sync-loss-2"), steps);
    } else if (ctx.fs.header.size < 24) {
      fsWriteRecordInternal(ctx, utf8ToBytes("sync-loss-padding"), steps);
    }

    fsWriteU16(ctx.fs, ctx.fs.header.tail, 0x1337, ctx.config);
    if (ctx.fs.header.size > 8) {
      fsWriteBytes(ctx.fs, ctx.fs.header.tail + 2, Uint8Array.from([0x99, 0x77, 0x55]), ctx.config);
    }
    ctx.fs.pendingScanningTicks = 1;
    pushStatus(
      steps,
      "info",
      "Inject sync-loss/noise",
      "Byte awal tail dirusak, read berikutnya akan menghasilkan SCANNING terlebih dahulu."
    );
    return buildResult(ctx, true, "Inject sync-loss/noise diterapkan.", steps);
  }

  function actionFlushRtcToFs(ctx, reason) {
    var steps = [];
    if (ctx.rtc.header.count === 0) {
      pushStatus(steps, "info", "Flush RTC->LittleFS", "RTC kosong, tidak ada record untuk dipindahkan.");
      return buildResult(ctx, true, "Flush selesai: RTC kosong.", steps, CACHE_ERROR.CACHE_EMPTY);
    }
    var flush = flushRtcToLittleFsInternal(ctx, steps, reason || "manual");
    var ok = !flush.failed;
    var summary = ok
      ? "Flush RTC->LittleFS selesai. Record dipindah: " + flush.moved + "."
      : "Flush berhenti. Sebagian record gagal ditulis ke LittleFS.";
    return buildResult(ctx, ok, summary, steps, ok ? CACHE_ERROR.NONE : CACHE_ERROR.OUT_OF_MEMORY);
  }

  function actionLoadPreset(ctx, presetKey) {
    var preset = CACHE_PRESETS[presetKey];
    if (!preset) {
      return buildResult(
        ctx,
        false,
        "Preset tidak dikenal: " + presetKey,
        [buildItem("fail", "Preset invalid", "Preset tidak ditemukan.")],
        CACHE_ERROR.CORRUPT_DATA
      );
    }

    resetContext(ctx, ctx.scaleMode);
    var steps = [];
    pushStatus(steps, "info", "Load preset", "Preset aktif: " + preset.label + ".");

    if (presetKey === "normal_ok") {
      actionRtcAppend(
        ctx,
        { timestamp: 1735689600, temp10: 253, hum10: 680, lux: 1234, rssi: -67 },
        { autoFlush: false }
      );
      actionRtcAppend(
        ctx,
        { timestamp: 1735689900, temp10: 255, hum10: 675, lux: 1242, rssi: -66 },
        { autoFlush: false }
      );
      fsWriteRecordInternal(ctx, utf8ToBytes("cache|normal|ok|frame=1"), steps);
      pushStatus(steps, "REDACTED", "REDACTED", "REDACTED");
    } else if (presetKey === "crc_mismatch") {
      fsWriteRecordInternal(ctx, utf8ToBytes("cache|crc|mismatch"), steps);
      var parsedCrc = parseRecordAtTail(ctx);
      if (parsedCrc) {
        fsWriteU32(ctx.fs, ctx.fs.header.tail + 4 + parsedCrc.length, parsedCrc.storedCrc ^ 0x000fffff, ctx.config);
      }
      pushStatus(steps, "info", "Preset crc_mismatch", "Record awal valid lalu CRC diubah.");
    } else if (presetKey === "magic_corrupt_salvage") {
      fsWriteRecordInternal(ctx, utf8ToBytes("cache|magic|salvage"), steps);
      var mg = fsReadU16(ctx.fs, ctx.fs.header.tail, ctx.config);
      fsWriteU16(ctx.fs, ctx.fs.header.tail, mg ^ 0x0001, ctx.config);
      pushStatus(
        steps,
        "info",
        "Preset magic_corrupt_salvage",
        "Magic dirusak tipis, payload + CRC dipertahankan."
      );
    } else if (presetKey === "sync_loss_rescan") {
      fsWriteRecordInternal(ctx, utf8ToBytes("cache|sync|first"), steps);
      fsWriteRecordInternal(ctx, utf8ToBytes("cache|sync|second"), steps);
      fsWriteU16(ctx.fs, ctx.fs.header.tail, 0x1111, ctx.config);
      ctx.fs.pendingScanningTicks = 1;
      pushStatus(
        steps,
        "info",
        "Preset sync_loss_rescan",
        "Record pertama dibuat invalid agar read awal masuk fase SCANNING."
      );
    }

    var result = buildResult(ctx, true, "Preset " + preset.label + " dimuat.", steps);
    result.presetNote = preset.note;
    return result;
  }

  var api = {
    ERROR_ENUM: CACHE_ERROR,
    PRESETS: CACHE_PRESETS,
    SCALE_CONFIG: SCALE_CONFIG,
    CONSTANTS: CONSTANTS,
    createContext: createContext,
    resetContext: resetContext,
    snapshot: snapshot,
    loadPreset: actionLoadPreset,
    rtcAppend: actionRtcAppend,
    rtcPeek: actionRtcPeek,
    rtcPop: actionRtcPop,
    rtcClear: actionRtcClear,
    rtcInjectMagicInvalid: actionRtcInjectMagicInvalid,
    rtcInjectCrcMismatch: actionRtcInjectCrcMismatch,
    rtcInjectBoundaryCorrupt: actionRtcInjectBoundaryCorrupt,
    simulateRtcIngressFallback: actionRtcSimulateIngressFallback,
    fsWrite: actionFsWrite,
    fsReadOne: actionFsReadOne,
    fsPopOne: actionFsPopOne,
    fsClear: actionFsClear,
    fsInjectCrcMismatch: actionFsInjectCrcMismatch,
    fsInjectMagicCorrupt: actionFsInjectMagicCorrupt,
    fsInjectSyncLoss: actionFsInjectSyncLoss,
    flushRtcToLittleFs: actionFlushRtcToFs,
  };

  window.CacheCrcTrace = api;
})();
