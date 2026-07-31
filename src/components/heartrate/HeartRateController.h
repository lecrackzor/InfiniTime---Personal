#pragma once

#include <cstdint>
#include <components/ble/HeartRateService.h>

namespace Pinetime {
  namespace Applications {
    class HeartRateTask;
  }

  namespace Controllers {
    class HeartRateController {
    public:
      enum class States : uint8_t { Disabled, Stopped, NotEnoughData, Searching, Ready, NoTouch };

      HeartRateController() = default;
      void Enable();
      void Disable();
      void UpdateState(States newState);
      // Update face + notify BLE only when the BPM value changes (stock behaviour).
      void UpdateHeartRate(uint8_t heartRate);
      // Update face only — no BLE (Searching hold).
      void UpdateDisplayedHeartRate(uint8_t heartRate);
      // Update face and attempt BLE notify. True only if the stack accepted the notify.
      bool TryNotifyHeartRateToService(uint8_t heartRate);

      void SetHeartRateTask(Applications::HeartRateTask* task);

      States State() const {
        return state;
      }

      uint8_t HeartRate() const {
        return heartRate;
      }

      void SetService(Pinetime::Controllers::HeartRateService* service);

    private:
      Applications::HeartRateTask* task = nullptr;
      States state = States::Disabled;
      uint8_t heartRate = 0;
      Pinetime::Controllers::HeartRateService* service = nullptr;
    };
  }
}
