#include <Arduino.h>
#include <USB.h>
#include <USBCDC.h>

#include "named_usb_hid.h"
#include "pedals_config.h"
#include "pedals_constants.h"
#include "pedals_input.h"

namespace {
// Three independent 16-bit Generic Desktop axes in sequential X/Y/Z order so
// games expose gas, brake, and clutch as Axis 1, Axis 2, and Axis 3.
constexpr uint8_t PEDALS_REPORT_DESCRIPTOR[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x04,        // Usage (Joystick)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x09, 0x30,        //   Usage (X / Axis 1 / Gas)
    0x09, 0x31,        //   Usage (Y / Axis 2 / Brake)
    0x09, 0x32,        //   Usage (Z / Axis 3 / Clutch)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x7F,  //   Logical Maximum (32767)
    0x75, 0x10,        //   Report Size (16)
    0x95, 0x03,        //   Report Count (3)
    0x81, 0x02,        //   Input (Data, Variable, Absolute)
    0xC0               // End Collection
};

struct __attribute__((packed)) PedalsReport {
  uint16_t accelerator;
  uint16_t brake;
  uint16_t clutch;
};

NamedUSBHID gamepad("Pedals", PEDALS_REPORT_DESCRIPTOR,
                    sizeof(PEDALS_REPORT_DESCRIPTOR));
USBCDC usbSerial;
uint16_t rawGas = 0;
uint16_t rawBrake = 0;
uint16_t rawClutch = 0;
uint16_t filteredGas = 0;
uint16_t filteredBrake = 0;
uint16_t filteredClutch = 0;
uint16_t reportedGas = 0;
uint16_t reportedBrake = 0;
uint16_t reportedClutch = 0;
bool filterInitialized = false;
uint32_t lastHidReportUs = 0;
uint32_t lastDiagnosticsMs = 0;

uint16_t filterSample(const uint16_t current, const uint16_t sample) {
  const int32_t difference = static_cast<int32_t>(sample) - current;
  return static_cast<uint16_t>(
      static_cast<int32_t>(current) +
      difference / static_cast<int32_t>(PedalsConfig::ADC_FILTER_DIVISOR));
}

void sendPedalReport(const uint16_t gas, const uint16_t brake,
                     const uint16_t clutch) {
  const PedalsReport report{gas, brake, clutch};
  gamepad.send(&report, sizeof(report));
  reportedGas = gas;
  reportedBrake = brake;
  reportedClutch = clutch;
  lastHidReportUs = micros();
}

void printDiagnostics(const uint16_t gas, const uint16_t brake,
                      const uint16_t clutch) {
  const uint32_t gasPercent =
      static_cast<uint32_t>(gas) * 100U / PedalsInput::HID_AXIS_MAX;
  const uint32_t brakePercent =
      static_cast<uint32_t>(brake) * 100U / PedalsInput::HID_AXIS_MAX;
  const uint32_t clutchPercent =
      static_cast<uint32_t>(clutch) * 100U / PedalsInput::HID_AXIS_MAX;
  usbSerial.printf(
      "rawGas=%u rawBrake=%u rawClutch=%u filteredGas=%u "
      "filteredBrake=%u filteredClutch=%u gas=%u%% brake=%u%% clutch=%u%%\r\n",
      rawGas, rawBrake, rawClutch, filteredGas, filteredBrake, filteredClutch,
      gasPercent, brakePercent, clutchPercent);
}

void printSerialWelcome() {
  usbSerial.println();
  usbSerial.println("ESP32 Gas, Brake, and Clutch diagnostics connected");
  usbSerial.println(
      "Release or press a pedal, then type C to copy raw readings.");
  usbSerial.printf(
      "calibration gas={%u, %u} brake={%u, %u} clutch={%u, %u} "
      "deadZone=%u\r\n",
      PedalsConfig::GAS_RELEASED_RAW, PedalsConfig::GAS_PRESSED_RAW,
      PedalsConfig::BRAKE_RELEASED_RAW, PedalsConfig::BRAKE_PRESSED_RAW,
      PedalsConfig::CLUTCH_RELEASED_RAW, PedalsConfig::CLUTCH_PRESSED_RAW,
      PedalsConfig::END_DEAD_ZONE);
}

void processSerialCommands() {
  while (usbSerial.available() > 0) {
    const char command = static_cast<char>(usbSerial.read());
    if (command == 'c' || command == 'C') {
      usbSerial.printf(
          "copy current readings: gas=%u brake=%u clutch=%u\r\n", rawGas,
          rawBrake, rawClutch);
    } else if (command == '?' || command == 'h' || command == 'H') {
      usbSerial.println(
          "Commands: C = print current raw readings; H or ? = help");
    }
  }
}

}  // namespace

void setup() {
  analogReadResolution(12);
  analogSetPinAttenuation(PedalsConfig::GAS_PIN, ADC_11db);
  analogSetPinAttenuation(PedalsConfig::BRAKE_PIN, ADC_11db);
  analogSetPinAttenuation(PedalsConfig::CLUTCH_PIN, ADC_11db);

  usbSerial.begin(115200);
  gamepad.begin();
  USB.PID(0x4011);
  USB.productName("Pedals");
  USB.manufacturerName("DIY Sim Controls");
  USB.begin();
  delay(250);
  sendPedalReport(0, 0, 0);
  printSerialWelcome();
}

void loop() {
  const uint32_t nowMs = millis();
  rawGas = analogRead(PedalsConfig::GAS_PIN);
  rawBrake = analogRead(PedalsConfig::BRAKE_PIN);
  rawClutch = analogRead(PedalsConfig::CLUTCH_PIN);

  if (!filterInitialized) {
    filteredGas = rawGas;
    filteredBrake = rawBrake;
    filteredClutch = rawClutch;
    filterInitialized = true;
  } else {
    filteredGas = filterSample(filteredGas, rawGas);
    filteredBrake = filterSample(filteredBrake, rawBrake);
    filteredClutch = filterSample(filteredClutch, rawClutch);
  }

  const uint16_t gas = PedalsInput::scaleRawToHid(
      filteredGas, PedalsConfig::GAS_RELEASED_RAW,
      PedalsConfig::GAS_PRESSED_RAW, PedalsConfig::END_DEAD_ZONE);
  const uint16_t brake = PedalsInput::scaleRawToHid(
      filteredBrake, PedalsConfig::BRAKE_RELEASED_RAW,
      PedalsConfig::BRAKE_PRESSED_RAW, PedalsConfig::END_DEAD_ZONE);
  const uint16_t clutch = PedalsInput::scaleRawToHid(
      filteredClutch, PedalsConfig::CLUTCH_RELEASED_RAW,
      PedalsConfig::CLUTCH_PRESSED_RAW, PedalsConfig::END_DEAD_ZONE);

  const uint32_t nowUs = micros();
  if (gas != reportedGas || brake != reportedBrake ||
      clutch != reportedClutch ||
      nowUs - lastHidReportUs >= PedalsConstants::HID_REFRESH_INTERVAL_US) {
    sendPedalReport(gas, brake, clutch);
  }

  processSerialCommands();
  if (nowMs - lastDiagnosticsMs >=
      PedalsConfig::DIAGNOSTICS_INTERVAL_MS) {
    lastDiagnosticsMs = nowMs;
    printDiagnostics(gas, brake, clutch);
  }

  delay(1);
}
