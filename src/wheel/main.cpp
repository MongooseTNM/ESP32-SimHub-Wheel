#include <Arduino.h>
#include <FastLED.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

#include "espnow_protocol.h"
#include "wheel_config.h"
#include "wheel_constants.h"

namespace {
using namespace WheelProtocol;

constexpr uint8_t BROADCAST_ADDRESS[] = {0xFF, 0xFF, 0xFF,
                                         0xFF, 0xFF, 0xFF};

uint16_t sequenceNumber = 0;
uint16_t inputSequenceNumber = 0;
uint32_t lastHeartbeatSentMs = 0;
TelemetryPacket latestTelemetry{};
volatile bool telemetryPending = false;
CRGB rpmLeds[WheelConfig::RPM_LED_COUNT];
CRGB nextRpmLeds[WheelConfig::RPM_LED_COUNT];
uint16_t rawButtonMask = 0;
uint16_t debouncedButtonMask = 0;
uint32_t buttonChangedAtMs[WheelConstants::BUTTON_COUNT]{};
uint32_t lastButtonReportUs = 0;

void onDataReceived(const uint8_t *, const uint8_t *data, const int length) {
  if (isValidTelemetry(data, length)) {
    memcpy(&latestTelemetry, data, sizeof(latestTelemetry));
    telemetryPending = true;
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
                               DeviceRole::Wheel,
                               0,
                               sequenceNumber++,
                               millis()};

  if (esp_now_send(BROADCAST_ADDRESS,
                   reinterpret_cast<const uint8_t *>(&packet),
                   sizeof(packet)) != ESP_OK) {}
}

uint16_t readButtonMask() {
  uint16_t mask = 0;
  for (uint8_t index = 0; index < WheelConstants::BUTTON_COUNT; ++index) {
    if (digitalRead(WheelConfig::BUTTON_PINS[index]) == LOW) {
      mask |= static_cast<uint16_t>(1U << index);
    }
  }
  return mask;
}

void sendButtonState() {
  const WheelInputPacket packet{MAGIC, VERSION, MessageType::WheelInput,
                                inputSequenceNumber++, debouncedButtonMask};
  if (esp_now_send(BROADCAST_ADDRESS,
                   reinterpret_cast<const uint8_t *>(&packet),
                   sizeof(packet)) != ESP_OK) {}
}

void scanButtons(const uint32_t nowMs, const uint32_t nowUs) {
  const uint16_t newRawMask = readButtonMask();
  bool stateChanged = false;

  for (uint8_t index = 0; index < WheelConstants::BUTTON_COUNT; ++index) {
    const uint16_t bit = static_cast<uint16_t>(1U << index);
    const bool rawPressed = (newRawMask & bit) != 0;
    const bool previousRawPressed = (rawButtonMask & bit) != 0;
    const bool debouncedPressed = (debouncedButtonMask & bit) != 0;

    if (rawPressed != previousRawPressed) {
      buttonChangedAtMs[index] = nowMs;
    } else if (rawPressed != debouncedPressed &&
               nowMs - buttonChangedAtMs[index] >=
                   WheelConfig::BUTTON_DEBOUNCE_MS) {
      if (rawPressed) {
        debouncedButtonMask |= bit;
      } else {
        debouncedButtonMask &= ~bit;
      }
      stateChanged = true;
    }
  }
  rawButtonMask = newRawMask;

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

void renderRpmLeds(const TelemetryPacket &telemetry, const bool flashOn) {
  fill_solid(nextRpmLeds, WheelConfig::RPM_LED_COUNT, CRGB::Black);

  if (telemetry.maxRpm == 0 || telemetry.displayedRpmPercentX100 == 0) {
    showRpmLedsIfChanged();
    return;
  }

  // SimHub owns the redline trigger through CarSettings_RPMRedLineReached.
  if (telemetry.rpmRedLineReached != 0) {
    fill_solid(nextRpmLeds, WheelConfig::RPM_LED_COUNT,
               flashOn ? CRGB::Blue : CRGB::Black);
    showRpmLedsIfChanged();
    return;
  }

  const uint16_t firstLedEnd = WheelConfig::RPM_LED_COUNT / 3;
  const uint16_t secondLedEnd = (WheelConfig::RPM_LED_COUNT * 2) / 3;
  const auto segmentCount = [](const uint16_t progressX1000,
                               const uint16_t ledCount) -> uint16_t {
    const uint16_t clamped = min<uint16_t>(progressX1000, 1000);
    return static_cast<uint16_t>((static_cast<uint32_t>(clamped) * ledCount +
                                  999) /
                                 1000);
  };

  const uint16_t firstLit = segmentCount(
      telemetry.shiftLight1ProgressX1000, firstLedEnd);
  const uint16_t secondLit = segmentCount(
      telemetry.shiftLight2ProgressX1000, secondLedEnd - firstLedEnd);

  uint16_t thirdProgressX1000 = 0;
  if (telemetry.redLineDisplayedPercentX100 < 10000) {
    const uint32_t amountPastRedline =
        telemetry.displayedRpmPercentX100 -
        min(telemetry.displayedRpmPercentX100,
            telemetry.redLineDisplayedPercentX100);
    thirdProgressX1000 = static_cast<uint16_t>(min<uint32_t>(
        1000, amountPastRedline * 1000 /
                  (10000 - telemetry.redLineDisplayedPercentX100)));
  }
  const uint16_t thirdLit = segmentCount(
      thirdProgressX1000, WheelConfig::RPM_LED_COUNT - secondLedEnd);

  for (uint16_t index = 0; index < firstLit; ++index) {
    nextRpmLeds[index] = colorForLed(index);
  }
  for (uint16_t index = firstLedEnd; index < firstLedEnd + secondLit; ++index) {
    nextRpmLeds[index] = colorForLed(index);
  }
  for (uint16_t index = secondLedEnd; index < secondLedEnd + thirdLit; ++index) {
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
  rawButtonMask = readButtonMask();
  debouncedButtonMask = rawButtonMask;

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
  const uint32_t now = millis();
  scanButtons(now, micros());

  static TelemetryPacket activeTelemetry{};
  static bool haveTelemetry = false;
  static bool previousFlashOn = false;
  const bool flashOn =
      (now / WheelConfig::SHIFT_FLASH_INTERVAL_MS) % 2 == 0;
  const bool flashPhaseChanged =
      haveTelemetry && activeTelemetry.rpmRedLineReached != 0 &&
      flashOn != previousFlashOn;

  if (telemetryPending) {
    telemetryPending = false;
    memcpy(&activeTelemetry, &latestTelemetry, sizeof(activeTelemetry));
    haveTelemetry = true;
    renderRpmLeds(activeTelemetry, flashOn);
    previousFlashOn = flashOn;
  } else if (flashPhaseChanged) {
    renderRpmLeds(activeTelemetry, flashOn);
    previousFlashOn = flashOn;
  }

  if (now - lastHeartbeatSentMs >= WheelProtocol::HEARTBEAT_INTERVAL_MS) {
    lastHeartbeatSentMs = now;
    sendHeartbeat();
  }

  delay(1);
}
