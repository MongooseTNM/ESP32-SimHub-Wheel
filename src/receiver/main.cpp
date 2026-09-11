#include <Arduino.h>
#include <USB.h>
#include <USBHIDGamepad.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "espnow_protocol.h"
#include "wheel_config.h"
#include "wheel_constants.h"

namespace {
using namespace WheelProtocol;

constexpr uint8_t BROADCAST_ADDRESS[] = {0xFF, 0xFF, 0xFF,
                                         0xFF, 0xFF, 0xFF};

USBHIDGamepad gamepad;

uint16_t sequenceNumber = 0;
uint32_t lastHeartbeatSentMs = 0;
volatile uint32_t lastWheelInputMs = 0;
volatile uint16_t receivedButtonMask = 0;
volatile bool inputPending = false;
uint16_t reportedButtonMask = 0;
uint16_t activeButtonMask = 0;
uint32_t lastHidReportUs = 0;
char serialLine[96]{};
size_t serialLineLength = 0;

void onDataReceived(const uint8_t *, const uint8_t *data, const int length) {
  if (isValidWheelInput(data, length)) {
    const auto *packet = reinterpret_cast<const WheelInputPacket *>(data);
    receivedButtonMask = packet->buttonMask;
    lastWheelInputMs = millis();
    inputPending = true;
  }
}

bool addBroadcastPeer() {
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, BROADCAST_ADDRESS, sizeof(BROADCAST_ADDRESS));
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;
  return esp_now_add_peer(&peer) == ESP_OK;
}

void sendHeartbeat() {
  const HeartbeatPacket packet{MAGIC,
                               VERSION,
                               MessageType::Heartbeat,
                               DeviceRole::Receiver,
                               0,
                               sequenceNumber++,
                               millis()};

  if (esp_now_send(BROADCAST_ADDRESS,
                   reinterpret_cast<const uint8_t *>(&packet),
                   sizeof(packet)) != ESP_OK) {}
}

uint16_t parseClampedU16(const char *text, const uint32_t scale = 1) {
  const float value = strtof(text, nullptr);
  if (value <= 0.0F) {
    return 0;
  }
  return static_cast<uint16_t>(
      std::min(value * static_cast<float>(scale), 65535.0F));
}

bool parseAndSendTelemetry(char *line) {
  // T;<rpm>;<display %>;<redline rpm>;<redline %>;<max>;<minimum>;
  //   <at redline>;<shift 1 progress>;<shift 2 progress>;<gear>
  char *savePointer = nullptr;
  const char *prefix = strtok_r(line, ";", &savePointer);
  const char *rpm = strtok_r(nullptr, ";", &savePointer);
  const char *displayedRpmPercent = strtok_r(nullptr, ";", &savePointer);
  const char *redLineRpm = strtok_r(nullptr, ";", &savePointer);
  const char *redLineDisplayedPercent = strtok_r(nullptr, ";", &savePointer);
  const char *maxRpm = strtok_r(nullptr, ";", &savePointer);
  const char *minimumShownRpm = strtok_r(nullptr, ";", &savePointer);
  const char *redLineReached = strtok_r(nullptr, ";", &savePointer);
  const char *shiftLight1Progress = strtok_r(nullptr, ";", &savePointer);
  const char *shiftLight2Progress = strtok_r(nullptr, ";", &savePointer);
  const char *gear = strtok_r(nullptr, ";", &savePointer);

  if (prefix == nullptr || strcmp(prefix, "T") != 0 || rpm == nullptr ||
      displayedRpmPercent == nullptr || redLineRpm == nullptr ||
      redLineDisplayedPercent == nullptr || maxRpm == nullptr ||
      minimumShownRpm == nullptr || redLineReached == nullptr ||
      shiftLight1Progress == nullptr || shiftLight2Progress == nullptr ||
      gear == nullptr) {
    return false;
  }

  TelemetryPacket packet{};
  packet.magic = MAGIC;
  packet.version = VERSION;
  packet.type = MessageType::Telemetry;
  packet.rpm = parseClampedU16(rpm);
  packet.displayedRpmPercentX100 = parseClampedU16(displayedRpmPercent, 100);
  packet.redLineRpm = parseClampedU16(redLineRpm);
  packet.redLineDisplayedPercentX100 =
      parseClampedU16(redLineDisplayedPercent, 100);
  packet.maxRpm = parseClampedU16(maxRpm);
  packet.minimumShownRpm = parseClampedU16(minimumShownRpm);
  packet.shiftLight1ProgressX1000 =
      parseClampedU16(shiftLight1Progress, 1000);
  packet.shiftLight2ProgressX1000 =
      parseClampedU16(shiftLight2Progress, 1000);
  packet.rpmRedLineReached = strcmp(redLineReached, "1") == 0;
  strncpy(packet.gear, gear, sizeof(packet.gear) - 1);

  const esp_err_t result =
      esp_now_send(BROADCAST_ADDRESS,
                   reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
  if (result != ESP_OK) {
    return false;
  }
  return true;
}

void processSimHubSerial() {
  while (Serial.available() > 0) {
    const char character = static_cast<char>(Serial.read());
    if (character == '\r') {
      continue;
    }
    if (character == '\n') {
      serialLine[serialLineLength] = '\0';
      if (serialLineLength > 0) {
        parseAndSendTelemetry(serialLine);
      }
      serialLineLength = 0;
      continue;
    }

    if (serialLineLength < sizeof(serialLine) - 1) {
      serialLine[serialLineLength++] = character;
    } else {
      serialLineLength = 0;
    }
  }
}

void sendGamepadReport(const uint16_t buttonMask) {
  // Neutral axes and hat; the low eight bits map to gamepad buttons 1-8.
  gamepad.send(0, 0, 0, 0, 0, 0, 0, buttonMask);
  reportedButtonMask = buttonMask;
}
}  // namespace

void setup() {
  Serial.begin(115200);
  gamepad.begin();
  USB.productName("ESP-NOW Sim Racing Wheel");
  USB.manufacturerName("DIY Sim Wheel");
  USB.begin();
  delay(1000);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_wifi_set_channel(WheelProtocol::ESPNOW_CHANNEL,
                           WIFI_SECOND_CHAN_NONE) != ESP_OK) {
    delay(2000);
    ESP.restart();
  }

  if (esp_now_init() != ESP_OK) {
    delay(2000);
    ESP.restart();
  }

  esp_now_register_recv_cb(onDataReceived);
  if (!addBroadcastPeer()) {
    delay(2000);
    ESP.restart();
  }
}

void loop() {
  processSimHubSerial();

  const uint32_t now = millis();
  if (inputPending) {
    inputPending = false;
    const uint16_t buttonMask = receivedButtonMask;
    if (buttonMask != activeButtonMask) {
      activeButtonMask = buttonMask;
    }
  }

  if (activeButtonMask != 0 &&
      now - lastWheelInputMs >= WheelProtocol::INPUT_TIMEOUT_MS) {
    activeButtonMask = 0;
  }

  const uint32_t nowUs = micros();
  if (activeButtonMask != reportedButtonMask ||
      nowUs - lastHidReportUs >=
          WheelConstants::INPUT_SAFETY_REFRESH_INTERVAL_US) {
    lastHidReportUs = nowUs;
    sendGamepadReport(activeButtonMask);
  }
  if (now - lastHeartbeatSentMs >= WheelProtocol::HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatSentMs = now;
    sendHeartbeat();
  }

  delay(1);
}
