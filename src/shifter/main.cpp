#include <Arduino.h>
#include <USB.h>
#include <USBCDC.h>

#include "named_usb_hid.h"
#include "shifter_config.h"
#include "shifter_constants.h"
#include "shifter_input.h"

namespace {
using ShifterInput::AxisZone;
using ShifterInput::Gear;

// Seven buttons and one padding bit. The shifter does not advertise
// nonexistent axes or a hat switch.
constexpr uint8_t SHIFTER_REPORT_DESCRIPTOR[] = {
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x04,  // Usage (Joystick)
    0xA1, 0x01,  // Collection (Application)
    0x85, 0x01,  //   Report ID (1)
    0x05, 0x09,  //   Usage Page (Button)
    0x19, 0x01,  //   Usage Minimum (Button 1)
    0x29, 0x07,  //   Usage Maximum (Button 7)
    0x15, 0x00,  //   Logical Minimum (0)
    0x25, 0x01,  //   Logical Maximum (1)
    0x75, 0x01,  //   Report Size (1)
    0x95, 0x07,  //   Report Count (7)
    0x81, 0x02,  //   Input (Data, Variable, Absolute)
    0x75, 0x01,  //   Report Size (1)
    0x95, 0x01,  //   Report Count (1)
    0x81, 0x03,  //   Input (Constant, Variable, Absolute)
    0xC0         // End Collection
};

NamedUSBHID gamepad("Shifter", SHIFTER_REPORT_DESCRIPTOR,
                    sizeof(SHIFTER_REPORT_DESCRIPTOR));
USBCDC usbSerial;

uint16_t rawX = 0;
uint16_t rawY = 0;
uint16_t filteredX = 0;
uint16_t filteredY = 0;
bool filterInitialized = false;
AxisZone xZone = AxisZone::Center;
AxisZone yZone = AxisZone::Center;
Gear candidateGear = Gear::Neutral;
Gear reportedGear = Gear::Neutral;
uint32_t candidateSinceMs = 0;
uint32_t lastHidReportUs = 0;
uint32_t lastDiagnosticsMs = 0;
uint8_t diagnosticsLinesSinceCalibration = 20;

uint16_t orientSample(const uint16_t value, const bool reversed) {
  return reversed ? ShifterConfig::ADC_MAX - value : value;
}

uint16_t filterSample(const uint16_t current, const uint16_t sample) {
  const int32_t difference = static_cast<int32_t>(sample) - current;
  return static_cast<uint16_t>(
      static_cast<int32_t>(current) +
      difference / static_cast<int32_t>(ShifterConfig::ADC_FILTER_DIVISOR));
}

void sendGamepadReport(const Gear gear) {
  const uint8_t buttons =
      static_cast<uint8_t>(ShifterInput::buttonMaskForGear(gear));
  gamepad.send(&buttons, sizeof(buttons));
  lastHidReportUs = micros();
}

void printDiagnostics(const bool reverseActive) {
  usbSerial.printf("rawX=%u rawY=%u filteredX=%u filteredY=%u reverse=%u "
                   "candidate=%s reported=%s mask=0x%02X\r\n",
                   rawX, rawY, filteredX, filteredY, reverseActive ? 1 : 0,
                   ShifterInput::gearName(candidateGear),
                   ShifterInput::gearName(reportedGear),
                   ShifterInput::buttonMaskForGear(reportedGear));
}

void printCalculatedCalibration() {
  usbSerial.printf(
      "calibration xReversed=%u yReversed=%u xLeftMax=%u "
      "xRightMin=%u yForwardMax=%u yBackMin=%u hysteresis=%u\r\n",
      ShifterConfig::X_REVERSED ? 1 : 0,
      ShifterConfig::Y_REVERSED ? 1 : 0, ShifterConfig::X_LEFT_MAX,
      ShifterConfig::X_RIGHT_MIN, ShifterConfig::Y_FORWARD_MAX,
      ShifterConfig::Y_BACK_MIN, ShifterConfig::AXIS_HYSTERESIS);
}

void printSerialWelcome() {
  usbSerial.println();
  usbSerial.println("ESP32 H-Pattern Shifter diagnostics connected");
  usbSerial.println(
      "Move through neutral and gears 1-6; record rawX and rawY.");
  printCalculatedCalibration();
}

void processSerialCommands() {
  while (usbSerial.available() > 0) {
    const char command = static_cast<char>(usbSerial.read());
    if (command == 'c' || command == 'C') {
      usbSerial.printf("copy this position: {%u, %u}\r\n", rawX, rawY);
    } else if (command == '?' || command == 'h' || command == 'H') {
      usbSerial.println(
          "Commands: C = print current {rawX, rawY}; H or ? = help");
    }
  }
}
}  // namespace

