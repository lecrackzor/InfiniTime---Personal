#include "heartratetask/HeartRateTask.h"
#include <components/heartrate/HeartRateController.h>
#include <drivers/Hrs3300.h>
#include <drivers/Bma421.h>
#include <limits>
#include <optional>
#include <cstdlib>
#include <algorithm>
#include <cmath>

#include "utility/Math.h"

using namespace Pinetime::Applications;
using ControllerStates = Pinetime::Controllers::HeartRateController::States;

namespace {
  constexpr TickType_t backgroundMeasurementTimeLimit = 30 * configTICK_RATE_HZ;

  inline bool in_isr() {
    return (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk) != 0;
  }
}

std::optional<TickType_t> HeartRateTask::BackgroundMeasurementInterval() const {
  auto interval = settings.GetHeartRateBackgroundMeasurementInterval();
  if (!interval.has_value()) {
    return std::nullopt;
  }
  return interval.value() * configTICK_RATE_HZ;
}

bool HeartRateTask::BackgroundMeasurementNeeded() const {
  if (pausedByCharging) {
    return false;
  }
  auto backgroundPeriod = BackgroundMeasurementInterval();
  if (!backgroundPeriod.has_value()) {
    return false;
  }
  return xTaskGetTickCount() - lastMeasurementTime >= backgroundPeriod.value();
};

TickType_t HeartRateTask::CurrentTaskDelay() {
  auto backgroundPeriod = BackgroundMeasurementInterval();
  TickType_t currentTime = xTaskGetTickCount();
  auto CalculateSleepTicks = [&]() {
    TickType_t elapsed = currentTime - measurementStartTime;

    // Target system tick is the elapsed sensor ticks multiplied by the sensor tick duration (i.e. the elapsed time)
    // multiplied by the system tick rate
    // Since the sensor tick duration is a whole number of milliseconds, we compute in milliseconds and then divide by 1000
    // To avoid the number of milliseconds overflowing a u32, we take a factor of 2 out of the divisor and dividend
    // (1024 / 2) * 65536 * 100 = 3355443200 which is less than 2^32

    constexpr uint16_t deltaTms = Controllers::Ppg::sampleDuration * 1000;
    // Guard against future tick rate changes
    static_assert((configTICK_RATE_HZ / 2ULL) * (std::numeric_limits<decltype(count)>::max() + 1ULL) * static_cast<uint64_t>((deltaTms)) <
                    std::numeric_limits<uint32_t>::max(),
                  "Overflow");
    TickType_t elapsedTarget = Utility::RoundedDiv(static_cast<uint32_t>(configTICK_RATE_HZ / 2) * (static_cast<uint32_t>(count) + 1U) *
                                                     static_cast<uint32_t>((deltaTms)),
                                                   static_cast<uint32_t>(1000 / 2));

    // On count overflow, reset both count and start time
    // Count is 16bit to avoid overflow in elapsedTarget
    // Count overflows every 100ms * u16 max = ~2 hours, much more often than the tick count (~48 days)
    // So no need to check for tick count overflow
    if (count == std::numeric_limits<decltype(count)>::max()) {
      count = 0;
      measurementStartTime = currentTime;
    }
    if (elapsedTarget > elapsed) {
      return elapsedTarget - elapsed;
    }
    return static_cast<TickType_t>(0);
  };
  switch (state) {
    case States::Disabled:
      return portMAX_DELAY;
    case States::Waiting:
      // Sleep until a new event if background measuring disabled or paused for charging
      if (pausedByCharging || !backgroundPeriod.has_value()) {
        return portMAX_DELAY;
      }
      // Sleep until the next background measurement
      if (currentTime - lastMeasurementTime < backgroundPeriod.value()) {
        return backgroundPeriod.value() - (currentTime - lastMeasurementTime);
      }
      // If one is due now, go straight away
      return 0;
    case States::BackgroundMeasuring:
    case States::ForegroundMeasuring:
      return CalculateSleepTicks();
  }
  // Needed to keep dumb compiler happy, this is unreachable
  // Any new additions to States will cause the above switch statement not to compile, so this is safe
  return portMAX_DELAY;
}

HeartRateTask::HeartRateTask(Drivers::Hrs3300& heartRateSensor,
                             Controllers::HeartRateController& controller,
                             Controllers::Settings& settings,
                             Drivers::Bma421& motionSensor)
  : heartRateSensor {heartRateSensor}, controller {controller}, settings {settings}, motionSensor {motionSensor} {
}

void HeartRateTask::Start() {
  messageQueue = xQueueCreate(10, 1);
  controller.SetHeartRateTask(this);

  // PPGv2 adaptive filter needs headroom; keep above upstream's 400-word default.
  if (pdPASS != xTaskCreate(HeartRateTask::Process, "Heartrate", 500, this, 1, &taskHandle)) {
    APP_ERROR_HANDLER(NRF_ERROR_NO_MEM);
  }

  if (settings.GetHeartRateEnabledOnBoot()) {
    controller.Enable();
  }
}

