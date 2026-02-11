#ifndef I_CONFIG_OBSERVER_H
#define I_CONFIG_OBSERVER_H

// ============================================================================
// IConfigObserver - Virtual Interface (Kept for Observer Pattern)
// ============================================================================
// This interface MUST remain virtual because:
// - ConfigManager stores multiple observer pointers
// - Different classes (Application, etc.) register as observers
// - Runtime polymorphism is required for the observer pattern
//
// Note: CRTP cannot be used here due to heterogeneous observer storage.

class IConfigObserver {
public:
  virtual ~IConfigObserver() = default;

  // Called by the ConfigManager after a configuration has been successfully saved.
  virtual void onConfigUpdated() = 0;
};

#endif  // I_CONFIG_OBSERVER_H