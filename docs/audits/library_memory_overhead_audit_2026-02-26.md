# Library Memory/Overhead Audit - 2026-02-26

## Scope and Policy
- Included: `lib/GreenhouseCommon`, `lib/vendor/ESPAsyncWebServer`, `lib/vendor/ESPAsyncTCP`, `lib/vendor/arduino-sht`, `lib/vendor/BH1750`.
- Excluded: `.platformio/packages`, Node dependencies, long-soak on-device instrumentation.
- Huge-overhead threshold used in this audit:
  - RAM `>= 8 KB`
  - Flash `>= 30 KB`
  - Any clear unbounded heap-growth risk in active path.
- Method used: static analysis (`pio check`), build metrics (`pio run -t size`, archives, `nm`), targeted ownership/lifetime code review, existing stress test (`pio test -e native -f test_native_stress -v`).

## Build and Test Evidence
- Firmware size (`wemosd1mini_usb`):
  - `text=765224`, `data=1732`, `bss=29728`, `dec=796684`.
  - Derived RAM use (`data+bss`): `31460` bytes.
  - Derived flash image (`text+data`): `766956` bytes.
- Static analyzer (`pio check -e wemosd1mini_usb --skip-packages --fail-on-defect=high`):
  - `HIGH=0`, `MEDIUM=4`, `LOW=92`.
  - Medium warnings are in `lib/vendor/arduino-sht/SHTSensor.h`.
- Stress test (`pio test -e native -f test_native_stress -v`):
  - Passed (`6 tests, 0 failures`) including 10,000-cycle simulation.

## Quantified Overhead Tables

### A. Archive Size Contributors (Build Artifact)
| Archive | Size (bytes) | Class | Notes |
|---|---:|---|---|
| `libGreenhouseCommon.a` | 1,342,226 | Local | Largest controllable archive |
| `libFrameworkArduino.a` | 1,105,806 | Framework | Out of scope for local code changes |
| `libESPAsyncWebServer.a` | 733,640 | Local vendor | Major local vendor contributor |
| `libESP8266WiFi.a` | 581,630 | Framework | Out of scope |
| `libESP8266mDNS.a` | 368,248 | Framework | Out of scope |
| `libLittleFS.a` | 235,530 | Framework | Out of scope |
| `libESPAsyncTCP.a` | 187,656 | Local vendor | Local vendor contributor |
| `libarduino-sht.a` | 25,518 | Local vendor | Small |
| `libBH1750.a` | 11,404 | Local vendor | Small |

Local-library archive sum (controllable): `2,300,444` of `4,858,784` total archive bytes (`47.35%`).

### B. Top Linked Symbols (Firmware ELF)
| Symbol | Size (bytes) | Component |
|---|---:|---|
| `_ZL11PORTAL_HTML` | 30,205 | GreenhouseCommon web assets |
| `_ZL9CRYPTO_JS` | 17,478 | GreenhouseCommon web assets (copy 1) |
| `_ZL9CRYPTO_JS` | 17,478 | GreenhouseCommon web assets (copy 2) |
| `_ZL15CONNECTING_HTML` | 11,103 | GreenhouseCommon web assets |
| `_ZL10INDEX_HTML` | 9,278 | GreenhouseCommon web assets |
| `_ZL14REBOOTING_HTML` | 8,348 | GreenhouseCommon web assets |
| `_ZL13TERMINAL_HTML` | 6,670 | GreenhouseCommon web assets |
| `_ZL11ROOT_CA_PEM` | 1,940 | GreenhouseCommon cert literal (copy 1) |
| `_ZL11ROOT_CA_PEM` | 1,940 | GreenhouseCommon cert literal (copy 2) |

## Section A - Confirmed/Potential Leak Risks

### AUD-001
- ID: `AUD-001`
- Component: `lib/vendor/ESPAsyncTCP`
- File:Line: `lib/vendor/ESPAsyncTCP/src/ESPAsyncTCPbuffer.cpp:349`, `lib/vendor/ESPAsyncTCP/src/ESPAsyncTCPbuffer.cpp:378`
- Type: `Memory safety (allocator mismatch)`
- Certainty: `High`
- Impact: `Heap corruption risk in active TX path; can manifest as crash/fragmentation/leak-like behavior`
- Why: Buffer is allocated with `new[]` and released with scalar `delete`.
- Evidence:
  - `char *out = new (std::nothrow) char[available];` at line 349.
  - `delete out;` at line 378.
  - This occurs in `_sendBuffer()` loop during normal send path.
