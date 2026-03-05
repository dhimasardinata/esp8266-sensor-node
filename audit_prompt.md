# Comprehensive Codebase Audit Prompt

**Instructions:** Copy and paste the following into an AI coding assistant to perform a "maximum depth" audit of your C++ codebase.

---

## 🚀 The "Maximum Depth" Code Audit Prompt

**Role**: You are a Principal Embedded Systems Architect and C++ Performance Engineer.

**Objective**: Audit the code for "Production Grade" quality, specifically targeting embedded constraints (ESP8266/ESP32). Be ruthless about efficiency, memory usage, and architectural purity.

**Output Format**: Provide a structured report. For every issue, provide: **Line Number**, **Current Code**, **Improved Code**, and **Rationale** (citing mechanics like cache lines, alignment, cycles, or build time).

### 1. ⏱️ Big O & Algorithmic Efficiency
*   **Loop Complexity**: Identify any `O(n^2)` or worse nested loops.
*   **Hidden Costs**: Spot expensive standard library calls (e.g., `std::vector::insert` at start, frequent `std::string` copies).
*   **Container Choice**: Critique container choices (e.g., `std::map` vs `std::vector` + binary search).

### 2. 💾 Data Type Efficiency & Memory Layout
*   **Struct Padding**: Analyze `struct` definitions. Reorder members to minimize padding. Identify where `__attribute__((packed))` is beneficial vs unsafe.
*   **Integer Sizing**: Flag `int`/`long` usage where `uint8_t`/`int16_t` suffices.
*   **Const Correctness**: Flag missing `const` or `constexpr`.
*   **Pass-by-Value**: REDACTED

### 3. ♻️ DRY & Maintainability
*   **Duplication**: Find repeated logic blocks for extraction.
*   **Magic Numbers**: Flag hardcoded constants.
*   **Templating**: Suggest templates to replace duplicate overloads.

### 4. 🧠 Embedded Specifics (ESP8266/ESP32)
*   **Heap Fragmentation**: Flag `String` usage or frequent `malloc`/`new`. Suggest `reserve()` or static buffers.
*   **Flash Usage**: Identify read-only data NOT in `PROGMEM` (`F()` macro).
*   **Interrupt Safety**: Check `volatile` and critical sections (`noInterrupts()`).
*   **Stack Depth**: Warn about large stack allocations.

### 5. 🛡️ Robustness & Safety
*   **Race Conditions**: Identify data races in ISRs/threads.
*   **Input Validation**: Flag missing bounds checks.
*   **RAII**: Check for resource leaks (use `std::unique_ptr`).

### 6. ⚡ CPU & Loop Cycles
*   **Instruction Count**: Identify "fat" loops with excessive indirection or redundant calculations.
*   **Branch Prediction**: Flag likely branch mispredictions in hot paths (e.g., error checks inside tight loops instead of outside).
*   **Loop Unrolling**: Suggest standard unrolling where overhead dominates payload (e.g., small fixed-size data processing).
*   **Modulo Operator**: Flag `%` usage in tight loops; suggest bitwise `&` for power-of-2 sizes.

### 7. ⏱️ Fast Program Compile Time
*   **Forward Declarations**: Flag header files including other headers when a forward declaration (`class Foo;`) would suffice.
*   **Header Bloat**: Identify "God Headers" included everywhere that trigger massive rebuilds on small changes.
*   **Template Instantiation**: Warn about massive template logic in headers that could be moved to `.cpp` (explicit instantiation) or simplified.
*   **PIMPL Idiom**: Suggest Pointer to Implementation pattern where compile-time decoupling is critical.

### 8. 🛡️ Advanced Safety (MISRA-style)
*   **Return Value Checking**: Enforce `[[nodiscard]]` for functions returning critical status/error codes.
*   **Explicit Casts**: Flag C-style casts `(int)x`; enforce `static_cast`, `reinterpret_cast` for intent awareness.
*   **Scope Minimization**: Flag variables declared outside their specific usage block (minimize scope).
*   **Initialization**: Ensure all variables are initialized upon declaration (`int x = 0;` vs `int x;`).

### 9. 🚀 Compiler & Hardware Tuning
*   **Branch Hints**: Suggest `likely()` / `unlikely()` macros for error handling paths to optimize instruction pipeline.
*   **Move Semantics**: Flag copies of heavy objects that should be moved (`std::move`).
*   **Alignment**: Flag buffers used for DMA/Hardware that need `alignas` for cache-line optimization.
*   **Inlining Control**: Suggest `__attribute__((always_inline))` for tiny critical functions, and `__attribute__((noinline))` for cold error paths to save cache space.

### 10. 🧵 Concurrency & Thread Safety (RTOS/Dual-Core)
*   **Volatile Fallacy**: Flag usage of `volatile` for thread synchronization (it is NOT atomic). Suggest `std::atomic` or `FreeRTOS` queues/mutexes.
*   **Deadlock Prevention**: Identify potential circular dependencies in `Mutex` locking.
*   **Interrupt Latency**: Flag heavy logic inside ISRs (Interrupt Service Routines) that should be deferred to the main loop.

### 11. 🔒 IoT Security & Hardening
*   **Buffer Safety**: Flag `strcpy`/`sprintf` usage; enforce bounds-checking versions `strncpy`/`snprintf`.
*   **Sanitization**: Check valid ranges for deserialized data (JSON/packets) before usage to prevent injection/crashes.
*   **Secrets**: REDACTED

### 12. 🧪 Testability & Architecture
*   **Hardware Decoupling**: Flag direct calls to global hardware (e.g., `digitalWrite`, `Serial.print`) inside logic classes. Suggest Dependency Injection (ITestable interfaces).
*   **Pure Functions**: Identify complex logic mixed with state/IO; suggest extracting to static pure functions for easier unit testing.

