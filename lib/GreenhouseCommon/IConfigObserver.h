#ifndef I_CONFIG_OBSERVER_H
#define I_CONFIG_OBSERVER_H

/**
 * @brief Interface for classes that need to be notified of configuration changes.
 */
class IConfigObserver {
public:
  virtual ~IConfigObserver() = default;

  /**
   * @brief Called by the ConfigManager after a configuration has been successfully saved.
   */
  virtual void onConfigUpdated() = 0;
};

#endif  // I_CONFIG_OBSERVER_H