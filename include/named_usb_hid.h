#pragma once

#include <cstdint>
#include <cstring>

#include "esp32-hal-tinyusb.h"

// Minimal HID implementation with configurable report and interface strings.
// Arduino's USBHIDGamepad hard-codes "TinyUSB HID" and always advertises six
// axes, 32 buttons, and a hat, even when a controller does not provide them.
class NamedUSBHID {
 public:
  NamedUSBHID(const char *interfaceName, const uint8_t *reportDescriptor,
              const uint16_t reportDescriptorLength) {
    interfaceNameStorage() = interfaceName;
    reportDescriptorStorage() = reportDescriptor;
    reportDescriptorLengthStorage() = reportDescriptorLength;
    tinyusb_enable_interface(USB_INTERFACE_HID, TUD_HID_INOUT_DESC_LEN,
                             loadInterfaceDescriptor);
  }

  void begin() {}

  bool send(const void *report, const uint16_t reportLength) {
    return tud_hid_n_ready(0) &&
           tud_hid_n_report(0, REPORT_ID, report, reportLength);
  }

  static const uint8_t *reportDescriptor() {
    return reportDescriptorStorage();
  }

 private:
  static constexpr uint8_t REPORT_ID = 1;
  static uint16_t loadInterfaceDescriptor(uint8_t *destination,
                                          uint8_t *interfaceNumber) {
    const uint8_t stringIndex =
        tinyusb_add_string_descriptor(interfaceNameStorage());
    const uint8_t endpointIn = tinyusb_get_free_in_endpoint();
    const uint8_t endpointOut = tinyusb_get_free_out_endpoint();
    if (endpointIn == 0 || endpointOut == 0) return 0;

    const uint8_t descriptor[TUD_HID_INOUT_DESC_LEN] = {
        TUD_HID_INOUT_DESCRIPTOR(
            *interfaceNumber, stringIndex, HID_ITF_PROTOCOL_NONE,
            reportDescriptorLengthStorage(), endpointOut,
            static_cast<uint8_t>(0x80U | endpointIn), 64, 1)};
    ++*interfaceNumber;
    memcpy(destination, descriptor, sizeof(descriptor));
    return sizeof(descriptor);
  }

  static const char *&interfaceNameStorage() {
    static const char *name = "Gamepad";
    return name;
  }

  static const uint8_t *&reportDescriptorStorage() {
    static const uint8_t *descriptor = nullptr;
    return descriptor;
  }

  static uint16_t &reportDescriptorLengthStorage() {
    static uint16_t length = 0;
    return length;
  }
};

// TinyUSB HID callbacks. Each firmware has one NamedUSBHID instance.
extern "C" const uint8_t *tud_hid_descriptor_report_cb(uint8_t) {
  return NamedUSBHID::reportDescriptor();
}

extern "C" uint16_t tud_hid_get_report_cb(uint8_t, uint8_t,
                                           hid_report_type_t, uint8_t *,
                                           uint16_t) {
  return 0;
}

extern "C" void tud_hid_set_report_cb(uint8_t, uint8_t, hid_report_type_t,
                                       const uint8_t *, uint16_t) {}
