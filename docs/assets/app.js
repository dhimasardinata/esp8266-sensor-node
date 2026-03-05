(function () {
  "use strict";

  if (typeof AesTrace === "undefined") {
    throw new Error("AesTrace library belum dimuat");
  }

  var ENCRYPT_PRESETS = {
    demo: {
      plaintext: AesTrace.DEMO_VECTOR.plaintext,
      timestamp: AesTrace.DEMO_VECTOR.timestamp,
      ivHex: AesTrace.DEMO_VECTOR.ivHex,
      mode: "terminal",
    },
    portal_cmd: {
      plaintext: "settoken GH-01|SECRET-ROTATION",
      timestamp: "now",
      ivHex: AesTrace.DEMO_VECTOR.ivHex,
      mode: "portal",
    },
    ws_long: {
      plaintext: AesTrace.LONG_PRESETS.wsStatusText,
      timestamp: "now",
      ivHex: AesTrace.DEMO_VECTOR.ivHex,
      mode: "terminal",
    },
  };

  function makeDecryptPresetPayload(plaintext, timestamp, outputMode) {
    try {
      var trace = AesTrace.encryptTrace({
        plaintext: plaintext,
        timestamp: Number(timestamp) >>> 0,
        ivHex: AesTrace.DEMO_VECTOR.ivHex,
        keyText: AesTrace.DEMO_VECTOR.key,
      });
      return outputMode === "portal"
        ? trace.outputs.portalPayload
        : trace.outputs.terminalPayload;
    } catch (error) {
      return outputMode === "portal"
        ? AesTrace.DEMO_VECTOR.expectedPortalPayload
        : AesTrace.DEMO_VECTOR.expectedTerminalPayload;
    }
  }

  function buildDecryptPresets() {
    var deviceEpoch = AesTrace.DEMO_VECTOR.timestamp >>> 0;
    var validPayloadEpoch = deviceEpoch;
    var replayPayloadEpoch = (deviceEpoch - 7200) >>> 0;
    return {
      valid: {
        payload: makeDecryptPresetPayload(
          "status=ok|mode=normal|src=valid",
          validPayloadEpoch,
          "portal"
        ),
        mode: "auto",
        deviceEpoch: deviceEpoch,
        payloadEpoch: validPayloadEpoch,
        timeSynced: true,
        skewProfile: "strict",
        skewCustom: AesTrace.WS_REPLAY_SKEW_SEC_STRICT,
        note:
          "Payload baru: timestamp payload sama dengan epoch perangkat, harus lolos replay check.",
      },
      replay_fail: {
        payload: makeDecryptPresetPayload(
          "status=ok|mode=normal|src=replay-old",
          replayPayloadEpoch,
          "portal"
        ),
        mode: "auto",
        deviceEpoch: deviceEpoch,
        payloadEpoch: replayPayloadEpoch,
        timeSynced: true,
        skewProfile: "strict",
        skewCustom: AesTrace.WS_REPLAY_SKEW_SEC_STRICT,
        note:
          "Payload lama 2 jam: timestamp payload jauh di belakang epoch perangkat, harus gagal replay check.",
      },
      invalid_format: {
        payload: "ENC:ini-bukan-format-base64-valid",
        mode: "auto",
        deviceEpoch: deviceEpoch,
        payloadEpoch: null,
        timeSynced: true,
        skewProfile: "strict",
        skewCustom: AesTrace.WS_REPLAY_SKEW_SEC_STRICT,
        note: "Format payload rusak: gagal di parse/split sebelum masuk dekripsi AES.",
      },
    };
  }

  var DECRYPT_PRESETS = buildDecryptPresets();

  var el = {
    mainTabButtons: document.querySelectorAll("[data-main-tab-target]"),
    mainTabPanels: document.querySelectorAll(".main-tab-panel"),
    aesTabButtons: document.querySelectorAll("[data-aes-tab-target]"),
    aesTabPanels: document.querySelectorAll(".aes-tab-panel"),
    encryptPreset: document.getElementById("encrypt-preset-select"),
    encryptLoadPresetBtn: document.getElementById("encrypt-load-preset-btn"),
    decryptPreset: document.getElementById("decrypt-preset-select"),
    decryptLoadPresetBtn: document.getElementById("decrypt-load-preset-btn"),
    decryptPresetHint: document.getElementById("decrypt-preset-hint"),
    encryptTraceSummary: document.getElementById("encrypt-trace-summary"),
    encryptTraceList: document.getElementById("encrypt-trace-list"),
    encryptTraceSection: document.getElementById("section-2"),
    encryptOutputSection: document.getElementById("section-3"),
    ivRandomBtn: document.getElementById("iv-random-btn"),

    plaintext: document.getElementById("plaintext-input"),
    plaintextPreviewText: document.getElementById("plaintext-preview-text"),
    plaintextPreviewHex: document.getElementById("plaintext-preview-hex"),
    timestampPreviewText: document.getElementById("timestamp-preview-text"),
    timestampPreviewHex: document.getElementById("timestamp-preview-hex"),
    payloadCombinedPreviewText: document.getElementById("payload-combined-preview-text"),
    payloadCombinedPreviewHex: document.getElementById("payload-combined-preview-hex"),
    timestamp: document.getElementById("timestamp-input"),
    timestampReadable: document.getElementById("timestamp-readable-input"),
    timestampNowBtn: document.getElementById("timestamp-now-btn"),
    ivHex: document.getElementById("iv-input"),
    forceWsChunking: document.getElementById("force-ws-chunking"),
    mainIvMicrosInput: document.getElementById("main-iv-micros-input"),
    mainIvRssiInput: document.getElementById("main-iv-rssi-input"),
    mainIvMixEnabled: document.getElementById("main-iv-mix-enabled"),
    mainIvMixToggle: document.getElementById("main-iv-mix-toggle"),
    mainIvRawPreview: document.getElementById("main-iv-raw-preview"),
    mainIvMixedPreview: document.getElementById("main-iv-mixed-preview"),
    mainIvRawText: document.getElementById("main-iv-raw-text"),
    mainIvMixedText: document.getElementById("main-iv-mixed-text"),
    mainIvMixMeta: document.getElementById("main-iv-mix-meta"),
    mainIvMixProcess: document.getElementById("main-iv-mix-process"),
    mainIvMixByteDiff: document.getElementById("main-iv-mix-byte-diff"),
    mode: document.getElementById("mode-select"),
    runBtn: document.getElementById("run-btn"),
    demoBtn: document.getElementById("demo-btn"),
    alert: document.getElementById("input-alert"),

    encEpochUtc: document.getElementById("enc-epoch-utc"),
    encEpochLocal: document.getElementById("enc-epoch-local"),
    encEpochRelative: document.getElementById("enc-epoch-relative"),

    keyOrigin: document.getElementById("key-origin"),
    keyDemoMask: document.getElementById("key-demo-mask"),
    outputCipherHex: document.getElementById("output-ciphertext-hex"),
    outputTerminal: document.getElementById("output-terminal"),
    outputPortal: document.getElementById("output-portal"),
    outputIvB64: document.getElementById("output-iv-b64"),
    outputCtB64: document.getElementById("output-ct-b64"),
    outputPrimaryActiveWrap: document.getElementById("output-primary-active-wrap"),
    outputPrimaryActiveLabel: document.getElementById("output-primary-active-label"),
    outputPrimaryActive: document.getElementById("output-primary-active"),
    outputOtherDetails: document.getElementById("output-other-details"),

    stageContextLabel: document.getElementById("stage-context-label"),
    stagePlaintext: document.getElementById("stage-plaintext"),
    stageTimestamp: document.getElementById("stage-timestamp"),
    stagePayload: document.getElementById("stage-payload"),
    stagePadded: document.getElementById("stage-padded"),
    stageMeta: document.getElementById("stage-meta"),
    cbcContextLabel: document.getElementById("cbc-context-label"),
    cbcTableBody: document.getElementById("cbc-table-body"),

    longPreset: document.getElementById("long-preset-select"),
    longPayload: document.getElementById("long-payload-input"),
    loadPresetBtn: document.getElementById("load-preset-btn"),
    runLongBtn: document.getElementById("run-long-btn"),
    longAlert: document.getElementById("long-alert"),
    metricTotalBytes: REDACTED
    metricTotalFrames: REDACTED
    metricTotalBlocks: REDACTED
    metricTotalRounds: REDACTED
    wsFramesBody: document.getElementById("ws-frames-body"),

    scenarioSelect: document.getElementById("scenario-select"),
    frameSelect: document.getElementById("frame-select"),
    blockSelect: document.getElementById("block-select"),
    roundSelect: document.getElementById("round-select"),
    roundContextLabel: document.getElementById("round-context-label"),
    roundTitle: document.getElementById("round-title"),
    roundNote: document.getElementById("round-note"),
    roundTableBody: document.getElementById("round-table-body"),
    decryptBlockSelect: document.getElementById("decrypt-block-select"),
    decryptRoundSelect: document.getElementById("decrypt-round-select"),
    decryptRoundContextLabel: document.getElementById("decrypt-round-context-label"),
    decryptRoundTitle: document.getElementById("decrypt-round-title"),
    decryptRoundNote: document.getElementById("decrypt-round-note"),
    decryptRoundTableBody: document.getElementById("decrypt-round-table-body"),

    ivRawInput: document.getElementById("iv-raw-input"),
    ivMicrosInput: document.getElementById("iv-micros-input"),
    ivRssiInput: document.getElementById("iv-rssi-input"),
    ivRunBtn: document.getElementById("iv-run-btn"),
    ivUseCurrentBtn: document.getElementById("iv-use-current-btn"),
    ivAlert: document.getElementById("iv-alert"),
    ivBeforeHex: document.getElementById("iv-before-hex"),
    ivBeforeB64: document.getElementById("iv-before-b64"),
    ivAfterHex: document.getElementById("iv-after-hex"),
    ivAfterB64: document.getElementById("iv-after-b64"),
    ivMasks: document.getElementById("iv-masks"),
    ivByteDiffBody: document.getElementById("iv-byte-diff-body"),
    useLongToMainBtn: document.getElementById("use-long-to-main-btn"),
    useIvmixToMainBtn: document.getElementById("use-ivmix-to-main-btn"),

    validationPayload: document.getElementById("validation-payload"),
    payloadTsEpoch: document.getElementById("payload-ts-epoch"),
    payloadTsUtc: document.getElementById("payload-ts-utc"),
    payloadTsLocal: document.getElementById("payload-ts-local"),
    payloadTsNote: document.getElementById("payload-ts-note"),
    validationMode: document.getElementById("validation-mode"),
    deviceEpoch: document.getElementById("device-epoch-input"),
    deviceEpochReadable: document.getElementById("device-epoch-readable-input"),
    deviceEpochNowBtn: document.getElementById("device-epoch-now-btn"),
    timeSynced: document.getElementById("time-synced-input"),
    skewProfile: document.getElementById("skew-profile"),
    skewCustomWrap: document.getElementById("skew-custom-wrap"),
    skewCustomInput: document.getElementById("skew-custom-input"),
    validateBtn: document.getElementById("validate-btn"),
    decryptTraceSection: document.getElementById("section-5"),
    decryptOutputSection: document.getElementById("section-6"),
    validationSteps: document.getElementById("validation-steps"),
    validationSummary: document.getElementById("validation-summary"),
    validationFailAnchor: document.getElementById("validation-fail-anchor"),
    valEpochUtc: document.getElementById("val-epoch-utc"),
    valEpochLocal: document.getElementById("val-epoch-local"),
    valEpochRelative: document.getElementById("val-epoch-relative"),
    decryptStageContextLabel: document.getElementById("decrypt-stage-context-label"),
    decryptStageMode: document.getElementById("decrypt-stage-mode"),
    decryptStageIv: document.getElementById("decrypt-stage-iv"),
    decryptStageCipher: document.getElementById("decrypt-stage-cipher"),
    decryptStagePadded: document.getElementById("decrypt-stage-padded"),
    decryptStageUnpadded: document.getElementById("decrypt-stage-unpadded"),
    decryptStageTimestamp: document.getElementById("decrypt-stage-timestamp"),
    decryptStagePlaintextHex: document.getElementById("decrypt-stage-plaintext-hex"),
    decryptStagePlaintextUtf8: document.getElementById("decrypt-stage-plaintext-utf8"),
    decryptCbcBody: document.getElementById("decrypt-cbc-body"),

    selfcheckBtn: document.getElementById("selfcheck-btn"),
    selfcheckResults: document.getElementById("selfcheck-results"),
    themeToggle: document.getElementById("theme-toggle"),
    toggleCompactBtn: document.getElementById("toggle-compact-btn"),
    stepContainer: document.getElementById("step-container"),
    advancedPanel: document.getElementById("advanced-panel"),
    decryptScenarioSelect: document.getElementById("decrypt-scenario-select"),
    decryptFrameSelect: document.getElementById("decrypt-frame-select"),
    decryptCompactBtn: document.getElementById("decrypt-compact-btn"),

    cacheScaleMode: document.getElementById("cache-scale-mode"),
    cachePreset: document.getElementById("cache-preset-select"),
    cacheLoadPresetBtn: document.getElementById("cache-load-preset-btn"),
    cachePresetNote: document.getElementById("cache-preset-note"),
    cacheAutoFlushToggle: document.getElementById("cache-auto-flush-toggle"),
    cacheFlushBtn: document.getElementById("cache-flush-btn"),
    cacheInputAlert: document.getElementById("cache-input-alert"),
    cacheSubtabButtons: document.querySelectorAll("[data-cache-tab-target]"),
    cacheSubtabPanels: document.querySelectorAll(".cache-subtab-panel"),

    rtcTsEpoch: document.getElementById("rtc-ts-epoch-input"),
    rtcTsLocal: document.getElementById("rtc-ts-local-input"),
    rtcTemp10: document.getElementById("rtc-temp10-input"),
    rtcHum10: document.getElementById("rtc-hum10-input"),
    rtcLux: document.getElementById("rtc-lux-input"),
    rtcRssi: document.getElementById("rtc-rssi-input"),
    rtcAppendBtn: document.getElementById("rtc-append-btn"),
    rtcPeekBtn: document.getElementById("rtc-peek-btn"),
    rtcPopBtn: document.getElementById("rtc-pop-btn"),
    rtcClearBtn: document.getElementById("rtc-clear-btn"),
    rtcInjectMagicBtn: document.getElementById("rtc-inject-magic-btn"),
    rtcInjectCrcBtn: document.getElementById("rtc-inject-crc-btn"),
    rtcInjectBoundaryBtn: document.getElementById("rtc-inject-boundary-btn"),
    rtcSimFallbackOkBtn: document.getElementById("rtc-sim-fallback-ok-btn"),
    rtcSimFallbackBothFailBtn: document.getElementById("rtc-sim-fallback-both-fail-btn"),

    fsPayloadInput: document.getElementById("fs-payload-input"),
    fsReadBuffer: document.getElementById("fs-read-buffer-input"),
    fsWriteBtn: document.getElementById("fs-write-btn"),
    fsReadBtn: document.getElementById("fs-read-btn"),
    fsPopBtn: document.getElementById("fs-pop-btn"),
    fsClearBtn: document.getElementById("fs-clear-btn"),
    fsInjectCrcBtn: document.getElementById("fs-inject-crc-btn"),
    fsInjectMagicBtn: document.getElementById("fs-inject-magic-btn"),
    fsInjectSyncBtn: document.getElementById("fs-inject-sync-btn"),

    cacheTraceSummary: document.getElementById("cache-trace-summary"),
    cacheTraceList: document.getElementById("cache-trace-list"),
    cacheTraceSection: document.getElementById("cache-section-2"),
    cacheOutputSection: document.getElementById("cache-section-3"),
    cacheRtcState: document.getElementById("cache-rtc-state"),
    cacheFsState: document.getElementById("cache-fs-state"),
    cacheEmergencyState: document.getElementById("cache-emergency-state"),
    cacheLastErrorBadge: document.getElementById("cache-last-error-badge"),
    cacheLastRead: document.getElementById("cache-last-read"),
    cacheRtcBar: document.getElementById("cache-rtc-occupancy-bar"),
    cacheRtcLabel: document.getElementById("cache-rtc-occupancy-label"),
    cacheFsBar: document.getElementById("cache-fs-occupancy-bar"),
    cacheFsLabel: document.getElementById("cache-fs-occupancy-label"),
    cacheCrcDetail: document.getElementById("cache-crc-detail-content"),
  };

  var mainState = {
    activeMainTab: "main-tab-aes",
    activeTab: "encrypt",
    singleTrace: null,
    wsTrace: null,
    validationResult: null,
    encryptTraceToken: REDACTED
    decryptTraceToken: REDACTED
    cacheTraceToken: REDACTED
    lastEncryptMode: "terminal",
  };

  var advancedState = {
    selectedScenario: "single",
    selectedFrame: 0,
    selectedBlock: 0,
    selectedRound: 0,
    selectedDecryptBlock: 0,
    selectedDecryptRound: AesTrace.AES_ROUNDS,
    compactRounds: false,
    compactDecrypt: false,
    traceSingle: null,
    traceWs: null,
  };

  var cacheState = {
    activeTab: "cache-subtab-rtc",
    autoFlush: true,
    context: null,
  };

  function initThemeToggle() {
    var saved = localStorage.getItem("aes-docs-theme");
    if (saved) {
      document.documentElement.setAttribute("data-theme", saved);
    }
    updateThemeIcon();
    if (!el.themeToggle) return;
    el.themeToggle.addEventListener("click", function () {
      var current = document.documentElement.getAttribute("data-theme");
      var next = current === "light" ? "dark" : "light";
      document.documentElement.setAttribute("data-theme", next);
      localStorage.setItem("aes-docs-theme", next);
      updateThemeIcon();
    });
  }

  function updateThemeIcon() {
    if (!el.themeToggle) return;
    var theme = document.documentElement.getAttribute("data-theme");
    el.themeToggle.textContent = theme === "light" ? "☀️" : "🌙";
  }

  function initCopyButtons() {
    document.addEventListener("click", function (e) {
      var btn = e.target.closest(".copy-btn");
      if (!btn) return;
      var targetId = btn.getAttribute("data-copy-target");
      var target = document.getElementById(targetId);
      if (!target) return;
      var text = target.textContent || "";
      if (!text || text === "-") return;
      navigator.clipboard.writeText(text).then(function () {
        btn.textContent = "Copied!";
        btn.classList.add("copied");
        setTimeout(function () {
          btn.textContent = "Copy";
          btn.classList.remove("copied");
        }, 1500);
      });
    });
  }

  function activateMainTab(tabId) {
    mainState.activeMainTab = tabId === "main-tab-cache" ? "main-tab-cache" : "main-tab-aes";
    for (var i = 0; i < el.mainTabPanels.length; i++) {
      var panel = el.mainTabPanels[i];
      var active = panel.id === mainState.activeMainTab;
      panel.classList.toggle("active", active);
      panel.hidden = !active;
    }
    for (var j = 0; j < el.mainTabButtons.length; j++) {
      var btn = el.mainTabButtons[j];
      var target = btn.getAttribute("data-main-tab-target");
      var isActive = target === mainState.activeMainTab;
      btn.classList.toggle("active", isActive);
      btn.setAttribute("aria-selected", isActive ? "true" : "false");
    }
  }

  function initMainTabs() {
    if (!el.mainTabButtons || el.mainTabButtons.length === 0) return;
    for (var i = 0; i < el.mainTabButtons.length; i++) {
      el.mainTabButtons[i].addEventListener("click", function (event) {
        var target = event.currentTarget.getAttribute("data-main-tab-target");
        activateMainTab(target);
      });
    }
    activateMainTab("main-tab-aes");
  }

  function activateTab(tabId) {
    mainState.activeTab = tabId === "tab-decrypt" ? "decrypt" : "encrypt";
    for (var i = 0; i < el.aesTabPanels.length; i++) {
      var panel = el.aesTabPanels[i];
      var active = panel.id === tabId;
      panel.classList.toggle("active", active);
      panel.hidden = !active;
    }
    for (var j = 0; j < el.aesTabButtons.length; j++) {
      var btn = el.aesTabButtons[j];
      var target = btn.getAttribute("data-aes-tab-target");
      var isActive = target === tabId;
      btn.classList.toggle("active", isActive);
      btn.setAttribute("aria-selected", isActive ? "true" : "false");
    }
  }

  function initAesTabs() {
    if (!el.aesTabButtons || el.aesTabButtons.length === 0) return;
    for (var i = 0; i < el.aesTabButtons.length; i++) {
      el.aesTabButtons[i].addEventListener("click", function (event) {
        var target = event.currentTarget.getAttribute("data-aes-tab-target");
        activateTab(target);
      });
    }
    activateTab("tab-encrypt");
  }

  function activateCacheSubtab(tabId) {
    cacheState.activeTab = tabId === "cache-subtab-littlefs" ? "cache-subtab-littlefs" : "cache-subtab-rtc";
    if (!el.cacheSubtabPanels) return;
    for (var i = 0; i < el.cacheSubtabPanels.length; i++) {
      var panel = el.cacheSubtabPanels[i];
      var active = panel.id === cacheState.activeTab;
      panel.classList.toggle("active", active);
      panel.hidden = !active;
    }
    if (!el.cacheSubtabButtons) return;
    for (var j = 0; j < el.cacheSubtabButtons.length; j++) {
      var btn = el.cacheSubtabButtons[j];
      var target = btn.getAttribute("data-cache-tab-target");
      var isActive = target === cacheState.activeTab;
      btn.classList.toggle("active", isActive);
      btn.setAttribute("aria-selected", isActive ? "true" : "false");
    }
  }

  function nowEpochSeconds() {
    return Math.floor(Date.now() / 1000);
  }

  function truncateMiddle(text, max) {
    var str = String(text || "");
    if (str.length <= max) {
      return str;
    }
    var head = Math.floor((max - 3) / 2);
    var tail = max - 3 - head;
    return str.slice(0, head) + "..." + str.slice(str.length - tail);
  }

  function pad2(value) {
    var n = Number(value) || 0;
    return n < 10 ? "0" + n : String(n);
  }

  function epochToDatetimeLocalValue(epoch) {
    var sec = Number(epoch);
    if (!Number.isFinite(sec)) return "";
    var date = new Date((sec >>> 0) * 1000);
    if (!Number.isFinite(date.getTime())) return "";
    return (
      date.getFullYear() +
      "-" +
      pad2(date.getMonth() + 1) +
      "-" +
      pad2(date.getDate()) +
      "T" +
      pad2(date.getHours()) +
      ":" +
      pad2(date.getMinutes()) +
      ":" +
      pad2(date.getSeconds())
    );
  }

  function datetimeLocalValueToEpoch(value) {
    var raw = String(value || "").trim();
    if (!raw) return null;

    var y;
    var mo;
    var d;
    var h;
    var mi;
    var s;

    var isoMatch = raw.match(
      /^(\d{4})-(\d{2})-(\d{2})[T ](\d{2}):(\d{2})(?::(\d{2}))?$/
    );
    if (isoMatch) {
      y = Number(isoMatch[1]);
      mo = Number(isoMatch[2]);
      d = Number(isoMatch[3]);
      h = Number(isoMatch[4]);
      mi = Number(isoMatch[5]);
      s = Number(isoMatch[6] || "0");
    } else {
      var localMatch = raw.match(
        /^(\d{2})\/(\d{2})\/(\d{4})[ T](\d{2}):(\d{2})(?::(\d{2}))?$/
      );
      if (localMatch) {
        d = Number(localMatch[1]);
        mo = Number(localMatch[2]);
        y = Number(localMatch[3]);
        h = Number(localMatch[4]);
        mi = Number(localMatch[5]);
        s = Number(localMatch[6] || "0");
      } else {
        var fallbackMillis = new Date(raw).getTime();
        if (!Number.isFinite(fallbackMillis)) return null;
        var fallbackSec = Math.floor(fallbackMillis / 1000);
        if (
          !Number.isFinite(fallbackSec) ||
          fallbackSec < 0 ||
          fallbackSec > 0xffffffff
        ) {
          return null;
        }
        return fallbackSec >>> 0;
      }
    }

    var localDate = new Date(y, mo - 1, d, h, mi, s, 0);
    if (!Number.isFinite(localDate.getTime())) return null;
    if (
      localDate.getFullYear() !== y ||
      localDate.getMonth() !== mo - 1 ||
      localDate.getDate() !== d ||
      localDate.getHours() !== h ||
      localDate.getMinutes() !== mi ||
      localDate.getSeconds() !== s
    ) {
      return null;
    }

    var sec = Math.floor(localDate.getTime() / 1000);
    if (!Number.isFinite(sec) || sec < 0 || sec > 0xffffffff) return null;
    return sec >>> 0;
  }

  function syncTimestampReadableFromEpoch(epochValue) {
    if (!el.timestampReadable) return;
    var next = epochToDatetimeLocalValue(epochValue);
    if (!next) {
      el.timestampReadable.value = "";
      return;
    }
    el.timestampReadable.value = next;
  }

  function syncDeviceEpochReadableFromEpoch(epochValue) {
    if (!el.deviceEpochReadable) return;
    var next = epochToDatetimeLocalValue(epochValue);
    if (!next) {
      el.deviceEpochReadable.value = "";
      return;
    }
    el.deviceEpochReadable.value = next;
  }

  function setAlert(node, message, kind) {
    if (!node) {
      return;
    }
    if (!message) {
      node.hidden = true;
      node.textContent = "";
      node.className = "alert-box";
      return;
    }
    node.hidden = false;
    node.textContent = message;
    node.className = "alert-box " + (kind || "info");
  }

  function clearNode(node) {
    while (node && node.firstChild) {
      node.removeChild(node.firstChild);
    }
  }

  function createCell(text, className) {
    var td = document.createElement("td");
    td.textContent = text;
    if (className) {
      td.className = className;
    }
    return td;
  }

  function matrixTextFromHex(hex) {
    return AesTrace.toStateMatrixLines(hex).join("\n");
  }

  function renderEpochContext(epochValue, utcEl, localEl, relEl, nowEpoch) {
    if (!utcEl || !localEl || !relEl) return;
    var ctx = AesTrace.formatEpochContext(epochValue, nowEpoch);
    utcEl.textContent = ctx.utcIso;
    localEl.textContent = ctx.localString;
    relEl.textContent = ctx.relativeToNow;
    if (!ctx.valid) {
      utcEl.className = "mono invalid";
      localEl.className = "mono invalid";
      relEl.className = "mono invalid";
    } else {
      utcEl.className = "mono";
      localEl.className = "mono";
      relEl.className = "mono";
    }
  }

  function updateEncryptionEpochContext() {
    var epoch = Number(el.timestamp.value);
    syncTimestampReadableFromEpoch(epoch);
    renderEpochContext(
      epoch,
      el.encEpochUtc,
      el.encEpochLocal,
      el.encEpochRelative
    );
  }

  function updateValidationEpochContext() {
    var epoch = Number(el.deviceEpoch.value);
    syncDeviceEpochReadableFromEpoch(epoch);
    renderEpochContext(
      epoch,
      el.valEpochUtc,
      el.valEpochLocal,
      el.valEpochRelative
    );
    updatePayloadTimestampPreview();
  }

  function updatePayloadTimestampPreview() {
    if (!el.payloadTsEpoch || !el.payloadTsUtc || !el.payloadTsLocal) {
      return;
    }
    var payload = (el.validationPayload ? el.validationPayload.value : "").trim();
    if (!payload) {
      setText(el.payloadTsEpoch, "-");
      setText(el.payloadTsUtc, "-");
      setText(el.payloadTsLocal, "-");
      if (el.payloadTsNote) {
        el.payloadTsNote.textContent = "Isi payload untuk melihat timestamp payload.";
      }
      return;
    }

    var modeRaw = el.validationMode ? el.validationMode.value : "auto";
    var mode = modeRaw === "auto" ? undefined : modeRaw;
    var deviceEpoch = Number(el.deviceEpoch ? el.deviceEpoch.value : nowEpochSeconds());

    var result;
    try {
      result = AesTrace.validateSerialized(payload, {
        mode: mode,
        keyText: AesTrace.DEMO_VECTOR.key,
        deviceEpoch: deviceEpoch,
        timeSynced: false,
        replaySkewSec: AesTrace.WS_REPLAY_SKEW_SEC_MAX,
      });
    } catch (error) {
      result = { ok: false, steps: [], error: error.message || String(error) };
    }

    if (result && typeof result.timestamp === "number") {
      var ts = result.timestamp >>> 0;
      var ctx =
        result.timestampContext || AesTrace.formatEpochContext(ts, deviceEpoch);
      setText(el.payloadTsEpoch, String(ts));
      setText(el.payloadTsUtc, ctx.utcIso);
      setText(el.payloadTsLocal, ctx.localString);
      if (el.payloadTsNote) {
        el.payloadTsNote.textContent = result.ok
          ? "Timestamp payload berhasil terbaca."
          : "Timestamp payload terbaca, meskipun validasi gagal di langkah lain.";
      }
      return;
    }

    setText(el.payloadTsEpoch, "-");
    setText(el.payloadTsUtc, "-");
    setText(el.payloadTsLocal, "-");

    var failStep = null;
    if (result && result.steps && result.steps.length) {
      for (var i = 0; i < result.steps.length; i++) {
        if (!result.steps[i].ok) {
          failStep = result.steps[i];
          break;
        }
      }
    }
    if (el.payloadTsNote) {
      if (failStep) {
        el.payloadTsNote.textContent =
          'Timestamp payload belum bisa dibaca: gagal di "' +
          failStep.name +
          '".';
      } else if (result && result.error) {
        el.payloadTsNote.textContent =
          "Timestamp payload belum bisa dibaca: " + result.error;
      } else {
        el.payloadTsNote.textContent = "Timestamp payload belum bisa dibaca.";
      }
    }
  }

  function setText(node, value) {
    if (!node) return;
    if (value === undefined || value === null || value === "") {
      node.textContent = "-";
      return;
    }
    node.textContent = String(value);
  }

  function escapeVisibleText(text) {
    return String(text || "")
      .replace(/\\/g, "\\\\")
      .replace(/\r/g, "\\r")
      .replace(/\n/g, "\\n")
      .replace(/\t/g, "\\t");
  }

  function describeHexAsText(hex) {
    var raw = String(hex || "").trim();
    if (!raw || raw === "-") return "-";
    try {
      var bytes = AesTrace.hexToBytes(raw);
      if (!bytes || bytes.length === 0) return "(kosong)";
      var decoded = AesTrace.bytesToUtf8(bytes);
      if (decoded.indexOf("\uFFFD") >= 0) {
        return "(biner/non-printable, tidak bisa UTF-8 penuh)";
      }
      var visible = decoded.replace(/[\r\n\t ]/g, "");
      if (!visible) {
        return "(kosong / whitespace)";
      }
      return "\"" + truncateMiddle(escapeVisibleText(decoded), 120) + "\"";
    } catch (error) {
      return "(hex tidak valid)";
    }
  }

  function formatHexWithTextPreview(hex) {
    var raw = String(hex || "-");
    return raw + "\nUTF-8: " + describeHexAsText(raw);
  }

  function u32ToBytesBE(value) {
    var v = Number(value) >>> 0;
    return new Uint8Array([
      (v >>> 24) & 0xff,
      (v >>> 16) & 0xff,
      (v >>> 8) & 0xff,
      v & 0xff,
    ]);
  }

  function concatBytes2(a, b) {
    var left = a || new Uint8Array(0);
    var right = b || new Uint8Array(0);
    var out = new Uint8Array(left.length + right.length);
    out.set(left, 0);
    out.set(right, left.length);
    return out;
  }

  function maybeHexWithPreview(value) {
    var raw = String(value || "-").trim();
    if (raw === "-" || raw.indexOf("Frame ") === 0 || raw.indexOf("\nFrame ") >= 0) {
      return raw;
    }
    if (!/^[0-9a-fA-F]+$/.test(raw) || raw.length % 2 !== 0) {
      return raw;
    }
    return formatHexWithTextPreview(raw);
  }

  function parseMainIvMixInputs() {
    var microsRaw = el.mainIvMicrosInput ? el.mainIvMicrosInput.value : "0";
    var rssiRaw = el.mainIvRssiInput ? el.mainIvRssiInput.value : "-67";
    var micros = Number(microsRaw === "" ? "0" : microsRaw);
    var rssi = Number(rssiRaw === "" ? "-67" : rssiRaw);
    if (!Number.isFinite(micros)) micros = 0;
    if (!Number.isFinite(rssi)) rssi = -67;
    return { micros: micros, rssi: rssi };
  }

  function updateMainIvMixToggleUi() {
    if (!el.mainIvMixToggle || !el.mainIvMixEnabled) return;
    var isEnabled = !!el.mainIvMixEnabled.checked;
    el.mainIvMixToggle.classList.toggle("active", isEnabled);
    el.mainIvMixToggle.classList.toggle("inactive", !isEnabled);
    el.mainIvMixToggle.setAttribute("aria-pressed", isEnabled ? "true" : "false");
    el.mainIvMixToggle.textContent = isEnabled ? "IV Mix Aktif" : "IV Mix Nonaktif";
  }

  function formatU32Hex(value) {
    var v = Number(value) >>> 0;
    var hex = v.toString(16);
    while (hex.length < 8) hex = "0" + hex;
    return hex;
  }

  function renderMainIvMixProcess(ivCfg) {
    if (!el.mainIvMixProcess || !el.mainIvMixByteDiff) {
      return;
    }
    if (!ivCfg || !ivCfg.rawIvHex) {
      setText(el.mainIvMixProcess, "-");
      setText(el.mainIvMixByteDiff, "-");
      return;
    }

    if (!ivCfg.mixEnabled || !ivCfg.mixResult) {
      setText(
        el.mainIvMixProcess,
        "Mode: MIX NONAKTIF\n" +
        "Raw IV : " +
        (ivCfg.rawIvHex || "-") +
        "\nFinal  : " +
        (ivCfg.finalIvHex || "-") +
        "\nDelay/micros & RSSI tidak diterapkan."
      );
      setText(
        el.mainIvMixByteDiff,
        "b0..b15: mask 00 -> semua byte tetap (raw = final)"
      );
      return;
    }

    var mix = ivCfg.mixResult;
    var lines = [];
    lines.push("Mode: MIX AKTIF");
    lines.push("Raw IV      : " + mix.rawIvHex);
    lines.push(
      "Delay/micros: " +
      mix.micros +
      " (0x" +
      formatU32Hex(mix.micros) +
      ")"
    );
    lines.push(
      "RSSI        : " +
      mix.rssi +
      " dBm (mask 0x" +
      mix.rssiMaskHex +
      ")"
    );
    lines.push("Mask b0..b3 : " + mix.masksHex.join(" "));
    lines.push("Aturan      : b0^=micros[0], b1^=micros[1], b2^=micros[2], b3^=rssiMask");
    lines.push("Final IV    : " + mix.finalIvHex);

    var diffLines = [];
    var maxBytes = Math.min(4, mix.byteDiff.length);
    for (var i = 0; i < maxBytes; i++) {
      var row = mix.byteDiff[i];
      diffLines.push(
        "b" +
        row.index +
        ": " +
        row.beforeHex +
        " ^ " +
        row.maskHex +
        " = " +
        row.afterHex +
        " (" +
        row.source +
        ")"
      );
    }
    diffLines.push("b4..b15: mask 00 -> unchanged");

    setText(el.mainIvMixProcess, lines.join("\n"));
    setText(el.mainIvMixByteDiff, diffLines.join("\n"));
  }

  function deriveMainIvConfig() {
    var rawIvHex = (el.ivHex ? el.ivHex.value : "").trim();
    var mixInputs = parseMainIvMixInputs();
    var mixEnabled = !!(el.mainIvMixEnabled && el.mainIvMixEnabled.checked);
    if (!mixEnabled) {
      return {
        rawIvHex: rawIvHex,
        finalIvHex: rawIvHex,
        mixEnabled: false,
        micros: mixInputs.micros,
        rssi: mixInputs.rssi,
        mixResult: null,
      };
    }
    var mixResult = AesTrace.deriveFirmwareIv({
      rawIvHex: rawIvHex,
      micros: mixInputs.micros,
      rssi: mixInputs.rssi,
    });
    return {
      rawIvHex: rawIvHex,
      finalIvHex: mixResult.finalIvHex,
      mixEnabled: true,
      micros: mixInputs.micros,
      rssi: mixInputs.rssi,
      mixResult: mixResult,
    };
  }

  function renderMainInputPreview() {
    updateMainIvMixToggleUi();

    var plaintext = el.plaintext ? el.plaintext.value : "";
    var plainBytes = AesTrace.utf8ToBytes(plaintext);
    var timestampRaw = el.timestamp ? el.timestamp.value : "";
    var timestamp = Number(timestampRaw);
    var tsValid =
      Number.isFinite(timestamp) &&
      timestamp >= 0 &&
      timestamp <= 0xffffffff;
    var tsUint = tsValid ? timestamp >>> 0 : null;
    setText(el.plaintextPreviewText, plaintext || "(kosong)");
    setText(
      el.plaintextPreviewHex,
      (plainBytes.length ? AesTrace.bytesToHex(plainBytes) : "-") +
      "\nBytes: " +
      plainBytes.length
    );
    if (tsValid) {
      var tsBytes = u32ToBytesBE(tsUint);
      var tsHex = AesTrace.bytesToHex(tsBytes);
      var tsCtx = AesTrace.formatEpochContext(tsUint);
      setText(
        el.timestampPreviewText,
        "Epoch: " +
        tsUint +
        "\nUTC: " +
        tsCtx.utcIso +
        "\nLocal: " +
        tsCtx.localString +
        "\nRelatif: " +
        tsCtx.relativeToNow +
        "\nPreview UTF-8: " +
        describeHexAsText(tsHex)
      );
      setText(
        el.timestampPreviewHex,
        tsHex + "\nFormat: u32 big-endian (4 bytes)"
      );
      var combined = concatBytes2(tsBytes, plainBytes);
      var combinedHex = AesTrace.bytesToHex(combined);
      setText(
        el.payloadCombinedPreviewText,
        "Timestamp(hex): " +
        tsHex +
        "\nTimestamp(text): " +
        describeHexAsText(tsHex) +
        "\nPlaintext(hex): " +
        (plainBytes.length ? AesTrace.bytesToHex(plainBytes) : "-") +
        "\nPlaintext(text): " +
        (plaintext ? "\"" + truncateMiddle(escapeVisibleText(plaintext), 120) + "\"" : "(kosong)")
      );
      setText(
        el.payloadCombinedPreviewHex,
        "Payload(hex): " +
        (combinedHex || "-") +
        "\nPayload(text): " +
        describeHexAsText(combinedHex) +
        "\nBytes total: REDACTED
        combined.length
      );
    } else {
      setText(
        el.timestampPreviewText,
        "Timestamp belum valid. Isi angka 0 sampai 4294967295."
      );
      setText(el.timestampPreviewHex, "-");
      setText(
        el.payloadCombinedPreviewText,
        "Menunggu timestamp valid untuk menyusun payload awal."
      );
      setText(el.payloadCombinedPreviewHex, "-");
    }

    var rawIvHex = (el.ivHex ? el.ivHex.value : "").trim();
    setText(el.mainIvRawPreview, rawIvHex || "-");
    setText(el.mainIvRawText, describeHexAsText(rawIvHex));

    try {
      var ivCfg = deriveMainIvConfig();
      setText(el.mainIvMixedPreview, ivCfg.finalIvHex || "-");
      setText(el.mainIvMixedText, describeHexAsText(ivCfg.finalIvHex));
      setText(
        el.mainIvMixMeta,
        ivCfg.mixEnabled
          ? "Mix aktif | micros=" + ivCfg.micros + " | rssi=" + ivCfg.rssi + " dBm"
          : "Mix nonaktif | IV final = IV mentah"
      );
      renderMainIvMixProcess(ivCfg);
    } catch (error) {
      setText(el.mainIvMixedPreview, "-");
      setText(el.mainIvMixedText, "(gagal hitung mix)");
      setText(el.mainIvMixMeta, "Mix error: " + (error.message || String(error)));
      setText(el.mainIvMixProcess, "Mix error: " + (error.message || String(error)));
      setText(el.mainIvMixByteDiff, "-");
    }
  }

  function addClassByIds(ids, className) {
    for (var i = 0; i < ids.length; i++) {
      var node = document.getElementById(ids[i]);
      if (node) node.classList.add(className);
    }
  }

  function applyFieldVisualKinds() {
    addClassByIds(
      [
        "output-ciphertext-hex",
        "decrypt-stage-iv",
        "decrypt-stage-cipher",
        "decrypt-stage-padded",
        "decrypt-stage-unpadded",
        "decrypt-stage-plaintext-hex",
        "iv-before-hex",
        "iv-after-hex",
        "stage-payload",
        "stage-padded",
        "main-iv-raw-preview",
        "main-iv-mixed-preview",
        "plaintext-preview-hex",
        "timestamp-preview-hex",
        "payload-combined-preview-hex",
        "main-iv-mix-byte-diff",
      ],
      "mono-hex"
    );
    addClassByIds(
      [
        "plaintext-preview-text",
        "timestamp-preview-text",
        "payload-combined-preview-text",
        "main-iv-raw-text",
        "main-iv-mixed-text",
        "decrypt-stage-plaintext-utf8",
      ],
      "mono-text"
    );
    addClassByIds(
      [
        "output-primary-active",
        "output-terminal",
        "output-portal",
        "output-iv-b64",
        "output-ct-b64",
      ],
      "mono-cryptic"
    );
  }

  function scrollToFlowTarget(node) {
    if (!node || typeof node.scrollIntoView !== "function") return;
    node.scrollIntoView({ behavior: "smooth", block: "start" });
  }

  function scrollFlowSequence(firstNode, secondNode, delayMs) {
    scrollToFlowTarget(firstNode);
    if (!secondNode) return;
    setTimeout(function () {
      scrollToFlowTarget(secondNode);
    }, delayMs || 420);
  }

  function isWsScenario() {
    return advancedState.selectedScenario === "ws" || advancedState.selectedScenario === "ws-short";
  }

  function getAdvancedWsTrace() {
    return advancedState.traceWs;
  }

  function getAdvancedSingleTrace() {
    return advancedState.traceSingle;
  }

  function getActiveTrace() {
    var wsTrace = getAdvancedWsTrace();
    if (isWsScenario()) {
      if (
        wsTrace &&
        wsTrace.frames &&
        wsTrace.frames.length > 0 &&
        advancedState.selectedFrame >= 0 &&
        advancedState.selectedFrame < wsTrace.frames.length
      ) {
        return wsTrace.frames[advancedState.selectedFrame].trace;
      }
      return null;
    }
    return getAdvancedSingleTrace();
  }

  function buildTraceItem(status, title, detail, primaryFail) {
    return {
      status: status,
      title: title,
      detail: detail,
      primaryFail: !!primaryFail,
    };
  }

  function renderAnimatedStatusList(node, items, tokenKey) {
    if (!node) return;
    mainState[tokenKey] = REDACTED
    var token = REDACTED
    clearNode(node);
    if (!items || items.length === 0) return;

    for (var i = 0; i < items.length; i++) {
      (function (idx) {
        setTimeout(function () {
          if (mainState[tokenKey] !=REDACTED
          var item = items[idx];
          var li = document.createElement("li");
          var status = item.status === "fail" ? "fail" : item.status === "pass" ? "pass" : "info";
          li.className = status + " trace-enter";
          if (item.primaryFail) {
            li.classList.add("fail-primary");
          }
          var prefix = status === "fail" ? "FAIL" : status === "pass" ? "PASS" : "INFO";
          li.textContent = prefix + " - " + item.title + " - " + (item.detail || "-");
          node.appendChild(li);
        }, idx * 110);
      })(i);
    }
  }

  function renderEncryptTimeline(summary, steps) {
    if (el.encryptTraceSummary) {
      el.encryptTraceSummary.textContent = summary || "-";
    }
    renderAnimatedStatusList(el.encryptTraceList, steps, "REDACTED");
  }

  function renderValidationTimeline(steps) {
    renderAnimatedStatusList(el.validationSteps, steps, "REDACTED");
  }

  function getCacheTraceLib() {
    if (typeof window === "undefined") return null;
    if (typeof window.CacheCrcTrace === "undefined") return null;
    return window.CacheCrcTrace;
  }

  function ensureCacheContext(forceReset) {
    var lib = getCacheTraceLib();
    if (!lib) return null;
    var scale = el.cacheScaleMode ? el.cacheScaleMode.value : "visual";
    if (
      forceReset ||
      !cacheState.context ||
      !cacheState.context.scaleMode ||
      cacheState.context.scaleMode !== scale
    ) {
      cacheState.context = lib.createContext(scale);
    }
    return cacheState.context;
  }

  function setCacheTraceSummary(summary) {
    if (!el.cacheTraceSummary) return;
    el.cacheTraceSummary.textContent = summary || "Belum ada trace cache.";
  }

  function renderCacheTraceTimeline(steps) {
    var items = [];
    var failFound = false;
    for (var i = 0; i < (steps || []).length; i++) {
      var step = steps[i];
      var status = step && step.status ? step.status : "info";
      var primaryFail = status === "fail" && !failFound;
      if (primaryFail) failFound = true;
      items.push(buildTraceItem(status, step.title || "-", step.detail || "-", primaryFail));
    }
    renderAnimatedStatusList(el.cacheTraceList, items, "REDACTED");
  }

  function setCacheErrorBadge(errorName) {
    if (!el.cacheLastErrorBadge) return;
    var name = errorName || "NONE";
    var kind = "info";
    if (name === "NONE") {
      kind = "pass";
    } else if (name === "CORRUPT_DATA" || name === "FILE_READ_ERROR" || name === "OUT_OF_MEMORY") {
      kind = "fail";
    }
    el.cacheLastErrorBadge.className = "status-badge " + kind;
    el.cacheLastErrorBadge.textContent = name;
  }

  function formatPercent(value) {
    var pct = Number(value);
    if (!Number.isFinite(pct)) return "0%";
    var bounded = Math.max(0, Math.min(1, pct));
    return (bounded * 100).toFixed(1) + "%";
  }

  function renderCacheOutputState(state) {
    if (!state) {
      setText(el.cacheRtcState, "-");
      setText(el.cacheFsState, "-");
      setText(el.cacheEmergencyState, "-");
      setText(el.cacheLastRead, "-");
      setText(el.cacheRtcLabel, "-");
      setText(el.cacheFsLabel, "-");
      setCacheErrorBadge("NONE");
      if (el.cacheRtcBar) el.cacheRtcBar.style.width = "0%";
      if (el.cacheFsBar) el.cacheFsBar.style.width = "0%";
      return;
    }

    var rtcText =
      "mode_scale: " +
      state.scaleLabel +
      "\nversion: " +
      (state.rtc.version !== undefined ? state.rtc.version : "-") +
      "\nhead: " +
      state.rtc.head +
      "\ntail: " +
      state.rtc.tail +
      "\ncount: " +
      state.rtc.count +
      " / " +
      state.rtc.maxRecords +
      "\nnext_seq: " +
      (state.rtc.nextSeq !== undefined ? state.rtc.nextSeq : "-") +
      "\nheader_magic: " +
      (state.rtc.headerMagicHex || state.rtc.magicHex || "-") +
      "\nrecord_magic: " +
      (state.rtc.recordMagicHex || "-") +
      "\nheader_crc(stored): " +
      (state.rtc.headerCrcHex || state.rtc.crcHex || "-") +
      "\nheader_crc(calc): " +
      (state.rtc.headerCalculatedCrcHex || state.rtc.calculatedCrcHex || "-") +
      "\nslots_valid: " +
      (state.rtc.validSlots !== undefined ? state.rtc.validSlots : "-") +
      "\nslots_corrupt: " +
      (state.rtc.corruptSlots !== undefined ? state.rtc.corruptSlots : "-") +
      "\nvalidasi: " +
      ((state.rtc.headerValid !== undefined ? state.rtc.headerValid : state.rtc.valid) ? "PASS" : "FAIL") +
      " (" +
      state.rtc.validReason +
      ")";
    setText(el.cacheRtcState, rtcText);

    var fsText =
      "size_bytes: " +
      state.littlefs.sizeBytes +
      " / " +
      state.littlefs.maxBytes +
      "\nhead: " +
      state.littlefs.head +
      "\ntail: " +
      state.littlefs.tail +
      "\nmagic: " +
      state.littlefs.magicHex +
      "\nversion: " +
      state.littlefs.version +
      "\nheader_crc(stored): " +
      state.littlefs.headerCrcHex +
      "\nheader_crc(calc): " +
      state.littlefs.headerCalculatedCrcHex +
      "\nheader_valid: " +
      (state.littlefs.headerValid ? "PASS" : REDACTED
    setText(el.cacheFsState, fsText);

    var emergency = state.emergency || {
      depth: 0,
      capacity: 0,
      head: 0,
      tail: 0,
      backpressure: false,
      enqueueTotal: REDACTED
      drainedTotal: REDACTED
      blockedSamples: 0,
      occupancyRatio: 0,
      occupancyText: "0 / 0 record",
    };
    var emergencyText =
      "depth: " +
      emergency.depth +
      " / " +
      emergency.capacity +
      "\nhead: " +
      emergency.head +
      "\ntail: " +
      emergency.tail +
      "\nbackpressure: " +
      (emergency.backpressure ? "ON" : "OFF") +
      "\nenqueue_total: REDACTED
      emergency.enqueueTotal +
      "\ndrained_total: REDACTED
      emergency.drainedTotal +
      "\nblocked_samples: " +
      emergency.blockedSamples +
      "\noccupancy: " +
      emergency.occupancyText +
      " (" +
      formatPercent(emergency.occupancyRatio) +
      ")";
    setText(el.cacheEmergencyState, emergencyText);

    var readText =
      "error: " +
      (state.lastRead.error || "NONE") +
      "\nlen: " +
      (state.lastRead.len || 0) +
      "\nsalvaged: " +
      (!!state.lastRead.salvaged) +
      "\nstored_crc: " +
      (state.lastRead.storedCrcHex || "-") +
      "\ncalculated_crc: " +
      (state.lastRead.calculatedCrcHex || "-") +
      "\npayload_text: " +
      (state.lastRead.payloadText ? JSON.stringify(state.lastRead.payloadText) : "-") +
      "\npayload_hex: " +
      (state.lastRead.payloadHex || "-");
    setText(el.cacheLastRead, readText);

    if (el.cacheRtcBar) {
      el.cacheRtcBar.style.width = formatPercent(state.rtc.occupancyRatio);
    }
    if (el.cacheFsBar) {
      el.cacheFsBar.style.width = formatPercent(state.littlefs.occupancyRatio);
    }
    setText(el.cacheRtcLabel, state.rtc.occupancyText + " (" + formatPercent(state.rtc.occupancyRatio) + ")");
    setText(
      el.cacheFsLabel,
      state.littlefs.occupancyText + " (" + formatPercent(state.littlefs.occupancyRatio) + ")"
    );
    setCacheErrorBadge(state.lastRead.error || "NONE");
  }

  function renderCacheCrcDetail(detailText) {
    if (!el.cacheCrcDetail) return;
    setText(el.cacheCrcDetail, detailText || "-");
  }

  function renderCacheResult(result, shouldScroll) {
    if (!result) return;
    setCacheTraceSummary(result.summary || "Trace cache selesai.");
    renderCacheTraceTimeline(result.steps || []);
    renderCacheOutputState(result.state || null);
    renderCacheCrcDetail(result.crcDetail || "-");
    if (el.cachePresetNote && result.presetNote) {
      el.cachePresetNote.textContent = result.presetNote;
    }
    if (shouldScroll !== false) {
      scrollFlowSequence(el.cacheTraceSection, el.cacheOutputSection, 520);
    }
  }

  function setCacheAutoFlushUi() {
    if (!el.cacheAutoFlushToggle) return;
    el.cacheAutoFlushToggle.setAttribute("aria-pressed", cacheState.autoFlush ? "true" : "false");
    el.cacheAutoFlushToggle.textContent = cacheState.autoFlush
      ? "Auto Flush RTC->LittleFS Aktif"
      : "Auto Flush RTC->LittleFS Nonaktif";
    el.cacheAutoFlushToggle.classList.toggle("active", cacheState.autoFlush);
  }

  function parseNumber(value, fallback) {
    var parsed = Number(value);
    return Number.isFinite(parsed) ? parsed : fallback;
  }

  function getRtcInputRecord() {
    return {
      timestamp: parseNumber(el.rtcTsEpoch ? el.rtcTsEpoch.value : nowEpochSeconds(), nowEpochSeconds()),
      temp10: parseNumber(el.rtcTemp10 ? el.rtcTemp10.value : 0, 0),
      hum10: parseNumber(el.rtcHum10 ? el.rtcHum10.value : 0, 0),
      lux: parseNumber(el.rtcLux ? el.rtcLux.value : 0, 0),
      rssi: parseNumber(el.rtcRssi ? el.rtcRssi.value : 0, 0),
    };
  }

  function updateRtcLocalFromEpochInput() {
    if (!el.rtcTsEpoch || !el.rtcTsLocal) return;
    var epoch = parseNumber(el.rtcTsEpoch.value, null);
    if (!Number.isFinite(epoch)) return;
    el.rtcTsLocal.value = epochToDatetimeLocalValue(epoch >>> 0);
  }

  function syncRtcEpochFromLocal(showWarn) {
    if (!el.rtcTsLocal || !el.rtcTsEpoch) return;
    var raw = el.rtcTsLocal.value;
    if (!raw) return;
    var epoch = datetimeLocalValueToEpoch(raw);
    if (epoch === null) {
      if (showWarn) {
        setAlert(
          el.cacheInputAlert,
          "Waktu lokal RTC tidak valid. Periksa format tanggal/jam input.",
          "warn"
        );
      }
      return;
    }
    el.rtcTsEpoch.value = String(epoch >>> 0);
    if (el.cacheInputAlert) {
      el.cacheInputAlert.hidden = true;
      el.cacheInputAlert.textContent = "";
    }
  }

  function runCacheAction(executor, shouldScroll) {
    var lib = getCacheTraceLib();
    if (!lib) {
      setAlert(
        el.cacheInputAlert,
        "Modul CacheCrcTrace belum dimuat. Pastikan file cache_crc_trace.js tersedia.",
        "error"
      );
      return;
    }
    var ctx = ensureCacheContext(false);
    if (!ctx) {
      setAlert(el.cacheInputAlert, "Gagal membuat context Cache CRC.", "error");
      return;
    }
    try {
      var result = executor(lib, ctx);
      if (el.cacheInputAlert) {
        el.cacheInputAlert.hidden = true;
        el.cacheInputAlert.textContent = "";
      }
      renderCacheResult(result, shouldScroll);
    } catch (error) {
      setAlert(
        el.cacheInputAlert,
        "Aksi Cache CRC gagal: " + (error && error.message ? error.message : String(error)),
        "error"
      );
    }
  }

  function activateCacheSubtabs() {
    if (!el.cacheSubtabButtons || el.cacheSubtabButtons.length === 0) return;
    for (var i = 0; i < el.cacheSubtabButtons.length; i++) {
      el.cacheSubtabButtons[i].addEventListener("click", function (event) {
        var target = event.currentTarget.getAttribute("data-cache-tab-target");
        activateCacheSubtab(target);
      });
    }
    activateCacheSubtab("cache-subtab-rtc");
  }

  function initCacheFlow() {
    if (!el.cacheScaleMode) return;
    if (!getCacheTraceLib()) {
      setCacheTraceSummary("Modul Cache CRC belum dimuat.");
      return;
    }

    if (el.cacheScaleMode) {
      el.cacheScaleMode.value = "visual";
    }
    if (el.cachePreset) {
      el.cachePreset.value = "normal_ok";
    }
    cacheState.autoFlush = true;
    setCacheAutoFlushUi();
    ensureCacheContext(true);

    if (el.rtcTsEpoch) {
      el.rtcTsEpoch.value = String(nowEpochSeconds());
    }
    updateRtcLocalFromEpochInput();
    if (el.rtcTemp10) el.rtcTemp10.value = "253";
    if (el.rtcHum10) el.rtcHum10.value = "680";
    if (el.rtcLux) el.rtcLux.value = "1234";
    if (el.rtcRssi) el.rtcRssi.value = "-67";
    if (el.fsPayloadInput) el.fsPayloadInput.value = "cache|manual|sample";
    if (el.fsReadBuffer) el.fsReadBuffer.value = "512";

    var initState = cacheState.context ? getCacheTraceLib().snapshot(cacheState.context) : null;
    setCacheTraceSummary("Belum ada trace cache.");
    renderCacheTraceTimeline([]);
    renderCacheOutputState(initState);
    renderCacheCrcDetail("-");
    if (el.cachePresetNote) {
      var presetMeta = getCacheTraceLib().PRESETS.normal_ok;
      el.cachePresetNote.textContent = presetMeta && presetMeta.note ? presetMeta.note : "";
    }
  }

  function renderEncryptSingleTimeline(trace, mode, meta) {
    if (!trace) return;
    var ivDetail = "raw IV dipakai tanpa mix";
    if (meta && meta.ivConfig) {
      ivDetail = meta.ivConfig.mixEnabled
        ? "mix aktif -> finalIV=" + meta.ivConfig.finalIvHex
        : "mix nonaktif -> finalIV=" + meta.ivConfig.finalIvHex;
    }
    var steps = [
      buildTraceItem(
        "REDACTED",
        "Input plaintext",
        "UTF-8 length=" + trace.inputs.plaintextLength + " byte | mode=" + mode
      ),
      buildTraceItem(
        "REDACTED",
        "Sisipkan timestamp",
        "epoch=" + trace.inputs.timestamp + " -> hex=" + trace.stage.timestampHex
      ),
      buildTraceItem("info", "Finalisasi IV", ivDetail),
      buildTraceItem(
        "REDACTED",
        "Gabung payload",
        "timestamp(4B)+plaintext=" + trace.stage.payloadLength + " byte"
      ),
      buildTraceItem(
        "REDACTED",
        "PKCS7 padding",
        "pad=" + trace.stage.padLength + " -> total=" + trace.stage.paddedLength + " byte"
      ),
      buildTraceItem(
        "REDACTED",
        "AES-256-CBC",
        "block=" + trace.blocks.length + " | rounds per block=" + (AesTrace.AES_ROUNDS + 1)
      ),
      buildTraceItem("pass", "Serialize base64", "Output siap: REDACTED
    ];
    renderEncryptTimeline("Mode " + mode + " | Single payload trace selesai.", steps);
  }

  function renderEncryptWsTimeline(wsTrace, reason, meta) {
    if (!wsTrace || !wsTrace.frames || wsTrace.frames.length === 0) return;
    var first = wsTrace.frames[0];
    var details = reason ? " | " + reason : "";
    var ivDetail = "base IV langsung dipakai";
    if (meta && meta.ivConfig) {
      ivDetail = meta.ivConfig.mixEnabled
        ? "mix aktif -> baseIV final=" + meta.ivConfig.finalIvHex
        : "mix nonaktif -> baseIV=" + meta.ivConfig.finalIvHex;
    }
    var steps = [
      buildTraceItem(
        "REDACTED",
        "Input plaintext",
        "UTF-8 length=" + wsTrace.sourceLengthBytes + " byte" + details
      ),
      buildTraceItem(
        "REDACTED",
        "Chunking WS",
        "Dipecah menjadi " + wsTrace.frameCount + " frame (maks " + AesTrace.WS_CHUNK_SIZE + " byte/frame)"
      ),
      buildTraceItem("info", "Base IV frame", ivDetail),
      buildTraceItem(
        "REDACTED",
        "Frame-0 timestamp",
        "epoch=" + first.timestamp + " | chunk=" + first.chunkLength + " byte"
      ),
      buildTraceItem(
        "REDACTED",
        "PKCS7 + AES-CBC",
        "total block=REDACTED
      ),
      buildTraceItem("REDACTED", "REDACTED", "REDACTED"),
      buildTraceItem("REDACTED", "REDACTED", "REDACTED"),
    ];
    renderEncryptTimeline(
      "Mode " + wsTrace.mode + " | WS trace selesai: " + wsTrace.frameCount + " frame.",
      steps
    );
  }

  function resolvePresetTimestamp(value) {
    if (value === "now") return nowEpochSeconds();
    return Number(value);
  }

  function renderDecryptPresetHint(key, preset) {
    if (!el.decryptPresetHint) return;
    if (!preset) {
      el.decryptPresetHint.textContent = "";
      return;
    }
    var parts = [];
    parts.push("Preset " + key + ".");
    if (preset.note) {
      parts.push(preset.note);
    }
    if (preset.payloadEpoch !== undefined && preset.payloadEpoch !== null) {
      var payloadCtx = AesTrace.formatEpochContext(
        preset.payloadEpoch,
        preset.deviceEpoch
      );
      var deviceCtx = AesTrace.formatEpochContext(preset.deviceEpoch);
      parts.push(
        "Timestamp payload=" +
        preset.payloadEpoch +
        " (" +
        payloadCtx.utcIso +
        ")."
      );
      parts.push(
        "Epoch perangkat=" +
        preset.deviceEpoch +
        " (" +
        deviceCtx.utcIso +
        ")."
      );
    }
    if (preset.payload) {
      parts.push("Contoh payload: " + truncateMiddle(preset.payload, 78));
    }
    el.decryptPresetHint.textContent = parts.join(" ");
  }

  function loadEncryptPresetToInput(runAfterLoad) {
    if (!el.encryptPreset) return;
    var key = el.encryptPreset.value;
    var preset = ENCRYPT_PRESETS[key];
    if (!preset) {
      setAlert(el.alert, "Preset enkripsi tidak dikenal.", "error");
      return;
    }
    if (el.plaintext) el.plaintext.value = preset.plaintext;
    if (el.timestamp) el.timestamp.value = String(resolvePresetTimestamp(preset.timestamp));
    if (el.ivHex) el.ivHex.value = preset.ivHex;
    if (el.mode) el.mode.value = preset.mode;
    updateEncryptionEpochContext();
    renderMainInputPreview();
    setAlert(el.alert, "Preset enkripsi '" + key + "' dimuat.", "info");
    if (runAfterLoad) runEncryptTrace();
  }

  function loadDecryptPresetToInput(runAfterLoad) {
    if (!el.decryptPreset) return;
    var key = el.decryptPreset.value;
    var preset = DECRYPT_PRESETS[key];
    if (!preset) {
      if (el.validationSummary) {
        el.validationSummary.className = "result-box fail";
        el.validationSummary.textContent = "Preset dekripsi tidak dikenal.";
      }
      return;
    }
    if (el.validationPayload) el.validationPayload.value = preset.payload;
    if (el.validationMode) el.validationMode.value = preset.mode;
    if (el.deviceEpoch) el.deviceEpoch.value = String(preset.deviceEpoch);
    if (el.timeSynced) el.timeSynced.checked = !!preset.timeSynced;
    if (el.skewProfile) el.skewProfile.value = preset.skewProfile;
    if (el.skewCustomInput) el.skewCustomInput.value = String(preset.skewCustom);
    updateSkewCustomVisibility();
    updateValidationEpochContext();
    renderDecryptPresetHint(key, preset);
    updatePayloadTimestampPreview();
    if (runAfterLoad) renderValidation();
  }

  function explainEncryptStep(step) {
    if (step === "Input (CBC XOR)") {
      return "State awal block ini, hasil XOR plain block dengan IV/cipher block sebelumnya.";
    }
    if (step === "Input") {
      return "State 16-byte yang masuk ke step berikutnya.";
    }
    if (step === "SubBytes") {
      return "Substitusi non-linear byte per byte untuk menambah confusion.";
    }
    if (step === "ShiftRows") {
      return "Geser baris state agar byte tersebar antar kolom.";
    }
    if (step === "MixColumns") {
      return "Campur setiap kolom agar perubahan satu byte menyebar ke byte lain (diffusion).";
    }
    if (step === "AddRoundKey") {
      return "XOR state dengan round key dari key schedule AES-256.";
    }
    return "-";
  }

  function buildRoundRows(roundData, blockData) {
    if (roundData.round === 0) {
      var rows = [];
      if (blockData) {
        rows.push({
          step: "Blok Plain",
          hex: blockData.plaintextBlockHex,
          purpose: "Blok plaintext 16-byte sebelum XOR dengan IV/cipher sebelumnya.",
        });
        rows.push({
          step: "CBC Source (" + (blockData.blockIndex === 0 ? "IV" : "C[" + (blockData.blockIndex - 1) + "]") + ")",
          hex: blockData.cbcSourceHex,
          purpose: blockData.blockIndex === 0 ? "IV acak 16-byte yang dipakai untuk XOR blok pertama." : "Ciphertext blok sebelumnya, dipakai sebagai sumber XOR mode CBC.",
        });
      }
      rows.push({
        step: "Input (CBC XOR)",
        hex: roundData.inputHex,
        purpose: explainEncryptStep("Input (CBC XOR)"),
      });
      rows.push({
        step: "AddRoundKey",
        hex: roundData.addRoundKeyHex,
        purpose: explainEncryptStep("AddRoundKey"),
      });
      return rows;
    }

    if (roundData.round === AesTrace.AES_ROUNDS) {
      return [
        { step: "Input", hex: roundData.inputHex, purpose: explainEncryptStep("Input") },
        {
          step: "SubBytes",
          hex: roundData.subBytesHex,
          purpose: explainEncryptStep("SubBytes"),
        },
        {
          step: "ShiftRows",
          hex: roundData.shiftRowsHex,
          purpose: explainEncryptStep("ShiftRows"),
        },
        {
          step: "AddRoundKey",
          hex: roundData.addRoundKeyHex,
          purpose: explainEncryptStep("AddRoundKey"),
        },
      ];
    }

    return [
      { step: "Input", hex: roundData.inputHex, purpose: explainEncryptStep("Input") },
      {
        step: "SubBytes",
        hex: roundData.subBytesHex,
        purpose: explainEncryptStep("SubBytes"),
      },
      {
        step: "ShiftRows",
        hex: roundData.shiftRowsHex,
        purpose: explainEncryptStep("ShiftRows"),
      },
      {
        step: "MixColumns",
        hex: roundData.mixColumnsHex,
        purpose: explainEncryptStep("MixColumns"),
      },
      {
        step: "AddRoundKey",
        hex: roundData.addRoundKeyHex,
        purpose: explainEncryptStep("AddRoundKey"),
      },
    ];
  }

  function explainDecryptStep(step) {
    if (step === "Input (Cipher Block)") {
      return "Cipher block mentah yang akan dibalik lewat ronde inverse AES.";
    }
    if (step === "Input") {
      return "State awal di ronde dekripsi ini.";
    }
    if (step === "AddRoundKey") {
      return "XOR dengan round key yang sesuai untuk membuka lapisan kunci ronde.";
    }
    if (step === "InvShiftRows") {
      return "Membalik pergeseran baris dari proses enkripsi.";
    }
    if (step === "InvSubBytes") {
      return "Membalik substitusi S-Box dengan inverse S-Box.";
    }
    if (step === "InvMixColumns") {
      return "Membalik pencampuran kolom agar state kembali ke bentuk sebelum MixColumns.";
    }
    if (step === "CBC XOR (jadi Plain Block)") {
      return "XOR output AES-inverse dengan IV/cipher block sebelumnya untuk mendapatkan plaintext block.";
    }
    return "-";
  }

  function buildDecryptRoundRows(roundData, blockData) {
    if (roundData.round === AesTrace.AES_ROUNDS) {
      return [
        {
          step: "Input (Cipher Block)",
          hex: roundData.inputHex,
          purpose: explainDecryptStep("Input (Cipher Block)"),
        },
        {
          step: "AddRoundKey",
          hex: roundData.addRoundKeyHex,
          purpose: explainDecryptStep("AddRoundKey"),
        },
      ];
    }

    if (roundData.round === 0) {
      var rows = [];
      rows.push({ step: "Input", hex: roundData.inputHex, purpose: explainDecryptStep("Input") });
      rows.push({
        step: "InvShiftRows",
        hex: roundData.invShiftRowsHex,
        purpose: explainDecryptStep("InvShiftRows"),
      });
      rows.push({
        step: "InvSubBytes",
        hex: roundData.invSubBytesHex,
        purpose: explainDecryptStep("InvSubBytes"),
      });
      rows.push({
        step: "AddRoundKey",
        hex: roundData.addRoundKeyHex,
        purpose: explainDecryptStep("AddRoundKey"),
      });
      if (blockData) {
        rows.push({
          step: "CBC Source (" + (blockData.blockIndex === 0 ? "IV" : "C[" + (blockData.blockIndex - 1) + "]") + ")",
          hex: blockData.cbcSourceHex,
          purpose: blockData.blockIndex === 0 ? "IV yang di-XOR untuk mendapatkan plain block." : "Ciphertext blok sebelumnya sebagai sumber XOR CBC.",
        });
      }
      rows.push({
        step: "CBC XOR (jadi Plain Block)",
        hex: roundData.cbcXorHex,
        purpose: explainDecryptStep("CBC XOR (jadi Plain Block)"),
      });
      return rows;
    }

    return [
      { step: "Input", hex: roundData.inputHex, purpose: explainDecryptStep("Input") },
      {
        step: "InvShiftRows",
        hex: roundData.invShiftRowsHex,
        purpose: explainDecryptStep("InvShiftRows"),
      },
      {
        step: "InvSubBytes",
        hex: roundData.invSubBytesHex,
        purpose: explainDecryptStep("InvSubBytes"),
      },
      {
        step: "AddRoundKey",
        hex: roundData.addRoundKeyHex,
        purpose: explainDecryptStep("AddRoundKey"),
      },
      {
        step: "InvMixColumns",
        hex: roundData.invMixColumnsHex,
        purpose: explainDecryptStep("InvMixColumns"),
      },
    ];
  }

  function renderCbcBlocks(trace) {
    clearNode(el.cbcTableBody);
    if (!trace || !el.cbcTableBody) {
      return;
    }
    for (var i = 0; i < trace.blocks.length; i++) {
      var block = trace.blocks[i];
      var tr = document.createElement("tr");
      tr.appendChild(createCell(String(i)));
      tr.appendChild(createCell(formatHexWithTextPreview(block.plaintextBlockHex), "mono mono-hex"));
      tr.appendChild(createCell(formatHexWithTextPreview(block.cbcSourceHex), "mono mono-hex"));
      tr.appendChild(createCell(formatHexWithTextPreview(block.cbcXorHex), "mono mono-hex"));
      tr.appendChild(createCell(formatHexWithTextPreview(block.ciphertextBlockHex), "mono mono-hex"));
      el.cbcTableBody.appendChild(tr);
    }
  }

  function buildWsAggregateOutputs(wsTrace) {
    if (!wsTrace || !wsTrace.frames || wsTrace.frames.length === 0) {
      return {
        cipherHex: "-",
        ivB64: "-",
        ciphertextB64: "-",
        terminalPayload: "-",
        portalPayload: "-",
      };
    }

    var cipherHexLines = [];
    var ivLines = [];
    var ctLines = [];
    var termLines = [];
    var portalLines = [];

    for (var i = 0; i < wsTrace.frames.length; i++) {
      var fr = wsTrace.frames[i];
      var tr = fr.trace;
      cipherHexLines.push("Frame " + fr.frameIndex + ": " + tr.outputs.ciphertextHex);
      ivLines.push("Frame " + fr.frameIndex + ": " + tr.outputs.ivB64);
      ctLines.push("Frame " + fr.frameIndex + ": " + tr.outputs.ciphertextB64);
      termLines.push("Frame " + fr.frameIndex + ": " + tr.outputs.terminalPayload);
      portalLines.push("Frame " + fr.frameIndex + ": " + tr.outputs.portalPayload);
    }

    return {
      cipherHex: cipherHexLines.join("\n"),
      ivB64: ivLines.join("\n"),
      ciphertextB64: ctLines.join("\n"),
      terminalPayload: termLines.join("\n"),
      portalPayload: portalLines.join("\n"),
    };
  }

  function populateOutputDetails(fields) {
    setText(el.outputCipherHex, maybeHexWithPreview(fields.cipherHex));
    setText(el.outputIvB64, fields.ivB64);
    setText(el.outputCtB64, fields.ciphertextB64);
    setText(el.outputTerminal, fields.terminalPayload);
    setText(el.outputPortal, fields.portalPayload);
  }

  function renderPrimaryActiveOutput(mode, traceOrWsTrace) {
    var activeMode = mode === "portal" ? "portal" : "terminal";
    mainState.lastEncryptMode = activeMode;

    var outputs = {
      cipherHex: "-",
      ivB64: "-",
      ciphertextB64: "-",
      terminalPayload: "-",
      portalPayload: "-",
    };

    if (traceOrWsTrace) {
      if (traceOrWsTrace.frames) {
        outputs = buildWsAggregateOutputs(traceOrWsTrace);
      } else if (traceOrWsTrace.outputs) {
        outputs = {
          cipherHex: traceOrWsTrace.outputs.ciphertextHex,
          ivB64: traceOrWsTrace.outputs.ivB64,
          ciphertextB64: traceOrWsTrace.outputs.ciphertextB64,
          terminalPayload: traceOrWsTrace.outputs.terminalPayload,
          portalPayload: traceOrWsTrace.outputs.portalPayload,
        };
      }
    }

    populateOutputDetails(outputs);
    if (el.outputPrimaryActiveLabel) {
      el.outputPrimaryActiveLabel.textContent = "Payload Aktif (" + activeMode + ")";
    }
    setText(
      el.outputPrimaryActive,
      activeMode === "portal" ? outputs.portalPayload : outputs.terminalPayload
    );
    if (el.outputOtherDetails) {
      el.outputOtherDetails.open = false;
    }
  }

  function renderPrimaryTrace(trace) {
    if (!trace) {
      return;
    }

    var stageLabel = "Single payload flow";
    var cbcLabel = "Single payload blocks";
    var wsTrace = getAdvancedWsTrace();
    if (
      isWsScenario() &&
      wsTrace &&
      wsTrace.frames &&
      wsTrace.frames[advancedState.selectedFrame]
    ) {
      var fr = wsTrace.frames[advancedState.selectedFrame];
      stageLabel =
        "WS frame #" +
        fr.frameIndex +
        " (bytes " +
        fr.chunkOffsetStart +
        "-" +
        fr.chunkOffsetEnd +
        ", " +
        fr.chunkLength +
        "B)";
      cbcLabel = "CBC detail untuk frame #" + fr.frameIndex;
    }
    if (el.stageContextLabel) el.stageContextLabel.textContent = stageLabel;
    if (el.cbcContextLabel) el.cbcContextLabel.textContent = cbcLabel;

    if (el.keyOrigin) el.keyOrigin.textContent =
      "Firmware runtime memakai DEVICE_AES_KEY di CryptoUtils. Halaman docs ini memakai demo key publik untuk audit visual.";
    if (el.keyDemoMask) el.keyDemoMask.textContent =
      trace.inputs.keyMasked + " (" + trace.inputs.keyLength + " byte)";

    var tsCtx = trace.inputs.timestampContext || AesTrace.formatEpochContext(trace.inputs.timestamp);
    if (el.stagePlaintext) el.stagePlaintext.textContent =
      "Text:\n" +
      (trace.inputs.plaintext || "-") +
      "\n\nUTF-8 bytes (hex):\n" +
      trace.stage.plaintextHex +
      "\n\nPreview dari hex:\n" +
      describeHexAsText(trace.stage.plaintextHex) +
      "\n\nLength: " +
      trace.inputs.plaintextLength +
      " byte";

    if (el.stageTimestamp) el.stageTimestamp.textContent =
      "Epoch: " +
      trace.inputs.timestamp +
      "\nUTC: " +
      tsCtx.utcIso +
      "\nLocal: " +
      tsCtx.localString +
      "\nRelative: " +
      tsCtx.relativeToNow +
      "\nHex (4-byte big-endian): " +
      trace.stage.timestampHex;

    if (el.stagePayload) el.stagePayload.textContent =
      "Payload = [timestamp(4B)] + [plaintext]\n\nHex:\n" +
      trace.stage.payloadHex +
      "\n\nPreview dari hex:\n" +
      describeHexAsText(trace.stage.payloadHex) +
      "\n\nLength: " +
      trace.stage.payloadLength +
      " byte";

    if (el.stagePadded) el.stagePadded.textContent =
      "PKCS7 pad length: " +
      trace.stage.padLength +
      "\n\nHex after padding:\n" +
      trace.stage.paddedHex +
      "\n\nPreview dari hex:\n" +
      describeHexAsText(trace.stage.paddedHex) +
      "\n\nTotal: REDACTED
      trace.stage.paddedLength +
      " byte";

    if (el.stageMeta) el.stageMeta.textContent =
      "Total block AES: REDACTED
      trace.blocks.length +
      " | Round per block: " +
      (AesTrace.AES_ROUNDS + 1) +
      " (0..14)";

    renderCbcBlocks(trace);
  }

  function populateFrameSelector() {
    clearNode(el.frameSelect);
    var wsTrace = getAdvancedWsTrace();
    if (
      !wsTrace ||
      !wsTrace.frames ||
      wsTrace.frames.length === 0
    ) {
      var emptyOpt = document.createElement("option");
      emptyOpt.value = "0";
      emptyOpt.textContent = "No frame";
      el.frameSelect.appendChild(emptyOpt);
      return;
    }

    for (var i = 0; i < wsTrace.frames.length; i++) {
      var frame = wsTrace.frames[i];
      var opt = document.createElement("option");
      opt.value = String(i);
      opt.textContent =
        "Frame " +
        frame.frameIndex +
        " (" +
        frame.chunkLength +
        "B, blocks=" +
        frame.blockCount +
        ")";
      el.frameSelect.appendChild(opt);
    }
    el.frameSelect.value = String(advancedState.selectedFrame);
  }

  function populateInspectorSelectors() {
    var trace = getActiveTrace();
    clearNode(el.blockSelect);
    clearNode(el.roundSelect);

    if (!trace) {
      return;
    }

    if (advancedState.selectedBlock >= trace.blocks.length) {
      advancedState.selectedBlock = 0;
    }

    for (var b = 0; b < trace.blocks.length; b++) {
      var blockOpt = document.createElement("option");
      blockOpt.value = String(b);
      blockOpt.textContent = "Block " + b;
      el.blockSelect.appendChild(blockOpt);
    }
    el.blockSelect.value = String(advancedState.selectedBlock);

    for (var r = 0; r <= AesTrace.AES_ROUNDS; r++) {
      var roundOpt = document.createElement("option");
      roundOpt.value = String(r);
      roundOpt.textContent =
        "Round " + r + (r === AesTrace.AES_ROUNDS ? " (final)" : "");
      el.roundSelect.appendChild(roundOpt);
    }
    el.roundSelect.value = String(advancedState.selectedRound);
  }

  function renderRoundInspector() {
    var trace = getActiveTrace();
    clearNode(el.roundTableBody);

    if (!trace || trace.blocks.length === 0) {
      el.roundContextLabel.textContent = "Belum ada trace aktif.";
      el.roundTitle.textContent = "-";
      el.roundNote.textContent = "-";
      return;
    }

    if (advancedState.selectedBlock >= trace.blocks.length) {
      advancedState.selectedBlock = 0;
    }

    var blockData = trace.blocks[advancedState.selectedBlock];
    if (advancedState.selectedRound >= blockData.rounds.length) {
      advancedState.selectedRound = 0;
    }
    var roundData = blockData.rounds[advancedState.selectedRound];
    var rows = buildRoundRows(roundData, blockData);
    var wsTrace = getAdvancedWsTrace();

    if (
      advancedState.selectedScenario === "ws" &&
      wsTrace &&
      wsTrace.frames &&
      wsTrace.frames[advancedState.selectedFrame]
    ) {
      var fr = wsTrace.frames[advancedState.selectedFrame];
      el.roundContextLabel.textContent =
        "Scenario: " + advancedState.selectedScenario + " | frame=" +
        fr.frameIndex +
        " | offset=" +
        fr.chunkOffsetStart +
        "-" +
        fr.chunkOffsetEnd +
        " | chunk=" +
        fr.chunkLength +
        "B";
    } else {
      el.roundContextLabel.textContent = "Scenario: single payload";
    }

    el.roundTitle.textContent =
      "Block " +
      advancedState.selectedBlock +
      " - Round " +
      advancedState.selectedRound +
      (advancedState.selectedRound === AesTrace.AES_ROUNDS
        ? " (final, tanpa MixColumns)"
        : "");

    el.roundNote.textContent =
      "Round input dihasilkan dari output ronde sebelumnya. CBC source = " +
      (advancedState.selectedBlock === 0 ? "IV" : "ciphertext block sebelumnya") +
      ".";

    for (var i = 0; i < rows.length; i++) {
      var tr = document.createElement("tr");
      tr.appendChild(createCell(rows[i].step, "step"));
      tr.appendChild(createCell(formatHexWithTextPreview(rows[i].hex), "mono mono-hex"));
      tr.appendChild(createCell(matrixTextFromHex(rows[i].hex), "matrix"));
      tr.appendChild(createCell(rows[i].purpose || "-", "purpose"));
      el.roundTableBody.appendChild(tr);
    }
  }

  function refreshActiveScenarioView() {
    var trace = getActiveTrace();
    if (!trace) {
      return;
    }
    renderPrimaryTrace(trace);
    populateInspectorSelectors();
    renderRoundInspector();
    if (el.decryptScenarioSelect) {
      el.decryptScenarioSelect.value = advancedState.selectedScenario;
      populateDecryptFrameSelector();
    }
  }

  function populateDecryptFrameSelector() {
    if (!el.decryptFrameSelect || !el.decryptScenarioSelect) return;
    clearNode(el.decryptFrameSelect);
    var decScenario = el.decryptScenarioSelect.value;
    if (decScenario === "single") return;
    var ws = getAdvancedWsTrace();
    if (!ws || !ws.frames || ws.frames.length === 0) {
      var empty = document.createElement("option");
      empty.value = "0";
      empty.textContent = "No frame";
      el.decryptFrameSelect.appendChild(empty);
      return;
    }
    for (var i = 0; i < ws.frames.length; i++) {
      var opt = document.createElement("option");
      opt.value = String(i);
      opt.textContent = "Frame " + i + " (" + ws.frames[i].chunkLength + "B)";
      el.decryptFrameSelect.appendChild(opt);
    }
    el.decryptFrameSelect.value = String(advancedState.selectedFrame);
  }

  function parseSingleInputAndTrace(ivHexOverride) {
    return AesTrace.encryptTrace({
      plaintext: el.plaintext.value,
      timestamp: Number(el.timestamp.value),
      ivHex: ivHexOverride || el.ivHex.value,
      keyText: AesTrace.DEMO_VECTOR.key,
    });
  }

  function runSingleTraceWithOptions(options) {
    var opts = options || {};
    var mode = opts.mode || (el.mode ? el.mode.value : "terminal");
    var syncMain = opts.syncMain !== false;
    var trace = parseSingleInputAndTrace(opts.ivHex);
    if (syncMain) {
      mainState.singleTrace = trace;
      mainState.wsTrace = null;
    }
    advancedState.traceSingle = trace;
    advancedState.traceWs = null;
    if (syncMain) {
      mainState.lastEncryptMode = mode;
    }
    advancedState.selectedScenario = "single";
    advancedState.selectedFrame = 0;
    advancedState.selectedBlock = 0;
    advancedState.selectedRound = 0;
    if (el.scenarioSelect) el.scenarioSelect.value = "single";
    if (el.decryptScenarioSelect) el.decryptScenarioSelect.value = "single";
    renderWsMetrics(null);
    populateFrameSelector();
    populateDecryptFrameSelector();
    refreshActiveScenarioView();
    if (syncMain) {
      renderPrimaryActiveOutput(mode, trace);
      renderEncryptSingleTimeline(trace, mode, opts.meta || null);
    }
    return trace;
  }

  function runWsTraceWithOptions(options) {
    var opts = options || {};
    var syncMain = opts.syncMain !== false;
    var skipTimeline = !!opts.skipTimeline;
    var mode = opts.mode || (el.mode ? el.mode.value : "terminal");
    var wsTrace = AesTrace.encryptWsFramesTrace({
      plaintext: opts.plaintext,
      baseTimestamp: Number(opts.baseTimestamp),
      mode: mode,
      keyText: AesTrace.DEMO_VECTOR.key,
      baseIvHex: opts.baseIvHex || AesTrace.DEMO_VECTOR.ivHex,
    });
    if (syncMain) {
      mainState.wsTrace = wsTrace;
      mainState.singleTrace = null;
    }
    advancedState.traceWs = wsTrace;
    advancedState.traceSingle = null;
    if (syncMain) {
      mainState.lastEncryptMode = mode;
    }
    advancedState.selectedScenario = opts.scenario || "ws";
    advancedState.selectedFrame = 0;
    advancedState.selectedBlock = 0;
    advancedState.selectedRound = 0;
    if (el.scenarioSelect) el.scenarioSelect.value = advancedState.selectedScenario;
    if (el.decryptScenarioSelect) el.decryptScenarioSelect.value = advancedState.selectedScenario;
    renderWsMetrics(wsTrace);
    populateFrameSelector();
    populateDecryptFrameSelector();
    refreshActiveScenarioView();
    if (syncMain) {
      renderPrimaryActiveOutput(mode, wsTrace);
    }
    if (!skipTimeline) {
      renderEncryptWsTimeline(wsTrace, opts.reason || "", opts.meta || null);
    }
    return wsTrace;
  }

  function runEncryptTrace() {
    try {
      var plaintext = el.plaintext.value;
      var timestamp = Number(el.timestamp.value);
      var ivHex = el.ivHex.value;
      var mode = el.mode.value;
      var plaintextLen = AesTrace.utf8ToBytes(plaintext).length;
      var forceWs = !!(el.forceWsChunking && el.forceWsChunking.checked);
      var ivConfig = deriveMainIvConfig();
      var effectiveIvHex = ivConfig.finalIvHex;
      mainState.lastEncryptMode = mode;

      if (forceWs && mode !== "terminal") {
        throw new Error("Paksa chunking WS hanya tersedia untuk mode terminal.");
      }

      if (mode === "portal" && plaintextLen > AesTrace.MAX_PLAINTEXT_SIZE) {
        throw new Error(
          "Mode portal mengikuti node single payload: plaintext maksimal 144 byte. Gunakan terminal untuk auto chunking."
        );
      }

      if (mode === "terminal" && (plaintextLen > AesTrace.MAX_PLAINTEXT_SIZE || forceWs)) {
        var chunkReason =
          plaintextLen > AesTrace.MAX_PLAINTEXT_SIZE
            ? "Auto chunking terminal (>144 byte)"
            : "Paksa chunking WS oleh user";
        runWsTraceWithOptions({
          plaintext: plaintext,
          baseTimestamp: timestamp,
          mode: mode,
          baseIvHex: effectiveIvHex || ivHex || AesTrace.DEMO_VECTOR.ivHex,
          scenario: "ws",
          reason: chunkReason,
          syncMain: true,
          meta: { ivConfig: ivConfig },
        });
        setAlert(
          el.alert,
          plaintextLen > AesTrace.MAX_PLAINTEXT_SIZE
            ? "Input terminal melebihi 144 byte. Payload dipecah otomatis per frame (sesuai perilaku node WS)."
            : "Chunking WS dipaksa manual. Payload diproses sebagai frame WS meskipun belum >144 byte.",
          "info"
        );
        scrollFlowSequence(el.encryptTraceSection, el.encryptOutputSection, 520);
        return;
      }

      var trace = runSingleTraceWithOptions({
        mode: mode,
        syncMain: true,
        ivHex: effectiveIvHex,
        meta: { ivConfig: ivConfig },
      });
      if (trace.warnings && trace.warnings.length > 0) {
        setAlert(el.alert, trace.warnings.join(" "), "warn");
      } else {
        setAlert(
          el.alert,
          ivConfig.mixEnabled
            ? "Trace AES berhasil dibuat dengan IV mix firmware aktif."
            : "Trace AES berhasil dibuat.",
          "ok"
        );
      }
      scrollFlowSequence(el.encryptTraceSection, el.encryptOutputSection, 520);
    } catch (error) {
      setAlert(el.alert, error.message || String(error), "error");
      renderEncryptTimeline("Trace enkripsi gagal.", [
        buildTraceItem("fail", "Validasi input", error.message || String(error)),
      ]);
      scrollToFlowTarget(el.encryptTraceSection);
    }
  }

  function runSingleTrace() {
    try {
      runSingleTraceWithOptions({ mode: el.mode.value });
    } catch (error) {
      setAlert(el.alert, error.message || String(error), "error");
    }
  }

  function renderWsMetrics(wsTrace) {
    var trace = wsTrace || { sourceLengthBytes: 0, frameCount: 0, totalBlocks: 0, totalRounds: 0, frames: [] };
    if (el.metricTotalBytes) el.metricTotalBytes.textContent = REDACTED
    if (el.metricTotalFrames) el.metricTotalFrames.textContent = REDACTED
    if (el.metricTotalBlocks) el.metricTotalBlocks.textContent = REDACTED
    if (el.metricTotalRounds) el.metricTotalRounds.textContent = REDACTED

    clearNode(el.wsFramesBody);
    if (!el.wsFramesBody || !trace.frames) return;
    for (var i = 0; i < trace.frames.length; i++) {
      var frame = trace.frames[i];
      var tr = document.createElement("tr");
      tr.appendChild(createCell(String(frame.frameIndex)));
      tr.appendChild(
        createCell(
          String(frame.chunkOffsetStart) + "-" + String(frame.chunkOffsetEnd)
        )
      );
      tr.appendChild(createCell(String(frame.chunkLength)));
      tr.appendChild(createCell(String(frame.blockCount)));
      tr.appendChild(createCell(String(frame.roundCountTotal)));
      tr.appendChild(createCell(frame.timestampContext.utcIso));
      tr.appendChild(createCell(truncateMiddle(frame.serializedPayload, 92), "mono"));
      el.wsFramesBody.appendChild(tr);
    }
  }

  function runLongPayloadTrace() {
    try {
      if (AesTrace.utf8ToBytes(el.longPayload.value).length === 0) {
        setAlert(el.longAlert, "Payload panjang kosong.", "error");
        return;
      }
      var wsTrace = runWsTraceWithOptions({
        plaintext: el.longPayload.value,
        baseTimestamp: Number(el.timestamp.value),
        mode: el.mode.value,
        baseIvHex: el.ivHex.value || AesTrace.DEMO_VECTOR.ivHex,
        scenario: "ws",
        reason: "Advanced long payload trace",
        syncMain: false,
        skipTimeline: true,
      });
      if (wsTrace.totalRounds > 14) {
        setAlert(
          el.longAlert,
          "Long payload trace siap. Total rounds = REDACTED
          "ok"
        );
      } else {
        setAlert(
          el.longAlert,
          "REDACTED",
          "warn"
        );
      }
    } catch (error) {
      setAlert(el.longAlert, error.message || String(error), "error");
    }
  }

  function loadLongPresetToInput() {
    var key = el.longPreset.value;
    if (AesTrace.LONG_PRESETS[key]) {
      el.longPayload.value = AesTrace.LONG_PRESETS[key];
      setAlert(
        el.longAlert,
        "Preset " + key + " dimuat. Jalankan trace untuk melihat frame/chunk.",
        "info"
      );
    } else {
      setAlert(el.longAlert, "Preset tidak dikenal.", "error");
    }
  }

  function resolveSkewWindow() {
    var profile = el.skewProfile ? el.skewProfile.value : "strict";
    if (profile === "strict") {
      return AesTrace.WS_REPLAY_SKEW_SEC_STRICT;
    }
    if (profile === "soft") {
      return AesTrace.WS_REPLAY_SKEW_SEC_SOFT;
    }
    var custom = Number(el.skewCustomInput ? el.skewCustomInput.value : AesTrace.WS_REPLAY_SKEW_SEC_STRICT);
    if (!Number.isFinite(custom)) {
      return AesTrace.WS_REPLAY_SKEW_SEC_STRICT;
    }
    if (custom < AesTrace.WS_REPLAY_SKEW_SEC_STRICT) {
      return AesTrace.WS_REPLAY_SKEW_SEC_STRICT;
    }
    if (custom > AesTrace.WS_REPLAY_SKEW_SEC_MAX) {
      return AesTrace.WS_REPLAY_SKEW_SEC_MAX;
    }
    return custom;
  }

  function setMono(node, value) {
    if (!node) return;
    node.textContent = value === undefined || value === null || value === "" ? "-" : String(value);
  }

  function clearDecryptPanels(message) {
    setMono(el.decryptStageMode, "-");
    setMono(el.decryptStageIv, "-");
    setMono(el.decryptStageCipher, "-");
    setMono(el.decryptStagePadded, "-");
    setMono(el.decryptStageUnpadded, "-");
    setMono(el.decryptStageTimestamp, "-");
    setMono(el.decryptStagePlaintextHex, "-");
    setMono(el.decryptStagePlaintextUtf8, "-");
    clearNode(el.decryptCbcBody);
    clearNode(el.decryptBlockSelect);
    clearNode(el.decryptRoundSelect);
    clearNode(el.decryptRoundTableBody);
    el.decryptStageContextLabel.textContent = message || "Belum ada trace dekripsi.";
    el.decryptRoundContextLabel.textContent = "Belum ada block dekripsi.";
    el.decryptRoundTitle.textContent = "-";
    el.decryptRoundNote.textContent = "-";
  }

  function populateDecryptSelectors(result) {
    clearNode(el.decryptBlockSelect);
    clearNode(el.decryptRoundSelect);

    var blocks = result && result.decryptBlocks ? result.decryptBlocks : [];
    if (blocks.length === 0) {
      var emptyBlock = document.createElement("option");
      emptyBlock.value = "0";
      emptyBlock.textContent = "No decrypt block";
      el.decryptBlockSelect.appendChild(emptyBlock);

      var emptyRound = document.createElement("option");
      emptyRound.value = String(AesTrace.AES_ROUNDS);
      emptyRound.textContent = "No round";
      el.decryptRoundSelect.appendChild(emptyRound);
      return;
    }

    if (advancedState.selectedDecryptBlock >= blocks.length) {
      advancedState.selectedDecryptBlock = 0;
    }

    for (var b = 0; b < blocks.length; b++) {
      var blockOpt = document.createElement("option");
      blockOpt.value = String(b);
      blockOpt.textContent = "Block " + b;
      el.decryptBlockSelect.appendChild(blockOpt);
    }
    el.decryptBlockSelect.value = String(advancedState.selectedDecryptBlock);

    for (var r = AesTrace.AES_ROUNDS; r >= 0; r--) {
      var roundOpt = document.createElement("option");
      roundOpt.value = String(r);
      if (r === AesTrace.AES_ROUNDS) {
        roundOpt.textContent = "Round 14 (start inverse)";
      } else if (r === 0) {
        roundOpt.textContent = "Round 0 (akhir AES + CBC XOR)";
      } else {
        roundOpt.textContent = "Round " + r;
      }
      el.decryptRoundSelect.appendChild(roundOpt);
    }

    var desiredRound = Number(advancedState.selectedDecryptRound);
    if (!Number.isFinite(desiredRound)) {
      desiredRound = AesTrace.AES_ROUNDS;
    }
    if (desiredRound < 0 || desiredRound > AesTrace.AES_ROUNDS) {
      desiredRound = AesTrace.AES_ROUNDS;
    }
    advancedState.selectedDecryptRound = desiredRound;
    el.decryptRoundSelect.value = String(desiredRound);
  }

  function renderDecryptRoundInspector() {
    clearNode(el.decryptRoundTableBody);
    if (
      !mainState.validationResult ||
      !mainState.validationResult.decryptBlocks ||
      mainState.validationResult.decryptBlocks.length === 0
    ) {
      el.decryptRoundContextLabel.textContent = "Belum ada block dekripsi.";
      el.decryptRoundTitle.textContent = "-";
      el.decryptRoundNote.textContent = "-";
      return;
    }

    var blockCount = mainState.validationResult.decryptBlocks.length;
    if (advancedState.selectedDecryptBlock >= blockCount) {
      advancedState.selectedDecryptBlock = 0;
    }
    var blockData = mainState.validationResult.decryptBlocks[advancedState.selectedDecryptBlock];
    var rounds = blockData.rounds || [];
    if (rounds.length === 0) {
      el.decryptRoundContextLabel.textContent = "Trace ronde dekripsi belum tersedia.";
      el.decryptRoundTitle.textContent = "-";
      el.decryptRoundNote.textContent = "-";
      return;
    }

    var selectedRound = Number(advancedState.selectedDecryptRound);
    if (!Number.isFinite(selectedRound)) {
      selectedRound = AesTrace.AES_ROUNDS;
    }
    var roundData = null;
    for (var i = 0; i < rounds.length; i++) {
      if (rounds[i].round === selectedRound) {
        roundData = rounds[i];
        break;
      }
    }
    if (!roundData) {
      roundData = rounds[0];
      selectedRound = roundData.round;
      advancedState.selectedDecryptRound = selectedRound;
      el.decryptRoundSelect.value = String(selectedRound);
    }

    el.decryptRoundContextLabel.textContent =
      "Decrypt block " +
      advancedState.selectedDecryptBlock +
      "REDACTED" +
      blockCount +
      " block.";

    if (selectedRound === AesTrace.AES_ROUNDS) {
      el.decryptRoundTitle.textContent = "Round 14 (awal inverse AES)";
    } else if (selectedRound === 0) {
      el.decryptRoundTitle.textContent = "Round 0 (akhir inverse AES + CBC XOR)";
    } else {
      el.decryptRoundTitle.textContent = "Round " + selectedRound;
    }

    el.decryptRoundNote.textContent =
      "Urutan dekripsi berjalan mundur 14 -> 0. CBC source block ini = " +
      (advancedState.selectedDecryptBlock === 0 ? "IV" : "ciphertext block sebelumnya") +
      ".";

    var rows = buildDecryptRoundRows(roundData, blockData);
    for (var r = 0; r < rows.length; r++) {
      var tr = document.createElement("tr");
      tr.appendChild(createCell(rows[r].step, "step"));
      tr.appendChild(createCell(formatHexWithTextPreview(rows[r].hex), "mono mono-hex"));
      tr.appendChild(createCell(matrixTextFromHex(rows[r].hex), "matrix"));
      tr.appendChild(createCell(rows[r].purpose || "-", "purpose"));
      el.decryptRoundTableBody.appendChild(tr);
    }
  }

  function renderDecryptCbcTable(result) {
    clearNode(el.decryptCbcBody);
    var blocks = result && result.decryptBlocks ? result.decryptBlocks : [];
    for (var i = 0; i < blocks.length; i++) {
      var row = blocks[i];
      var tr = document.createElement("tr");
      tr.appendChild(createCell(String(row.blockIndex)));
      tr.appendChild(createCell(formatHexWithTextPreview(row.ciphertextBlockHex || "-"), "mono mono-hex"));
      tr.appendChild(createCell(formatHexWithTextPreview(row.cbcSourceHex || "-"), "mono mono-hex"));
      tr.appendChild(createCell(formatHexWithTextPreview(row.aesOutputHex || "-"), "mono mono-hex"));
      tr.appendChild(createCell(formatHexWithTextPreview(row.plaintextBlockHex || "-"), "mono mono-hex"));
      el.decryptCbcBody.appendChild(tr);
    }
  }

  function renderDecryptDetail(result, deviceEpoch) {
    mainState.validationResult = result || null;
    if (!result) {
      clearDecryptPanels("Belum ada trace dekripsi.");
      return;
    }

    var blockCount = result.decryptBlocks ? result.decryptBlocks.length : 0;
    var contextLabel =
      "Trace dekripsi: mode=" +
      (result.mode || el.validationMode.value || "-") +
      " | blocks=" +
      blockCount +
      " | status=" +
      (result.ok ? "success" : "fail");
    el.decryptStageContextLabel.textContent = contextLabel;

    setMono(el.decryptStageMode, result.mode || "-");
    setMono(el.decryptStageIv, formatHexWithTextPreview(result.ivHex || "-"));
    setMono(el.decryptStageCipher, formatHexWithTextPreview(result.ciphertextHex || "-"));
    setMono(el.decryptStagePadded, formatHexWithTextPreview(result.decryptedPaddedHex || "-"));
    setMono(el.decryptStageUnpadded, formatHexWithTextPreview(result.unpaddedHex || result.payloadHex || "-"));
    setMono(el.decryptStagePlaintextHex, formatHexWithTextPreview(result.plaintextHex || "-"));
    setMono(el.decryptStagePlaintextUtf8, result.plaintext || "-");

    if (result.timestamp !== undefined && result.timestamp !== null) {
      var tsCtx =
        result.timestampContext ||
        AesTrace.formatEpochContext(result.timestamp, Number(deviceEpoch));
      setMono(
        el.decryptStageTimestamp,
        "Epoch: " +
        result.timestamp +
        "\nUTC: " +
        tsCtx.utcIso +
        "\nLocal: " +
        tsCtx.localString +
        "\nRelative: " +
        tsCtx.relativeToNow +
        "\nHex (4-byte BE): " +
        (result.timestampHex || "-")
      );
    } else {
      setMono(el.decryptStageTimestamp, "-");
    }

    renderDecryptCbcTable(result);
    populateDecryptSelectors(result);
    renderDecryptRoundInspector();
  }

  function renderValidation() {
    var payload = el.validationPayload ? el.validationPayload.value : "";
    var modeRaw = el.validationMode ? el.validationMode.value : "auto";
    var mode = modeRaw === "auto" ? undefined : modeRaw;
    var deviceEpoch = Number(el.deviceEpoch ? el.deviceEpoch.value : nowEpochSeconds());
    var timeSynced = el.timeSynced ? !!el.timeSynced.checked : true;
    var skewWindowSec = resolveSkewWindow();

    var result = AesTrace.validateSerialized(payload, {
      mode: mode,
      keyText: AesTrace.DEMO_VECTOR.key,
      deviceEpoch: deviceEpoch,
      timeSynced: timeSynced,
      skewWindowSec: skewWindowSec,
    });

    var timeline = [];
    var failStep = null;
    for (var i = 0; i < result.steps.length; i++) {
      var step = result.steps[i];
      var primaryFail = false;
      if (!step.ok && !failStep) {
        failStep = step;
        primaryFail = true;
      }
      timeline.push(
        buildTraceItem(
          step.ok ? "pass" : REDACTED
          step.name,
          step.detail,
          primaryFail
        )
      );
    }
    renderValidationTimeline(timeline);
    if (el.validationFailAnchor) {
      if (failStep) {
        el.validationFailAnchor.hidden = false;
        el.validationFailAnchor.textContent =
          "Fail utama: " +
          failStep.name +
          " - " +
          (failStep.detail || "-");
      } else {
        el.validationFailAnchor.hidden = true;
        el.validationFailAnchor.textContent = "";
      }
    }

    if (result.ok) {
      var tsCtx = result.timestampContext || AesTrace.formatEpochContext(result.timestamp, deviceEpoch);
      if (el.validationSummary) {
        el.validationSummary.className = "result-box ok";
        el.validationSummary.textContent =
        "Decrypt success | timestamp=" +
        result.timestamp +
        " | mode=" +
        (result.mode || modeRaw) +
        " | utc=" +
        tsCtx.utcIso +
        " | local=" +
        tsCtx.localString +
        " | relative=" +
        tsCtx.relativeToNow +
        " | plaintext=\"" +
        result.plaintext +
        "\"";
      }
    } else {
      if (el.validationSummary) {
        el.validationSummary.className = "result-box fail";
        el.validationSummary.textContent =
        "Decrypt gagal di langkah " +
        (failStep ? "\"" + failStep.name + "\"" : "unknown") +
        ". Detail: " +
        (failStep ? failStep.detail : "lihat list step di atas.");
      }
    }

    renderDecryptDetail(result, deviceEpoch);
    scrollFlowSequence(el.decryptTraceSection, el.decryptOutputSection, 520);
  }

  function updateSkewCustomVisibility() {
    if (!el.skewCustomWrap || !el.skewProfile) return;
    el.skewCustomWrap.hidden = el.skewProfile.value !== "custom";
  }

  function runIvMix() {
    try {
      var result = AesTrace.deriveFirmwareIv({
        rawIvHex: el.ivRawInput.value,
        micros: Number(el.ivMicrosInput.value),
        rssi: Number(el.ivRssiInput.value),
      });

      el.ivBeforeHex.textContent = result.rawIvHex;
      el.ivBeforeB64.textContent = result.rawIvB64;
      el.ivAfterHex.textContent = result.finalIvHex;
      el.ivAfterB64.textContent = result.finalIvB64;
      el.ivMasks.textContent =
        "masks[0..3] = " +
        result.masksHex.join(" ") +
        " | rssiMask=0x" +
        result.rssiMaskHex;

      clearNode(el.ivByteDiffBody);
      for (var i = 0; i < result.byteDiff.length; i++) {
        var row = result.byteDiff[i];
        var tr = document.createElement("tr");
        tr.appendChild(createCell(String(row.index)));
        tr.appendChild(createCell(row.beforeHex, "mono"));
        tr.appendChild(createCell(row.maskHex, "mono"));
        tr.appendChild(createCell(row.afterHex, "mono"));
        tr.appendChild(createCell(row.source));
        el.ivByteDiffBody.appendChild(tr);
      }

      setAlert(
        el.ivAlert,
        "IV firmware mix berhasil dihitung dari raw IV + micros + RSSI.",
        "ok"
      );
    } catch (error) {
      setAlert(el.ivAlert, error.message || String(error), "error");
    }
  }

  function randomHex(bytes) {
    var out = "";
    for (var i = 0; i < bytes; i++) {
      var v = Math.floor(Math.random() * 256);
      out += (v < 16 ? "0" : "") + v.toString(16);
    }
    return out;
  }

  function addSelfcheckResult(parent, label, ok, extra) {
    var li = document.createElement("li");
    li.className = ok ? "pass" : "fail";
    li.textContent =
      (ok ? "PASS" : REDACTED
    parent.appendChild(li);
  }

  function runSelfCheck() {
    clearNode(el.selfcheckResults);

    var trace = AesTrace.encryptTrace({
      plaintext: AesTrace.DEMO_VECTOR.plaintext,
      timestamp: AesTrace.DEMO_VECTOR.timestamp,
      ivHex: AesTrace.DEMO_VECTOR.ivHex,
      keyText: AesTrace.DEMO_VECTOR.key,
    });

    addSelfcheckResult(
      el.selfcheckResults,
      "Ciphertext hex vector",
      trace.outputs.ciphertextHex === AesTrace.DEMO_VECTOR.expectedCiphertextHex
    );
    addSelfcheckResult(
      el.selfcheckResults,
      "Terminal payload vector",
      trace.outputs.terminalPayload === AesTrace.DEMO_VECTOR.expectedTerminalPayload
    );
    addSelfcheckResult(
      el.selfcheckResults,
      "Portal payload vector",
      trace.outputs.portalPayload === AesTrace.DEMO_VECTOR.expectedPortalPayload
    );
    addSelfcheckResult(
      el.selfcheckResults,
      "Round count block-0",
      trace.blocks[0].rounds.length === 15
    );
    addSelfcheckResult(
      el.selfcheckResults,
      "Final round tanpa MixColumns",
      trace.blocks[0].rounds[14].mixColumnsHex === undefined
    );

    var decryptResult = AesTrace.validateSerialized(trace.outputs.terminalPayload, {
      mode: "terminal",
      keyText: AesTrace.DEMO_VECTOR.key,
      deviceEpoch: AesTrace.DEMO_VECTOR.timestamp,
      timeSynced: true,
      skewWindowSec: AesTrace.WS_REPLAY_SKEW_SEC_STRICT,
    });
    addSelfcheckResult(el.selfcheckResults, "Decrypt vector success", decryptResult.ok);
    addSelfcheckResult(
      el.selfcheckResults,
      "Decrypt block-0 punya 15 ronde",
      !!(
        decryptResult.decryptBlocks &&
        decryptResult.decryptBlocks[0] &&
        decryptResult.decryptBlocks[0].rounds &&
        decryptResult.decryptBlocks[0].rounds.length === 15
      )
    );
    addSelfcheckResult(
      el.selfcheckResults,
      "Decrypt round-14 tanpa InvMixColumns",
      !!(
        decryptResult.decryptBlocks &&
        decryptResult.decryptBlocks[0] &&
        decryptResult.decryptBlocks[0].rounds &&
        decryptResult.decryptBlocks[0].rounds[0] &&
        decryptResult.decryptBlocks[0].rounds[0].round === 14 &&
        decryptResult.decryptBlocks[0].rounds[0].invMixColumnsHex === undefined
      )
    );

    var wsJson = AesTrace.encryptWsFramesTrace({
      plaintext: AesTrace.LONG_PRESETS.statusJson,
      baseTimestamp: AesTrace.DEMO_VECTOR.timestamp,
      mode: "terminal",
      keyText: AesTrace.DEMO_VECTOR.key,
      baseIvHex: AesTrace.DEMO_VECTOR.ivHex,
    });
    var wsText = AesTrace.encryptWsFramesTrace({
      plaintext: AesTrace.LONG_PRESETS.wsStatusText,
      baseTimestamp: AesTrace.DEMO_VECTOR.timestamp,
      mode: "terminal",
      keyText: AesTrace.DEMO_VECTOR.key,
      baseIvHex: AesTrace.DEMO_VECTOR.ivHex,
    });

    addSelfcheckResult(
      el.selfcheckResults,
      "REDACTED",
      wsJson.totalRounds > 14,
      "total=REDACTED
    );
    addSelfcheckResult(
      el.selfcheckResults,
      "REDACTED",
      wsText.totalRounds > 14,
      "total=REDACTED
    );

    var chunkRuleOk = true;
    for (var i = 0; i < wsJson.frames.length; i++) {
      if (wsJson.frames[i].chunkLength > AesTrace.WS_CHUNK_SIZE) {
        chunkRuleOk = false;
        break;
      }
    }
    addSelfcheckResult(
      el.selfcheckResults,
      "WS chunk length <= 144 byte",
      chunkRuleOk
    );

    var ivCheck = AesTrace.deriveFirmwareIv({
      rawIvHex: "00112233445566778899aabbccddeeff",
      micros: 0x00a1b2c3,
      rssi: -67,
    });
    var expected0 = (0x00 ^ 0xc3).toString(16);
    var expected0Hex = expected0.length < 2 ? "0" + expected0 : expected0;
    addSelfcheckResult(
      el.selfcheckResults,
      "IV byte[0] XOR micros low byte",
      ivCheck.byteDiff[0].afterHex === expected0Hex
    );
  }

  function resetToDemo() {
    if (el.encryptPreset) el.encryptPreset.value = "demo";
    if (el.decryptPreset) el.decryptPreset.value = "valid";
    if (el.plaintext) el.plaintext.value = AesTrace.DEMO_VECTOR.plaintext;
    if (el.timestamp) el.timestamp.value = String(AesTrace.DEMO_VECTOR.timestamp);
    if (el.ivHex) el.ivHex.value = AesTrace.DEMO_VECTOR.ivHex;
    if (el.mode) el.mode.value = "terminal";
    if (el.forceWsChunking) el.forceWsChunking.checked = false;
    if (el.mainIvMixEnabled) el.mainIvMixEnabled.checked = true;
    if (el.mainIvMicrosInput) el.mainIvMicrosInput.value = "123456";
    if (el.mainIvRssiInput) el.mainIvRssiInput.value = "-67";
    if (el.validationMode) el.validationMode.value = DECRYPT_PRESETS.valid.mode;
    if (el.validationPayload) el.validationPayload.value = DECRYPT_PRESETS.valid.payload;
    if (el.deviceEpoch) el.deviceEpoch.value = String(DECRYPT_PRESETS.valid.deviceEpoch);
    if (el.timeSynced) el.timeSynced.checked = !!DECRYPT_PRESETS.valid.timeSynced;
    if (el.skewProfile) el.skewProfile.value = DECRYPT_PRESETS.valid.skewProfile;
    if (el.skewCustomInput) el.skewCustomInput.value = String(DECRYPT_PRESETS.valid.skewCustom);
    updateSkewCustomVisibility();

    if (el.longPreset) el.longPreset.value = "statusJson";
    if (el.longPayload) el.longPayload.value = AesTrace.LONG_PRESETS.statusJson;

    if (el.ivRawInput) el.ivRawInput.value = AesTrace.DEMO_VECTOR.ivHex;
    if (el.ivMicrosInput) el.ivMicrosInput.value = "123456";
    if (el.ivRssiInput) el.ivRssiInput.value = "-67";

    advancedState.selectedScenario = "single";
    advancedState.selectedFrame = 0;
    advancedState.selectedBlock = 0;
    advancedState.selectedRound = 0;
    advancedState.selectedDecryptBlock = 0;
    advancedState.selectedDecryptRound = AesTrace.AES_ROUNDS;
    mainState.singleTrace = null;
    mainState.wsTrace = null;
    mainState.validationResult = null;
    mainState.lastEncryptMode = "terminal";
    advancedState.traceSingle = null;
    advancedState.traceWs = null;

    renderWsMetrics(null);
    populateFrameSelector();
    populateDecryptFrameSelector();
    renderPrimaryActiveOutput("terminal", null);
    if (el.encryptTraceSummary) {
      el.encryptTraceSummary.textContent = "Belum ada trace enkripsi.";
    }
    clearNode(el.encryptTraceList);
    if (el.outputOtherDetails) {
      el.outputOtherDetails.open = false;
    }
    if (el.validationSummary) {
      el.validationSummary.className = "result-box";
      el.validationSummary.textContent = "Belum ada trace dekripsi.";
    }
    renderDecryptPresetHint("valid", DECRYPT_PRESETS.valid);
    clearNode(el.validationSteps);
    if (el.validationFailAnchor) {
      el.validationFailAnchor.hidden = true;
      el.validationFailAnchor.textContent = "";
    }
    if (el.advancedPanel) {
      el.advancedPanel.open = false;
      var advancedSections = el.advancedPanel.querySelectorAll("details");
      for (var s = 0; s < advancedSections.length; s++) {
        advancedSections[s].open = false;
      }
    }
    renderMainInputPreview();
    clearDecryptPanels("Belum ada trace dekripsi.");
  }

  function ensureWsShortScenario() {
    try {
      var wsTrace = AesTrace.encryptWsFramesTrace({
        plaintext: el.plaintext.value,
        baseTimestamp: Number(el.timestamp.value),
        mode: el.mode.value,
        keyText: AesTrace.DEMO_VECTOR.key,
        baseIvHex: el.ivHex.value || AesTrace.DEMO_VECTOR.ivHex,
      });
      advancedState.traceWs = wsTrace;
      advancedState.selectedFrame = 0;
      renderWsMetrics(wsTrace);
      populateFrameSelector();
      return true;
    } catch (error) {
      setAlert(el.alert, error.message || String(error), "error");
      return false;
    }
  }

  function handleScenarioChange(newScenario) {
    advancedState.selectedScenario = newScenario;
    advancedState.selectedBlock = 0;
    advancedState.selectedRound = 0;

    if (newScenario === "ws-short") {
      if (!ensureWsShortScenario()) {
        advancedState.selectedScenario = "single";
      }
    }
    if (newScenario === "ws") {
      var wsTrace = getAdvancedWsTrace();
      if (!wsTrace || !wsTrace.frames || wsTrace.frames.length === 0) {
        ensureWsShortScenario();
      }
    }

    if (el.scenarioSelect) el.scenarioSelect.value = advancedState.selectedScenario;
    if (el.decryptScenarioSelect) el.decryptScenarioSelect.value = advancedState.selectedScenario;
    refreshActiveScenarioView();
  }

  function wireEvents() {
    if (el.runBtn) {
      el.runBtn.addEventListener("click", function () {
        runEncryptTrace();
      });
    }
    if (el.demoBtn) {
      el.demoBtn.addEventListener("click", function () {
        resetToDemo();
        updateEncryptionEpochContext();
        runEncryptTrace();
        renderValidation();
      });
    }
    if (el.encryptLoadPresetBtn) {
      el.encryptLoadPresetBtn.addEventListener("click", function () {
        loadEncryptPresetToInput(false);
      });
    }
    if (el.encryptPreset) {
      el.encryptPreset.addEventListener("change", function () {
        loadEncryptPresetToInput(false);
      });
    }
    if (el.decryptLoadPresetBtn) {
      el.decryptLoadPresetBtn.addEventListener("click", function () {
        loadDecryptPresetToInput(false);
      });
    }
    if (el.decryptPreset) {
      el.decryptPreset.addEventListener("change", function () {
        loadDecryptPresetToInput(false);
      });
    }
    if (el.timestampNowBtn) {
      el.timestampNowBtn.addEventListener("click", function () {
        el.timestamp.value = String(nowEpochSeconds());
        updateEncryptionEpochContext();
        renderMainInputPreview();
      });
    }
    if (el.timestamp) {
      el.timestamp.addEventListener("input", function () {
        updateEncryptionEpochContext();
        renderMainInputPreview();
      });
      el.timestamp.addEventListener("change", function () {
        updateEncryptionEpochContext();
        renderMainInputPreview();
      });
    }
    if (el.timestampReadable) {
      var syncEpochFromReadableInput = function (warnIfInvalid) {
        var raw = el.timestampReadable.value;
        if (!raw) return;
        var epoch = datetimeLocalValueToEpoch(raw);
        if (epoch === null) {
          if (warnIfInvalid) {
            setAlert(
              el.alert,
              "Waktu lokal tidak valid. Gunakan format tanggal/jam yang benar.",
              "warn"
            );
          }
          return;
        }
        if (el.timestamp) {
          el.timestamp.value = String(epoch);
        }
        updateEncryptionEpochContext();
        renderMainInputPreview();
      };

      el.timestampReadable.addEventListener("input", function () {
        syncEpochFromReadableInput(false);
      });
      el.timestampReadable.addEventListener("change", function () {
        syncEpochFromReadableInput(true);
      });
    }
    if (el.plaintext) {
      el.plaintext.addEventListener("input", renderMainInputPreview);
    }
    if (el.ivHex) {
      el.ivHex.addEventListener("input", renderMainInputPreview);
    }
    if (el.mainIvMicrosInput) {
      el.mainIvMicrosInput.addEventListener("input", renderMainInputPreview);
    }
    if (el.mainIvRssiInput) {
      el.mainIvRssiInput.addEventListener("input", renderMainInputPreview);
    }
    if (el.mainIvMixToggle && el.mainIvMixEnabled) {
      el.mainIvMixToggle.addEventListener("click", function () {
        el.mainIvMixEnabled.checked = !el.mainIvMixEnabled.checked;
        renderMainInputPreview();
      });
    }
    if (el.mainIvMixEnabled) {
      el.mainIvMixEnabled.addEventListener("change", renderMainInputPreview);
    }
    if (el.forceWsChunking) {
      el.forceWsChunking.addEventListener("change", function () {
        if (el.forceWsChunking.checked && el.mode && el.mode.value !== "terminal") {
          el.forceWsChunking.checked = false;
          setAlert(el.alert, "Paksa chunking WS hanya bisa dipakai di mode terminal.", "warn");
        }
      });
    }
    if (el.deviceEpoch) {
      el.deviceEpoch.addEventListener("input", updateValidationEpochContext);
      el.deviceEpoch.addEventListener("change", updateValidationEpochContext);
    }
    if (el.deviceEpochReadable) {
      var syncDeviceEpochFromReadableInput = function (showValidationError) {
        var raw = el.deviceEpochReadable.value;
        if (!raw) return;
        var epoch = datetimeLocalValueToEpoch(raw);
        if (epoch === null) {
          if (showValidationError) {
            el.deviceEpochReadable.setCustomValidity(
              "Waktu lokal tidak valid untuk epoch perangkat."
            );
            if (typeof el.deviceEpochReadable.reportValidity === "function") {
              el.deviceEpochReadable.reportValidity();
            }
          }
          return;
        }
        el.deviceEpochReadable.setCustomValidity("");
        if (el.deviceEpoch) {
          el.deviceEpoch.value = String(epoch);
        }
        updateValidationEpochContext();
      };
      el.deviceEpochReadable.addEventListener("input", function () {
        syncDeviceEpochFromReadableInput(false);
      });
      el.deviceEpochReadable.addEventListener("change", function () {
        syncDeviceEpochFromReadableInput(true);
      });
    }
    if (el.deviceEpochNowBtn) {
      el.deviceEpochNowBtn.addEventListener("click", function () {
        if (el.deviceEpoch) {
          el.deviceEpoch.value = String(nowEpochSeconds());
          updateValidationEpochContext();
        }
      });
    }
    if (el.ivRandomBtn) {
      el.ivRandomBtn.addEventListener("click", function () {
        el.ivHex.value = randomHex(16);
        renderMainInputPreview();
      });
    }
    if (el.mode) {
      el.mode.addEventListener("change", function () {
        mainState.lastEncryptMode = el.mode.value || "terminal";
        if (el.mode.value !== "terminal" && el.forceWsChunking && el.forceWsChunking.checked) {
          el.forceWsChunking.checked = false;
          setAlert(el.alert, "Paksa chunking WS dinonaktifkan karena mode portal.", "info");
        }
        renderMainInputPreview();
      });
    }

    if (el.loadPresetBtn) el.loadPresetBtn.addEventListener("click", loadLongPresetToInput);
    if (el.runLongBtn) el.runLongBtn.addEventListener("click", runLongPayloadTrace);
    if (el.longPreset) el.longPreset.addEventListener("change", loadLongPresetToInput);

    if (el.scenarioSelect) {
      el.scenarioSelect.addEventListener("change", function () {
        handleScenarioChange(el.scenarioSelect.value);
      });
    }
    if (el.frameSelect) {
      el.frameSelect.addEventListener("change", function () {
        advancedState.selectedFrame = Number(el.frameSelect.value);
        advancedState.selectedBlock = 0;
        advancedState.selectedRound = 0;
        refreshActiveScenarioView();
      });
    }
    if (el.blockSelect) {
      el.blockSelect.addEventListener("change", function () {
        advancedState.selectedBlock = Number(el.blockSelect.value);
        renderRoundInspector();
      });
    }
    if (el.roundSelect) {
      el.roundSelect.addEventListener("change", function () {
        advancedState.selectedRound = Number(el.roundSelect.value);
        renderRoundInspector();
      });
    }
    if (el.decryptBlockSelect) {
      el.decryptBlockSelect.addEventListener("change", function () {
        advancedState.selectedDecryptBlock = Number(el.decryptBlockSelect.value);
        advancedState.selectedDecryptRound = AesTrace.AES_ROUNDS;
        if (el.decryptRoundSelect) {
          el.decryptRoundSelect.value = String(advancedState.selectedDecryptRound);
        }
        renderDecryptRoundInspector();
      });
    }
    if (el.decryptRoundSelect) {
      el.decryptRoundSelect.addEventListener("change", function () {
        advancedState.selectedDecryptRound = Number(el.decryptRoundSelect.value);
        renderDecryptRoundInspector();
      });
    }
    if (el.decryptScenarioSelect) {
      el.decryptScenarioSelect.addEventListener("change", function () {
        handleScenarioChange(el.decryptScenarioSelect.value);
      });
    }
    if (el.decryptFrameSelect) {
      el.decryptFrameSelect.addEventListener("change", function () {
        advancedState.selectedFrame = Number(el.decryptFrameSelect.value);
        if (el.frameSelect) el.frameSelect.value = String(advancedState.selectedFrame);
        advancedState.selectedBlock = 0;
        advancedState.selectedRound = 0;
        refreshActiveScenarioView();
      });
    }
    if (el.decryptCompactBtn) {
      el.decryptCompactBtn.addEventListener("click", function () {
        advancedState.compactDecrypt = !advancedState.compactDecrypt;
        var section = el.decryptCompactBtn.closest(".card-body");
        if (section) section.classList.toggle("compact-rounds", advancedState.compactDecrypt);
        el.decryptCompactBtn.textContent = advancedState.compactDecrypt ? "Lengkap" : "Ringkas";
      });
    }
    if (el.validationPayload) {
      el.validationPayload.addEventListener("input", updatePayloadTimestampPreview);
      el.validationPayload.addEventListener("change", updatePayloadTimestampPreview);
    }
    if (el.validationMode) {
      el.validationMode.addEventListener("change", updatePayloadTimestampPreview);
    }
    if (el.skewProfile) {
      el.skewProfile.addEventListener("change", updateSkewCustomVisibility);
    }
    if (el.validateBtn) {
      el.validateBtn.addEventListener("click", function () {
        updateValidationEpochContext();
        renderValidation();
      });
    }
    if (el.ivUseCurrentBtn) {
      el.ivUseCurrentBtn.addEventListener("click", function () {
        el.ivRawInput.value = el.ivHex.value || randomHex(16);
        if (!el.ivMicrosInput.value) {
          el.ivMicrosInput.value = String(Math.floor(Math.random() * 1000000));
        }
        if (!el.ivRssiInput.value) {
          el.ivRssiInput.value = String(-30 - Math.floor(Math.random() * 60));
        }
        runIvMix();
      });
    }
    if (el.ivRunBtn) {
      el.ivRunBtn.addEventListener("click", runIvMix);
    }
    if (el.useLongToMainBtn) {
      el.useLongToMainBtn.addEventListener("click", function () {
        if (el.plaintext) {
          el.plaintext.value = el.longPayload ? el.longPayload.value : "";
        }
        if (el.mode) {
          el.mode.value = "terminal";
          mainState.lastEncryptMode = "terminal";
        }
        renderMainInputPreview();
        activateMainTab("main-tab-aes");
        activateTab("tab-encrypt");
        setAlert(
          el.alert,
          "Payload panjang disalin ke input utama. Klik Trace Enkripsi untuk menjalankan flow utama.",
          "info"
        );
        scrollToFlowTarget(el.plaintext);
      });
    }
    if (el.useIvmixToMainBtn) {
      el.useIvmixToMainBtn.addEventListener("click", function () {
        var candidate = "";
        if (el.ivAfterHex && el.ivAfterHex.textContent && el.ivAfterHex.textContent !== "-") {
          candidate = el.ivAfterHex.textContent.trim();
        }
        if (!candidate && el.ivBeforeHex && el.ivBeforeHex.textContent && el.ivBeforeHex.textContent !== "-") {
          candidate = el.ivBeforeHex.textContent.trim();
        }
        if (!candidate && el.ivRawInput) {
          candidate = el.ivRawInput.value.trim();
        }
        try {
          var ivBytes = AesTrace.hexToBytes(candidate);
          if (ivBytes.length !== 16) {
            throw new Error("IV harus 16 byte (32 karakter hex).");
          }
          if (el.ivHex) {
            el.ivHex.value = candidate.toLowerCase();
          }
          renderMainInputPreview();
          activateMainTab("main-tab-aes");
          activateTab("tab-encrypt");
          setAlert(
            el.alert,
            "IV hasil advanced disalin ke input utama. Klik Trace Enkripsi untuk memakai IV ini.",
            "info"
          );
          scrollToFlowTarget(el.ivHex);
        } catch (error) {
          setAlert(
            el.ivAlert,
            "Gagal salin ke utama: " + (error && error.message ? error.message : "IV tidak valid."),
            "error"
          );
        }
      });
    }
    if (el.selfcheckBtn) {
      el.selfcheckBtn.addEventListener("click", runSelfCheck);
    }

    if (el.cachePreset) {
      el.cachePreset.addEventListener("change", function () {
        var lib = getCacheTraceLib();
        if (!lib || !el.cachePresetNote) return;
        var meta = lib.PRESETS[el.cachePreset.value];
        el.cachePresetNote.textContent = meta && meta.note ? meta.note : "";
      });
    }
    if (el.cacheLoadPresetBtn) {
      el.cacheLoadPresetBtn.addEventListener("click", function () {
        runCacheAction(function (lib, ctx) {
          return lib.loadPreset(ctx, el.cachePreset ? el.cachePreset.value : "normal_ok");
        });
      });
    }
    if (el.cacheScaleMode) {
      el.cacheScaleMode.addEventListener("change", function () {
        ensureCacheContext(true);
        var lib = getCacheTraceLib();
        var state = cacheState.context && lib ? lib.snapshot(cacheState.context) : null;
        setCacheTraceSummary(
          "Skala diubah ke " + (el.cacheScaleMode ? el.cacheScaleMode.value : "visual") + "."
        );
        renderCacheTraceTimeline([]);
        renderCacheOutputState(state);
        renderCacheCrcDetail("-");
      });
    }
    if (el.cacheAutoFlushToggle) {
      el.cacheAutoFlushToggle.addEventListener("click", function () {
        cacheState.autoFlush = !cacheState.autoFlush;
        setCacheAutoFlushUi();
      });
    }
    if (el.cacheFlushBtn) {
      el.cacheFlushBtn.addEventListener("click", function () {
        runCacheAction(function (lib, ctx) {
          return lib.flushRtcToLittleFs(ctx, "manual");
        });
      });
    }

    if (el.rtcTsEpoch) {
      el.rtcTsEpoch.addEventListener("input", updateRtcLocalFromEpochInput);
      el.rtcTsEpoch.addEventListener("change", updateRtcLocalFromEpochInput);
    }
    if (el.rtcTsLocal) {
      el.rtcTsLocal.addEventListener("input", function () {
        syncRtcEpochFromLocal(false);
      });
      el.rtcTsLocal.addEventListener("change", function () {
        syncRtcEpochFromLocal(true);
      });
    }

    if (el.rtcAppendBtn) {
      el.rtcAppendBtn.addEventListener("click", function () {
        runCacheAction(function (lib, ctx) {
          return lib.rtcAppend(ctx, getRtcInputRecord(), { autoFlush: cacheState.autoFlush });
        });
      });
    }
    if (el.rtcPeekBtn) {
      el.rtcPeekBtn.addEventListener("click", function () {
        runCacheAction(function (lib, ctx) {
          return lib.rtcPeek(ctx);
        });
      });
    }
    if (el.rtcPopBtn) {
      el.rtcPopBtn.addEventListener("click", function () {
        runCacheAction(function (lib, ctx) {
          return lib.rtcPop(ctx);
        });
      });
    }
    if (el.rtcClearBtn) {
      el.rtcClearBtn.addEventListener("click", function () {
        runCacheAction(function (lib, ctx) {
          return lib.rtcClear(ctx);
        });
      });
    }
    if (el.rtcInjectMagicBtn) {
      el.rtcInjectMagicBtn.addEventListener("click", function () {
        runCacheAction(function (lib, ctx) {
          return lib.rtcInjectMagicInvalid(ctx);
        });
      });
    }
    if (el.rtcInjectCrcBtn) {
      el.rtcInjectCrcBtn.addEventListener("click", function () {
        runCacheAction(function (lib, ctx) {
          return lib.rtcInjectCrcMismatch(ctx);
        });
      });
    }
    if (el.rtcInjectBoundaryBtn) {
      el.rtcInjectBoundaryBtn.addEventListener("click", function () {
        runCacheAction(function (lib, ctx) {
          return lib.rtcInjectBoundaryCorrupt(ctx);
        });
      });
    }
    if (el.rtcSimFallbackOkBtn) {
      el.rtcSimFallbackOkBtn.addEventListener("click", function () {
        runCacheAction(function (lib, ctx) {
          return lib.simulateRtcIngressFallback(ctx, "rtc_fail_to_fs", getRtcInputRecord());
        });
      });
    }
    if (el.rtcSimFallbackBothFailBtn) {
      el.rtcSimFallbackBothFailBtn.addEventListener("click", function () {
        runCacheAction(function (lib, ctx) {
          return lib.simulateRtcIngressFallback(ctx, "both_fail", getRtcInputRecord());
        });
      });
    }

    if (el.fsWriteBtn) {
      el.fsWriteBtn.addEventListener("click", function () {
        runCacheAction(function (lib, ctx) {
          return lib.fsWrite(ctx, el.fsPayloadInput ? el.fsPayloadInput.value : "");
        });
      });
    }
    if (el.fsReadBtn) {
      el.fsReadBtn.addEventListener("click", function () {
        runCacheAction(function (lib, ctx) {
          return lib.fsReadOne(ctx, parseNumber(el.fsReadBuffer ? el.fsReadBuffer.value : 512, 512));
        });
      });
    }
    if (el.fsPopBtn) {
      el.fsPopBtn.addEventListener("click", function () {
        runCacheAction(function (lib, ctx) {
          return lib.fsPopOne(ctx);
        });
      });
    }
    if (el.fsClearBtn) {
      el.fsClearBtn.addEventListener("click", function () {
        runCacheAction(function (lib, ctx) {
          return lib.fsClear(ctx);
        });
      });
    }
    if (el.fsInjectCrcBtn) {
      el.fsInjectCrcBtn.addEventListener("click", function () {
        runCacheAction(function (lib, ctx) {
          return lib.fsInjectCrcMismatch(ctx);
        });
      });
    }
    if (el.fsInjectMagicBtn) {
      el.fsInjectMagicBtn.addEventListener("click", function () {
        runCacheAction(function (lib, ctx) {
          return lib.fsInjectMagicCorrupt(ctx);
        });
      });
    }
    if (el.fsInjectSyncBtn) {
      el.fsInjectSyncBtn.addEventListener("click", function () {
        runCacheAction(function (lib, ctx) {
          return lib.fsInjectSyncLoss(ctx);
        });
      });
    }

    if (el.toggleCompactBtn) {
      el.toggleCompactBtn.addEventListener("click", function () {
        advancedState.compactRounds = !advancedState.compactRounds;
        if (el.stepContainer) {
          el.stepContainer.classList.toggle("compact-rounds", advancedState.compactRounds);
        }
        el.toggleCompactBtn.textContent = advancedState.compactRounds ? "Lengkap" : "Ringkas";
      });
    }
  }

  function bootstrap() {
    initThemeToggle();
    initCopyButtons();
    initMainTabs();
    initAesTabs();
    activateCacheSubtabs();
    applyFieldVisualKinds();
    resetToDemo();
    initCacheFlow();
    wireEvents();
    updateEncryptionEpochContext();
    updateValidationEpochContext();
    renderMainInputPreview();
    runEncryptTrace();
    renderValidation();
  }

  bootstrap();
})();
