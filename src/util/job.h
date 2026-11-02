#pragma once

#include <atomic>
#include <functional>
#include <system_error>
#include <thread>

#include "hal/hal.h"

// Runs one blocking task (network, decoding...) on a background thread so the UI keeps animating.
// Start it from the main loop, then poll take() from the main loop. One task at a time.
//
// A thread's stack comes from internal RAM: with several jobs out at once there may not be room for
// another. Starting a thread then throws std::system_error: the task waits, and take() tries again
// every RETRY_MS (another job ending frees its stack).
template <typename Result>
class Job {
 public:
  // TLS handshakes need far more stack than the ESP32's 3KB thread default.
  static constexpr uint32_t DEFAULT_STACK = 16 * 1024;

  ~Job() {
    if (thread_.joinable()) thread_.join();
  }

  void start(std::function<Result()> work, uint32_t stack_bytes = DEFAULT_STACK) {
    if (thread_.joinable()) thread_.join();
    finished_ = false;
    waiting_ = std::move(work);
    stack_ = stack_bytes;
    toldWaiting_ = false;
    launch();
  }

  bool take(Result &out) {  // true once, when the task has finished
    if (waiting_) {
      // Not every frame: each failed try logs an error and throws, which is slow.
      if (hal_millis() - triedAt_ >= RETRY_MS) launch();
      return false;
    }
    if (!finished_) return false;
    thread_.join();
    finished_ = false;
    out = std::move(result_);
    return true;
  }

 private:
  static constexpr uint32_t RETRY_MS = 500;

  void launch() {
    triedAt_ = hal_millis();
    hal_set_thread_stack(stack_);
    try {
      thread_ = std::thread([this, work = waiting_] {  // a copy: kept if the thread can't start
        result_ = work();
        finished_ = true;
      });
      waiting_ = nullptr;
    } catch (const std::system_error &) {
      if (!toldWaiting_) hal_log("[job] no room for a thread yet (%s): waiting\n", hal_memory_report().c_str());
      toldWaiting_ = true;
    }
  }

  std::thread thread_;
  std::function<Result()> waiting_;  // not started yet
  uint32_t stack_ = 0;
  bool toldWaiting_ = false;
  uint32_t triedAt_ = 0;
  std::atomic<bool> finished_{false};
  Result result_{};
};