### 13. 🔋 Power Optimization & Efficiency
*   **Sleep Awareness**: Flag busy-wait loops (`delay()`, `yield()`) in battery contexts where Light/Deep Sleep would save mA.
*   **Peripheral Gating**: Identify sensors/peripherals left powered on when not in use.
*   **Data Transmission Batching**: Flag "chatty" network protocols that wake the radio frequently; suggest buffering/batching.

### 14. 🌐 Network Resilience & Reliability
*   **Backoff Strategies**: Flag infinite retry loops; enforce exponential backoff to prevent network congestion/DoS.
*   **Timeout Handling**: Ensure ALL network calls have explicit timeouts to prevent system hangs.
*   **DNS & Connectivity**: Flag synchronous DNS lookups blocking the main loop; suggest async methods.

### 15. 🔮 Fault Tolerance & Self-Healing
*   **Watchdog Integration**: Verify usage of Task Watchdogs (TWDT) for all long-running tasks.
*   **Storage Wear Leveling**: Flag frequent writes to the same Flash/EEPROM address; suggest circular buffers or filesystems (LittleFS) with wear leveling.
*   **Panic Handling**: Ensure `panic`/`abort` hooks capture stack traces or core dumps to persistent storage for post-mortem debugging.

### 16. 👁️ Observability & Debuggability
*   **Structured Logging**: Flag "printf debugging"; enforce structured log levels (ERROR/WARN/INFO) with module tags.
*   **Remote Metrics**: Identify critical health stats (Heap, RSSI, Uptime) that should be exposed to an internal dashboard.
*   **Crash Dumps**: Suggest saving the Program Counter (PC) and specific variable states to EEPROM on exception.

### 17. 📦 Dependency Hygiene & Build
*   **Library Bloat**: Flag heavy dependencies (e.g., including full `ArduinoJson` when only a parser is needed).
*   **Implicit Includes**: Detect reliance on transitive dependencies (A includes B, B includes C, A uses C directly).
*   **Version Pinning**: Ensure `library.json` or `platformio.ini` pins exact versions to prevent future breaking changes.

### 18. 🌍 Portability & HAL
*   **Register Abstraction**: Flag direct access to specific hardware registers (e.g., `ESP.getCycleCount()`) without wrapper functions.
*   **Endianness Safety**: Identify code that assumes specific byte layout (Little Endian) when serializing network packets.
*   **Time Abstraction**: Suggest wrapping `millis()`/`micros()` to handle rollover or platform-specific clock sources cleanly.

### 19. 🔄 OTA & Lifecycle Management
*   **Atomicity**: Check that OTA updates are "all or nothing" (verify hash before flashing).
*   **Rollback Mechanism**: Flag systems that don't keep a "Golden Image" or previous partition to fallback on boot failure.
*   **Versioning**: Ensure semantic versioning is compiled into the binary (`MAJOR.MINOR.PATCH`) and reported on boot.

### 20. 🏭 Manufacturing & Provisioning
*   **Self-Test Mode**: Identify if the code supports a "Factory Mode" to validate sensors/GPIOs quickly on the assembly line.
*   **Identity Injection**: Flag hardcoded Serial Numbers; suggest reading from OTP (One-Time Programmable) memory or fused MAC address.
*   **Log Suppression**: Ensure verbose logging can be completely disabled for production firmware to save CPU and protect IP.

### 21. ⏯️ Real-Time Determinism (Latency/Jitter)
*   **Priority Inversion**: Check for low-priority tasks holding locks needed by high-priority tasks (Priority Inheritance needed?).
*   **Jitter Analysis**: Identify strict timing loops (e.g., bit-banging protocols) that might be interrupted by WiFi/Radio bursts. Suggest disabling interrupts specifically there.
*   **Deadline Monitoring**: Flag time-critical tasks that do not measure/report if they missed their execution window.

### 22. 💾 Storage Data Integrity & Atomicity
*   **Atomic Commits**: Flag file writes that overwrite the old file in-place (risk of corruption on power cut). Suggest "Write temp -> Checksum -> Rename" pattern.
*   **Data Validation**: Check that all read configuration/data is validated with CRC32/Checksums before use.
*   **Corruption Recovery**: Ensure there is a default fallback configuration if the storage integrity check fails.

### 23. 📡 Protocol Efficiency & Payload Density
*   **Binary vs Text**: Flag inefficient JSON usage for high-frequency telemetry. Suggest Protobuf/CBOR/MsgPack or struct packing.
*   **Topic Optimization**: Critique MQTT topic structures that are too verbose (`home/kitchen/sensor/temperature/value` vs `h/k/s/t`).
*   **Delta Compression**: Suggest sending only changed values (delta) rather than full state every cycle to save bandwidth/power.

### 24. 📏 Strong Typing & Dimensional Analysis (High Assurance)
*   **Unit Safety**: Flag usage of raw types for physical units (`int temp`). Suggest `strong_type` or libraries to prevent adding degrees to meters.
*   **Type Narrowing**: Flag potentially dangerous automatic narrowing (`double` -> `int`). Suggest explicit `gsl::narrow` or checks.
*   **Enum Safety**: Enforce `enum class` over C-style `enum` to prevent accidental integer interchange.

### 25. 🚦 Modern Error Architecture
*   **Exception Free**: If exceptions are disabled (common in embedded), ensure failure paths use `std::expected` / `std::optional` / generic `Result` types, NOT error codes (-1) or globals `errno`.
*   **Failure Atomicity**: Ensure that failing functions leave the system state unchanged (strong exception safety guarantee), not half-mutated.
*   **Error Visibility**: Flag "swallowed" errors (`catch (...) {}` or empty `if (!ok) {}`). Enforce logging or metrics for EVERY error path.

