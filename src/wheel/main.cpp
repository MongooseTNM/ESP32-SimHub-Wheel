#include <Arduino.h>
#include <FastLED.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_system.h>
#include <esp_wifi.h>

#include "espnow_protocol.h"
#include "reliable_sender.h"
#include "wheel_config.h"
#include "wheel_constants.h"
#include "wheel_input.h"

namespace {
using namespace WheelProtocol;

constexpr uint8_t BROADCAST_ADDRESS[] = {0xFF, 0xFF, 0xFF,
                                         0xFF, 0xFF, 0xFF};

uint16_t sequenceNumber = 0;
uint16_t inputSequenceNumber = 0;
uint32_t lastHeartbeatSentMs = 0;
TelemetryPayload latestTelemetry{};
volatile bool telemetryPending = false;
CRGB rpmLeds[WheelConfig::RPM_LED_COUNT];
CRGB nextRpmLeds[WheelConfig::RPM_LED_COUNT];
uint16_t rawContactMask = 0;
uint16_t debouncedContactMask = 0;
uint32_t contactChangedAtMs[WheelInput::CONTACT_COUNT]{};
uint32_t lastButtonReportUs = 0;
Preferences preferences;
ReliableSender reliableSender;
uint8_t peerAddress[6]{};
uint32_t sessionId = 0;
uint32_t wheelNonce = 0;
uint32_t receiverNonce = 0;
uint32_t lastDiscoveryMs = 0;
uint32_t lastTelemetryMs = 0;
uint16_t lastTelemetrySequence = 0;
bool haveTelemetrySequence = false;
volatile bool paired = false;
volatile bool pairingLockedUntilRestart = false;
volatile bool pairingSuccessPending = false;
uint32_t lastReceiverUptimeMs = 0;

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
  memset(peerAddress, 0, sizeof(peerAddress));
}

template <typename Payload>
void sendDirect(const uint8_t *address, const MessageType type,
                const uint32_t packetSession, const Payload &payload) {
  const auto packet = makePacket(type, DeviceRole::Wheel, packetSession,
                                 sequenceNumber++, payload);
  esp_now_send(address, reinterpret_cast<const uint8_t *>(&packet),
               sizeof(packet));
}

