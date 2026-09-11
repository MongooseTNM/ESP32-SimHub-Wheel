#pragma once

#include <Arduino.h>
#include <esp_now.h>

#include <cstring>

#include "espnow_protocol.h"

namespace WheelProtocol {

// One in-flight frame plus one replaceable latest-value frame. This keeps
// retries correct while naturally coalescing telemetry or state updates that
// arrive faster than ESP-NOW send-completion callbacks.
class ReliableSender {
 public:
  bool send(const uint8_t *address, const void *data, const size_t length) {
    if (address == nullptr || data == nullptr || length > sizeof(current_.bytes)) {
      return false;
    }
    Frame &target = (currentPending_ || inFlight_) ? queued_ : current_;
    memcpy(target.address, address, sizeof(target.address));
    memcpy(target.bytes, data, length);
    target.length = length;
    if (&target == &queued_) {
      queuedPending_ = true;
      ++coalesced;
      return true;
    }
    currentPending_ = true;
    retries_ = 0;
    retryAtMs_ = 0;
    service(millis());
    return true;
  }

  bool service(const uint32_t nowMs) {
    if (!currentPending_ && queuedPending_) promoteQueued();
    if (!currentPending_ || inFlight_ ||
        (retryAtMs_ != 0 && static_cast<int32_t>(nowMs - retryAtMs_) < 0)) {
      return false;
    }
    if (esp_now_send(current_.address, current_.bytes, current_.length) == ESP_OK) {
      inFlight_ = true;
      return true;
    }
    scheduleRetry(nowMs);
    return false;
  }

  void onComplete(const esp_now_send_status_t status, const uint32_t nowMs) {
    if (!inFlight_) return;
    inFlight_ = false;
    if (status == ESP_NOW_SEND_SUCCESS) {
      currentPending_ = false;
      ++delivered;
      if (queuedPending_) promoteQueued();
    } else {
      ++failedAttempts;
      scheduleRetry(nowMs);
    }
  }

  volatile uint32_t delivered = 0;
  volatile uint32_t failedAttempts = 0;
  volatile uint32_t abandoned = 0;
  volatile uint32_t coalesced = 0;

 private:
  struct Frame {
    uint8_t address[6]{};
    uint8_t bytes[250]{};
    size_t length = 0;
  };

  void promoteQueued() {
    current_ = queued_;
    currentPending_ = true;
    queuedPending_ = false;
    retries_ = 0;
    retryAtMs_ = 0;
  }

  void scheduleRetry(const uint32_t nowMs) {
    if (retries_ >= MAX_SEND_RETRIES) {
      currentPending_ = false;
      ++abandoned;
      if (queuedPending_) promoteQueued();
      return;
    }
    ++retries_;
    retryAtMs_ = nowMs + SEND_RETRY_DELAY_MS;
  }

  Frame current_{};
  Frame queued_{};
  uint8_t retries_ = 0;
  uint32_t retryAtMs_ = 0;
  volatile bool currentPending_ = false;
  volatile bool queuedPending_ = false;
  volatile bool inFlight_ = false;
};

}  // namespace WheelProtocol
