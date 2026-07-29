#include "components/heartrate/HeartRateController.h"

#include <cstdint>
#include "heartratetask/HeartRateTask.h"

using namespace Pinetime::Controllers;

void HeartRateController::UpdateState(HeartRateController::States newState) {
  this->state = newState;
}

void HeartRateController::NotifyServiceIfChanged(uint8_t heartRate) {
  if (lastReportedHeartRate == heartRate) {
    return;
  }
  lastReportedHeartRate = heartRate;
  if (service != nullptr) {
    service->OnNewHeartRateValue(heartRate);
  }
}

void HeartRateController::UpdateHeartRate(uint8_t heartRate) {
  this->heartRate = heartRate;
  NotifyServiceIfChanged(heartRate);
}

void HeartRateController::ReportHeartRateToService(uint8_t heartRate) {
  NotifyServiceIfChanged(heartRate);
}

void HeartRateController::NotifyHeartRateToService(uint8_t heartRate) {
  this->heartRate = heartRate;
  lastReportedHeartRate = heartRate;
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
    task->PushMessage(Pinetime::Applications::HeartRateTask::Messages::Disable);
  }
}

void HeartRateController::SetHeartRateTask(Pinetime::Applications::HeartRateTask* task) {
  this->task = task;
}

void HeartRateController::SetService(Pinetime::Controllers::HeartRateService* service) {
  this->service = service;
}
