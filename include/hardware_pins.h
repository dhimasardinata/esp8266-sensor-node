/**
 * @file hardware_pins.h
 * @brief Definisi pin perangkat keras terpusat untuk Node Greenhouse Monitoring.
 * @details File ini menyediakan satu sumber kebenaran untuk semua penetapan pin
 *          GPIO yang digunakan dalam proyek.
 */
#ifndef HARDWARE_PINS_H
#define HARDWARE_PINS_H

/**
 * @defgroup i2c_pins Pin Bus I2C
 * @brief Definisi pin untuk bus I2C utama yang digunakan untuk komunikasi sensor.
 * @{
 */

/**
 * @def PIN_I2C_SDA
 * @brief Pin GPIO untuk data I2C (SDA). Sesuai dengan D2 pada papan
 *        NodeMCU/WEMOS.
 */
#define PIN_I2C_SDA 4

/**
 * @def PIN_I2C_SCL
 * @brief Pin GPIO untuk clock I2C (SCL). Sesuai dengan D1 pada papan
 *        NodeMCU/WEMOS.
 */
#define PIN_I2C_SCL 5

/** @} end of i2c_pins group */

#endif  // HARDWARE_PINS_H