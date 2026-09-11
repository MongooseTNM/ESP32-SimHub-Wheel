#include <Arduino.h>
#include <FastLED.h>
#include <Preferences.h>
#include <USB.h>
#include <USBHIDGamepad.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_system.h>
#include <esp_wifi.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "espnow_protocol.h"
#include "receiver_config.h"
#include "reliable_sender.h"
#include "wheel_config.h"
#include "wheel_constants.h"

namespace {
using namespace WheelProtocol;

constexpr uint8_t BROADCAST_ADDRESS[] = {0xFF, 0xFF, 0xFF,
                                         0xFF, 0xFF, 0xFF};

USBHIDGamepad gamepad;
CRGB statusLed[1];

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
Preferences preferences;
ReliableSender reliableSender;
uint8_t peerAddress[6]{};
uint32_t sessionId = 0;
uint32_t wheelNonce = 0;
uint32_t receiverNonce = 0;
uint32_t lastPairingSendMs = 0;
uint32_t pairingStartedMs = 0;
uint16_t lastInputSequence = 0;
bool haveInputSequence = false;
volatile bool paired = false;
volatile bool pairingLockedUntilRestart = false;
volatile bool pairingSuccessPending = false;
uint32_t lastWheelUptimeMs = 0;
enum class PairingStage : uint8_t { Idle, RequestSent, CommitSent };
volatile PairingStage pairingStage = PairingStage::Idle;
bool pairingSuccessActive = false;
uint8_t pairingSuccessPhase = 0;
uint32_t pairingSuccessPhaseStartedMs = 0;

bool sameAddress(const uint8_t *left, const uint8_t *right) {
  return left != nullptr && right != nullptr && memcmp(left, right, 6) == 0;
}

bool addPeer(const uint8_t *address) {
  if (esp_now_is_peer_exist(address)) return true;
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, address, 6);
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;
  return esp_now_add_peer(&peer) == ESP_OK;
}

void savePairing() {
  preferences.begin("wheel-link", false);
  preferences.putBytes("peer", peerAddress, sizeof(peerAddress));
  preferences.putUInt("session", sessionId);
  preferences.end();
}

bool loadPairing() {
  preferences.begin("wheel-link", true);
  const bool valid = preferences.getBytesLength("peer") == sizeof(peerAddress);
  if (valid) preferences.getBytes("peer", peerAddress, sizeof(peerAddress));
  sessionId = preferences.getUInt("session", 0);
  preferences.end();
  paired = valid && sessionId != 0;
  return paired;
}

void clearPairing() {
  preferences.begin("wheel-link", false);
  preferences.clear();
  preferences.end();
  paired = false;
  sessionId = 0;
  haveInputSequence = false;
  activeButtonMask = 0;
}

template <typename Payload>
void sendDirect(const uint8_t *address, const MessageType type,
                const uint32_t packetSession, const Payload &payload) {
  const auto packet = makePacket(type, DeviceRole::Receiver, packetSession,
                                 sequenceNumber++, payload);
  esp_now_send(address, reinterpret_cast<const uint8_t *>(&packet),
               sizeof(packet));
}