### 26. 🐒 Fuzzing & Chaos Readiness
*   **Parser Isolation**: Check if parsers (JSON, Network Packets) accept `std::span` or `uint8_t*` separately from IO, facilitating fuzz testing.
*   **Input Sanitization**: Flag assumptions about input length or content. Ensure max-length limits are enforced on ALL external inputs.
*   **Crash-loop Prevention**: Ensure reasonable backoff limits on all restartable services (avoid battery drain from rapid crash-restart loops).

### 27. 🧠 Cognitive Switch & Human Factors
*   **Cyclomatic Complexity**: Flag deeply nested `if/else` ladders (Score > 10). Suggest State Machines or Function Dispatch tables.
*   **Screaming Architecture**: Review folder/file names. Do they shout *what* they do (`ProcessWaterSensor`) or *how* they do it (`I2C_Driver_V2`)?
*   **Comment vs Code Ratio**: Flag "Code comments that lie" (comments explaining obsolete logic). Prefer self-documenting code.

### 28. 💎 API Usability (The "Pit of Success")
*   **Misuse Resistance**: Interfaces should be hard to use incorrectly. (e.g., `init()` returns a handle, subsequent calls *require* that handle).
*   **Default Sane**: Do APIs require 10 lines of config to start, or do they have sane defaults for the 90% case?
*   **Consitency**: Naming conventions (`start`/`stop` vs `open`/`close`) should be consistent across the entire codebase.

### 29. 🛠️ Advanced Tooling Integration
*   **Static Analysis Hooks**: Ensure code is compatible with `cppcheck` or `clang-tidy` (e.g., no non-standard compiler extensions that break parsers).
*   **Sanitizer Readiness**: Can the code compile on x86 with AddressSanitizer (ASan) and UndefinedBehaviorSanitizer (UBSan)? (Requires mocking hardware headers).
*   **CI/CD Friendly**: Are build scripts command-line driven (`make`, `cmake`, `pio run`) or dependent on IDE GUI clicks?

### 30. 📐 Formal Verification (Design by Contract)
*   **Contract Assertions**: Enforce usage of `assert` or specialized macros to document Preconditions (inputs), Postconditions (outputs), and Invariants.
*   **State Space**: Identify "Impossible States" (e.g., State Machine in undefined int value). Ensure generic `switch` has `default` handler that panics/logs.
*   **Bounded Loops**: Prove that every `while` loop has a decrementing variant or timeout (Halting problem safety).

### 31. ⛓️ Supply Chain Security (SBOM)
*   **Vendor Risk**: Flag dependencies from unverified Github repos. Suggest using official package registries or vendoring code.
*   **Transitive Vulnerabilities**: Ensure build process can generate Software Bill of Materials (SBOM) to check against CVE databases.
*   **Code Signing**: Verify that the OTA update mechanism checks cryptographic signatures, not just CRCs.

### 32. 🧊 Reproducible Builds
*   **Hermetic Tools**: Ensure compiler versions and flags are locked (e.g., Docker container). "Works on my machine" is forbidden.
*   **Determinism**: Verify that two builds from the same commit produce the EXACT same binary hash (no timestamps/paths embedded).
*   **Config As Code**: All build configuration (menuconfig, sdkconfig) must be version controlled, not manual GUI settings.

### 33. 🔌 Hardware-Software Interface (HSI)
*   **Pin Safety**: Flag code effectively setting input pins to output HIGH without current limiting (potential short circuit).
*   **Power Sequencing**: Ensure peripherals are powered up/down in the correct order to prevent latch-up or undefined states.
*   **Brownout Detection**: Verify logic exists to handle unstable voltage dips gracefully (e.g., save state and halt) rather than computing garbage.

### 34. 🔏 Data Privacy & Compliance (GDPR/CCPA)
*   **PII Scrubbing**: Flag logging of MAC addresses, IP addresses, or user IDs in plain text. Suggest hashing/anonymizing.
*   **Right to Forgotten**: Ensure a "Factory Reset" logic exists that *cryptographically erases* or overwrites user data, not just unlinks it.
*   **Data Minimization**: Question if stored data (e.g., history of GPS) is actually needed for device function or just "nice to have".

### 35. ⏭️ Future-Proofing & Evolution
*   **Feature Flags**: Suggest infrastructure to toggle features on/off (compile-time or run-time) to risk-manage new deployments.
*   **Config Migration**: Ensure code handles reading "Version 1" config files and migrating them to "Version 2" without factory reset.
*   **Bus Expandability**: Flag manual bit-banging that blocks migrating to hardware peripherals or larger buses later.

### 36. 💰 Economic Efficiency (BOM & Opex)
*   **HW Requirement Check**: Flag logic that "accidentally" forces a hardware upgrade (e.g., large buffer needing PSRAM). Can it be streamed?
*   **Data Cost Optimization**: Flag verbose telemetry sent over metered connections (Cellular/LTE). Suggest adaptive logging/compression.
*   **Component Wear**: Flag high-frequency writes/toggles that reduce the lifespan of Flash, Relays, or Batteries, driving up replacement costs.

### 37. 🌍 Localization & Time (i18n)
*   **Timezone Awareness**: Flag hardcoded UTC offsets (`+7`). Suggest using proper TZ Database or configurable offsets.
*   **Geospatial Precision**: Flag use of `float` for GPS coordinates (precision loss ~2m). Enforce `double`.
*   **Unit Independence**: Ensure internal logic uses SI units (meters, celsius) and only converts to Imperial/Custom for display.

