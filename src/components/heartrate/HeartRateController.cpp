#include "components/heartrate/HeartRateController.h"

#include <cstdint>
#include <task.h>
#include "heartratetask/HeartRateTask.h"

using namespace Pinetime::Controllers;

namespace {
  // Cont only. Timed 1/3/5m sends one point per schedule window — not this floor.
  constexpr TickType_t maxBleSilentTicks = pdMS_TO_TICKS(30000);
}

void HeartRateController::UpdateState(HeartRateController::States newState) {
  this->state = newState;
}

void HeartRateController::NotifyServiceIfChanged(uint8_t heartRate) {
  const TickType_t now = xTaskGetTickCount();
  const bool changed = (lastReportedHeartRate != heartRate);
  // Never stale-refresh 0 — that would re-push Searching zeros into GB charts.
  const bool staleRefresh = (heartRate > 0) && ((now - lastBleNotifyTick) >= maxBleSilentTicks);
  if (!changed && !staleRefresh) {
    return;
  }
  lastReportedHeartRate = heartRate;
  lastBleNotifyTick = now;
  if (service != nullptr) {
    service->OnNewHeartRateValue(heartRate);
  }
}

void HeartRateController::UpdateHeartRate(uint8_t heartRate) {
  this->heartRate = heartRate;
  NotifyServiceIfChanged(heartRate);
}

void HeartRateController::UpdateDisplayedHeartRate(uint8_t heartRate) {
  this->heartRate = heartRate;
}

void HeartRateController::ReportHeartRateToService(uint8_t heartRate) {
  NotifyServiceIfChanged(heartRate);
}

void HeartRateController::NotifyHeartRateToService(uint8_t heartRate) {
  this->heartRate = heartRate;
  lastReportedHeartRate = heartRate;
  lastBleNotifyTick = xTaskGetTickCount();
  if (service != nullptr) {
    service->OnNewHeartRateValue(heartRate);
  }
}

void HeartRateController::Enable() {
  if (task != nullptr) {
    state = States::Stopped;
    task->PushMessage(Pinetime::Applications::HeartRateTask::Messages::Enable);
  }
}

void HeartRateController::Disable() {
  if (task != nullptr) {
    state = States::Disabled;
    lastReportedHeartRate = 0;
    lastBleNotifyTick = 0;
    task->PushMessage(Pinetime::Applications::HeartRateTask::Messages::Disable);
  }
}

void HeartRateController::SetHeartRateTask(Pinetime::Applications::HeartRateTask* task) {
  this->task = task;
}

void HeartRateController::SetService(Pinetime::Controllers::HeartRateService* service) {
  this->service = service;
}