void onDataReceived(const uint8_t *source, const uint8_t *data, const int length) {
  if (!paired && !pairingLockedUntilRestart &&
      validatePacket<DiscoveryPayload>(
                     data, length, MessageType::Discovery, DeviceRole::Wheel)) {
    DiscoveryPacket packet{};
    memcpy(&packet, data, sizeof(packet));
    memcpy(peerAddress, source, sizeof(peerAddress));
    wheelNonce = packet.payload.nonce;
    receiverNonce = esp_random();
    if (receiverNonce == 0) receiverNonce = 1;
    sessionId = deriveSessionId(wheelNonce, receiverNonce);
    addPeer(peerAddress);
    const PairingPayload payload{wheelNonce, receiverNonce};
    sendDirect(peerAddress, MessageType::PairRequest, 0, payload);
    pairingStartedMs = lastPairingSendMs = millis();
    pairingStage = PairingStage::RequestSent;
    return;
  }
  if (!paired && !pairingLockedUntilRestart &&
      sameAddress(source, peerAddress) &&
      validatePacket<PairingPayload>(data, length, MessageType::PairAccept,
                                     DeviceRole::Wheel, sessionId, true)) {
    PairingPacket packet{};
    memcpy(&packet, data, sizeof(packet));
    sendDirect(peerAddress, MessageType::PairCommit, sessionId, packet.payload);
    lastPairingSendMs = millis();
    pairingStage = PairingStage::CommitSent;
    return;
  }
  if (!paired && !pairingLockedUntilRestart &&
      sameAddress(source, peerAddress) &&
      validatePacket<PairingPayload>(data, length, MessageType::PairConfirmed,
                                     DeviceRole::Wheel, sessionId, true)) {
    paired = true;
    pairingStage = PairingStage::Idle;
    savePairing();
    pairingSuccessPending = true;
    return;
  }
  if (!paired || !sameAddress(source, peerAddress)) return;
  if (validatePacket<PairResetPayload>(data, length, MessageType::PairReset,
                                       DeviceRole::Wheel, sessionId, true)) {
    pairingLockedUntilRestart = true;
    pairingStage = PairingStage::Idle;
    clearPairing();
    return;
  }
  if (validatePacket<HeartbeatPayload>(data, length, MessageType::Heartbeat,
                                       DeviceRole::Wheel, sessionId, true)) {
    HeartbeatPacket packet{};
    memcpy(&packet, data, sizeof(packet));
    if (packet.payload.uptimeMs < lastWheelUptimeMs) haveInputSequence = false;
    lastWheelUptimeMs = packet.payload.uptimeMs;
    return;
  }
  if (validatePacket<WheelInputPayload>(
          data, length, MessageType::WheelInput, DeviceRole::Wheel, sessionId,
          true)) {
    WheelInputPacket packet{};
    memcpy(&packet, data, sizeof(packet));
    if (haveInputSequence &&
        !isSequenceNewer(packet.header.sequence, lastInputSequence)) return;
    lastInputSequence = packet.header.sequence;
    haveInputSequence = true;
    receivedButtonMask = packet.payload.buttonMask;
    lastWheelInputMs = millis();
    inputPending = true;
  }
}

void onDataSent(const uint8_t *, const esp_now_send_status_t status) {
  reliableSender.onComplete(status, millis());
}

bool addBroadcastPeer() {
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, BROADCAST_ADDRESS, sizeof(BROADCAST_ADDRESS));
  peer.channel = ESPNOW_CHANNEL;
  peer.encrypt = false;
  return esp_now_add_peer(&peer) == ESP_OK;
}

void sendHeartbeat() {
  if (!paired) return;
  const auto packet = makePacket(MessageType::Heartbeat, DeviceRole::Receiver,
                                 sessionId, sequenceNumber++,
                                 HeartbeatPayload{millis()});
  reliableSender.send(peerAddress, &packet, sizeof(packet));
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
  //   <at redline>;<shift 1 progress>;<shift 2 progress>;
  //   <current gear redline rpm>;<gear>
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
  const char *currentGearRedLineRpm = strtok_r(nullptr, ";", &savePointer);
  const char *gear = strtok_r(nullptr, ";", &savePointer);

  if (prefix == nullptr || strcmp(prefix, "T") != 0 || rpm == nullptr ||
      displayedRpmPercent == nullptr || redLineRpm == nullptr ||
      redLineDisplayedPercent == nullptr || maxRpm == nullptr ||
      minimumShownRpm == nullptr || redLineReached == nullptr ||
      shiftLight1Progress == nullptr || shiftLight2Progress == nullptr ||
      currentGearRedLineRpm == nullptr || gear == nullptr) {
    return false;
  }

  if (!paired) return false;
  TelemetryPayload telemetry{};
  telemetry.rpm = parseClampedU16(rpm);
  telemetry.displayedRpmPercentX100 = parseClampedU16(displayedRpmPercent, 100);
  telemetry.redLineRpm = parseClampedU16(redLineRpm);
  telemetry.redLineDisplayedPercentX100 =
      parseClampedU16(redLineDisplayedPercent, 100);
  telemetry.maxRpm = parseClampedU16(maxRpm);
  telemetry.minimumShownRpm = parseClampedU16(minimumShownRpm);
  telemetry.shiftLight1ProgressX1000 =
      parseClampedU16(shiftLight1Progress, 1000);
  telemetry.shiftLight2ProgressX1000 =
      parseClampedU16(shiftLight2Progress, 1000);
  telemetry.currentGearRedLineRpm = parseClampedU16(currentGearRedLineRpm);
  telemetry.rpmRedLineReached = strcmp(redLineReached, "1") == 0;
  strncpy(telemetry.gear, gear, sizeof(telemetry.gear) - 1);
  const auto packet = makePacket(MessageType::Telemetry, DeviceRole::Receiver,
                                 sessionId, sequenceNumber++, telemetry);
  return reliableSender.send(peerAddress, &packet, sizeof(packet));
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

void showStatusLed(const CRGB color) {
  if (statusLed[0] == color) return;
  statusLed[0] = color;
  FastLED.show();
}

void renderPairingStatus(const uint32_t now) {
  if (pairingSuccessPending) {
    pairingSuccessPending = false;
    pairingSuccessActive = true;
    pairingSuccessPhase = 0;
    pairingSuccessPhaseStartedMs = now;
  }

  if (pairingSuccessActive) {
    const uint8_t phaseCount =
        WheelConfig::PAIRING_SUCCESS_FLASH_COUNT * 2;
    while (pairingSuccessPhase < phaseCount) {
      const uint32_t phaseDurationMs =
          pairingSuccessPhase % 2 == 0
              ? WheelConfig::PAIRING_SUCCESS_FLASH_ON_MS
              : WheelConfig::PAIRING_SUCCESS_FLASH_OFF_MS;
      if (now - pairingSuccessPhaseStartedMs < phaseDurationMs) break;
      pairingSuccessPhaseStartedMs += phaseDurationMs;
      ++pairingSuccessPhase;
    }

    if (pairingSuccessPhase < phaseCount) {
      showStatusLed(pairingSuccessPhase % 2 == 0 ? CRGB::Green : CRGB::Black);
      return;
    }
    pairingSuccessActive = false;
  }

  if (!paired && !pairingLockedUntilRestart) {
    const bool flashOn =
        (now / WheelConfig::PAIRING_SEARCH_FLASH_INTERVAL_MS) % 2 == 0;
    showStatusLed(flashOn ? CRGB::Blue : CRGB::Black);
    return;
  }

  showStatusLed(CRGB::Black);
}
}  // namespace