- Recommended Fix:
  - Replace `delete out;` with `delete[] out;`.
  - Optional follow-up: avoid per-iteration heap allocation by reusing a scratch buffer.
- Risk Level: `Critical`

No other reviewed path in this audit met the leak criteria with an ownership/lifetime path proving unbounded growth.

## Section B - High-Impact Inefficiencies and Waste

### AUD-002
- ID: `AUD-002`
- Component: `lib/GreenhouseCommon` web assets
- File:Line: `include/WebAppData.h:2395`, `include/WebAppData.h:4285`, `include/WebAppData.h:4287`, `lib/GreenhouseCommon/PortalServer.Routes.cpp:65`
- Type: `Flash overhead (static payload)`
- Certainty: `High`
- Impact: `30,205 bytes flash for a single symbol` (`>=30 KB`, meets huge-overhead threshold)
- Why: `PORTAL_HTML` is embedded and referenced directly; `PORTAL_HTML_GZIPPED` is `false`.
- Evidence:
  - `PORTAL_HTML_LEN = 30205`, `PORTAL_HTML_GZIPPED = false`.
  - Route uses `PORTAL_HTML` in `beginResponse_P(...)`.
  - ELF symbol `_ZL11PORTAL_HTML` size is `0x75fd` (30,205 bytes).
- Recommended Fix:
  - Move portal page to compressed asset flow (or split dynamic parts so static shell can be gzipped).
  - Alternative: host heavy UI in LittleFS and stream compressed content.
- Risk Level: `High`

### AUD-003
- ID: `AUD-003`
- Component: `lib/vendor/ESPAsyncWebServer`
- File:Line: `lib/vendor/ESPAsyncWebServer/src/WebResponseImpl.h:80`, `lib/vendor/ESPAsyncWebServer/src/WebResponses.cpp:523`, `:526`, `:577`, `:582`, `:604`, `:617`
- Type: `Heap churn / fragmentation risk`
- Certainty: `Medium`
- Impact: `Frequent-path allocator churn in templated responses; destabilization risk under repeated portal/app requests`
- Why: `_cache` is a `std::vector<uint8_t>` with repeated front `erase()` and front `insert()` in template processing loop.
- Evidence:
  - `_cache.erase(_cache.begin(), ...)` in streaming path.
  - `_cache.insert(_cache.begin(), ...)` appears repeatedly during placeholder handling.
  - These operations are O(n) and can trigger realloc/memmove on heap.
- Recommended Fix:
  - Replace front-insert/erase vector pattern with ring buffer/deque-like queue (head/tail indices).
  - Pre-reserve bounded capacity for template working set.
- Risk Level: `High`

### AUD-004
- ID: `AUD-004`
- Component: `lib/GreenhouseCommon` web assets linkage
- File:Line: `include/WebAppData.h:708`, `lib/GreenhouseCommon/AppServer.Routes.cpp:73`, `lib/GreenhouseCommon/PortalServer.Routes.cpp:86`
- Type: `Flash waste (duplicate embedded asset)`
- Certainty: `High`
- Impact: `17,478 bytes duplicate flash` (one extra `CRYPTO_JS` copy)
- Why: `CRYPTO_JS` is defined in a header as `const` and consumed by two translation units.
- Evidence:
  - `nm -A libGreenhouseCommon.a` shows `_ZL9CRYPTO_JS` in both `AppServer.Routes.cpp.o` and `PortalServer.Routes.cpp.o`, each `0x4446`.
  - `firmware.elf` also contains two `_ZL9CRYPTO_JS` symbols of `0x4446`.
- Recommended Fix:
  - Convert header definition to `extern` declaration and keep single definition in one `.cpp`.
- Risk Level: `Medium`

