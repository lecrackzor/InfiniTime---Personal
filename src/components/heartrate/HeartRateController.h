#pragma once

#include <cstdint>
#include <components/ble/HeartRateService.h>

namespace Pinetime {
  namespace Applications {
    class HeartRateTask;
  }

  namespace System {
    class SystemTask;
  }

  namespace Controllers {
    class HeartRateController {
    public:
      enum class States : uint8_t { Disabled, Stopped, NotEnoughData, Searching, Ready, NoTouch };

      HeartRateController() = default;
      void Enable();
      void Disable();
      void UpdateState(States newState);
      void UpdateHeartRate(uint8_t heartRate);
      // Notify BLE without changing the value shown on the watch face.
      void ReportHeartRateToService(uint8_t heartRate);
      // Always notify BLE (even if BPM unchanged) — used for background samples / subscribe seed.
      void NotifyHeartRateToService(uint8_t heartRate);

      void SetHeartRateTask(Applications::HeartRateTask* task);

      States State() const {
        return state;
      }

      uint8_t HeartRate() const {
        return heartRate;
      }

      void SetService(Pinetime::Controllers::HeartRateService* service);

    private:
      void NotifyServiceIfChanged(uint8_t heartRate);

      Applications::HeartRateTask* task = nullptr;
      States state = States::Disabled;
      uint8_t heartRate = 0;
      uint8_t lastReportedHeartRate = 0;
      Pinetime::Controllers::HeartRateService* service = nullptr;
    };
  }
}
