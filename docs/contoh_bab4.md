# BAB IV
# HASIL PENGUJIAN DAN ANALISIS

> Catatan: data pada tabel di bawah adalah contoh penulisan (dummy realistis).
> Ganti angka sesuai hasil pengujian lapangan kamu.

## 4.1 Lingkungan Pengujian

Pengujian dilakukan pada sistem node sensor berbasis ESP8266 dengan firmware versi `9.9.8`.
Konfigurasi interval yang digunakan:
- `DATA_UPLOAD_INTERVAL_MS = 600000` (10 menit)
- `SENSOR_SAMPLE_INTERVAL_MS = 60000` (1 menit)
- `CACHE_SEND_INTERVAL_MS = 15000` (15 detik)

Topologi jaringan menggunakan WiFi 2.4 GHz, mode operasi node diuji pada `cloud`, `edge`, dan `auto`.

## 4.2 Hasil Pengujian Node Sensor

### 4.2.1 Uji Fungsional Firmware (Node)

**Tabel 4.1 Hasil Uji Fungsional Node**

| No | Skenario | Input/Kondisi | Hasil Observasi | Status |
|---|---|---|---|---|
| 1 | Akuisisi data sensor | Node aktif, sensor valid | Data suhu, RH, lux terbaca periodik | Berhasil |
| 2 | Upload cloud normal | Internet tersedia | Data terkirim HTTPS ke endpoint `/api/sensor` | Berhasil |
| 3 | Offline caching | Internet diputus | Data tersimpan di RTC/LittleFS, tidak hilang | Berhasil |
| 4 | Recovery kirim cache | Internet dipulihkan | Data backlog terkirim bertahap | Berhasil |
| 5 | Mode `auto` fallback | Cloud gagal berulang | Node pindah ke gateway mode (edge) | Berhasil |
| 6 | Mode `cloud` paksa | Perintah mode cloud | Node tetap kirim ke cloud | Berhasil |
| 7 | Mode `edge` paksa | Perintah mode edge | Node kirim ke gateway lokal | Berhasil |

**Bukti Visual Uji Fungsional**

![Gambar 4.1 Serial monitor node saat upload cloud](assets/bukti/gambar-4-1-upload-cloud.jpg)
*Gambar 4.1 Serial monitor node saat upload cloud berhasil.*

![Gambar 4.2 Dashboard server menerima data realtime](assets/bukti/gambar-4-2-dashboard-realtime.jpg)
*Gambar 4.2 Dashboard server menampilkan data sensor realtime.*

### 4.2.2 Uji Caching dan Integritas Data

**Tabel 4.2 Hasil Uji Caching**

| Kondisi Jaringan | Aktivitas Node | Indikator Serial Monitor | Status Data di Server | Hasil |
|---|---|---|---|---|
| Online | Kirim data normal | `[UPLOAD] HTTPS send` | Bertambah realtime | Berhasil |
| Offline | Simpan data | `RTC/LittleFS write` | Tidak bertambah | Berhasil |
| Offline | Coba kirim | `Cloud unreachable` | Tetap tidak bertambah | Berhasil |
| Online kembali | Flush backlog | `Cloud recovered` | Data backlog masuk | Berhasil |

**Tabel 4.3 Hasil Uji Integritas Cache (CRC)**

| Skenario | Perlakuan | Hasil Sistem | Hasil |
|---|---|---|---|
| Record normal | Tidak ada fault | Record terbaca valid | Berhasil |
| CRC mismatch | Injeksi korupsi payload | Record korup dibuang | Berhasil |
| Magic byte rusak | Injeksi fault ringan | Deep recovery mencoba salvage | Berhasil |
| Sync loss | Korupsi beruntun | Resync scan cache | Berhasil |

**Bukti Visual Uji Caching dan CRC**

![Gambar 4.3 Log saat internet terputus dan data masuk cache](assets/bukti/gambar-4-3-offline-cache.jpg)
*Gambar 4.3 Log firmware saat internet terputus dan data disimpan pada cache lokal.*