void setup() {
  pinMode(ShifterConfig::REVERSE_PIN, INPUT_PULLDOWN);
  analogReadResolution(12);
  analogSetPinAttenuation(ShifterConfig::X_AXIS_PIN, ADC_11db);
  analogSetPinAttenuation(ShifterConfig::Y_AXIS_PIN, ADC_11db);

  usbSerial.begin(115200);
  gamepad.begin();
  USB.PID(0x4010);
  USB.productName("Shifter");
  USB.manufacturerName("DIY Sim Controls");
  USB.begin();
  delay(250);
  sendGamepadReport(Gear::Neutral);
  printSerialWelcome();
}

void loop() {
  const uint32_t nowMs = millis();
  rawX = analogRead(ShifterConfig::X_AXIS_PIN);
  rawY = analogRead(ShifterConfig::Y_AXIS_PIN);
  const uint16_t orientedX =
      orientSample(rawX, ShifterConfig::X_REVERSED);
  const uint16_t orientedY =
      orientSample(rawY, ShifterConfig::Y_REVERSED);

  if (!filterInitialized) {
    filteredX = orientedX;
    filteredY = orientedY;
    filterInitialized = true;
  } else {
    filteredX = filterSample(filteredX, orientedX);
    filteredY = filterSample(filteredY, orientedY);
  }

  xZone = ShifterInput::classifyAxis(
      filteredX, xZone, ShifterConfig::X_LEFT_MAX,
      ShifterConfig::X_RIGHT_MIN, ShifterConfig::AXIS_HYSTERESIS);
  yZone = ShifterInput::classifyAxis(
      filteredY, yZone, ShifterConfig::Y_FORWARD_MAX,
      ShifterConfig::Y_BACK_MIN, ShifterConfig::AXIS_HYSTERESIS);
  const bool reverseActive =
      digitalRead(ShifterConfig::REVERSE_PIN) == HIGH;
  const Gear observedGear =
      ShifterInput::gearFromZones(xZone, yZone, reverseActive);

  if (observedGear != candidateGear) {
    candidateGear = observedGear;
    candidateSinceMs = nowMs;
  } else if (reportedGear != candidateGear &&
             nowMs - candidateSinceMs >= ShifterConfig::GEAR_STABILITY_MS) {
    reportedGear = candidateGear;
    sendGamepadReport(reportedGear);
  }

  const uint32_t nowUs = micros();
  if (nowUs - lastHidReportUs >=
      ShifterConstants::HID_REFRESH_INTERVAL_US) {
    sendGamepadReport(reportedGear);
  }

  processSerialCommands();

  if (nowMs - lastDiagnosticsMs >=
      ShifterConfig::DIAGNOSTICS_INTERVAL_MS) {
    lastDiagnosticsMs = nowMs;
    if (++diagnosticsLinesSinceCalibration >= 20) {
      diagnosticsLinesSinceCalibration = 0;
      printCalculatedCalibration();
    }
    printDiagnostics(reverseActive);
  }

  delay(1);
}