void setup() {
  FastLED.addLeds<WS2812B, ReceiverConfig::STATUS_LED_DATA_PIN, GRB>(statusLed,
                                                                    1);
  FastLED.setBrightness(ReceiverConfig::STATUS_LED_BRIGHTNESS);
  statusLed[0] = CRGB::Black;
  FastLED.show();

  pinMode(ReceiverConfig::PAIRING_RESET_BUTTON_PIN, INPUT_PULLUP);
  bool resetRequested = false;
  if (digitalRead(ReceiverConfig::PAIRING_RESET_BUTTON_PIN) == LOW) {
    const uint32_t heldFrom = millis();
    while (digitalRead(ReceiverConfig::PAIRING_RESET_BUTTON_PIN) == LOW &&
           millis() - heldFrom < ReceiverConfig::PAIRING_RESET_HOLD_MS) {
      delay(10);
    }
    resetRequested =
        millis() - heldFrom >= ReceiverConfig::PAIRING_RESET_HOLD_MS;
  }

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
  esp_now_register_send_cb(onDataSent);
  if (!addBroadcastPeer()) {
    delay(2000);
    ESP.restart();
  }
  loadPairing();
  if (paired && !addPeer(peerAddress)) clearPairing();
  if (resetRequested) {
    pairingLockedUntilRestart = true;
    if (paired) {
      const auto resetPacket = makePacket(
          MessageType::PairReset, DeviceRole::Receiver, sessionId,
          sequenceNumber++, PairResetPayload{sessionId});
      for (uint8_t attempt = 0; attempt < 3; ++attempt) {
        esp_now_send(peerAddress,
                     reinterpret_cast<const uint8_t *>(&resetPacket),
                     sizeof(resetPacket));
        delay(20);
      }
    }
    clearPairing();
  }
}

void loop() {
  const uint32_t serviceNow = millis();
  reliableSender.service(serviceNow);
  if (!paired && !pairingLockedUntilRestart &&
      pairingStage != PairingStage::Idle &&
      serviceNow - pairingStartedMs >= PAIRING_TIMEOUT_MS) {
    pairingStage = PairingStage::Idle;
    sessionId = 0;
    memset(peerAddress, 0, sizeof(peerAddress));
  } else if (!paired && !pairingLockedUntilRestart &&
             pairingStage != PairingStage::Idle &&
             serviceNow - lastPairingSendMs >= PAIRING_RETRY_INTERVAL_MS) {
    const PairingPayload payload{wheelNonce, receiverNonce};
    sendDirect(peerAddress,
               pairingStage == PairingStage::RequestSent
                   ? MessageType::PairRequest
                   : MessageType::PairCommit,
               pairingStage == PairingStage::RequestSent ? 0 : sessionId,
               payload);
    lastPairingSendMs = serviceNow;
  }
  processSimHubSerial();

  const uint32_t now = millis();
  renderPairingStatus(now);
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