void HeartRateTask::Process(void* instance) {
  auto* app = static_cast<HeartRateTask*>(instance);
  app->Work();
}

void HeartRateTask::Work() {
  // measurementStartTime is always initialised before use by StartMeasurement
  // Need to initialise lastMeasurementTime so that the first background measurement happens at a reasonable time
  lastMeasurementTime = xTaskGetTickCount();
  valueCurrentlyShown = false;

  while (true) {
    TickType_t delay = CurrentTaskDelay();
    Messages msg;
    States newState = state;

    if (xQueueReceive(messageQueue, &msg, delay) == pdTRUE) {
      switch (msg) {
        case Messages::GoToSleep:
          // Ignore power state changes when disabled
          if (state == States::Disabled) {
            break;
          }
          // State is necessarily ForegroundMeasuring
          // As previously screen was on and measurement is enabled
          if (BackgroundMeasurementNeeded()) {
            newState = States::BackgroundMeasuring;
          } else {
            newState = States::Waiting;
          }
          break;
        case Messages::WakeUp:
          // Ignore power state changes when disabled
          if (state == States::Disabled) {
            break;
          }
          if (pausedByCharging) {
            newState = States::Waiting;
          } else {
            newState = States::ForegroundMeasuring;
          }
          break;
        case Messages::Enable:
          settings.SetHeartRateEnabledOnBoot(true);
          settings.SaveSettings();
          valueCurrentlyShown = false;
          // Can only be enabled when the screen is on
          // If this constraint is somehow violated, the unexpected state
          // will self-resolve at the next screen on event
          if (pausedByCharging) {
            newState = States::Waiting;
          } else {
            newState = States::ForegroundMeasuring;
          }
          break;
        case Messages::Disable:
          settings.SetHeartRateEnabledOnBoot(false);
          settings.SaveSettings();
          newState = States::Disabled;
          break;
        case Messages::PauseForCharging:
          if (pausedByCharging) {
            break;
          }
          pausedByCharging = true;
          if (state == States::ForegroundMeasuring || state == States::BackgroundMeasuring) {
            newState = States::Waiting;
          }
          // Clear stale BPM / "measuring" presentation while the sensor is forced off.
          SendHeartRate(ControllerStates::NotEnoughData, 0);
          break;
        case Messages::ResumeFromCharging:
          if (!pausedByCharging) {
            break;
          }
          pausedByCharging = false;
          if (state == States::Disabled) {
            break;
          }
          // Prefer waiting; WakeUp from GoToRunning (charging unplug wakes the watch) will raise to foreground
          newState = States::Waiting;
          break;
      }
    }
    if (newState == States::Waiting && BackgroundMeasurementNeeded()) {
      newState = States::BackgroundMeasuring;
    } else if (newState == States::BackgroundMeasuring && !BackgroundMeasurementNeeded()) {
      newState = States::Waiting;
    }

    // Apply state transition (switch sensor on/off)
    if ((newState == States::ForegroundMeasuring || newState == States::BackgroundMeasuring) &&
        (state == States::Waiting || state == States::Disabled)) {
      StartMeasurement();
    } else if ((newState == States::Waiting || newState == States::Disabled) &&
               (state == States::ForegroundMeasuring || state == States::BackgroundMeasuring)) {
      StopMeasurement();
      controller.UpdateState(ControllerStates::Stopped);
    }
    if (newState == States::Disabled) {
      SendHeartRate(ControllerStates::Disabled, 0);
    }
    state = newState;

    if (state == States::ForegroundMeasuring || state == States::BackgroundMeasuring) {
      HandleSensorData();
      count++;
    }
  }
}

void HeartRateTask::PushMessage(HeartRateTask::Messages msg) {
  if (in_isr()) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(messageQueue, &msg, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  } else {
    xQueueSend(messageQueue, &msg, portMAX_DELAY);
  }
}

void HeartRateTask::StartMeasurement() {
  heartRateSensor.Enable();
  ppg.Reset();
  lastHrs = 0;
  count = 0;
  backgroundBleSent = false;
  measurementStartTime = xTaskGetTickCount();
}

void HeartRateTask::StopMeasurement() {
  heartRateSensor.Disable();
  ppg.Reset();
  lastHrs = 0;
}

