#pragma once

#include <Arduino.h>

namespace WheelProtocol {

constexpr uint16_t MAGIC = 0x5347;  // "SG" (sim gear)
constexpr uint8_t VERSION = 3;
constexpr uint8_t ESPNOW_CHANNEL = 6;
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 500;
constexpr uint32_t LINK_TIMEOUT_MS = 2000;
constexpr uint32_t INPUT_TIMEOUT_MS = 250;

enum class DeviceRole : uint8_t {
  Wheel = 1,
  Receiver = 2,
};

enum class MessageType : uint8_t {
  Heartbeat = 1,
  Telemetry = 2,
  WheelInput = 3,
};

struct __attribute__((packed)) HeartbeatPacket {
  uint16_t magic;
  uint8_t version;
  MessageType type;
  DeviceRole sender;
  uint8_t reserved;
  uint16_t sequence;
  uint32_t uptimeMs;
};

struct __attribute__((packed)) TelemetryPacket {
  uint16_t magic;
  uint8_t version;
  MessageType type;
  uint16_t rpm;
  uint16_t displayedRpmPercentX100;
  uint16_t redLineRpm;
  uint16_t redLineDisplayedPercentX100;
  uint16_t maxRpm;
  uint16_t minimumShownRpm;
  uint16_t shiftLight1ProgressX1000;
  uint16_t shiftLight2ProgressX1000;
  uint8_t rpmRedLineReached;
  char gear[4];
};

struct __attribute__((packed)) WheelInputPacket {
  uint16_t magic;
  uint8_t version;
  MessageType type;
  uint16_t sequence;
  uint16_t buttonMask;
};

static_assert(sizeof(HeartbeatPacket) <= 250,
              "ESP-NOW packet must remain below 250 bytes");
static_assert(sizeof(TelemetryPacket) <= 250,
              "ESP-NOW packet must remain below 250 bytes");
static_assert(sizeof(WheelInputPacket) <= 250,
              "ESP-NOW packet must remain below 250 bytes");

inline bool isValidHeartbeat(const uint8_t *data, const int length,
                             const DeviceRole expectedSender) {
  if (data == nullptr || length != sizeof(HeartbeatPacket)) {
    return false;
  }

  const auto *packet = reinterpret_cast<const HeartbeatPacket *>(data);
  return packet->magic == MAGIC && packet->version == VERSION &&
         packet->type == MessageType::Heartbeat &&
         packet->sender == expectedSender;
}

inline bool isValidTelemetry(const uint8_t *data, const int length) {
  if (data == nullptr || length != sizeof(TelemetryPacket)) {
    return false;
  }

  const auto *packet = reinterpret_cast<const TelemetryPacket *>(data);
  return packet->magic == MAGIC && packet->version == VERSION &&
         packet->type == MessageType::Telemetry;
}

inline bool isValidWheelInput(const uint8_t *data, const int length) {
  if (data == nullptr || length != sizeof(WheelInputPacket)) {
    return false;
  }

  const auto *packet = reinterpret_cast<const WheelInputPacket *>(data);
  return packet->magic == MAGIC && packet->version == VERSION &&
         packet->type == MessageType::WheelInput;
}

}  // namespace WheelProtocol
