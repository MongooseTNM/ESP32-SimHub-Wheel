#include <unity.h>

#include <cstring>
#include <initializer_list>

#include "espnow_protocol.h"
#include "pedals_input.h"
#include "shifter_calibration.h"
#include "shifter_input.h"
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

void test_shifter_maps_every_gate_and_reverse() {
  using ShifterInput::AxisZone;
  using ShifterInput::Gear;
  using ShifterInput::gearFromZones;

  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Gear::First),
                          static_cast<uint8_t>(gearFromZones(
                              AxisZone::Low, AxisZone::Low, false)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Gear::Second),
                          static_cast<uint8_t>(gearFromZones(
                              AxisZone::Low, AxisZone::High, false)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Gear::Third),
                          static_cast<uint8_t>(gearFromZones(
                              AxisZone::Center, AxisZone::Low, false)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Gear::Fourth),
                          static_cast<uint8_t>(gearFromZones(
                              AxisZone::Center, AxisZone::High, false)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Gear::Fifth),
                          static_cast<uint8_t>(gearFromZones(
                              AxisZone::High, AxisZone::Low, false)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Gear::Sixth),
                          static_cast<uint8_t>(gearFromZones(
                              AxisZone::High, AxisZone::High, false)));
  TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Gear::Reverse),
                          static_cast<uint8_t>(gearFromZones(
                              AxisZone::High, AxisZone::High, true)));
}

void test_shifter_neutral_row_releases_all_columns() {
  using ShifterInput::AxisZone;
  using ShifterInput::Gear;
  for (const AxisZone column :
       {AxisZone::Low, AxisZone::Center, AxisZone::High}) {
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(Gear::Neutral),
        static_cast<uint8_t>(
            ShifterInput::gearFromZones(column, AxisZone::Center, true)));
  }
}

