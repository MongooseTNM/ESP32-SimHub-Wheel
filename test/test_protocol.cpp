#include <unity.h>

#include <cstring>

#include "espnow_protocol.h"
#include "wheel_input.h"

using namespace WheelProtocol;

void test_protocol_v6_packed_sizes() {
  TEST_ASSERT_EQUAL_UINT32(12, sizeof(PacketHeader));
  TEST_ASSERT_EQUAL_UINT32(4, sizeof(DiscoveryPayload));
  TEST_ASSERT_EQUAL_UINT32(8, sizeof(PairingPayload));
  TEST_ASSERT_EQUAL_UINT32(4, sizeof(PairResetPayload));
  TEST_ASSERT_EQUAL_UINT32(4, sizeof(HeartbeatPayload));
  TEST_ASSERT_EQUAL_UINT32(23, sizeof(TelemetryPayload));
  TEST_ASSERT_EQUAL_UINT32(3, sizeof(WheelInputPayload));

  TEST_ASSERT_EQUAL_UINT32(18, sizeof(DiscoveryPacket));
  TEST_ASSERT_EQUAL_UINT32(22, sizeof(PairingPacket));
  TEST_ASSERT_EQUAL_UINT32(18, sizeof(PairResetPacket));
  TEST_ASSERT_EQUAL_UINT32(18, sizeof(HeartbeatPacket));
  TEST_ASSERT_EQUAL_UINT32(37, sizeof(TelemetryPacket));
  TEST_ASSERT_EQUAL_UINT32(17, sizeof(WheelInputPacket));
  TEST_ASSERT_EQUAL_UINT32(sizeof(TelemetryPacket), MAX_PACKET_SIZE);
}

void test_crc_standard_vector() {
  const uint8_t text[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  TEST_ASSERT_EQUAL_HEX16(0x29B1, crc16(text, sizeof(text)));
}

void test_packet_validation_and_corruption_rejection() {
  const WheelInputPayload payload{0x055A,
                                  static_cast<uint8_t>(WheelInput::Pov::Left)};
  auto packet = makePacket(MessageType::WheelInput, DeviceRole::Wheel,
                           0x12345678, 42, payload);
  TEST_ASSERT_TRUE(validatePacket<WheelInputPayload>(
      reinterpret_cast<const uint8_t *>(&packet), sizeof(packet),
      MessageType::WheelInput, DeviceRole::Wheel, 0x12345678, true));

  packet.payload.buttonMask ^= 1U;
  TEST_ASSERT_FALSE(validatePacket<WheelInputPayload>(
      reinterpret_cast<const uint8_t *>(&packet), sizeof(packet),
      MessageType::WheelInput, DeviceRole::Wheel, 0x12345678, true));
}

void test_pov_cardinal_diagonal_and_conflicting_directions() {
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WheelInput::Pov::Neutral),
                          static_cast<uint8_t>(WheelInput::povFromDirections(
                              false, false, false, false)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WheelInput::Pov::Up),
                          static_cast<uint8_t>(WheelInput::povFromDirections(
                              true, false, false, false)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WheelInput::Pov::UpRight),
                          static_cast<uint8_t>(WheelInput::povFromDirections(
                              true, true, false, false)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WheelInput::Pov::DownLeft),
                          static_cast<uint8_t>(WheelInput::povFromDirections(
                              false, false, true, true)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WheelInput::Pov::Neutral),
                          static_cast<uint8_t>(WheelInput::povFromDirections(
                              true, true, true, true)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WheelInput::Pov::Right),
                          static_cast<uint8_t>(WheelInput::povFromDirections(
                              true, true, true, false)));
}

void test_wrong_metadata_and_length_are_rejected() {
  const auto packet = makePacket(MessageType::Heartbeat, DeviceRole::Receiver,
                                 9, 7, HeartbeatPayload{1234});
  const auto *bytes = reinterpret_cast<const uint8_t *>(&packet);
  TEST_ASSERT_FALSE(validatePacket<HeartbeatPayload>(
      bytes, sizeof(packet) - 1, MessageType::Heartbeat, DeviceRole::Receiver,
      9, true));
  TEST_ASSERT_FALSE(validatePacket<HeartbeatPayload>(
      bytes, sizeof(packet), MessageType::Heartbeat, DeviceRole::Wheel, 9,
      true));
  TEST_ASSERT_FALSE(validatePacket<HeartbeatPayload>(
      bytes, sizeof(packet), MessageType::Heartbeat, DeviceRole::Receiver, 10,
      true));
}

void test_sequence_comparison_wraps_safely() {
  TEST_ASSERT_TRUE(isSequenceNewer(11, 10));
  TEST_ASSERT_FALSE(isSequenceNewer(10, 10));
  TEST_ASSERT_FALSE(isSequenceNewer(9, 10));
  TEST_ASSERT_TRUE(isSequenceNewer(0, 0xFFFF));
  TEST_ASSERT_FALSE(isSequenceNewer(0xFFFF, 0));
  TEST_ASSERT_FALSE(isSequenceNewer(0x8000, 0));
}

void test_session_derivation_is_stable_and_nonzero() {
  const uint32_t first = deriveSessionId(0x11223344, 0x55667788);
  TEST_ASSERT_NOT_EQUAL(0, first);
  TEST_ASSERT_EQUAL_HEX32(first, deriveSessionId(0x11223344, 0x55667788));
  TEST_ASSERT_NOT_EQUAL(first, deriveSessionId(0x11223345, 0x55667788));
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_protocol_v6_packed_sizes);
  RUN_TEST(test_crc_standard_vector);
  RUN_TEST(test_packet_validation_and_corruption_rejection);
  RUN_TEST(test_wrong_metadata_and_length_are_rejected);
  RUN_TEST(test_sequence_comparison_wraps_safely);
  RUN_TEST(test_session_derivation_is_stable_and_nonzero);
  RUN_TEST(test_pov_cardinal_diagonal_and_conflicting_directions);
  return UNITY_END();
}