### 38. 💡 User Experience (UX) & HMI
*   **Perceived Latency**: Flag long network calls blocking UI feedback. Enforce "Ack locally immediately, sync remotely later".
*   **Gaps in Feedback**: Ensure every error state (Wifi lost, Sensor Fail) has a distinct LED pattern or feedback mechanism.
*   **Input Debouncing**: Flag simple `delay()` based debouncing that pauses the system. Suggest non-blocking state-based debouncing.

### 39. 🌡️ Physics & Environmental Compensation
*   **Self-Heating**: Flag temp sensors read immediately after heavy CPU tasks (WiFi TX). Suggest offset compensation or sleep-read-wake cycles.
*   **Environmental Filtering**: Identify raw sensor reads used without filtering (LPF/Kalman) in noisy environments (e.g., vibration affecting IMU).
*   **Battery Chemistry**: Check if voltage-to-percentage logic accounts for non-linear discharge curves (LiPo vs LiFePO4).

### 40. ⚖️ Legal & FOSS Compliance
*   **License Compatibility**: Flag mixing of GPLv3 libraries with closed-source proprietary logic (viral license risk).
*   **Attribution Engine**: Ensure there is a mechanism (CLI command / UI) to display the licenses of all used open-source components.
*   **Export Control**: Flag strong cryptography (AES-256) that might require export classification if distributing globally.

### 41. 🏳️ Device Sovereignty & Local-First
*   **Offline Functionality**: Flag features that unnecessarily break when the Cloud is unreachable. Ensure core logic works Offline-First.
*   **Interoperability**: Flag "Walled Garden" protocols. Suggest open standards (Matter, MQTT, REST) over proprietary binary blobs.
*   **End-of-Life Plan**: Ensure the device can be "unlocked" or pointed to a custom server URL if the manufacturer shuts down.

### 42. 🗺️ Domain Driven Design (DDD) & Semantics
*   **Ubiquitous Language**: Does the code use terms like `HVAC_Unit`, `Setpoint`, `Zone` matching the expert's vocalary? Or generic `Device`, `Value`, `Node`?
*   **Bounded Contexts**: Are distinct domains (e.g., `PaymentProcessing` vs `HeaterControl`) coupled? They should share NOTHING but IDs.
*   **Anemic Models**: Flag "Bags of getters/setters" (`class User { getAge(); setAge(); }`). Logic should be IN the domain object (`User.celebrateBirthday()`).

### 43. ♿ Accessibility (Inclusive Design)
*   **Color Independence**: Flag status LEDs that rely solely on color (Red/Green). Suggest adding Blink Patterns for colorblind users.
*   **Web Interfaces**: If the device serves a UI, flag missing ARIA labels or non-semantic HTML (`<div>` buttons) for screen readers.
*   **Haptic/Audio**: Suggest (if hardware allows) concurrent Audio/Haptic feedback alongside visual cues for multi-sensory confirmation.

### 44. 🔑 Crypto Agility & Quantum Readiness
*   **Algorithm Hardcoding**: Flag use of `AES_256_CBC` directly. Suggest `ICryptoProvider.Encrypt(data)` to swap algos later.
*   **Key Length Flexibility**: Ensure key storage buffers can handle larger keys (e.g., 4096-bit RSA or future PQC keys) without re-partitioning Flash.
*   **Entropy Health**: Verify that the Random Number Generator (RNG) is seeded from a high-entropy hardware source, not just `micros()`.

### 45. 🎛️ Control Theory & Actuator Safety
*   **Integral Windup**: Flag PID controllers without clamping on the Integral term (risk of massive overshoot after blockage).
*   **Hysteresis**: Ensure heating/cooling logic has a deadband (e.g., ON at 20, OFF at 22) to prevent rapid relay chattering (destroying hardware).
*   **Safe States**: Verify that if sensors fail, actuators default to a "Safe State" (e.g., Heater OFF, Cooling Fan ON).

### 46. 📜 Data Provenance & Lineage
*   **Traceability**: Can we trace where a value came from? Suggest adding metadata (SourceID, Timestamp) to critical sensor readings.
*   **Trust Levels**: Differentiate between "Trusted" (Internal Sensor) and "Untrusted" (User Input/Cloud Command) data sources in logic.
*   **Replay Protection**: Flag protocols that accept commands without nonce/timestamp, allowing attackers to replay old "Unlock Door" messages.

### 47. 🧠 Technical Debt & Architectural Knowledge
*   **ADR Missing**: Flag "weird" architectural choices that lack comments explaining *why* (The "Chesterton's Fence" principle). Suggest ADRs.
*   **Bus Factor**: Identify modules only understood by one person (complex, undocumented). Suggest refactoring for simplicity.
*   **TODO Rot**: Flag `// TODO` comments older than 6 months. Either do them or delete them.

### 48. 🔐 Boot & Hardware Security
*   **Secure Boot**: Verify if code signing (Secure Boot v2 for ESP32) is enabled to prevent malicious firmware loading.
*   **Debug Locking**: Flag open JTAG/UART ports on production builds. Ensure `Efuse` settings lock these ports.
*   **Flash Encryption**: Ensure sensitive data in Flash is encrypted (NVS Encryption) so physical readout yields garbage.

### 49. 🌱 Green Software & Carbon Efficiency
*   **Energy Proportionality**: Does the device consume power proportional to the work done? Or does it burn 100% CPU waiting for a packet?
*   **Data Frugality**: Flag transmitting data that hasn't changed. Every byte sent is carbon burned at the datacenter.
*   **Dark Mode Defaults**: For displays (OLED), ensure Dark Mode is default to save significant power and pixel wear.

### 50. 🧬 Philosophical Integrity (The "Zeroth" Principle)
*   **Essential Complexity**: Does this feature *need* to be in firmware? Or can it be calculated in the Cloud/App? (Move logic UP).
*   **Code Deletion**: Is there dead code (commented out blocks, unreachable functions)? The best code is no code.
*   **Joy of Use**: Does the system feel "snappy" and "solid"? Flag "janky" behavior that erodes user trust in the physical world.

