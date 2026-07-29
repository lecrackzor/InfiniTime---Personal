#pragma once
#include <FreeRTOS.h>
#include <cstdint>
#include <optional>
#include <task.h>
#include <queue.h>
#include "components/heartrate/Ppg.h"
#include "components/settings/Settings.h"
#include "components/heartrate/HeartRateController.h"

namespace Pinetime {
  namespace Drivers {
    class Hrs3300;
    class Bma421;
  }

  namespace Applications {
    class HeartRateTask {
    public:
      enum class Messages : uint8_t { GoToSleep, WakeUp, Enable, Disable, PauseForCharging, ResumeFromCharging };
      explicit HeartRateTask(Drivers::Hrs3300& heartRateSensor,
                             Controllers::HeartRateController& controller,
                             Controllers::Settings& settings,
                             Pinetime::Drivers::Bma421& motionSensor);
      void Start();
      void Work();
      void PushMessage(Messages msg);

    private:
      enum class States : uint8_t { Disabled, Waiting, BackgroundMeasuring, ForegroundMeasuring };
      static void Process(void* instance);
      void HandleSensorData();
      void StartMeasurement();
      void StopMeasurement();

      [[nodiscard]] bool BackgroundMeasurementNeeded() const;
      [[nodiscard]] std::optional<TickType_t> BackgroundMeasurementInterval() const;
      TickType_t CurrentTaskDelay();
      void SendHeartRate(Controllers::HeartRateController::States state, int bpm);

      TaskHandle_t taskHandle;
      QueueHandle_t messageQueue;
      bool valueCurrentlyShown;
      bool pausedByCharging = false;
      // One BLE attempt consumed for this StartMeasurement session.
      bool backgroundBleSent = false;
      // Timed 1/3/5m: last successful GB point (rate-limit screen-wake duplicates).
      bool timedBleEverSent = false;
      TickType_t lastTimedBleTick = 0;
      States state = States::Disabled;
      uint16_t count;
      uint16_t lastHrs = 0;
      Drivers::Hrs3300& heartRateSensor;
      Controllers::HeartRateController& controller;
      Controllers::Settings& settings;
      Drivers::Bma421& motionSensor;
      Controllers::Ppg ppg;
      TickType_t lastMeasurementTime;
      TickType_t measurementStartTime;
    };

  }
}