### AUD-005
- ID: `AUD-005`
- Component: `lib/GreenhouseCommon` cert data linkage
- File:Line: `include/root_ca_data.h:22`, `lib/GreenhouseCommon/ApiClient.cpp:125`, `lib/GreenhouseCommon/OtaManager.cpp:98`
- Type: `Flash waste (duplicate cert blob)`
- Certainty: `High`
- Impact: `1,940 bytes duplicate flash`
- Why: `ROOT_CA_PEM` is `static const` in header and is emitted in multiple translation units.
- Evidence:
  - `nm -A libGreenhouseCommon.a` shows `_ZL11ROOT_CA_PEM` in both `ApiClient.cpp.o` and `OtaManager.cpp.o`, each `0x794`.
  - `firmware.elf` contains two `_ZL11ROOT_CA_PEM` symbols.
- Recommended Fix:
  - Use single-definition model (`extern` declaration in header; one `.cpp` definition).
- Risk Level: `Low`

### AUD-006
- ID: `AUD-006`
- Component: `lib/vendor/arduino-sht`
- File:Line: `lib/vendor/arduino-sht/SHTSensor.h:211`, `:212`, `:235`; `lib/vendor/arduino-sht/SHTSensor.cpp:43`
- Type: `Initialization safety (not a leak)`
- Certainty: `Medium`
- Impact: `Potential undefined read before first successful sample`
- Why: Base class `SHTSensorDriver` members `mTemperature` and `mHumidity` are not initialized; default `readSample()` returns false.
- Evidence:
  - Members declared at lines 211-212 without constructor initialization.
  - Static analysis reports medium warning `uninitDerivedMemberVar`.
- Recommended Fix:
  - Initialize both members to invalid sentinel in base constructor.
- Risk Level: `Medium`

## Section C - Aggressive Redesign Candidates
| Candidate | Expected Gain | Migration Risk | Notes |
|---|---|---|---|
| Patch `ESPAsyncTCPbuffer.cpp` to fix `delete[]` and remove per-loop heap alloc | Stability first; lower fragmentation/crash risk | Low-Medium | Immediate candidate |
| Move large web assets out of header-defined const blobs | `~19.4 KB` flash reduction from known duplicates now; additional savings possible with compression | Medium | No API/wire change required |
| Rework template response cache to ring buffer | Lower allocator churn during dynamic page serving | Medium-High | Vendor patch maintenance burden |
| Compress or externalize `PORTAL_HTML` | Symbol currently `30,205 B` (huge threshold hit) | Medium | Best flash win in one item |

## Section D - Quick Wins First Patch Queue
1. `AUD-001`: Fix `delete[]` mismatch in `ESPAsyncTCPbuffer.cpp` and retest (`Critical`, low effort).
2. `AUD-004` and `AUD-005`: Deduplicate header-defined large constants using `extern` + single `.cpp` definitions (`Medium/Low`, low effort).
3. `AUD-002`: Reduce portal HTML footprint (compression/splitting strategy) (`High`, medium effort).
4. `AUD-003`: Replace `_cache` front insert/erase pattern with ring buffer approach (`High`, medium-high effort).
5. `AUD-006`: Initialize `arduino-sht` base members to sentinel values (`Medium`, low effort).

## Reviewed Non-Issues (Leak Guardrails)
- `AsyncWebSocket` queue growth is bounded (`WS_MAX_QUEUED_MESSAGES` on ESP8266) and handles overflow by close/discard:
  - `lib/vendor/ESPAsyncWebServer/src/AsyncWebSocket.h:22`
  - `lib/vendor/ESPAsyncWebServer/src/AsyncWebSocket.cpp:472-495`
- `AsyncWebServerRequest` upload buffer has explicit free paths in destructor and upload completion:
  - `lib/vendor/ESPAsyncWebServer/src/WebRequest.cpp:120-122`
  - `lib/vendor/ESPAsyncWebServer/src/WebRequest.cpp:723-727`
  - `lib/vendor/ESPAsyncWebServer/src/WebRequest.cpp:791-792`
- Local WS diagnostics state is lazily allocated and explicitly released:
  - `lib/GreenhouseCommon/utils.cpp:213-223`
  - `lib/GreenhouseCommon/DiagnosticsTerminal.cpp:132-142`