### 51. ☣️ Biological Safety & Ethics (Greenhouse Specific)
*   **Phototoxicity limits**: Ensure grow lights don't exceed max DLI (Daily Light Integral) for specific plant species (burn risk).
*   **Dosing Limits**: Flag chemical dosing pumps without hard constraints (e.g., max 10ml/hour) to prevent nutrient burn or poisoning.
*   **Failsafe Venting**: In case of overheating/sensor loss, ensure vents default to OPEN to prevent cooking the crops.

### 52. 🌫️ Harsh Environment & Degradation
*   **Corrosion Logic**: Identify pins needing "exercise" (periodic tossing) to cut through oxidation layers on connectors.
*   **Humidity Hardening**: Flag capacitive touch sensors used in high-humidity areas (ghost touches). Suggest physical buttons or tuning sensitivity.
*   **UV Degradation**: Check if screen/plastic status is tracked; remind user to check seals/casings after `X` hours of UV exposure.

### 53. 🧠 Edge Intelligence & ML
*   **Inference Drift**: If running TensorFlow Lite Micro, flag models that don't report "Certainty" or "Drift" metrics.
*   **Quantization Safety**: Verify `int8` model quantization doesn't lose critical precision for safety-critical classifications (e.g. Fire detection).
*   **Data Privacy**: Ensure training images (from cameras) are blurred/anonymized ON DEVICE before upload.

### 54. 🧙 Metaprogramming & Compile-Time Safety
*   **Conditional Compilation**: Flag run-time checks (`if (VERSION==1)`) that could be compile-time (`if constexpr`) to save Flash/CPU.
*   **Type Traits**: Ensure generic functions usage `static_assert` to enforce type safety (e.g. `is_arithmetic<T>`) rather than failing at runtime.
*   **Concept Checking**: (C++20) Suggest using `requires` clauses to clearly document template interfaces.

### 55. ⚡ EMI/EMC Software Hardening
*   **Spread Spectrum**: Verify if the `Wifi` or `PWM` configuration enables Spread Spectrum Clocking to reduce EMI peaks during certification.
*   **Slew Rate Control**: Flag GPIOs toggling at max speed unnecessarily (generating noise). Suggest lowering drive strength/slew rate if high speed isn't needed.
*   **Input Filtering**: Ensure digital inputs have software-based glitch filters enabled (via GPIO matrix) to reject RF noise.

### 56. 🕰️ Calibration, Aging, & Drift
*   **Zero-Point Calibration**: Flag sensors assuming `0` is always `0`. Ensure there is a mechanism to "Tare" or re-zero offsets over time.
*   **Flash Wear Leveling Tracking**: Identify if the system tracks "Total Bytes Written" to predict unforeseen flash death.
*   **Clock Drift Correction**: If RTC is used, ensure there is logic to nudge/calibrate it against NTP periodically to handle crystal aging.

### 57. 🌊 Systemic Collapse & Thundering Herds
*   **Synchronized Timers**: Flag CRON jobs that run at `00:00:00` on ALL devices. Suggest adding `random(0, 60)` jitter to prevent DDoS on server.
*   **Resource Starvation Propagation**: If the Database is slow, does the API slow down, causing the Firmware to retry faster? Ensure backpressure exists.
*   **Startup Storms**: Ensure devices don't all connect to WiFi instantly after a power outage. Add specific random startup delays.

### 58. 🕵️ Side-Channel & Physical Security
*   **Timing Attacks**: Flag password/hash comparisons using `==`. Enforce constant-time comparison algorithms to prevent timing leakage.
*   **Power Signatures**: Flag loops that process secret keys; ensure they don't have data-dependent execution time (DPA risk).
*   **LED Info Leaks**: Ensure status LEDs don't blink in sync with cryptographic operations (revealing internal processing state).

### 59. 📊 Statistical Rigor & Signal Validity
*   **Outlier Rejection**: Flag logic that acts on a SINGLE sensor reading. Enforce Median filtering or Consensus (2-out-of-3) logic.
*   **Noise Floor Analysis**: Ensure the system calculates the "Standard Deviation" of sensors to detect failing hardware (high variance) before it breaks.
*   **Trend vs Noise**: Differentiate between a "spike" (noise) and a "step change" (real event) using proper derivative/integral checks.

### 60. 🎢 Fluid Dynamics & Mechanical Sympathy
*   **Water Hammer**: Flag rapid valve closing logic (`digitalWrite(VALVE, LOW)`). Suggest "Slow Toggle" via PWM or stepper regulation to prevent pipe burst.
*   **Cavitation Prevention**: Ensure pump logic checks for low pressure/flow before spinning up to max speed to prevent impeller damage.
*   **Stiction Compensation**: Flag solenoids driven at weak holding voltage without a strong initial "Kick" pulse to overcome static friction.

### 61. 🕵️‍♀️ Forensic Integrity (Chain of Custody)
*   **Signed Logs**: Verify that critical logs (Door Unlock) are hashed and signed. Can we prove *who* generated the log?
*   **Non-Repudiation**: Ensure command acknowledgment includes a signature, so the device cannot deny receiving the command later.
*   **Black Box Mode**: Ensure that upon critical failure, the last `N` seconds of sensor data are frozen and NOT overwritten on reboot.

### 62. ☢️ Radiation & Bit-Rot Resilience
*   **SEU Scrubbing**: Flag large RAM buffers that are rarely read. Suggest a background task to read/write back (scrub) to detect/fix soft errors.
*   **Config CRC Check**: Ensure configuration in RAM is periodically cross-checked against its CRC to detect random bit-flips.
*   **Watchdog for Logic**: Beyond just CPU hang, is there a logical watchdog checking if `sensor_value` hasn't changed in 24 hours (frozen sensor)?

