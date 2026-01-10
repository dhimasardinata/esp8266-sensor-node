/**
 * @file sensor_data.h
 * @brief Mendefinisikan struktur data standar untuk pembacaan sensor.
 */
#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

/**
 * @struct SensorReading
 * @brief Menampung sebuah nilai pembacaan sensor beserta status validitasnya.
 * @details Ini menghindari penggunaan "magic numbers" (seperti -999) untuk
 *          menandakan data yang tidak valid. Modul yang menggunakan ini dapat
 *          dengan mudah memeriksa flag `isValid`.
 */
struct SensorReading {
  float value;   ///< Nilai sensor yang dibaca.
  bool isValid;  ///< Status validitas pembacaan (true jika valid, false jika tidak).
};

#endif  // SENSOR_DATA_H