![Gambar 4.4 Log CRC mismatch pada record cache](assets/bukti/gambar-4-4-crc-mismatch.jpg)
*Gambar 4.4 Log deteksi CRC mismatch dan pembuangan record korup.*

![Gambar 4.5 Log flush backlog setelah koneksi pulih](assets/bukti/gambar-4-5-flush-backlog.jpg)
*Gambar 4.5 Log pengiriman backlog setelah koneksi internet kembali normal.*

### 4.2.3 Uji Akurasi Sensor

Rumus error yang digunakan:

\[
\text{Error (\%)} = \frac{|X_{node} - X_{validator}|}{X_{validator}} \times 100
\]

**Tabel 4.4 Hasil Uji Akurasi Suhu**

| Node | Suhu Node (C) | Suhu Validator (C) | Error (%) |
|---|---:|---:|---:|
| 1 | 25.4 | 25.5 | 0.39 |
| 2 | 25.7 | 25.6 | 0.39 |
| 3 | 25.2 | 25.3 | 0.40 |
| 4 | 25.9 | 26.0 | 0.38 |
| 5 | 26.1 | 26.0 | 0.38 |
| 6 | 25.6 | 25.7 | 0.39 |
| 7 | 25.3 | 25.4 | 0.39 |
| 8 | 25.8 | 25.7 | 0.39 |
| 9 | 26.0 | 26.1 | 0.38 |
| 10 | 25.5 | 25.6 | 0.39 |
| **Rata-rata** |  |  | **0.39** |

**Tabel 4.5 Hasil Uji Akurasi Kelembapan**

| Node | RH Node (%) | RH Validator (%) | Error (%) |
|---|---:|---:|---:|
| 1 | 68.0 | 67.2 | 1.19 |
| 2 | 67.5 | 68.4 | 1.32 |
| 3 | 69.1 | 68.2 | 1.32 |
| 4 | 68.3 | 67.6 | 1.04 |
| 5 | 67.8 | 68.7 | 1.31 |
| 6 | 68.9 | 68.1 | 1.17 |
| 7 | 67.2 | 68.0 | 1.18 |
| 8 | 69.0 | 68.3 | 1.02 |
| 9 | 68.1 | 67.4 | 1.04 |
| 10 | 67.9 | 68.6 | 1.02 |
| **Rata-rata** |  |  | **1.16** |

**Tabel 4.6 Hasil Uji Akurasi Intensitas Cahaya**

| Node | Lux Node (lx) | Lux Validator (lx) | Error (lx) |
|---|---:|---:|---:|
| 1 | 512 | 500 | 12 |
| 2 | 498 | 510 | 12 |
| 3 | 521 | 505 | 16 |
| 4 | 540 | 525 | 15 |
| 5 | 476 | 490 | 14 |
| 6 | 532 | 520 | 12 |
| 7 | 488 | 500 | 12 |
| 8 | 506 | 520 | 14 |
| 9 | 515 | 505 | 10 |
| 10 | 495 | 510 | 15 |
| **Rata-rata** |  |  | **13.2** |

**Bukti Visual Uji Akurasi**

![Gambar 4.6 Pengukuran suhu dan kelembapan node dibanding alat validator](assets/bukti/gambar-4-6-akurasi-sht.jpg)
*Gambar 4.6 Proses pembandingan pembacaan sensor SHT dengan alat validator.*

![Gambar 4.7 Pengukuran intensitas cahaya node dibanding lux meter](assets/bukti/gambar-4-7-akurasi-bh1750.jpg)
*Gambar 4.7 Proses pembandingan sensor BH1750 dengan lux meter.*

### 4.2.4 Uji QoS Komunikasi (Firmware QoS)

Parameter yang dihitung firmware: packet loss, latency min/avg/max, dan jitter (max-min).

**Tabel 4.7 Hasil QoS Upload API (5 Sampel)**

| Sampel | Status HTTP | Latency (ms) |
|---|---|---:|
| 1 | Sukses | 182 |
| 2 | Sukses | 175 |
| 3 | Sukses | 190 |
| 4 | Sukses | 201 |
| 5 | Sukses | 187 |