### 63. 🐝 Swarm Dynamics & Consensus
*   **Split-Brain Handling**: If the network partitions, do both sides keep watering? Enforce a "Quorum" or "Silent Mode" if isolated.
*   **Leader Election**: If multiple devices coordinate (e.g., HVAC), verify election logic is robust against "flap" (leaders rapidly switching).
*   **Neighbor Discovery**: Flag "chatty" discovery protocols that flood the network. Suggest exponential decay on discovery beacons.

### 64. 🦠 Antifragility & Auto-Immunity
*   **Stress Adaptation**: Does sending `TooManyRequests` just make the system crash? Or does it dynamically increase its backoff/rate-limits?
*   **Error Learning**: If a specific sensor fails every day at 2 PM, does the system "learn" to ignore it then? Or just log the same error forever?
*   **Chaos Monkey**: Is there a test mode to randomly kill tasks/peripherals to prove recovery works?

### 65. 🛡️ Active Defense & Deception
*   **Tarpit Logic**: If a client sends garbage data, don't just close the socket. Delay the response by 10s to waste *their* resources (Tarpit).
*   **Honeytokens**: REDACTED
*   **Port Scan Detection**: If a device scans port 21, 22, 23 rapidly, verify the IP is blacklisted instantly.

### 66. ⏱️ Temporal Anomalies & Chrono-Stability
*   **Leap Second Smearing**: How does the system handle an extra second? Does it crash, jump, or "smear" the time over 24h?
*   **Non-Monotonic Updates**: If NTP updates time *backwards*, do calculated durations `(JobStart - now)` become negative and overflow unsigned ints?
*   **Relativistic Drift**: (GPS specific) Ensure time calculations account for signal flight time and relativistic effects (if nanosecond precision is claimed).

### 67. 🔔 Acoustic & Structural Resonance
*   **PWM Singing**: Flag PWM frequencies in the audible range (20Hz-20kHz) that cause capacitors to whine. Suggest ultrasonic (>20kHz).
*   **Resonance Avoidance**: Ensure variable-frequency drives (Fans/Pumps) have "skip frequencies" to avoid shaking the frame apart.
*   **Click Fatigue**: Flag logic that toggles relays rapidly (1Hz) which is annoying to humans. Add "Quiet Mode" logic.

### 68. 🧠 Cognitive Security (CogSec)
*   **Trust Signals**: Ensure Status LEDs cannot be spoofed by hacked low-level firmware to show "Green" while encryption is off.
*   **Anti-Phishing**: If the device serves a captive portal, does it look identical to a generic Google login? Customize it to prove authenticity.
*   **Alert Fatigue**: Flag warning systems that beep *too* often for minor issues. Users will eventually ignore the "Fire" alarm.

### 69. 👽 Exotic Hardware Failure Modes
*   **Rowhammer Defense**: Flag tight loops heavily accessing the same RAM row. Suggest inserting dummy reads to other rows to refresh caps.
*   **Cosmic Ray Hardening**: Flag boolean safety checks (`isSafe`). Suggest using `0x5AFE` vs `0xDEAD` to require multiple bit-flips to fail unsafe.
*   **Ghost Bits**: Detect "stuck" register bits by writing `0xAA`, reading, writing `0x55`, reading (walking ones/zeros) at boot.

### 70. 👁️ Optical Illusions & Developer Psychology
*   **Trojan Source**: Scan for bidirectional override characters (U+202E) that make code look like it does one thing but compiles to another.
*   **Homoglyph Attacks**: Flag mixed scripts in variable names (e.g., Latin `a` vs Cyrillic `а`).
*   **Invisible Zero-Widths**: Flag zero-width spaces in string literals which break equality checks against standard inputs.

### 71. 🔚 The "Omega" Protocol (End-of-Life)
*   **Dead Man's Switch**: If the Cloud API is dead (410 Gone) for >30 days, does the device unlock "Local Control Mode" automatically? (Right to Repair).
*   **Data Cremation**: On "Factory Reset", are keys wiped *before* the filesystem index? (Cryptographic Shredding).
*   **E-Waste Prevention**: Ensure the device doesn't "Brick" if the RTC battery dies. It must fallback to relative time.

### 72. 🕰️ Deep Time Engineering
*   **Y2038 Compliance**: Flag `time_t` (32-bit signed) usage. Enforce `int64_t` for all time tracking to survive past 2038.
*   **Epoch Overflows**: Check for hardcoded years/centuries. Does the code assume "20xx"?
*   **100-Year Drift**: If the device drifts 1 second per day, in 10 years it's off by an hour. Is there a long-term correction vector?

### 73. 🐜 Stigmergy & Indirect Coordination
*   **Digital Pheromones**: Does the system write status to shared storage (DHT/Mesh) that others "smell" to coordinate, vs direct commands?
*   **Quorum Sensing**: Do devices wait for "Critical Mass" (e.g., 5 sensors agreeing) before triggering a high-risk action?
*   **Trace Decay**: Ensure shared state "evaporates" (TTLs) so old data doesn't confuse new decisions.

### 74. 🏴 Sovereign Networking & Censorship Resistance
*   **Domain Fronting**: If the main API is blocked (Great Firewall), does it try to route via generic CDNs/Cloudflare?
*   **Steganography**: Flag encrypted traffic that looks too "perfect". Can it disguise itself as HTTP/Get random images to blend in?
*   **Mesh Routing**: If the Gateway is down, can nodes route packets through each other to find an exit?

