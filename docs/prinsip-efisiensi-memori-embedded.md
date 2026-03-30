# Prinsip Efisiensi Memori untuk Proyek Embedded Ini

Dokumen ini merangkum prinsip coding yang dipakai dan sebaiknya dipertahankan di proyek ini agar sistem tetap stabil pada perangkat dengan RAM kecil, heap rapuh, dan workload yang campuran: WiFi, WebSocket, TLS, OTA, cache, dan filesystem.

Fokus utama proyek ini bukan sekadar "hemat byte", tetapi:

- menjaga sistem tetap hidup saat heap menipis
- menekan fragmentasi heap jangka panjang
- membuat kegagalan memori menjadi terkontrol, bukan acak
- mempertahankan fitur inti walau harus menurunkan mode operasi

## 1. Flash adalah rumah untuk string statis

Semua literal yang tidak berubah harus tinggal di flash, bukan RAM.

Praktik yang dipakai:

- gunakan `F("...")` untuk log dan output tetap
- gunakan `PSTR("...")`, `PGM_P`, dan `__FlashStringHelper`
- gunakan API versi flash-aware seperti:
  - `printf_P`
  - `snprintf_P`
  - `vsnprintf_P`
  - `strcmp_P`
  - `strncmp_P`
  - `strcasecmp_P`
  - `strstr_P`
  - `memcmp_P`
  - `strncpy_P`

Kenapa:

- string statis sangat banyak dan diam-diam memakan RAM bila dibiarkan sebagai literal biasa
- pada ESP8266, perbedaan ini terasa langsung saat fitur bertambah

Contoh pola di repo:

- `lib/NodeCore/support/Utils.cpp`
- `lib/NodeCore/terminal/TerminalFormatting.cpp`
- `lib/NodeCore/commands/ICommand.h`
- `lib/NodeCore/terminal/DiagnosticsTerminal.Commands.cpp`

## 2. Heap adalah pilihan terakhir, bukan default

Jangan mengalokasikan heap hanya karena lebih nyaman.

Urutan preferensi:

1. `constexpr` / ukuran compile-time
2. stack buffer kecil
3. buffer statis/fixed-size
4. heap on-demand bila memang perlu

Kenapa:

- heap kecil lebih mudah terfragmentasi daripada "REDACTED"
- bug di embedded sering muncul bukan karena RAM nol, tetapi karena blok bebas terbesar terlalu kecil

Contoh pola:

- `std::array<>` untuk buffer tetap
- ring buffer ukuran tetap
- queue command dan queue WS yang dibatasi kapasitasnya

## 3. Semua struktur harus punya batas eksplisit

Tidak boleh ada struktur yang tumbuh tanpa limit.

Yang wajib punya batas:

- queue
- cache
- payload
- scan result
- command line
- buffer WebSocket
- buffer TLS

Implementasi yang dipakai:

- `CryptoUtils::MAX_PLAINTEXT_SIZE`
- `WS_QUEUE_SIZE`
- `CMD_QUEUE_SIZE`
- `MAX_SCAN_RESULTS`
- ukuran TLS di `include/config/constants.h`

Prinsipnya:

- lebih baik data dipotong, ditunda, atau dijatuhkan dengan log
- daripada heap membengkak tanpa prediksi

## 4. Alokasi dilakukan saat perlu, lalu dilepas lagi

Data besar tidak boleh hidup permanen kalau hanya dipakai sesekali.

Pola yang dipakai:

- `std::unique_ptr<>`
- `new (std::nothrow)`
- buffer dibangun saat dibutuhkan
- buffer besar dilepas lagi setelah idle atau setelah operasi selesai

Contoh:

- state WebSocket di `lib/NodeCore/support/Utils.cpp`
- scan cache WiFi di `lib/NodeCore/net/WifiManager.cpp`
- crash-log handling di `lib/NodeCore/system/CrashHandler.cpp`
- heap reserve dan mode portal di `src/main.cpp`

Maknanya:

- RAM harus mengikuti fase sistem
- bukan semua fase memesan RAM maksimum sepanjang waktu

## 5. Reuse buffer lebih baik daripada alloc/free berulang

Kalau suatu operasi sering terjadi, pakai buffer yang bisa dipakai ulang.

Pola:

- shared heap buffer untuk `ws_printf`
- buffer persisten dengan batas maksimum lalu dibebaskan lagi jika terlalu besar
- array internal yang diisi ulang, bukan buat objek baru terus-menerus

Kenapa:

- ini menekan fragmentasi
- mengurangi churn allocator

Contoh:

- `lib/NodeCore/support/Utils.cpp`

## 6. Operasi besar harus dipecah menjadi chunk

Jangan membangun atau mengirim data besar dalam satu langkah bila bisa dipecah.

Yang dipakai di proyek ini:

- chunking WebSocket terenkripsi
- parsing header HTTP secara streaming
- pembacaan crash log bertahap
- pengiriman help terminal sebagai payload terukur

Kenapa:

- chunking menjaga puncak penggunaan RAM tetap rendah
- lebih aman terhadap backpressure socket
- memudahkan yield/watchdog

Contoh:

- `lib/NodeCore/support/Utils.cpp`
- `lib/NodeCore/system/CrashHandler.cpp`
- `lib/NodeCore/api/ApiClient.Transport.cpp`

## 7. Boundary library boleh paksa `String`, tetapi hanya di sana

Beberapa library Arduino/ESP memang memaksa `String`.

Prinsip proyek:

- terima `String` hanya di boundary API library
- di luar boundary itu, tetap pakai buffer C, `string_view`, atau flash string
- lepaskan string config secepat mungkin setelah dipakai

Contoh:

- `ConfigManager::releaseStrings()` dipanggil agresif setelah save, auth, WiFi, OTA, upload, dan command

Kenapa:

- `String` paling berbahaya jika dibiarkan menyebar ke seluruh code path
- semakin jauh `String` masuk ke domain logic, semakin besar risiko fragmentasi

## 8. Kode harus sadar backpressure I/O

Pada embedded, kegagalan kirim sering terjadi bukan karena logika salah, tetapi karena socket belum siap.

Prinsip yang dipakai:

- cek `client->canSend()`
- kirim bertahap
- tunggu singkat dan `yield()` saat perlu
- bedakan partial write, timeout, dan connection lost

Contoh:

- `lib/NodeCore/support/Utils.cpp`
- `lib/NodeCore/api/ApiClient.Transport.cpp`

Kenapa:

- asumsi "REDACTED" itu salah untuk WiFi/TLS
- sistem yang memory-efficient juga harus transport-efficient

## 9. OOM bukan exceptional case, tapi skenario normal yang harus ditangani

Semua alokasi heap harus diasumsikan bisa gagal.

Aturan:

- gunakan `new (std::nothrow)`
- cek hasil alokasi
- bila gagal, turunkan mode operasi atau skip fitur non-kritis
- jangan memaksa lanjut dengan asumsi memori akan ada

Contoh:

- terminal bisa dimatikan bila buffer gagal
- scan bisa ditunda bila low heap
- portal bisa turun ke mode lebih ringan
- WebSocket/help memberi fallback saat buffer tidak tersedia

## 10. Graceful degradation lebih penting daripada mempertahankan semua fitur

Saat heap rendah, sistem harus tetap hidup walau fitur sekunder dikurangi.

Contoh keputusan arsitektur:

- mode portal memakai profil TLS lebih kecil
- scan WiFi bisa dibatasi/dilewati
- WebSocket/terminal bisa dinonaktifkan
- operasi berat ditunda sampai kondisi heap membaik

Ini prinsip penting:

- core function harus bertahan lebih dulu
- kenyamanan admin/debugging datang setelah stabilitas dasar

## 11. Hindari duplikasi data yang sama

Satu data tidak boleh disimpan berkali-kali dalam bentuk berbeda tanpa alasan kuat.

Pola yang dipakai:

- snapshot scan WiFi dibagi, bukan disalin ke banyak cache
- help terminal dihasilkan dari metadata command, bukan daftar kedua yang terpisah
- label mode/source/status dipusatkan ke helper flash-aware

Kenapa:

- duplikasi tidak hanya memboroskan RAM
- duplikasi juga membuat bug sinkronisasi dan dokumentasi palsu

Contoh:

- `lib/NodeCore/terminal/DiagnosticsTerminal.Commands.cpp`
- `lib/NodeCore/commands/ICommand.h`
- `lib/NodeCore/net/WifiManager.cpp`
- `lib/NodeCore/web/PortalServer.cpp`

## 12. Simpan data jangka panjang di RTC atau filesystem, bukan di RAM

RAM dipakai untuk working set saat ini, bukan untuk histori panjang.

Yang disimpan di luar RAM:

- cache upload
- credential WiFi
- crash log
- state RTC tertentu

Media yang dipakai:

- RTC untuk data volatile antar-reboot singkat
- LittleFS untuk data persisten

Keuntungan:

- RAM tetap fokus untuk operasi aktif
- sistem lebih tahan reboot dan recovery

## 13. File operation harus atomik dan diverifikasi

Efisiensi memori tidak berarti kita boleh sembrono pada integritas data.

Prinsip yang dipakai:

- tulis ke file sementara
- verifikasi ukuran / hasil tulis
- rename atomik bila lolos
- hapus file sementara jika gagal

Contoh:

- `lib/NodeCore/net/WifiCredentialStore.cpp`

Alasan:

- korupsi file sering memicu recovery mahal, scan ulang, atau fallback yang justru memakan memori lebih besar

## 14. Gunakan buffer kecil di stack untuk kasus pendek

Untuk output pendek atau format cepat, stack buffer kecil lebih sehat daripada heap.

Pola:

- buffer 32 sampai 256 byte di stack
- bila hasil melebihi stack buffer, baru naik ke heap terkontrol

Contoh:

- `ws_printf` dan `ws_printf_P`
- formatter waktu/uptime/status

Ini memberi dua keuntungan:

- jalur umum cepat
- jalur besar tetap aman

## 15. Fragmentasi harus dianggap metrik utama

Di embedded, `free heap` saja tidak cukup.

Yang harus diperhatikan:

- free heap
- max free block
- umur buffer besar
- frekuensi alloc/free
- ukuran objek sementara

Pola proyek:

- memantau free heap dan max block
- membatasi umur buffer besar
- menghindari `String` yang menyebar
- melepaskan buffer persisten jika ukurannya terlalu besar

## 16. Satu metadata, banyak pemakai

Informasi command harus menjadi single source of truth.

Prinsipnya:

- nama command
- deskripsi
- section help
- auth requirement

harus hidup di metadata command itu sendiri, bukan di tempat lain.

Manfaat:

- help tidak mudah usang
- mengurangi string duplikat
- memudahkan pemindahan metadata ke flash

Contoh:

- `lib/NodeCore/commands/ICommand.h`
- seluruh `lib/NodeCore/commands/*.h`

## 17. Ukuran resource penting harus dinyatakan di compile-time

Konfigurasi resource perlu kelihatan dan bisa diaudit.

Contoh:

- ukuran TLS
- queue size
- plaintext max
- jumlah scan result
- panjang command

Kenapa:

- compile-time constant memudahkan review
- mencegah growth diam-diam
- memudahkan membuat mode ringan vs mode penuh

## 18. Yield adalah bagian dari manajemen memori dan stabilitas

Loop panjang tanpa `yield()` bisa men-trigger masalah lain:

- watchdog reset
- socket tidak sempat drain
- queue menumpuk
- retry makin mahal

Karena itu, operasi berat atau iteratif harus memberi kesempatan sistem bernapas.

Contoh:

- scan WiFi
- WebSocket chunking
- loop utama aplikasi

## 19. Optimasi memori harus selalu mempertahankan observabilitas

Hemat memori bukan alasan untuk buta terhadap kondisi sistem.

Prinsip:

- log penting tetap ada, tetapi disimpan di flash
- error harus spesifik
- low-memory path harus bisa dibedakan dari path normal

Contoh:

- log low heap, short write, queue overflow, alloc fail

Tanpa observabilitas, optimasi memori justru sulit dipertahankan.

## 20. Prinsip review untuk setiap fitur baru

Setiap menambah fitur, cek daftar ini:

1. Apakah ada string statis baru yang belum dipindah ke flash?
2. Apakah ada queue/cache/buffer yang belum punya limit?
3. Apakah ada `String` baru yang sebenarnya bisa diganti buffer biasa?
4. Apakah alokasi heap-nya memakai `nothrow` dan fallback?
5. Apakah data besar bisa di-stream/chunk?
6. Apakah buffer besar dilepas lagi setelah idle?
7. Apakah ada duplikasi data yang bisa disatukan?
8. Apakah low-memory path tetap aman?
9. Apakah jalur error tetap memberi pesan yang jelas?
10. Apakah fitur ini tetap aman bila heap kecil, WiFi lambat, dan socket penuh?

## 21. Anti-pattern yang harus dihindari

Hal-hal berikut sebaiknya dianggap red flag:

- menyimpan banyak literal biasa tanpa `F()`/`PSTR()`
- membuat `String` di loop yang sering
- menggabungkan payload besar lewat banyak `String` temporary
- queue tanpa kapasitas maksimum
- alokasi heap tanpa cek gagal
- membangun cache ganda untuk data yang sama
- melakukan `write()` jaringan dengan asumsi pasti habis sekali
- menyimpan buffer besar permanen padahal dipakai jarang
- memakai filesystem non-atomik untuk data penting

## 22. Ringkasan filosofi proyek

Filosofi memory-efficient di proyek ini bisa diringkas begini:

- flash untuk yang statis
- stack untuk yang kecil dan singkat
- heap untuk yang benar-benar perlu
- filesystem/RTC untuk yang persisten
- chunking untuk yang besar
- limit untuk semua hal
- fallback untuk semua kegagalan
- release segera setelah selesai
- jangan duplikasi data
- lebih baik turun mode daripada crash

Kalau semua prinsip di atas dijaga, sistem akan tetap responsif, lebih tahan fragmentasi, dan jauh lebih stabil dalam operasi jangka panjang.