void onDataReceived(const uint8_t *source, const uint8_t *data, const int length) {
  if (validatePacket<PairingPayload>(data, length, MessageType::PairRequest,
                                     DeviceRole::Receiver)) {
    PairingPacket packet{};
    memcpy(&packet, data, sizeof(packet));
    if (!paired && !pairingLockedUntilRestart &&
        packet.payload.wheelNonce == wheelNonce) {
      memcpy(peerAddress, source, sizeof(peerAddress));
      receiverNonce = packet.payload.receiverNonce;
      sessionId = deriveSessionId(wheelNonce, receiverNonce);
      addPeer(peerAddress);
      sendDirect(peerAddress, MessageType::PairAccept, sessionId,
                 packet.payload);
    }
    return;
  }
  if (!pairingLockedUntilRestart &&
      validatePacket<PairingPayload>(data, length, MessageType::PairCommit,
                                     DeviceRole::Receiver, sessionId, true) &&
      sameAddress(source, peerAddress)) {
    PairingPacket packet{};
    memcpy(&packet, data, sizeof(packet));
    if (packet.payload.wheelNonce == wheelNonce || paired) {
      const bool pairingJustCompleted = !paired;
      paired = true;
      savePairing();
      sendDirect(peerAddress, MessageType::PairConfirmed, sessionId,
                 packet.payload);
      if (pairingJustCompleted) pairingSuccessPending = true;
    }
    return;
  }
  if (!paired || !sameAddress(source, peerAddress)) return;
  if (validatePacket<PairResetPayload>(data, length, MessageType::PairReset,
                                       DeviceRole::Receiver, sessionId, true)) {
    pairingLockedUntilRestart = true;
    clearPairing();
    return;
  }
  if (validatePacket<HeartbeatPayload>(data, length, MessageType::Heartbeat,
                                       DeviceRole::Receiver, sessionId, true)) {
    HeartbeatPacket packet{};
    memcpy(&packet, data, sizeof(packet));
    if (packet.payload.uptimeMs < lastReceiverUptimeMs) {
      haveTelemetrySequence = false;
    }
    lastReceiverUptimeMs = packet.payload.uptimeMs;
    return;
  }
  if (validatePacket<TelemetryPayload>(data, length, MessageType::Telemetry,
                                       DeviceRole::Receiver, sessionId, true)) {
    TelemetryPacket packet{};
    memcpy(&packet, data, sizeof(packet));
    if (haveTelemetrySequence &&
        !isSequenceNewer(packet.header.sequence, lastTelemetrySequence)) return;
    lastTelemetrySequence = packet.header.sequence;
    haveTelemetrySequence = true;
    latestTelemetry = packet.payload;
    lastTelemetryMs = millis();
    telemetryPending = true;
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

void sendDiscovery() {
  const auto packet = makePacket(MessageType::Discovery, DeviceRole::Wheel, 0,
                                 sequenceNumber++, DiscoveryPayload{wheelNonce});
  esp_now_send(BROADCAST_ADDRESS, reinterpret_cast<const uint8_t *>(&packet),
               sizeof(packet));
}

void sendHeartbeat() {
  if (!paired) return;
  const auto packet = makePacket(MessageType::Heartbeat, DeviceRole::Wheel,
                                 sessionId, sequenceNumber++,
                                 HeartbeatPayload{millis()});
  reliableSender.send(peerAddress, &packet, sizeof(packet));
}

uint16_t readContactMask() {
  uint16_t mask = 0;
  for (uint8_t index = 0; index < WheelInput::BUTTON_COUNT; ++index) {
    if (digitalRead(WheelConfig::BUTTON_PINS[index]) == LOW) {
      mask |= static_cast<uint16_t>(1U << index);
    }
  }
  for (uint8_t index = 0; index < WheelInput::POV_DIRECTION_COUNT; ++index) {
    if (digitalRead(WheelConfig::POV_PINS[index]) == LOW) {
      mask |= static_cast<uint16_t>(1U << (WheelInput::BUTTON_COUNT + index));
    }
  }
  return mask;
}

WheelInput::Pov currentPov() {
  const auto pressed = [](const uint8_t directionIndex) {
    return (debouncedContactMask &
            static_cast<uint16_t>(
                1U << (WheelInput::BUTTON_COUNT + directionIndex))) != 0;
  };
  return WheelInput::povFromDirections(pressed(0), pressed(1), pressed(2),
                                       pressed(3));
}

void sendButtonState() {
  if (!paired) return;
  const auto packet = makePacket(MessageType::WheelInput, DeviceRole::Wheel,
                                 sessionId, inputSequenceNumber++,
                                 WheelInputPayload{
                                     static_cast<uint16_t>(
                                         debouncedContactMask &
                                         WheelInput::BUTTON_MASK),
                                     static_cast<uint8_t>(currentPov())});
  reliableSender.send(peerAddress, &packet, sizeof(packet));
}

void scanButtons(const uint32_t nowMs, const uint32_t nowUs) {
  const uint16_t newRawMask = readContactMask();
  bool stateChanged = false;

  for (uint8_t index = 0; index < WheelInput::CONTACT_COUNT; ++index) {
    const uint16_t bit = static_cast<uint16_t>(1U << index);
    const bool rawPressed = (newRawMask & bit) != 0;
    const bool previousRawPressed = (rawContactMask & bit) != 0;
    const bool debouncedPressed = (debouncedContactMask & bit) != 0;

    if (rawPressed != previousRawPressed) {
      contactChangedAtMs[index] = nowMs;
    } else if (rawPressed != debouncedPressed &&
               nowMs - contactChangedAtMs[index] >=
                   WheelConfig::BUTTON_DEBOUNCE_MS) {
      if (rawPressed) {
        debouncedContactMask |= bit;
      } else {
        debouncedContactMask &= ~bit;
      }
      stateChanged = true;
    }
  }
  rawContactMask = newRawMask;

  if (stateChanged ||
      nowUs - lastButtonReportUs >=
          WheelConstants::INPUT_SAFETY_REFRESH_INTERVAL_US) {
    lastButtonReportUs = nowUs;
    sendButtonState();
  }
}

CRGB colorForLed(const uint16_t index) {
  const uint16_t firstBoundary = WheelConfig::RPM_LED_COUNT / 3;
  const uint16_t secondBoundary = (WheelConfig::RPM_LED_COUNT * 2) / 3;
  if (index >= secondBoundary) {
    return CRGB::Red;
  }
  if (index >= firstBoundary) {
    return CRGB::Yellow;
  }
  return CRGB::Green;
}

void clearRpmLeds() {
  fill_solid(rpmLeds, WheelConfig::RPM_LED_COUNT, CRGB::Black);
  FastLED.show();
}

void showStartupFlash() {
  for (uint8_t flash = 0; flash < WheelConfig::STARTUP_FLASH_COUNT; ++flash) {
    fill_solid(rpmLeds, WheelConfig::RPM_LED_COUNT, CRGB::Red);
    FastLED.show();
    delay(WheelConfig::STARTUP_FLASH_ON_MS);

    clearRpmLeds();
    delay(WheelConfig::STARTUP_FLASH_OFF_MS);
  }
}

void showRpmLedsIfChanged() {
  if (memcmp(rpmLeds, nextRpmLeds, sizeof(rpmLeds)) == 0) {
    return;
  }
  memcpy(rpmLeds, nextRpmLeds, sizeof(rpmLeds));
  FastLED.show();
}

void renderSolidLeds(const CRGB color) {
  fill_solid(nextRpmLeds, WheelConfig::RPM_LED_COUNT, color);
  showRpmLedsIfChanged();
}

bool renderPairingStatus(const uint32_t now) {
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
          (pairingSuccessPhase % 2 == 0)
              ? WheelConfig::PAIRING_SUCCESS_FLASH_ON_MS
              : WheelConfig::PAIRING_SUCCESS_FLASH_OFF_MS;
      if (now - pairingSuccessPhaseStartedMs < phaseDurationMs) break;
      pairingSuccessPhaseStartedMs += phaseDurationMs;
      ++pairingSuccessPhase;
    }

    if (pairingSuccessPhase < phaseCount) {
      renderSolidLeds(pairingSuccessPhase % 2 == 0 ? CRGB::Green
                                                   : CRGB::Black);
      return true;
    }
    pairingSuccessActive = false;
  }

  if (!paired) {
    if (pairingLockedUntilRestart) {
      renderSolidLeds(CRGB::Black);
      return true;
    }
    const bool flashOn =
        (now / WheelConfig::PAIRING_SEARCH_FLASH_INTERVAL_MS) % 2 == 0;
    renderSolidLeds(flashOn ? CRGB::Blue : CRGB::Black);
    return true;
  }

  return false;
}

