#include "semphr.h"
#include <SDL.h>
#include <mutex>
#include <stdexcept>

namespace {
  struct RecursiveMutex_t {
    std::recursive_mutex mutex;
  };
}

QueueHandle_t xSemaphoreCreateMutex() {
  SemaphoreHandle_t xSemaphore = xQueueCreate(1, 1);
  Queue_t* pxQueue = (Queue_t*) xSemaphore;
  // Queue full represents taken semaphore/locked mutex
  pxQueue->queue.push_back(0);
  return xSemaphore;
}

QueueHandle_t xSemaphoreCreateRecursiveMutex() {
  return new RecursiveMutex_t;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait) {
  Queue_t* pxQueue = (Queue_t*) xSemaphore;
  constexpr TickType_t DELAY_BETWEEN_ATTEMPTS = 25;
  do {
    if (pxQueue->mutex.try_lock()) {
      std::lock_guard<std::mutex> lock(pxQueue->mutex, std::adopt_lock);
      if (pxQueue->queue.empty()) {
        pxQueue->queue.push_back(0);
        return true;
      }
    }
    // Prevent underflow
    if (xTicksToWait >= DELAY_BETWEEN_ATTEMPTS) {
      // Someone else is modifying queue, wait for them to finish
      SDL_Delay(DELAY_BETWEEN_ATTEMPTS);
      xTicksToWait -= DELAY_BETWEEN_ATTEMPTS;
    }
  } while (xTicksToWait >= DELAY_BETWEEN_ATTEMPTS);
  return false;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore) {
  Queue_t* pxQueue = (Queue_t*) xSemaphore;
  std::lock_guard<std::mutex> guard(pxQueue->mutex);
  if (pxQueue->queue.size() != 1) {
    throw std::runtime_error("Mutex released without being held");
  }
  pxQueue->queue.pop_back();
  return true;
}

BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait) {
  auto* recursive = static_cast<RecursiveMutex_t*>(xSemaphore);
  constexpr TickType_t DELAY_BETWEEN_ATTEMPTS = 25;
  if (xTicksToWait == portMAX_DELAY) {
    recursive->mutex.lock();
    return true;
  }
  do {
    if (recursive->mutex.try_lock()) {
      return true;
    }
    if (xTicksToWait >= DELAY_BETWEEN_ATTEMPTS) {
      SDL_Delay(DELAY_BETWEEN_ATTEMPTS);
      xTicksToWait -= DELAY_BETWEEN_ATTEMPTS;
    }
  } while (xTicksToWait >= DELAY_BETWEEN_ATTEMPTS);
  return false;
}

BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t xSemaphore) {
  auto* recursive = static_cast<RecursiveMutex_t*>(xSemaphore);
  recursive->mutex.unlock();
  return true;
}
