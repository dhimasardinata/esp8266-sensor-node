#ifndef BOOT_GUARD_H
#define BOOT_GUARD_H

#include <Arduino.h>

class BootGuard {
public:
  /**
   * @brief Memeriksa dan menaikkan hitungan crash boot.
   * @return Selalu true (tidak pernah memblokir boot), tugasnya hanya mencatat.
   */
  static bool check();

  /**
   * @brief Mengambil jumlah crash berturut-turut saat ini.
   */
  static uint32_t getCrashCount();

  /**
   * @brief Panggil ini setelah sistem stabil > 60 detik. Mereset hitungan crash ke 0.
   */
  static void markStable();

  /**
   * @brief Reset manual hitungan crash.
   */
  static void clear();
};

#endif  // BOOT_GUARD_H