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
      [[nodiscard]] bool IsTimedInterval() const;
      TickType_t CurrentTaskDelay();
      void SendHeartRate(Controllers::HeartRateController::States state, int bpm);
      void PublishTimedOrContinuous(uint8_t bpm);
      void TryTimedNotifyHeld();
      void AdvanceTimedSchedule(TickType_t deliveredAt);

      TaskHandle_t taskHandle;
      QueueHandle_t messageQueue;
      bool valueCurrentlyShown;
      bool pausedByCharging = false;
      States state = States::Disabled;
      uint16_t count;
      uint16_t lastHrs = 0;
      Drivers::Hrs3300& heartRateSensor;
      Controllers::HeartRateController& controller;
      Controllers::Settings& settings;
      Drivers::Bma421& motionSensor;
      Controllers::Ppg ppg;
      // Last successful timed GB deliver (due = now - this >= period). Never advance on
      // face-only locks, failed notifies, or failed BG wakes — those burned daytime slots.
      TickType_t lastMeasurementTime;
      // When BG may wake again. Separate from lastMeasurementTime so a miss can back off
      // without pretending a sample was delivered.
      TickType_t nextBackgroundAttempt;
      TickType_t measurementStartTime;
    };

  }
}