**Ringkasan QoS:**
- Success: 5/5
- Packet loss: 0%
- Avg latency: 187 ms
- Min latency: 175 ms
- Max latency: 201 ms
- Jitter: 26 ms

**Bukti Visual Uji QoS**

![Gambar 4.8 Hasil command qosupload di terminal diagnostik](assets/bukti/gambar-4-8-qosupload.jpg)
*Gambar 4.8 Hasil pengujian QoS upload API melalui terminal diagnostik.*

### 4.2.5 Uji Enkripsi Komunikasi Lokal (WebSocket AES)

**Tabel 4.8 Hasil Uji Enkripsi WebSocket**

| Perintah Input | Payload Jaringan | Hasil Enkripsi | Hasil Dekripsi |
|---|---|---|---|
| `status` | `iv_b64:cipher_b64` | Tidak terbaca plaintext | Valid |
| `read` | `iv_b64:cipher_b64` | Tidak terbaca plaintext | Valid |
| `setwifi ...` | `iv_b64:REDACTED

**Bukti Visual Uji Enkripsi**

![Gambar 4.9 Capture frame WebSocket terenkripsi](assets/bukti/gambar-4-9-ws-encrypted.jpg)
*Gambar 4.9 Payload WebSocket terlihat terenkripsi (bukan plaintext perintah).*

### 4.2.6 Uji Autentikasi Akses

**Tabel 4.9 Hasil Uji Autentikasi**

| Fitur | Skenario | Input Kredensial | Respon Sistem | Hasil |
|---|---|---|---|---|
| Web Terminal | Akses command admin tanpa login | Belum login | Ditolak (`Access Denied`) | Berhasil |
| Web Terminal | Login salah berulang | Password salah | Lockout sementara | Berhasil |
| Web Terminal | Login benar | Password benar | Akses admin diberikan | Berhasil |
| API Upload | Token salah | Bearer REDACTED_TOKEN | HTTP unauthorized | Berhasil |
| API Upload | Token benar | Bearer REDACTED_TOKEN | Data diterima server | Berhasil |

**Bukti Visual Uji Autentikasi**

![Gambar 4.10 Akses command admin ditolak sebelum login](assets/bukti/gambar-4-10-auth-denied.jpg)
*Gambar 4.10 Command admin ditolak ketika sesi belum login.*

![Gambar 4.11 Login berhasil dan command admin dapat dijalankan](assets/bukti/gambar-4-11-auth-success.jpg)
*Gambar 4.11 Login berhasil dan command admin diterima sistem.*

### 4.2.7 Uji OTA

**Tabel 4.10 Hasil Uji OTA**

| Versi Firmware Saat Ini | Versi OTA Server | Proses Update | Versi Setelah Reboot | Hasil |
|---|---|---|---|---|
| 9.9.8 | 9.9.8 | Cek berkala, no update | 9.9.8 | Berhasil |
| 9.9.8 | 9.9.9 | Download + flash + reboot | 9.9.9 | Berhasil |
| 9.9.9 | URL tidak valid | Update dibatalkan aman | 9.9.9 | Berhasil |

**Bukti Visual Uji OTA**

![Gambar 4.12 Log pengecekan OTA dan deteksi firmware baru](assets/bukti/gambar-4-12-ota-check.jpg)
*Gambar 4.12 Log firmware saat pengecekan OTA dan deteksi versi terbaru.*

![Gambar 4.13 Log proses update OTA hingga reboot](assets/bukti/gambar-4-13-ota-update.jpg)
*Gambar 4.13 Log proses update OTA sampai perangkat reboot.*

## 4.3 Hasil Uji Otomatis Internal (PlatformIO)

**Tabel 4.11 Ringkasan Unit Test Internal**

| Environment Test | Hasil | Keterangan |
|---|---|---|
| `pio test -e native` (test_native_json) | Lulus | 3 test case berhasil |
| `pio test -e native` (test_native_stress) | Gagal build | Dependency header belum terpenuhi |
| `pio test -e integration_test_mocked` | Gagal compile | Mock test belum sinkron dengan interface terbaru |

**Bukti Visual Unit Test**

![Gambar 4.14 Hasil eksekusi pio test native](assets/bukti/gambar-4-14-pio-test-native.jpg)
*Gambar 4.14 Output pengujian otomatis menggunakan PlatformIO.*

## 4.4 Analisis

1. Node berhasil menjalankan fungsi inti: akuisisi, caching, upload, fallback mode, dan OTA.
2. Jalur no-data-loss bekerja karena data tidak langsung dibuang saat koneksi gagal.
3. Akurasi suhu dan kelembapan berada dalam rentang yang dapat diterima untuk monitoring mikroklimat.
4. QoS upload menunjukkan latensi stabil dengan packet loss 0% pada skenario uji ini.
5. Keamanan komunikasi lokal dan API sudah berlapis melalui enkripsi payload dan autentikasi.
6. Test otomatis internal sudah berjalan sebagian, tetapi pipeline test perlu dirapikan agar seluruh suite bisa lulus.

## 4.5 Rekapitulasi Akhir Pengujian

**Tabel 4.12 Rekapitulasi**

| No | Kategori Pengujian | Status Akhir |
|---|---|---|
| 1 | Fungsional node | Berhasil |
| 2 | Caching & recovery | Berhasil |
| 3 | Integritas data (CRC) | Berhasil |
| 4 | Akurasi sensor | Berhasil |
| 5 | QoS komunikasi | Berhasil |
| 6 | Enkripsi & autentikasi | Berhasil |
| 7 | OTA | Berhasil |
| 8 | Unit test internal | Sebagian berhasil |

Berdasarkan seluruh hasil pengujian, node sensor telah memenuhi kebutuhan utama sistem pemantauan greenhouse dari sisi fungsional, keandalan data, dan keamanan komunikasi.

## 4.6 Matriks Jejak Bukti (Evidence Matrix)

**Tabel 4.13 Jejak Bukti Pengujian**

| No Uji | Nama Uji | Kode Bukti | File Bukti | Keterangan |
|---|---|---|---|---|
| U-01 | Upload cloud | B-01 | `assets/bukti/gambar-4-1-upload-cloud.jpg` | Log kirim HTTPS sukses |
| U-02 | Realtime dashboard | B-02 | `assets/bukti/gambar-4-2-dashboard-realtime.jpg` | Data tampil di dashboard |
| U-03 | Offline caching | B-03 | `assets/bukti/gambar-4-3-offline-cache.jpg` | Data masuk cache lokal |
| U-04 | CRC handling | B-04 | `assets/bukti/gambar-4-4-crc-mismatch.jpg` | Record korup ditolak |
| U-05 | Flush backlog | B-05 | `assets/bukti/gambar-4-5-flush-backlog.jpg` | Backlog terkirim |
| U-06 | Akurasi SHT | B-06 | `assets/bukti/gambar-4-6-akurasi-sht.jpg` | Perbandingan alat validator |
| U-07 | Akurasi BH1750 | B-07 | `assets/bukti/gambar-4-7-akurasi-bh1750.jpg` | Perbandingan lux meter |
| U-08 | QoS upload | B-08 | `assets/bukti/gambar-4-8-qosupload.jpg` | Nilai latency/loss/jitter |
| U-09 | Enkripsi WS | B-09 | `assets/bukti/gambar-4-9-ws-encrypted.jpg` | Payload terenkripsi |
| U-10 | Auth deny | B-10 | `assets/bukti/gambar-4-10-auth-denied.jpg` | Akses admin ditolak |
| U-11 | Auth success | B-11 | `assets/bukti/gambar-4-11-auth-success.jpg` | Login berhasil |
| U-12 | OTA check | B-12 | `assets/bukti/gambar-4-12-ota-check.jpg` | Deteksi update |
| U-13 | OTA update | B-13 | `assets/bukti/gambar-4-13-ota-update.jpg` | Update dan reboot |
| U-14 | Unit test | B-14 | `assets/bukti/gambar-4-14-pio-test-native.jpg` | Output test internal |