void renderRpmLeds(const TelemetryPayload &telemetry, const bool flashOn) {
  fill_solid(nextRpmLeds, WheelConfig::RPM_LED_COUNT, CRGB::Black);

  // SimHub owns the redline trigger through CarSettings_RPMRedLineReached.
  if (telemetry.rpmRedLineReached != 0) {
    fill_solid(nextRpmLeds, WheelConfig::RPM_LED_COUNT,
               flashOn ? CRGB::Blue : CRGB::Black);
    showRpmLedsIfChanged();
    return;
  }

  static_assert(WheelConfig::RPM_LED_FILL_START_PERCENT < 100,
                "RPM LED fill start must be below 100 percent");
  static_assert(WheelConfig::RPM_LED_FULL_BELOW_REDLINE_PERCENT < 100,
                "RPM LED full offset must be below 100 percent");
  static_assert(WheelConfig::RPM_LED_FILL_START_PERCENT <
                    100 - WheelConfig::RPM_LED_FULL_BELOW_REDLINE_PERCENT,
                "RPM LED fill start must be below the full threshold");
  if (telemetry.currentGearRedLineRpm == 0) {
    showRpmLedsIfChanged();
    return;
  }

  const uint32_t fillStartRpm =
      static_cast<uint32_t>(telemetry.currentGearRedLineRpm) *
      WheelConfig::RPM_LED_FILL_START_PERCENT / 100;
  const uint32_t fillFullRpm =
      static_cast<uint32_t>(telemetry.currentGearRedLineRpm) *
      (100 - WheelConfig::RPM_LED_FULL_BELOW_REDLINE_PERCENT) / 100;
  const uint32_t clampedRpm =
      min<uint32_t>(max<uint32_t>(telemetry.rpm, fillStartRpm), fillFullRpm);
  const uint32_t amountAboveStartRpm = clampedRpm - fillStartRpm;
  const uint32_t fillRangeRpm = fillFullRpm - fillStartRpm;
  const uint16_t litCount = static_cast<uint16_t>(min<uint32_t>(
      WheelConfig::RPM_LED_COUNT,
      (amountAboveStartRpm * WheelConfig::RPM_LED_COUNT + fillRangeRpm - 1) /
          fillRangeRpm));

  for (uint16_t index = 0; index < litCount; ++index) {
    nextRpmLeds[index] = colorForLed(index);
  }
  showRpmLedsIfChanged();
}
}  // namespace

