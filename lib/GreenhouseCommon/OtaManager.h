/**
 * @file OtaManager.h
 * @brief Mengelola pembaruan firmware Over-The-Air (OTA) dari server jarak jauh.
 */
#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <IntervalTimer.h>

#include "ConfigManager.h"  // <-- ADD THIS INCLUDE

// Forward declare dependencies
class NtpClient;
class WifiManager;
namespace BearSSL {
  class WiFiClientSecure;
}

// Konstanta OTA
constexpr long INITIAL_UPDATE_DELAY_MS = 2 * 60 * 1000;          // 2 menit
constexpr long REGULAR_UPDATE_INTERVAL_MS = 1 * 60 * 60 * 1000;  // 1 jam

/**
 * @class OtaManager
 * @brief Menangani pengecekan dan penerapan pembaruan firmware secara otomatis.
 * @details Kelas ini secara berkala menghubungi server API untuk memeriksa apakah
 *          ada versi firmware baru yang tersedia. Jika ada, ia akan mengunduh
 *          dan menerapkan pembaruan tersebut secara aman melalui HTTPS.
 */
class OtaManager {
public:
  /**
   * @brief Konstruktor untuk OtaManager.
   * @param ntpClient Referensi ke instance NtpClient untuk memastikan waktu sudah sinkron.
   * @param wifiManager Referensi ke instance WifiManager untuk memastikan koneksi ada.
   * @param secureClient Referensi ke instance WiFiClientSecure bersama untuk koneksi HTTPS.
   * @param configManager Referensi ke instance ConfigManager untuk mengakses konfigurasi.
   */
  OtaManager(NtpClient& ntpClient,
             WifiManager& wifiManager,
             BearSSL::WiFiClientSecure& secureClient,
             ConfigManager& configManager);

  OtaManager(const OtaManager&) = delete;
  OtaManager& operator=(const OtaManager&) = delete;

  /**
   * @brief Menginisialisasi timer internal untuk pengecekan pembaruan.
   */
  void init();

  /**
   * @brief Menerapkan konfigurasi yang relevan (seperti interval) ke OtaManager.
   * @param config Objek AppConfig yang berisi pengaturan baru.
   */
  void applyConfig(const AppConfig& config);

  /**
   * @brief Fungsi utama yang harus dipanggil di setiap iterasi loop.
   * @details Menjalankan logika pengecekan pembaruan secara berkala.
   */
  void handle();
  /**
   * @brief Menjadwalkan pengecekan pembaruan firmware secara paksa pada siklus berikutnya.
   * @details Berguna untuk memicu pembaruan dari terminal diagnostik.
   */
  void forceUpdateCheck();

private:
  void checkForUpdates();

  NtpClient& m_ntpClient;
  WifiManager& m_wifiManager;
  BearSSL::WiFiClientSecure& m_secureClient;
  ConfigManager& m_configManager;

  // State Internal
  IntervalTimer m_updateCheckTimer;
  bool m_force_check = false;
  bool m_is_first_check = true;
};

#endif  // OTA_MANAGER_H