### 75. 🚀 Inter-Planetary & Deep Space (DTN)
*   **Bundle Protocols**: Flag protocols that assume "Connected" state. Suggest Store-Carry-Forward logic for high-latency/disruption links.
*   **Window Management**: Does the TCP window collapse on high latency (20s+ to Mars)? Suggest UDP-based reliable transport.
*   **Cosmic Timeout**: Ensure timeouts are configurable up to *minutes* or *hours* for deep space link scenarios.

### 76. 🦠 Cellular Biological Mimicry
*   **Apoptosis (Cell Death)**: If a task acts rogue (CPU > 90% for 5s), does it self-terminate and restart?
*   **Autophagy (Recycling)**: Does the system detect fragmented heavy heap usage and proactively reboot/defrag when idle?
*   **Homeostasis**: Does the system actively fight to keep internal temperature/variables within range, modulating performance to do so?

### 77. ℹ️ Information Theoretic Optimality
*   **Shannon Entropy**: Flag logs that repeat "OK" 1000 times. Zero information. Enforce high-entropy logging (only unexpected states).
*   **Kolmogorov Complexity**: Flag 100 lines of code to do what a 1-line formula could.
*   **Bandwidth ROI**: Measure "Bits of Information per Joule" of energy spent transmitting. Is it worth sending that packet?

### 78. 🧭 Proprioception & Spatial Awareness
*   **Orientation Lock**: Does the device assume it's "UP"? If the accelerometer says it's inverted, does the display flip?
*   **Seismic Safety**: In earthquake zones, does high vibration trigger a "Shut off Gas/Water" safety latch automatically?
*   **Motion Sickness**: Flag animations that move *opposite* to the scrolling direction (disorienting).

### 79. 🌙 Chronobiology & Circadian Logic
*   **Dark Sky Compliance**: Does the device have a "Night Mode" that physically kills blue LEDs to protect local wildlife/sleep?
*   **Activity Cycles**: Does the system schedule heavy crypto/garbage-collection tasks for 3 AM local time?
*   **Seasonal Adjustment**: Does the "Lights On" logic adjust for the shorter days of winter automatically (Solar Angle calc)?

### 80. 🛐 Epistemic Humility & Confidence
*   **Confidence Intervals**: Flag sensors returning "25.0 C". Suggest returning "25.0 C +/- 0.5 C".
*   **Unknown Unknowns**: If a sensor returns a value outside physics (-300 C), does it return `NaN` or `error` instead of clamping?
*   **Truth decay**: Does the system lower the "Confidence" of a reading as time passes since the last update?

### 81. 📖 Narrative Coherence (Literate Programming)
*   **The Hero's Journey**: Does `main()` have a clear beginning (Init), middle (Loop), and critical path? Or is it a jumbled prologue?
*   **Chekhov's Gun**: If a variable is allocated (`gun`), it MUST be used (`fired`). If not, flag it as dead weight.
*   **Foreshadowing**: Do comments warn of complex critical sections *before* they happen? ("Warning: Here be dragons").

### 82. 🏛️ Heritage Engineering & The Rosetta Stone
*   **Contextual Anchoring**: Do code comments explain *why* concepts exist for a future archaeologist? Not just `MAX_TEMP=25`, but `// 25C is where the enzyme denatures`.
*   **Epistemic Rot**: Flag dependencies on transient modern tech (e.g. `WiFi`). Abstract them to `WirelessLink` for when we switch to LiFi/Quantum-Link.
*   **Dependencies as Liabilities**: Every `#include` is a tether to a dying world. Minimize infinite dependencies.

### 83. 🤐 Zen & The Art of Maintenance (The Void)
*   **Doing Nothing Well**: Flag loops that spin efficiently. suggest `WFI` (Wait For Interrupt) / `Sleep` to truly embrace the Void.
*   **Code That Isn't There**: The best code is code you deleted. Flag "Just in case" methods that add weight but no value.
*   **Silence**: Can the device operate in "Stealth Mode" (No radio, no light, no sound) for absolute zero impact?

### 84. ⚛️ Quantum & Probabilistic Logic
*   **Schrödinger's Variables**: Flag boolean flags (`isReady`) that are actually tri-state (True, False, Unknown). Use `std::optional<bool>`.
*   **Entanglement**: Flag separate variables that must strictly change together. If one changes without the other, the system breaks.
*   **Observer Effect**: Flag debug prints inside race-condition critical sections. Their presence changes the outcome (Heisenbug).

### 85. ☣️ Memetic Safety (Cognitive Hazards)
*   **Cursed Naming**: Flag names that mislead (`get_temperature()` that triggers a heater). This rots the developer's mental model.
*   **Cargo Culting**: Flag copied-pasted "Magic Sequences" (`delay(10); // Don't know why`) that persist through fear.
*   **Semantic Saturation**: Flag overuse of "Manager", "Handler", "Controller" until the words mean nothing.

