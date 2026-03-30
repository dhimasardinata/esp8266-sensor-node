(function () {
  var storageKey = "gh-node-theme";
  var root = document.documentElement;
  var toggleButtons = document.querySelectorAll("[data-theme-toggle]");
  var mediaQuery =
    window.matchMedia &&
    window.matchMedia("(prefers-color-scheme: dark)");

  function getStoredTheme() {
    try {
      var value = window.localStorage.getItem(storageKey);
      return value === "light" || value === "dark" ? value : null;
    } catch (e) {
      return null;
    }
  }

  function syncToggle(theme) {
    toggleButtons.forEach(function (button) {
      var nextTheme = theme === "dark" ? "light" : "dark";
      var icon = button.querySelector(".theme-toggle-icon");
      button.dataset.nextTheme = nextTheme;
      button.title =
        nextTheme === "light"
          ? "Switch to light mode"
          : "Switch to dark mode";
      button.setAttribute("aria-label", button.title);
      if (icon) {
        icon.textContent = nextTheme === "light" ? "☀" : "🌙";
      }
    });
  }

  function applyTheme(theme, persist) {
    root.setAttribute("data-theme", theme);
    root.style.colorScheme = theme;
    if (persist) {
      try {
        window.localStorage.setItem(storageKey, theme);
      } catch (e) { }
    }
    syncToggle(theme);
  }

  toggleButtons.forEach(function (button) {
    button.addEventListener("click", function () {
      applyTheme(this.dataset.nextTheme || "light", true);
    });
  });

  if (mediaQuery) {
    var handleSystemThemeChange = function (event) {
      if (getStoredTheme()) return;
      applyTheme(event.matches ? "dark" : "light", false);
    };
    if (typeof mediaQuery.addEventListener === "function") {
      mediaQuery.addEventListener("change", handleSystemThemeChange);
    } else if (typeof mediaQuery.addListener === "function") {
      mediaQuery.addListener(handleSystemThemeChange);
    }
  }

  syncToggle(root.getAttribute("data-theme") || "dark");
})();

(function () {
  const RAW_KEY = "12345678901234567890123456789012";
  let KEY_PARSED = null;

  function getKey() {
    if (!KEY_PARSED && window.CryptoJS) {
      KEY_PARSED = CryptoJS.enc.Utf8.parse(RAW_KEY);
    }
    return KEY_PARSED;
  }

  // Updated encryption function for terminal.html / crypto.js
  window.encryptMessage = async function (plaintext) {
    if (!window.CryptoJS) return null;

    // 1. Get Timestamp (Seconds)
    const now = Math.floor(Date.now() / 1000);

    // 2. Create timestamp bytes (4 bytes, big-endian)
    const tsBytes = [
      (now >> 24) & 0xff,
      (now >> 16) & 0xff,
      (now >> 8) & 0xff,
      now & 0xff,
    ];

    // 3. Convert plaintext to bytes
    const textBytes = CryptoJS.enc.Utf8.parse(plaintext);

    // 4. Combine: [4 bytes TS] + [text bytes]
    // Create a new WordArray with timestamp
    const tsWA = CryptoJS.lib.WordArray.create(
      [
        (tsBytes[0] << 24) |
        (tsBytes[1] << 16) |
        (tsBytes[2] << 8) |
        tsBytes[3],
      ],
      4,
    );

    // Concatenate text
    const combined = tsWA.clone();
    combined.concat(textBytes);

    // 5. Encrypt
    const key = getKey();
    const iv = CryptoJS.lib.WordArray.random(16);

    const encrypted = CryptoJS.AES.encrypt(combined, key, {
      iv: iv,
      mode: CryptoJS.mode.CBC,
      padding: CryptoJS.pad.Pkcs7,
    });

    const ivB64 = CryptoJS.enc.Base64.stringify(iv);
    const cipherB64 = CryptoJS.enc.Base64.stringify(encrypted.ciphertext);
    return ivB64 + ":" + cipherB64;
  };

  window.decryptMessage = async function (payload) {
    if (!window.CryptoJS || !payload) return null;
    const colonIndex = payload.indexOf(":");
    if (colonIndex === -1 || colonIndex === 0) return null;
    const ivB64 = payload.substring(0, colonIndex);
    const cipherB64 = payload.substring(colonIndex + 1);
    if (!ivB64 || !cipherB64) return null;
    try {
      const key = getKey();
      const iv = CryptoJS.enc.Base64.parse(ivB64);
      const ciphertext = CryptoJS.enc.Base64.parse(cipherB64);
      if (iv.sigBytes !== 16) return null;
      if (ciphertext.sigBytes === 0 || ciphertext.sigBytes % 16 !== 0)
        return null;
      const cipherParams = CryptoJS.lib.CipherParams.create({
        ciphertext: ciphertext,
      });
      const decrypted = CryptoJS.AES.decrypt(cipherParams, key, {
        iv: iv,
        mode: CryptoJS.mode.CBC,
        padding: CryptoJS.pad.Pkcs7,
      });
      if (decrypted.sigBytes <= 4) return null;

      // Proper way to remove first 4 bytes (timestamp):
      // Convert to Latin1 string, slice bytes, then parse back as UTF8

      const latin1Str = CryptoJS.enc.Latin1.stringify(decrypted);
      const textWithoutTimestamp = latin1Str.substring(4); // Remove first 4 bytes

      // Convert the remaining bytes to UTF8 string
      try {
        return decodeURIComponent(escape(textWithoutTimestamp));
      } catch (e) {
        // Fallback if UTF8 decode fails
        return textWithoutTimestamp;
      }
    } catch (e) {
      return null;
    }
  };
})();

