#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace WheelProtocol {

constexpr uint16_t MAGIC = 0x5347;  // "SG" (sim gear)
constexpr uint8_t VERSION = 4;
constexpr uint8_t ESPNOW_CHANNEL = 6;
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 500;
constexpr uint32_t LINK_TIMEOUT_MS = 2000;
constexpr uint32_t INPUT_TIMEOUT_MS = 250;
constexpr uint32_t TELEMETRY_TIMEOUT_MS = 500;
constexpr uint32_t DISCOVERY_INTERVAL_MS = 300;
constexpr uint32_t PAIRING_RETRY_INTERVAL_MS = 150;
constexpr uint32_t PAIRING_TIMEOUT_MS = 5000;
constexpr uint8_t MAX_SEND_RETRIES = 2;
constexpr uint32_t SEND_RETRY_DELAY_MS = 3;

enum class DeviceRole : uint8_t {
  Wheel = 1,
  Receiver = 2,
};

enum class MessageType : uint8_t {
  Discovery = 1,
  PairRequest = 2,
  PairAccept = 3,
  PairCommit = 4,
  PairConfirmed = 5,
  PairReset = 6,
  Heartbeat = 7,
  Telemetry = 8,
  WheelInput = 9,
};

struct __attribute__((packed)) PacketHeader {
  uint16_t magic;
  uint8_t version;
  MessageType type;
  DeviceRole sender;
  uint8_t payloadLength;
  uint32_t sessionId;
  uint16_t sequence;
};

struct __attribute__((packed)) DiscoveryPayload {
  uint32_t nonce;
};

struct __attribute__((packed)) PairingPayload {
  uint32_t wheelNonce;
  uint32_t receiverNonce;
};

struct __attribute__((packed)) PairResetPayload {
  uint32_t sessionId;
};

struct __attribute__((packed)) HeartbeatPayload {
  uint32_t uptimeMs;
};

struct __attribute__((packed)) TelemetryPayload {
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

struct __attribute__((packed)) WheelInputPayload {
  uint16_t buttonMask;
};

template <typename Payload>
struct __attribute__((packed)) Packet {
  PacketHeader header;
  Payload payload;
  uint16_t crc;
};

using DiscoveryPacket = Packet<DiscoveryPayload>;
using PairingPacket = Packet<PairingPayload>;
using PairResetPacket = Packet<PairResetPayload>;
using HeartbeatPacket = Packet<HeartbeatPayload>;
using TelemetryPacket = Packet<TelemetryPayload>;
using WheelInputPacket = Packet<WheelInputPayload>;

static_assert(std::is_trivially_copyable<PacketHeader>::value,
              "Protocol packets must be trivially copyable");
static_assert(sizeof(TelemetryPacket) <= 250,
              "ESP-NOW packet must remain below 250 bytes");

// CRC-16/CCITT-FALSE: polynomial 0x1021, initial value 0xFFFF.
inline uint16_t crc16(const uint8_t *data, const size_t length) {
  uint16_t crc = 0xFFFF;
  for (size_t index = 0; index < length; ++index) {
    crc ^= static_cast<uint16_t>(data[index]) << 8;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000U) != 0
                ? static_cast<uint16_t>((crc << 1U) ^ 0x1021U)
                : static_cast<uint16_t>(crc << 1U);
    }
  }
  return crc;
}

template <typename Payload>
inline Packet<Payload> makePacket(const MessageType type,
                                  const DeviceRole sender,
                                  const uint32_t sessionId,
                                  const uint16_t sequence,
                                  const Payload &payload) {
  Packet<Payload> packet{};
  packet.header = {MAGIC, VERSION, type, sender,
                   static_cast<uint8_t>(sizeof(Payload)), sessionId, sequence};
  packet.payload = payload;
  packet.crc = crc16(reinterpret_cast<const uint8_t *>(&packet),
                     sizeof(packet) - sizeof(packet.crc));
  return packet;
}

template <typename Payload>
inline bool validatePacket(const uint8_t *data, const int length,
                           const MessageType expectedType,
                           const DeviceRole expectedSender,
                           const uint32_t expectedSession = 0,
                           const bool requireSession = false) {
  if (data == nullptr || length != static_cast<int>(sizeof(Packet<Payload>))) {
    return false;
  }

  Packet<Payload> packet{};
  memcpy(&packet, data, sizeof(packet));
  if (packet.header.magic != MAGIC || packet.header.version != VERSION ||
      packet.header.type != expectedType ||
      packet.header.sender != expectedSender ||
      packet.header.payloadLength != sizeof(Payload) ||
      (requireSession &&
       (expectedSession == 0 || packet.header.sessionId != expectedSession))) {
    return false;
  }

  return packet.crc ==
         crc16(data, sizeof(packet) - sizeof(packet.crc));
}

inline bool isSequenceNewer(const uint16_t candidate,
                            const uint16_t previous) {
  const uint16_t difference = static_cast<uint16_t>(candidate - previous);
  return difference != 0 && difference < 0x8000U;
}

inline uint32_t deriveSessionId(const uint32_t wheelNonce,
                                const uint32_t receiverNonce) {
  uint8_t nonces[sizeof(wheelNonce) + sizeof(receiverNonce)]{};
  memcpy(nonces, &wheelNonce, sizeof(wheelNonce));
  memcpy(nonces + sizeof(wheelNonce), &receiverNonce, sizeof(receiverNonce));
  const uint16_t high = crc16(nonces, sizeof(nonces));
  const uint16_t low = crc16(nonces, sizeof(wheelNonce));
  const uint32_t session = (static_cast<uint32_t>(high) << 16U) | low;
  return session == 0 ? 1 : session;
}

}  // namespace WheelProtocol
