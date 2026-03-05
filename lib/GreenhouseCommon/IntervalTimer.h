#ifndef INTERVAL_TIMER_H
#define INTERVAL_TIMER_H

#include <Arduino.h>

class IntervalTimer {
public:
  explicit IntervalTimer(unsigned long interval = 0) : m_interval(interval), m_previousMillis(millis()) {}

  bool hasElapsed(bool autoReset = true) {
    unsigned long now = millis();
    if (now - m_previousMillis >= m_interval) {
      if (autoReset) {
        m_previousMillis = now;
      }
      return true;
    }
    return false;
  }

  void reset() {
    m_previousMillis = millis();
  }
  void setInterval(unsigned long interval) {
    m_interval = interval;
  }
  unsigned long getInterval() const noexcept {
    return m_interval;
  }

  // --- METODE BARU UNTUK TESTING ---
  // Memaksa timer seolah-olah waktunya telah berlalu dengan menggeser
  // waktu 'previous' ke masa lalu. Hanya boleh digunakan dalam unit test.
  void _force_elapsed_for_test() {
    m_previousMillis = millis() - m_interval - 1;
  }
  // ---------------------------------

private:
  unsigned long m_interval;
  unsigned long m_previousMillis;
};

#endif  // INTERVAL_TIMER_H
