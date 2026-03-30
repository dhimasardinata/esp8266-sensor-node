// Centralized hardware pin definitions for the Greenhouse Monitoring Node.
// Provides a single source of truth for all GPIO assignments.
#ifndef HARDWARE_PINS_H
#define HARDWARE_PINS_H

// I2C Bus Pins
// Definitions for the primary I2C bus used for sensor communication.

// I2C Data (SDA). Corresponds to D2 on NodeMCU/WEMOS.
#define PIN_I2C_SDA 4

// I2C Clock (SCL). Corresponds to D1 on NodeMCU/WEMOS.
#define PIN_I2C_SCL 5



#endif  // HARDWARE_PINS_H