void setup() {
  FastLED.addLeds<WS2812B, WheelConfig::RPM_LED_DATA_PIN, GRB>(
      rpmLeds, WheelConfig::RPM_LED_COUNT);
  FastLED.setBrightness(WheelConfig::RPM_LED_BRIGHTNESS);
  clearRpmLeds();
  showStartupFlash();

  for (const uint8_t pin : WheelConfig::BUTTON_PINS) {
    pinMode(pin, INPUT_PULLUP);
  }
  for (const uint8_t pin : WheelConfig::POV_PINS) {
    pinMode(pin, INPUT_PULLUP);
  }
  rawContactMask = readContactMask();
  debouncedContactMask = rawContactMask;

  bool resetRequested = false;
  if (digitalRead(WheelConfig::PAIRING_RESET_PIN) == LOW) {
    const uint32_t heldFrom = millis();
    while (digitalRead(WheelConfig::PAIRING_RESET_PIN) == LOW &&
           millis() - heldFrom < WheelConfig::PAIRING_RESET_HOLD_MS) {
      delay(10);
    }
    resetRequested = millis() - heldFrom >= WheelConfig::PAIRING_RESET_HOLD_MS;
  }
  loadPairing();
  wheelNonce = esp_random();
  if (wheelNonce == 0) wheelNonce = 1;

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
  if (paired && !addPeer(peerAddress)) clearPairing();
  if (resetRequested) {
    pairingLockedUntilRestart = true;
    if (paired) {
      const auto resetPacket = makePacket(
          MessageType::PairReset, DeviceRole::Wheel, sessionId,
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
  const uint32_t now = millis();
  reliableSender.service(now);
  if (!paired && !pairingLockedUntilRestart &&
      now - lastDiscoveryMs >= DISCOVERY_INTERVAL_MS) {
    lastDiscoveryMs = now;
    sendDiscovery();
  }
  scanButtons(now, micros());

  static TelemetryPayload activeTelemetry{};
  static bool haveTelemetry = false;
  static bool previousFlashOn = false;
  static bool pairingStatusWasActive = false;
  bool rpmRenderNeeded = false;
  const bool flashOn =
      (now / WheelConfig::SHIFT_FLASH_INTERVAL_MS) % 2 == 0;
  const bool flashPhaseChanged =
      haveTelemetry && activeTelemetry.rpmRedLineReached != 0 &&
      flashOn != previousFlashOn;

  if (telemetryPending) {
    telemetryPending = false;
    activeTelemetry = latestTelemetry;
    haveTelemetry = true;
    rpmRenderNeeded = true;
    previousFlashOn = flashOn;
  } else if (flashPhaseChanged) {
    rpmRenderNeeded = true;
    previousFlashOn = flashOn;
  }
  if (haveTelemetry &&
      now - lastTelemetryMs >= WheelConfig::TELEMETRY_STALE_TIMEOUT_MS) {
    haveTelemetry = false;
    activeTelemetry = {};
    rpmRenderNeeded = true;
  }

  const bool pairingStatusActive = renderPairingStatus(now);
  if (!pairingStatusActive && (rpmRenderNeeded || pairingStatusWasActive)) {
    renderRpmLeds(activeTelemetry, flashOn);
  }
  pairingStatusWasActive = pairingStatusActive;

  if (now - lastHeartbeatSentMs >= WheelProtocol::HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatSentMs = now;
    sendHeartbeat();
  }

  delay(1);
}