void test_shifter_axis_hysteresis() {
  using ShifterInput::AxisZone;
  using ShifterInput::classifyAxis;
  constexpr uint16_t low = 1400;
  constexpr uint16_t high = 2700;
  constexpr uint16_t hysteresis = 80;

  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(AxisZone::Low),
      static_cast<uint8_t>(classifyAxis(1470, AxisZone::Low, low, high,
                                        hysteresis)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(AxisZone::Center),
      static_cast<uint8_t>(classifyAxis(1490, AxisZone::Low, low, high,
                                        hysteresis)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(AxisZone::Center),
      static_cast<uint8_t>(classifyAxis(1350, AxisZone::Center, low, high,
                                        hysteresis)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(AxisZone::Low),
      static_cast<uint8_t>(classifyAxis(1310, AxisZone::Center, low, high,
                                        hysteresis)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(AxisZone::High),
      static_cast<uint8_t>(classifyAxis(2630, AxisZone::High, low, high,
                                        hysteresis)));
  TEST_ASSERT_EQUAL_UINT8(
      static_cast<uint8_t>(AxisZone::Center),
      static_cast<uint8_t>(classifyAxis(2610, AxisZone::High, low, high,
                                        hysteresis)));
}

void test_shifter_button_masks_are_one_hot() {
  using ShifterInput::Gear;
  TEST_ASSERT_EQUAL_HEX16(0, ShifterInput::buttonMaskForGear(Gear::Neutral));
  for (uint8_t gear = 1; gear <= 7; ++gear) {
    const uint16_t mask =
        ShifterInput::buttonMaskForGear(static_cast<Gear>(gear));
    TEST_ASSERT_EQUAL_HEX16(static_cast<uint16_t>(1U << (gear - 1U)), mask);
    TEST_ASSERT_EQUAL_UINT8(1, __builtin_popcount(mask));
  }
  TEST_ASSERT_EQUAL_HEX16(
      0, ShifterInput::buttonMaskForGear(static_cast<Gear>(8)));
}

void test_shifter_calibration_calculates_midpoint_thresholds() {
  using ShifterCalibration::Position;
  constexpr auto model = ShifterCalibration::makeModel(
      Position{2000, 2100}, Position{800, 700}, Position{820, 3300},
      Position{1980, 720}, Position{2020, 3280}, Position{3220, 710},
      Position{3200, 3310}, 4095);

  TEST_ASSERT_FALSE(model.xReversed);
  TEST_ASSERT_FALSE(model.yReversed);
  TEST_ASSERT_EQUAL_UINT16(810, model.xLeftCenter);
  TEST_ASSERT_EQUAL_UINT16(2000, model.xCenterCenter);
  TEST_ASSERT_EQUAL_UINT16(3210, model.xRightCenter);
  TEST_ASSERT_EQUAL_UINT16(1405, model.xLeftMaximum);
  TEST_ASSERT_EQUAL_UINT16(2605, model.xRightMinimum);
  TEST_ASSERT_EQUAL_UINT16(1405, model.yForwardMaximum);
  TEST_ASSERT_EQUAL_UINT16(2698, model.yBackMinimum);
}

void test_shifter_calibration_detects_reversed_axes() {
  using ShifterCalibration::Position;
  constexpr auto model = ShifterCalibration::makeModel(
      Position{2000, 2100}, Position{3300, 3400}, Position{3280, 800},
      Position{2020, 3380}, Position{1980, 820}, Position{800, 3390},
      Position{820, 780}, 4095);

  TEST_ASSERT_TRUE(model.xReversed);
  TEST_ASSERT_TRUE(model.yReversed);
  TEST_ASSERT_TRUE(model.xLeftCenter < model.xCenterCenter);
  TEST_ASSERT_TRUE(model.xCenterCenter < model.xRightCenter);
  TEST_ASSERT_TRUE(model.yForwardCenter < model.yNeutralCenter);
  TEST_ASSERT_TRUE(model.yNeutralCenter < model.yBackCenter);
}

void test_pedals_scale_forward_calibration_and_clamp() {
  TEST_ASSERT_EQUAL_UINT16(
      0, PedalsInput::scaleRawToHid(50, 100, 3900, 0));
  TEST_ASSERT_EQUAL_UINT16(
      0, PedalsInput::scaleRawToHid(100, 100, 3900, 0));
  TEST_ASSERT_UINT16_WITHIN(
      1, PedalsInput::HID_AXIS_MAX / 2,
      PedalsInput::scaleRawToHid(2000, 100, 3900, 0));
  TEST_ASSERT_EQUAL_UINT16(
      PedalsInput::HID_AXIS_MAX,
      PedalsInput::scaleRawToHid(3900, 100, 3900, 0));
  TEST_ASSERT_EQUAL_UINT16(
      PedalsInput::HID_AXIS_MAX,
      PedalsInput::scaleRawToHid(4000, 100, 3900, 0));
}

void test_pedals_scale_reversed_calibration() {
  TEST_ASSERT_EQUAL_UINT16(
      0, PedalsInput::scaleRawToHid(3900, 3900, 100, 0));
  TEST_ASSERT_UINT16_WITHIN(
      1, PedalsInput::HID_AXIS_MAX / 2,
      PedalsInput::scaleRawToHid(2000, 3900, 100, 0));
  TEST_ASSERT_EQUAL_UINT16(
      PedalsInput::HID_AXIS_MAX,
      PedalsInput::scaleRawToHid(100, 3900, 100, 0));
}

void test_pedals_end_dead_zones_reach_exact_limits() {
  constexpr uint16_t deadZone = 100;
  TEST_ASSERT_EQUAL_UINT16(
      0, PedalsInput::scaleRawToHid(200, 100, 3900, deadZone));
  TEST_ASSERT_TRUE(
      PedalsInput::scaleRawToHid(201, 100, 3900, deadZone) > 0);
  TEST_ASSERT_EQUAL_UINT16(
      PedalsInput::HID_AXIS_MAX,
      PedalsInput::scaleRawToHid(3800, 100, 3900, deadZone));
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
  RUN_TEST(test_shifter_maps_every_gate_and_reverse);
  RUN_TEST(test_shifter_neutral_row_releases_all_columns);
  RUN_TEST(test_shifter_axis_hysteresis);
  RUN_TEST(test_shifter_button_masks_are_one_hot);
  RUN_TEST(test_shifter_calibration_calculates_midpoint_thresholds);
  RUN_TEST(test_shifter_calibration_detects_reversed_axes);
  RUN_TEST(test_pedals_scale_forward_calibration_and_clamp);
  RUN_TEST(test_pedals_scale_reversed_calibration);
  RUN_TEST(test_pedals_end_dead_zones_reach_exact_limits);
  return UNITY_END();
}