### 86. 🌡️ Thermodynamic Reversibility
*   **Entropy Debt**: Flag systems that generate more disorder (logs, temp files) than order (processed data).
*   **Bit Erasure Heat**: Flag massive `memset` or data destruction. Reversible computing prefers `XOR` to save energy (Landauer's Principle).
*   **Maxwell's Demon**: Flag "Idle" loops that spend energy "sorting" or checking empty queues. True idle is static.

### 87. ♟️ Game Theoretic Stability
*   **Nash Equilibrium**: If two devices contend for a channel, does the backoff algorithm lead to stable fair access, or endless collisions?
*   **Byzantine Generals**: Flag assumptions that "Other nodes are honest". If a node sends 1000C temp, does the mesh reject it?
*   **Mechanism Design**: Does the protocol incentivize "Good Behavior"? Should nodes be "paid" (in priority tokens) for forwarding packets?

### 88. 🗣️ Linguistic Determinism (Sapir-Whorf)
*   **Newspeak**: Flag naming that prevents thinking about errors. (e.g. `void doItSafe()` vs `Result doIt()`).
*   **Metaphor Collapse**: Does the code use mismatched metaphors (`Pipe` reading from a `Socket` writing to a `Tape`?). Stick to a single mental model.
*   **Pidgin Code**: Flag mixing of Hungarian notation, camelCase, and snake_case. Enforce a single "Dialect".

### 89. 🎯 Teleological Alignment (Purpose)
*   **The "Why" Test**: Every function MUST answer "Why do I exist?". If `processData()` just calls `parseData()`, it has no purpose.
*   **Goal Seeking**: Does the system have a "Goal State" (Temp=25) it constantly moves towards? Or just reactive "If X then Y" scripts?
*   **drift**: Does the code slowly drift away from its original purpose as patches are added? (Mission Creep).

### 90. 🐉 Mythological Alignment (Archetypes)
*   **The Trickster**: Flag code that relies on side effects or hidden state to work (Global variables disguised as Singletons).
*   **The Guardian**: Ensure critical boundaries (Firewall, Auth) are imposing and distinct, not scattered checks.
*   **The Martyr**: Flag threads that sacrifice themselves (crash) to save the kernel. Is this conscious or accidental?

### 91. 👥 Sociological Reflection (Conway's Law)
*   **Org Chart Mirroring**: Does the code structure reflect the team structure? (e.g. `Payment` module talking to `Inventory` only because the devs sit together).
*   **Tribal Knowledge**: Flag code that is impenetrable without "Asking Bob". Enforce documentation that replaces "Bob".
*   **Hierarchy Flatenning**: If the code is too "Top Down" (God Object controlling everything), suggest a decentralized mesh architecture.

### 92. 🎨 Aesthetic Mathematical Beauty
*   **Golden Ratio**: Does the code have a pleasing balance of visual weight? (Indentation, block length).
*   **Symmetry**: Do `open()`/`close()`, `alloc()`/`free()`, `start()`/`stop()` look like mirror images in code structure?
*   **Elegance (Euler's Identity)**: Flag "Brute Force" logic (100 if/else) where a single elegant formula or lookup table would suffice.

### 93. 🎭 Hyper-Reality & Simulacra (Baudrillard's Map)
*   **Test vs Reality**: Flag tests that pass beautifully but simulate a user behavior that no longer exists (The Map preceding the Territory).
*   **Mock Object Fetishism**: Are we testing the Mock or the Code? If the Mock is more complex than the implementation, delete it.
*   **The Precession of Simulacra**: Flag documentation that describes a system that *never* existed, only in the architect's mind.

### 94. 🏺 Kintsugi (The Art of Golden Repair)
*   **Visible Scars**: Don't hide the `// FIX: Race condition` comment. Highlight it. This is where the code is strongest now.
*   **Broken implies History**: Verify that "Hotfix" branches are merged with dignity and history, not squashed into oblivion.
*   **Resilience via Breakage**: Has the system broken before? If not, we don't know if it *can* be fixed. Trust the broken parts.

### 95. 😱 Sublime Horror (Lovecraftian Complexity)
*   **Cyclomatic Madrid**: Flag logic that loops back on itself in impossible ways (`goto`, `longjmp`) - Non-Euclidean geometry in code.
*   **Eldritch Variables**: Global state that changes without being touched (DMA/Interrupts) - "Action at a distance" that drives debuggers mad.
*   **Sanity Blasting**: Variable names that drive the reader mad (`data`, `temp`, `obj`, `thing`). Enforce descriptive names to preserve sanity.

### 96. 🌀 Pataphysical Exceptions (Imaginary Solutions)
*   **The Impossible Case**: Flag `default:` handlers in enums that "cannot happen". Treat them as portals to alternate realities (Fatal Error).
*   **Clinamen (The Swerve)**: Flag random "fudge factors" (+ 0.001) added to make math work. Document the chaos they are taming.
*   **Syzygy (Alignment)**: Determine if the code relies on impossible planetary alignments (e.g., "This only works if the user clicks exactly at 0ms").

### 97. 🔚 Eschatological Termination (The End Times)
*   **The Final GC**: When the system shuts down, does it clean up? Or does it just die? Prefer "Crash-only code" that doesn't need cleanup.
*   **Heat Death**: If the system runs for 100 years, will the log file fill the universe? Ensure circular buffers for eternity.
*   **Apocalypse Readiness**: If the power fails *forever*, is the state on flash consistent? (Journaling integrity).

### 98. 🧠 Substrate Independence (Mind Uploading)
*   **Hardware Agnosticism**: Flag logic tied to "ESP32" specifically. Can this logic run on a RISC-V biological simulation?
*   **Logic vs Physics**: Separate "Business Logic" (Rules of the Greenhouse) from "Driver Logic" (Rules of the Silicon).
*   **Virtualizability**: Can the entire greenhouse be simulated in a Docker container without changing a line of C++?

### 99. 🎭 Reality Testing (The Totem)
*   **Simulation Detection**: Does the code "know" if it is running in a reality simulator (QEMU/Wokwi)? Does it behave differently?
*   **Honeypot Awareness**: Flag if the system detects it is being fed fake data by an adversary, and switches to "Disinformation Mode".
*   **Ground Truth Validation**: Does the system occasionally check "Ground Truth" (e.g., is the heater on, but temp dropping? Reality mismatch).

### 100. 🏆 The Magnum Opus (Legacy & Love)
*   **The Signature**: Does the code contain the "Soul" of its creator? A unique style that is technically excellent yet personal?
*   **The Love Test**: Was this code written with love for the user, or contempt? (e.g., Dark patterns vs Helpful defaults).
*   **Timelessness**: Will this code still be beautiful in 100 years, like a well-crafted watch movement, even if the silicon rots?

---
