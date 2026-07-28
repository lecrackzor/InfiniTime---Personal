#include "heartratetask/HeartRateTask.h"
#include <drivers/Hrs3300.h>
#include <components/heartrate/HeartRateController.h>
#include <nrf_log.h>

using namespace Pinetime::Applications;

HeartRateTask::HeartRateTask(Drivers::Hrs3300& heartRateSensor,
                             Controllers::HeartRateController& controller,
                             Controllers::Settings& settings,
                             Drivers::Bma421& motionSensor)
  : heartRateSensor {heartRateSensor}, controller {controller}, settings {settings}, motionSensor {motionSensor} {
}

void HeartRateTask::Start() {
  messageQueue = xQueueCreate(10, 1);
  controller.SetHeartRateTask(this);
}

void HeartRateTask::Work() {
}

void HeartRateTask::PushMessage(HeartRateTask::Messages msg) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  xQueueSendFromISR(messageQueue, &msg, &xHigherPriorityTaskWoken);
  if (xHigherPriorityTaskWoken) {
    /* Actual macro used here is port specific. */
    // portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}