(function () {
  const consolePre = document.getElementById("console");
  const cmdInput = document.getElementById("cmd-input");
  const sendBtn = document.getElementById("send");
  const clearBtn = document.getElementById("clear-btn");
  const statusInd = document.getElementById("status-ind");
  const statusText = document.getElementById("status-text");
  const nodeGh = document.getElementById("node-gh");
  const nodeFw = document.getElementById("node-fw");
  const inputBar = document.getElementById("input-bar");

  const wsUrl =
    (location.protocol === "https:" ? "wss:" : "ws:") +
    "//" +
    location.hostname +
    "/ws";
  let ws = null;
  let reconnectTimer = null;
  let bannerPrinted = false;
  const pendingSystemMessages = [];

  function measureCharWidth(fontPx, fontFamily) {
    const cvs = document.createElement("canvas");
    const ctx = cvs.getContext("2d");
    ctx.font = fontPx + "px " + fontFamily;
    return ctx.measureText("M").width || fontPx * 0.6;
  }

  function maxColumns(text) {
    return Math.max(
      ...text.split("\n").map((l) => l.replace(/\t/g, "    ").length),
      1,
    );
  }

  function lineCount(text) {
    return text.split("\n").length || 1;
  }

  function fitBannerFont(bannerSpan) {
    if (!bannerSpan) return;
    const text = bannerSpan.textContent || "";
    if (!text) return;
    const comp = window.getComputedStyle(bannerSpan);
    const baselinePx = parseFloat(comp.fontSize) || 24;
    const fontFamily = comp.fontFamily || "monospace";
    const cols = Math.max(1, maxColumns(text));
    const rows = Math.max(1, lineCount(text));
    const consoleWidth = Math.max(40, consolePre.clientWidth - 32) * 0.95;
    const consoleHeight = Math.max(40, consolePre.clientHeight - 32);
    const charWidth = measureCharWidth(baselinePx, fontFamily);
    const bannerWidthPx = cols * charWidth;
    const MAX_BANNER_FRACTION = 0.28;
    const maxBannerHeight = Math.max(
      40,
      consoleHeight * MAX_BANNER_FRACTION,
    );
    const baselineLineHeight =
      parseFloat(comp.lineHeight) || baselinePx * 0.95;
    const baselineBannerHeight = rows * baselineLineHeight;
    let scaleW =
      bannerWidthPx > consoleWidth ? consoleWidth / bannerWidthPx : 1;
    let scaleH =
      baselineBannerHeight > maxBannerHeight
        ? maxBannerHeight / baselineBannerHeight
        : 1;
    let scale = Math.max(0.15, Math.min(scaleW, scaleH));
    bannerSpan.style.fontSize =
      Math.max(8, Math.floor(baselinePx * scale)) + "px";
    bannerSpan.style.lineHeight = "1";
    setTimeout(() => {
      consolePre.scrollTop = consolePre.scrollHeight;
    }, 30);
  }

  function fitAllBanners() {
    consolePre
      .querySelectorAll(".ascii-banner")
      .forEach((b) => fitBannerFont(b));
  }

  function appendSpan(text, className) {
    const span = document.createElement("span");
    if (className) span.className = className;
    span.textContent = text;
    consolePre.appendChild(span);
    if (!text.endsWith("\n"))
      consolePre.appendChild(document.createTextNode("\n"));
    setTimeout(() => {
      consolePre.scrollTop = consolePre.scrollHeight;
    }, 20);
    return span;
  }

  function printBanner(nodeId, fwVersion) {
    const bannerText = [
      "███╗   ██╗ ██████╗ ██████╗ ███████╗     ██████╗██╗     ██╗",
      "████╗  ██║██╔═══██╗██╔══██╗██╔════╝    ██╔════╝██║     ██║",
      "██╔██╗ ██║██║   ██║██║  ██║█████╗      ██║     ██║     ██║",
      "██║╚██╗██║██║   ██║██║  ██║██╔══╝      ██║     ██║     ██║",
      "██║ ╚████║╚██████╔╝██████╔╝███████╗    ╚██████╗███████╗██║",
      "╚═╝  ╚═══╝ ╚═════╝ ╚═════╝ ╚══════╝     ╚═════╝╚══════╝╚═╝",
      "",
      `  📦 Firmware v${fwVersion}`,
      `  🏷️  Node ID: ${nodeId}`,
      "",
    ].join("\n");

    const wrap = document.createElement("span");
    wrap.className = "ascii-wrap";
    const bannerSpan = document.createElement("span");
    bannerSpan.className = "ascii-banner";
    bannerSpan.textContent = bannerText + "\n";
    wrap.appendChild(bannerSpan);
    consolePre.appendChild(wrap);

    appendSystem(
      "✅ Connection established. Type 'help' for available commands.",
    );
    bannerPrinted = true;
    while (pendingSystemMessages.length)
      appendSystem(pendingSystemMessages.shift());
    setTimeout(fitAllBanners, 60);
  }

  function appendSystem(msg) {
    if (!bannerPrinted) pendingSystemMessages.push(msg);
    else appendSpan(msg + "\n", "system-msg");
  }

  function syncClientTime() {
    const epoch = Date.now();
    fetch("/api/time?epoch=" + epoch, { method: "POST" }).catch(() => { });
  }

  function connectWS() {
    if (ws) return;
    try {
      ws = new WebSocket(wsUrl);
    } catch (e) {
      scheduleReconnect();
      return;
    }

    ws.onopen = function () {
      appendSystem("🔌 WebSocket connected to server.");
      statusInd.classList.add("connected");
      statusText.textContent = "CONNECTED";
      syncClientTime();
      setTimeout(() => {
        sendCmd("status");
      }, 200);
    };

    ws.onmessage = async function (ev) {
      try {
        const decryptedText = await window.decryptMessage(ev.data);
        if (decryptedText === null || decryptedText === "") {
          appendSystem("⚠️ Decryption failed.");
          return;
        }
        try {
          const data = JSON.parse(decryptedText);
          if (data.type === "init") {
            nodeGh.textContent = "GH-" + data.nodeId;
            nodeFw.textContent = "FW: " + data.firmwareVersion;
            printBanner(data.nodeId, data.firmwareVersion);
            return;
          }
        } catch (e) { }
        appendSpan(decryptedText);
        setTimeout(fitAllBanners, 40);
      } catch (e) { }
    };

    ws.onclose = function () {
      appendSystem("🔌 Disconnected. Attempting to reconnect...");
      statusInd.classList.remove("connected");
      statusText.textContent = "DISCONNECTED";
      ws = null;
      scheduleReconnect();
    };

    ws.onerror = function () {
      if (ws) {
        try {
          ws.close();
        } catch (e) { }
        ws = null;
      }
      scheduleReconnect();
    };
  }

  function scheduleReconnect() {
    if (reconnectTimer) return;
    reconnectTimer = setTimeout(() => {
      reconnectTimer = null;
      connectWS();
    }, 2000);
  }

  async function sendCmd(cmd) {
    if (!cmd) return;
    appendSpan("❯ " + cmd + "\n", "cmd-line");
    if (ws && ws.readyState === WebSocket.OPEN) {
      try {
        const encryptedCmd = await window.encryptMessage(cmd);
        if (encryptedCmd) {
          ws.send(encryptedCmd);
        } else {
          appendSystem("⚠️ Encryption failed.");
        }
      } catch (e) {
        appendSystem("⚠️ Command encryption failed.");
      }
    } else {
      appendSystem("⚠️ Not connected. Command not sent.");
    }
  }

  clearBtn.addEventListener("click", () => {
    consolePre.textContent = "";
    bannerPrinted = false;
    pendingSystemMessages.length = 0;
    appendSystem("🗑️ Console cleared.");
  });

  sendBtn.addEventListener("click", () => {
    const v = cmdInput.value.trim();
    if (v) {
      sendCmd(v);
      cmdInput.value = "";
    }
    cmdInput.focus();
  });

  cmdInput.addEventListener("keydown", (e) => {
    if (e.key === "Enter") {
      e.preventDefault();
      const v = cmdInput.value.trim();
      if (v) {
        sendCmd(v);
        cmdInput.value = "";
      }
    }
  });

  function adjustForKeyboard() {
    if (window.visualViewport) {
      const kb = Math.max(
        0,
        window.innerHeight - window.visualViewport.height,
      );
      inputBar.style.bottom = kb + 8 + "px";
      consolePre.scrollTop = consolePre.scrollHeight;
    } else {
      cmdInput.scrollIntoView({ behavior: "smooth", block: "center" });
    }
  }

  if (window.visualViewport) {
    window.visualViewport.addEventListener("resize", adjustForKeyboard);
    window.visualViewport.addEventListener("scroll", adjustForKeyboard);
  }

  cmdInput.addEventListener("focus", () =>
    setTimeout(adjustForKeyboard, 50),
  );
  cmdInput.addEventListener(
    "blur",
    () => (inputBar.style.bottom = "env(safe-area-inset-bottom, 0)"),
  );

  window.addEventListener("resize", () => setTimeout(fitAllBanners, 80));
  window.addEventListener("orientationchange", () =>
    setTimeout(fitAllBanners, 120),
  );

  window.addEventListener("load", () => {
    appendSystem("⏳ Initializing terminal...");
    connectWS();
    setTimeout(() => {
      consolePre.scrollTop = consolePre.scrollHeight;
    }, 80);
  });

  window.__fitAllBanners = fitAllBanners;
})();
