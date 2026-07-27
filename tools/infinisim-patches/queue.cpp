#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include <chrono>
#include <stdexcept>

QueueHandle_t xQueueCreate(const UBaseType_t uxQueueLength, const UBaseType_t uxItemSize) {
  Queue_t* xQueue = new Queue_t;
  if (uxItemSize != 1) {
    throw std::runtime_error("uxItemSize must be 1");
  }
  (void) uxQueueLength;
  return xQueue;
}

BaseType_t xQueueSend(QueueHandle_t xQueue, const void* const pvItemToQueue, TickType_t xTicksToWait) {
  (void) xTicksToWait;
  Queue_t* pxQueue = static_cast<Queue_t*>(xQueue);
  {
    std::lock_guard<std::mutex> guard(pxQueue->mutex);
    pxQueue->queue.push_back(*static_cast<const uint8_t*>(pvItemToQueue));
  }
  pxQueue->cv.notify_one();
  return pdTRUE;
}

BaseType_t xQueueSendFromISR(QueueHandle_t xQueue, const void* const pvItemToQueue, BaseType_t* xHigherPriorityTaskWoken) {
  *xHigherPriorityTaskWoken = pdFALSE;
  return xQueueSend(xQueue, pvItemToQueue, 0);
}

BaseType_t xQueueReceive(QueueHandle_t xQueue, void* const pvBuffer, TickType_t xTicksToWait) {
  Queue_t* pxQueue = static_cast<Queue_t*>(xQueue);
  std::unique_lock<std::mutex> lock(pxQueue->mutex);

  auto predicate = [&] { return !pxQueue->queue.empty(); };

  if (xTicksToWait == portMAX_DELAY) {
    pxQueue->cv.wait(lock, predicate);
  } else if (xTicksToWait == 0) {
    if (!predicate()) {
      return pdFALSE;
    }
  } else {
    const auto waitMs = std::chrono::milliseconds((static_cast<uint64_t>(xTicksToWait) * 1000ULL) / configTICK_RATE_HZ);
    if (!pxQueue->cv.wait_for(lock, waitMs, predicate)) {
      return pdFALSE;
    }
  }

  uint8_t* buf = static_cast<uint8_t*>(pvBuffer);
  *buf = pxQueue->queue.front();
  pxQueue->queue.pop_front();
  return pdTRUE;
}

UBaseType_t uxQueueMessagesWaiting(const QueueHandle_t xQueue) {
  Queue_t* pxQueue = static_cast<Queue_t*>(xQueue);
  std::lock_guard<std::mutex> guard(pxQueue->mutex);
  return static_cast<UBaseType_t>(pxQueue->queue.size());
}