void HeartRateTask::HandleSensorData() {
  auto sensorData = heartRateSensor.ReadHrsAls();
  auto motionValues = motionSensor.Process();

  // Always run AGC / no-touch — even when hrs is 0.
  // Skipping AutoGain on hrs==0 (old "sensor starting up" early-return) left NoTouch
  // unable to progress and skipped the background time-limit, so the green LED could
  // keep sampling forever off-wrist (see upstream #2371 discussion with tituscmd).
  auto ppgState = heartRateSensor.AutoGain(sensorData.hrs, sensorData.als);

  std::optional<uint8_t> bpm = std::nullopt;
  if (ppgState == Drivers::Hrs3300::PPGState::NoTouch) {
    SendHeartRate(ControllerStates::NoTouch, 0);
  } else if (ppgState == Drivers::Hrs3300::PPGState::Reset) {
    ppg.Reset();
    lastHrs = 0;
    SendHeartRate(ControllerStates::NotEnoughData, 0);
  } else if (ppgState == Drivers::Hrs3300::PPGState::Running) {
    uint16_t hrsSample = 0;
    if (sensorData.hrs != 0) {
      static constexpr float discontinuityThreshold = 0.2f;
      if (lastHrs != 0 && std::abs(static_cast<int32_t>(sensorData.hrs) - static_cast<int32_t>(lastHrs)) >
                            std::min(lastHrs, sensorData.hrs) * discontinuityThreshold) {
        ppg.ScaleHrs(static_cast<float>(sensorData.hrs) / static_cast<float>(lastHrs));
      }
      lastHrs = sensorData.hrs;
      hrsSample = sensorData.hrs;
    } else if (lastHrs != 0) {
      // hrs==0 while already running is a dropped sample — hold the last good reading so
      // the FFT keeps a uniform 48 ms timeline (skipping would stretch periods and bias BPM).
      hrsSample = lastHrs;
    }

    if (hrsSample != 0) {
      ppg.Ingest(hrsSample, motionValues.x, motionValues.y, motionValues.z);

      bpm = ppg.HeartRate();
      if (bpm.has_value()) {
        valueCurrentlyShown = true;
        controller.UpdateState(ControllerStates::Ready);
        if (state == States::BackgroundMeasuring) {
          // Cont (interval 0): sensor stays on; Ppg::HeartRate() returns ~every 48 ms
          // while locked — must be change-only or GB floods at ~20 Hz.
          // Timed intervals (30s/3m/…): one always-notify per StartMeasurement session.
          auto period = BackgroundMeasurementInterval();
          const bool continuousBackground = period.has_value() && period.value() == 0;
          if (continuousBackground) {
            controller.UpdateHeartRate(bpm.value());
          } else if (!backgroundBleSent) {
            controller.NotifyHeartRateToService(bpm.value());
            backgroundBleSent = true;
          } else {
            controller.UpdateHeartRate(bpm.value());
          }
        } else {
          // Foreground: same ~48 ms return rate — change-only only.
          controller.UpdateHeartRate(bpm.value());
        }
      } else if (ppg.SufficientData()) {
        // Keep last known BPM on the watch face and over BLE while re-acquiring.
        if (valueCurrentlyShown) {
          controller.UpdateState(ControllerStates::Searching);
        } else {
          SendHeartRate(ControllerStates::Searching, 0);
        }
      } else {
        // If there's currently a value shown, don't clear it
        // But still update the algorithm state
        if (valueCurrentlyShown) {
          controller.UpdateState(ControllerStates::NotEnoughData);
        } else {
          SendHeartRate(ControllerStates::NotEnoughData, 0);
        }
      }
    }
    // else: true startup (hrs still 0) — fall through for background timeout / AGC only
  }

  if (bpm.has_value()) {
    // Maintain constant frequency acquisition in background mode
    // If the last measurement time is set to the start time, then the next measurement
    // will start exactly one background period after this one
    // Avoid this if measurement exceeded the time limit (which happens with background intervals <= limit)
    if (state == States::BackgroundMeasuring && xTaskGetTickCount() - measurementStartTime < backgroundMeasurementTimeLimit) {
      lastMeasurementTime = measurementStartTime;
    } else {
      lastMeasurementTime = xTaskGetTickCount();
    }
    return;
  }
  // If been measuring for longer than the time limit, set the last measurement time
  // This allows giving up on background measurement after a while (including NoTouch / hrs==0)
  // and also means that background measurement won't begin immediately after
  // an unsuccessful long foreground measurement
  if (xTaskGetTickCount() - measurementStartTime > backgroundMeasurementTimeLimit) {
    if (state == States::BackgroundMeasuring) {
      lastMeasurementTime = xTaskGetTickCount() - backgroundMeasurementTimeLimit;
    } else {
      lastMeasurementTime = xTaskGetTickCount();
    }
  }
}

void HeartRateTask::SendHeartRate(ControllerStates state, int bpm) {
  valueCurrentlyShown = bpm != 0;
  controller.UpdateState(state);
  controller.UpdateHeartRate(static_cast<uint8_t>(bpm));
}
