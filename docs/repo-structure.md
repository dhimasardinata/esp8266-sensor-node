# Repo Structure

Dokumen ini sekarang menjelaskan struktur repo yang sudah dieksekusi, bukan
lagi roadmap transisi. Tujuannya adalah menjaga repo tetap mudah dirawat,
mudah dipindai, dan jujur terhadap peran tiap area.

Status saat ini:
- Root repo difokuskan ke file inti pengembangan dan build.
- Tooling, repro script, cert lokal, dan context mirror dipindah keluar dari root.
- `lib/NodeCore` dipakai sebagai rumah domain runtime firmware node.

## Prinsip Struktur Saat Ini

- Root repo hanya menyimpan file inti pengembangan dan entry point toolchain.
- File generated, mirror context, dan artifact lokal dipisahkan dari source utama.
- `lib/NodeCore` dipakai sebagai batas domain runtime firmware, bukan folder datar serba campur.
- Naming diarahkan ke nama yang lebih jujur dan lebih mudah dipindai.

## Struktur Saat Ini

Struktur berikut adalah bentuk yang sekarang dijaga:

```text
.
|-- data/
|-- docs/
|-- include/
|   |-- app/
|   |-- config/
|   +-- generated/
|-- lib/
|   +-- NodeCore/
|       |-- api/
|       |-- interfaces/
|       |-- commands/
|       |-- net/
|       |-- ota/
|       |-- sensor/
|       |-- storage/
|       |-- support/
|       |-- system/
|       |-- terminal/
|       +-- web/
|-- scripts/
|   |-- analysis/
|   |-- build/
|   |-- dev/
|   +-- assets/
|-- src/
|-- test/
|-- tools/
|   +-- prompt/
|-- var/
|   |-- dist/
|   +-- log/
```

Catatan:
- `include/` dipakai hanya untuk header level aplikasi, konfigurasi, dan
  generated headers.
- `lib/NodeCore/` dipecah per domain agar source tidak lagi semua datar.
- artifact lokal seperti `dist/` dan `log/` tinggal di `var/`.
- tooling prompt dipisah ke `tools/`.

## Mapping Domain

### `lib/NodeCore/api/`

Pindahkan semua yang berawalan `ApiClient.*`, misalnya:
- `ApiClient.cpp`
- `ApiClient.h`
- `ApiClient.State.h`
- `ApiClient.Context.h`
- `ApiClient.Health.h`
- `ApiClient.Control.cpp`
- `ApiClient.Lifecycle.cpp`
- `ApiClient.Queue*.cpp`
- `ApiClient.Transport*.cpp`
- `ApiClient.Upload*.cpp`
- `ApiClient.Qos.cpp`
- `ApiClient.*Controller.h`

### `lib/NodeCore/ota/`

- `OtaManager.cpp`
- `OtaManager.h`
- `OtaManager.Context.h`
- `OtaManager.Health.h`
- `OtaManager.Security.cpp`
- `BootGuard.cpp`
- `BootGuard.h`

### `lib/NodeCore/web/`

- `AppServer.cpp`
- `AppServer.h`
- `AppServer.Routes.cpp`
- `PortalServer.cpp`
- `PortalServer.h`
- `PortalServer.Routes.cpp`
- `WifiRouteUtils.h`

### `lib/NodeCore/terminal/`

- `DiagnosticsTerminal.cpp`
- `DiagnosticsTerminal.h`
- `DiagnosticsTerminal.Commands.cpp`
- `TerminalFormatting.cpp`
- `TerminalFormatting.h`

### `lib/NodeCore/sensor/`

- `SensorManager.cpp`
- `SensorManager.h`
- `SensorNormalization.h`
- `SensorData.h`
- `calibration.h` sengaja tetap di `include/config/` karena ia bagian dari konfigurasi aplikasi, bukan implementasi driver sensor

### `lib/NodeCore/storage/`

- `CacheManager.cpp`
- `CacheManager.h`
- `RtcManager.cpp`
- `RtcManager.h`
- `Paths.h`

### `lib/NodeCore/net/`

- `WifiManager.cpp`
- `WifiManager.h`
- `WifiCredentialStore.cpp`
- `WifiCredentialStore.h`
- `NtpClient.cpp`
- `NtpClient.h`

### `lib/NodeCore/system/`

- `ConfigManager.cpp`
- `ConfigManager.h`
- `CrashHandler.cpp`
- `CrashHandler.h`
- `Logger.cpp`
- `Logger.h`
- `SystemHealth.h`
- `MemoryTelemetry.h`
- `NodeIdentity.h`
- `IntervalTimer.h`

### `lib/NodeCore/support/`

- `CompileTimeJSON.h`
- `CompileTimeUtils.h`
- `CryptoUtils.cpp`
- `CryptoUtils.h`
- `Crc32.cpp`
- `Crc32.h`
- `GatewayTargeting.h`
- `TextBufferUtils.h`
- `Utils.cpp`
- `Utils.h`

### `lib/NodeCore/interfaces/`

- `IAuthManager.h`
- `IConfigObserver.h`
- `ICacheManager.h`
- `ISensorManager.h`
- `IWifiStateObserver.h`

### `lib/NodeCore/commands/`

Folder ini sudah benar. Tidak perlu dibongkar lagi.

## Aturan Naming Yang Disarankan

Supaya repo makin konsisten:

- Gunakan `PascalCase` untuk file domain utama:
  - `Utils.cpp` dan `Utils.h` lebih rapi daripada `utils.cpp` dan `utils.h`
  - `SensorData.h` lebih rapi daripada `sensor_data.h`
- Biarkan keluarga file domain yang memang sudah memakai prefix tetap konsisten:
  - `ApiClient.TransportState.cpp`
  - `ApiClient.UploadRuntimePolicy.cpp`
- Untuk helper internal, lebih baik tetap deskriptif daripada singkat:
  - `MemoryTelemetry.h` bagus
  - `NodeIdentity.h` bagus
  - `Paths.h` aman, tapi kalau nanti terlalu umum bisa jadi `StoragePaths.h`

## Catatan Migrasi

Migrasi yang sudah dieksekusi mengikuti urutan ini:

1. root repo dibersihkan lebih dulu
2. artifact/tooling dipindah ke area khusus
3. `lib/NodeCore` dipecah per domain
4. naming minor dirapikan belakangan

Urutan ini dipilih supaya perubahan path besar tetap bisa diverifikasi lewat build penuh di setiap gelombang.

## Kesimpulan

Repo ini tidak butuh restruktur total dari nol.

Yang paling layak dilakukan adalah:
- rapikan root repo
- pecah `lib/NodeCore` jadi domain folders
- konsistenkan naming file minor yang masih campur gaya

Itu sudah cukup untuk membawa repo dari "sangat rapi" ke "sangat rapi dan lebih enak dijelajahi".
