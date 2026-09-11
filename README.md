# ESP32 SimHub Wireless Wheel

A two-board wireless sim-racing wheel prototype built for the Waveshare
ESP32-S3-Zero. The wheel sends button states to a USB receiver over ESP-NOW,
while the receiver sends SimHub RPM telemetry back to the wheel.

The receiver appears to the PC as a composite USB device:

- A USB HID gamepad for wheel buttons.
- A USB CDC serial port for SimHub telemetry.

The wheel supports two shifters, a 5-way switch with POV hat output, eight
additional direct buttons, and a configurable WS2812B RPM strip. No display,
rotary encoders, analog axes, or battery monitoring are implemented yet.

## Features

- Two Waveshare ESP32-S3-Zero boards using the Arduino framework.
- Automatically paired bidirectional ESP-NOW communication on Wi-Fi channel 6.
- CRC-16 packet validation, sequence checks, unicast delivery status, and
  bounded retransmission with latest-value coalescing.
- Eleven active-low wheel buttons plus an eight-direction USB POV hat.
- Immediate button-change reports with a low-overhead 25 Hz safety refresh.
- A 250 ms receiver failsafe that releases buttons if wireless input stops.
- SimHub Custom Serial telemetry forwarded to the wheel.
- Configurable WS2812B LED count, data pin, brightness, and flash timing.
- SimHub per-car shift-light behavior.
- Full-strip blue flashing when SimHub reports that redline was reached.
- No debug text on the receiver CDC port, avoiding interference with SimHub.

## What you need

### Hardware

- Two Waveshare ESP32-S3-Zero boards:
  - One for the wireless wheel.
  - One for the PC-connected receiver.
- Two normally-open shifter switches, one 5-way switch, and eight additional
  normally-open momentary switches.
- A WS2812B-compatible LED strip; the default configuration uses 12 LEDs.
- A regulated 5 V supply suitable for the LED strip.
- A 330-470 ohm resistor for the LED data line.
- A 500-1000 uF capacitor for the LED supply input.
- A common ground between the wheel ESP32 and LED supply.
- A 74AHCT125 or similar 3.3 V-to-5 V level shifter is recommended.

### Software

- Visual Studio Code.
- The PlatformIO extension.
- SimHub with the Custom Serial Devices plugin enabled.

## Project layout

| Path | Purpose |
|---|---|
| `src/wheel/main.cpp` | Wireless wheel firmware. |
| `src/receiver/main.cpp` | USB receiver and SimHub bridge firmware. |
| `include/wheel_config.h` | User-adjustable pins, LED settings, and timing. |
| `include/wheel_constants.h` | Values derived from the user configuration. |
| `include/wheel_input.h` | Shared button and POV input model. |
| `include/espnow_protocol.h` | Shared ESP-NOW packet definitions. |
| `simhub/custom-serial-formula.txt` | NCalc telemetry expression for SimHub. |
| `platformio.ini` | PlatformIO board and environment configuration. |

## Wiring the wheel

### Buttons

The buttons are active-low. Connect one terminal of each switch to its assigned
GPIO and the other terminal to GND. The firmware enables the ESP32's internal
pull-up resistors, so external pull-up resistors are not required.

| Control | HID mapping | Default wheel GPIO |
|---|---:|---:|
| Left shifter | Button 1 | 10 |
| Right shifter | Button 2 | 11 |
| 5-way center press | Button 3 | 12 |
| Existing switch 1 | Button 4 | 2 |
| Existing switch 2 | Button 5 | 3 |
| Existing switch 3 | Button 6 | 4 |
| Existing switch 4 | Button 7 | 5 |
| Existing switch 5 | Button 8 | 6 |
| Existing switch 6 | Button 9 | 7 |
| Existing switch 7 | Button 10 | 8 |
| Existing switch 8 | Button 11 | 9 |
| 5-way up | POV up | 13 |
| 5-way right | POV right | 14 |
| 5-way down | POV down | 15 |
| 5-way left | POV left | 16 |

Change `BUTTON_PINS` or the `POV_*_PIN` constants in `include/wheel_config.h` if
different pins are needed. Adjacent POV contacts produce diagonal directions.
Opposing contacts cancel on that axis; for example, up + down is neutral while
up + down + right resolves to right.

### WS2812B RPM strip

The current configuration uses:

- 12 LEDs.
- GPIO 48 for LED data.
- Brightness 10 on a 0-255 scale.
- Two red startup flashes.

Wire the strip as follows:

```text
ESP32 GPIO 48 --- 330-470 ohm resistor --- WS2812 DIN
ESP32 GND -------------------------------- WS2812 GND
External regulated 5 V ------------------ WS2812 5V
External supply GND --------------------- ESP32 GND
```

Important electrical notes:

- The ESP32 and external LED supply must share ground.
- Do not power a high-current LED strip from the ESP32's 3.3 V pin.
- Place a 500-1000 uF capacitor across 5 V and GND near the strip input.
- A 74AHCT125 or similar logic-level shifter is recommended for reliable data.
- Estimate the supply for up to approximately 60 mA per LED at full white,
  even if normal firmware brightness is much lower.

Edit these values in `include/wheel_config.h` to match the hardware:

- `RPM_LED_DATA_PIN`
- `RPM_LED_COUNT`
- `RPM_LED_BRIGHTNESS`
- `STARTUP_FLASH_COUNT`
- `STARTUP_FLASH_ON_MS`
- `STARTUP_FLASH_OFF_MS`
- `SHIFT_FLASH_INTERVAL_MS`

## Build environments

The project contains two PlatformIO environments:

| Environment | Install on | Function |
|---|---|---|
| `wheel` | Battery-powered wheel board | Reads buttons, drives LEDs, and exchanges ESP-NOW packets. |
| `receiver` | USB-connected board | Receives buttons as USB HID and forwards SimHub telemetry. |

Both environments are based on `esp32-s3-devkitc-1` with settings adjusted for
the Waveshare ESP32-S3-Zero's 4 MB flash and 2 MB OPI PSRAM. The receiver uses
TinyUSB device mode so CDC serial and HID gamepad can coexist.

Wheel-input packets use protocol version 5. Flash both boards after updating;
version 5 firmware intentionally rejects input packets from older firmware.

## First-time build and upload

1. Open this project folder in Visual Studio Code with PlatformIO installed.
2. Connect only the board that will be used in the wheel.
3. Select the correct upload port in PlatformIO.
4. Upload the `wheel` environment.
5. Disconnect that board and label it as the wheel board.
6. Connect the board that will remain attached to the PC.
7. Upload the `receiver` environment.
8. Unplug and reconnect the receiver after uploading so the PC enumerates its
   composite CDC + HID interfaces.
9. Power the wheel board. Its LED strip should perform two red startup flashes.

PlatformIO toolbar tasks can be used, or run these commands from the project
directory:

```sh
platformio run -e wheel
platformio run -e receiver
platformio run -e wheel -t upload
platformio run -e receiver -t upload
```

If `platformio` is not available on the command line on macOS, PlatformIO's
virtual-environment executable is commonly located at:

```sh
/Users/your-name/.platformio/penv/bin/platformio
```

## Configure SimHub

This project uses SimHub's **Custom Serial Devices** plugin. The receiver reads
newline-terminated telemetry on its USB CDC port and forwards compact binary
packets to the wheel.

1. Connect the receiver to the PC and wait for its USB interfaces to appear.
2. Open SimHub.
3. Open **Settings > Plugins** and enable **Custom Serial Devices**.
4. Open the Custom Serial Devices plugin and add a device.
5. Select the receiver's USB serial port.
6. Set the baud rate to **115200**.
7. Enable automatic reconnect.
8. Leave RTS and DTR disabled initially.
9. Add an enabled update message.
10. Copy the complete expression from `simhub/custom-serial-formula.txt` and
    paste it into the update-message formula field.
11. Select a 20 Hz update rate. A 10 Hz rate is also suitable when limited by
    the SimHub edition in use.

Copy and paste this complete NCalc expression into the SimHub update message:

```text
'T;' + format(isnull([DataCorePlugin.GameData.NewData.Rpms],0),'0') + ';' + format(isnull([DataCorePlugin.GameData.NewData.CarSettings_CurrentDisplayedRPMPercent],0),'0.00') + ';' + format(isnull([DataCorePlugin.GameData.NewData.CarSettings_RedLineRPM],0),'0') + ';' + format(isnull([DataCorePlugin.GameData.NewData.CarSettings_RedLineDisplayedPercent],0),'0.00') + ';' + format(isnull([DataCorePlugin.GameData.NewData.CarSettings_MaxRPM],0),'0') + ';' + format(isnull([DataCorePlugin.GameData.NewData.CarSettings_MinimumShownRPM],0),'0') + ';' + format(isnull([DataCorePlugin.GameData.NewData.CarSettings_RPMRedLineReached],0),'0') + ';' + format(isnull([DataCorePlugin.GameData.NewData.CarSettings_RPMShiftLight1],0),'0.000') + ';' + format(isnull([DataCorePlugin.GameData.NewData.CarSettings_RPMShiftLight2],0),'0.000') + ';' + isnull([DataCorePlugin.GameData.NewData.Gear],'N') + '\n'
```

The same expression is kept as a copy-friendly single line in
`simhub/custom-serial-formula.txt`.

The serial message format is:

```text
T;<rpm>;<display percent>;<redline rpm>;<redline percent>;<maximum>;<minimum>;<redline reached>;<shift 1 progress>;<shift 2 progress>;<gear>\n
```

Do not configure a serial-monitor application on the receiver's CDC port while
SimHub is using it. Only one application should own the port at a time.

## RPM LED behavior

The LED bar uses SimHub's current per-car settings rather than fixed RPM
thresholds in the firmware:

- `CarSettings_RPMShiftLight1` progresses from 0 to 1 and fills the first third
  of the strip in green.
- `CarSettings_RPMShiftLight2` progresses from 0 to 1 and fills the second third
  in yellow.
- The final red third uses `CarSettings_CurrentDisplayedRPMPercent` relative to
  `CarSettings_RedLineDisplayedPercent` and maximum displayed RPM.
- `CarSettings_RPMRedLineReached` overrides normal rendering and flashes the
  full strip blue.

With 12 LEDs, each color section contains four LEDs. Counts that do not divide
evenly by three are distributed using integer section boundaries.

The wheel clears the RPM strip if valid telemetry stops for 500 ms, preventing
stale RPM or shift-light output from remaining visible after a link failure.

LED output is updated only when telemetry changes or the redline flash changes
phase. Unchanged frames are not transmitted to the strip, reducing processor
and peripheral overhead.

## Button and HID behavior

- Buttons are scanned continuously and debounced for 8 ms by default.
- A debounced change is sent over ESP-NOW immediately.
- The complete state is repeated at 25 Hz as protection against packet loss.
- The receiver sends changed states to USB HID immediately and repeats the state
  at the same 25 Hz safety rate.
- If no wheel input packet arrives for 250 ms while an input is active, the
  receiver releases every HID button and centers the POV to prevent a stuck
  control.

Change `INPUT_SAFETY_REFRESH_RATE_HZ` in `include/wheel_config.h` to adjust the
periodic refresh. The chosen rate must be from 1 through 1000 Hz and divide
1,000,000 evenly. This setting controls the safety refresh, not button-change
latency.

## ESP-NOW behavior

- Both devices operate in Wi-Fi station mode on channel 6.
- Unpaired devices use broadcast discovery and a confirmed pairing handshake.
- The mutually confirmed peer MAC and session identifier are stored in NVS.
- Normal button, telemetry, and heartbeat traffic is unicast to the stored peer.
- Packets include magic, version, type, sender role, payload length, session,
  sequence number, and CRC-16/CCITT-FALSE.
- Invalid, malformed, duplicate, stale, wrong-role, wrong-source, and
  wrong-session packets are ignored before application state is changed.
- ESP-NOW send completion drives up to two bounded retransmission attempts.
- If updates arrive while a send is in progress, only the newest queued update
  is retained, preserving low-latency latest-value behavior without an
  unbounded queue.
- Both devices send a heartbeat every 500 ms.

On first boot, power one wheel and one receiver near each other. Pairing is
automatic and normally completes within a second; no MAC address configuration
is required. After pairing, both devices reconnect using their persisted NVS
record.

To erase pairing, hold wheel button 4 (the existing switch on GPIO 2) while
powering the wheel and continue
holding it for two seconds. The wheel sends three reset notifications to its
stored receiver, clears its own record, and returns to discovery. Keep the
receiver powered during this operation so it clears its matching record too.
The reset pin and hold time are configured by `PAIRING_RESET_PIN` and
`PAIRING_RESET_HOLD_MS` in
`include/wheel_config.h`.

The application CRC adds two bytes. It supplements the Wi-Fi frame check and
protects the complete application envelope. Forward-error-correction codes are
not used: Wi-Fi normally discards corrupt frames before the receive callback,
so unicast acknowledgment and retransmission recover the more relevant failure
mode—a missing frame—with lower useful overhead.

## Verify operation

1. Power both boards.
2. Confirm that the wheel strip performs its red startup flashes.
3. Open the operating system's game-controller test panel.
4. Press each wheel button and confirm that gamepad buttons 1-11 respond.
5. Move the 5-way switch through cardinal and diagonal directions and confirm
   that the POV hat responds; confirm its center press reports button 3.
6. Start SimHub and connect the configured Custom Serial device.
7. Start a supported game or use SimHub's available telemetry-test features.
8. Confirm that the green and yellow sections fill with shift-light progress.
9. Confirm that the red section responds near the top of the displayed RPM
   range and that the strip flashes blue when redline is reported.

There is intentionally no serial debug output. This keeps the receiver's CDC
connection dedicated to SimHub and avoids mixing diagnostic text into its data
stream.

## Troubleshooting

### The receiver does not appear as a gamepad or serial port

- Unplug and reconnect it after flashing the `receiver` environment.
- Try a known data-capable USB cable and a direct PC USB port.
- Verify that the receiver, not the wheel environment, was uploaded.
- On Windows, check both **Game Controllers** and **Device Manager**.
- If uploading becomes difficult, hold BOOT, briefly press RESET, start the
  upload, and then release BOOT when the board enters its download mode.

### SimHub cannot open the serial port

- Close PlatformIO's serial monitor and every other terminal using that port.
- Confirm the selected port belongs to the receiver.
- Use 115200 baud.
- Reconnect the receiver and let SimHub retry with automatic reconnect enabled.
- Start with RTS and DTR disabled.

### Buttons do not respond

- Confirm each switch connects its configured GPIO to GND when pressed.
- Verify that both devices are running the correct environments.
- Keep both ESP32s on the configured Wi-Fi channel.
- Test at short range and remove nearby USB 3 equipment or other strong 2.4 GHz
  interference while diagnosing the link.

### The LEDs do not light

- Confirm the actual data wire matches `RPM_LED_DATA_PIN`.
- Confirm strip direction: the ESP32 must connect to DIN, not DOUT.
- Confirm that the strip has 5 V power and shares ground with the ESP32.
- Check the level shifter direction and output-enable pin, if one is installed.
- The startup flash works without SimHub; if it appears but live RPM does not,
  focus troubleshooting on the SimHub serial configuration.

### The LEDs show startup flashes but no live telemetry

- Confirm the Custom Serial Devices message is enabled and updating.
- Copy the formula directly from `simhub/custom-serial-formula.txt` without
  adding line breaks.
- Verify that the selected game exposes the required CarSettings properties.
- Confirm that SimHub is connected to the receiver's CDC port.

### TinyUSB disconnects or reconnects unexpectedly

- Use a short, high-quality USB data cable and a direct motherboard port.
- Avoid opening a serial monitor while SimHub owns the CDC interface.
- Keep the receiver power supply and USB ground stable.
- If composite USB remains unreliable on a particular PC, a robust alternative
  is native USB HID on the receiver plus a separate CP2102/CH340 USB-to-UART
  adapter dedicated to SimHub telemetry.

## Current limitations

- Pairing discovery is broadcast and normal unicast ESP-NOW traffic remains
  unencrypted. Pairing provides peer isolation and reliability, not secrecy or
  cryptographic authentication.
- Pair reset should be performed while the stored receiver is powered. If every
  reset notification is lost, its NVS record must be erased by reflashing with
  flash erase before pairing it to a different wheel.
- The receiver CDC and HID interfaces share the ESP32-S3 TinyUSB stack.
- Only direct digital inputs are implemented; there are no analog axes,
  encoders, analog paddles with calibration, or button matrix support.
- No wheel display or battery telemetry is implemented.

## AI-assisted development disclaimer

Generative AI was used to assist with the design, implementation,
documentation, and review of this project. AI-generated or AI-assisted output
can contain mistakes, incomplete assumptions, or unsafe recommendations. The
project maintainers and users remain responsible for reviewing the source,
testing it on their hardware, and verifying electrical and operational safety.

This software is provided without warranty. Do not rely on it for safety-
critical controls. Disconnect power before changing wiring, use appropriate
fusing and current-limited supplies, and test the wheel in a safe environment
before normal use.

## License and third-party software

Copyright (c) 2026 ESP32 SimHub Wireless Wheel contributors.

Original code and documentation in this repository are free software licensed
under the **GNU General Public License, version 3 or (at your option) any later
version** (`GPL-3.0-or-later`). You may redistribute and modify the project
under those terms. Distributed modified versions and derivative works must
remain available under the GPL, and distributions must provide the
corresponding source code as required by the license. See `LICENSE` for the
complete terms and warranty disclaimer.

This project builds against separately licensed open-source software. Those
dependencies are not relicensed by this repository and remain subject to their
own copyright notices and license terms:

- **FastLED 3.10.3** is distributed under the MIT License. PlatformIO downloads
  it from the dependency declared in `platformio.ini`.
- **Arduino core for ESP32**, including the Wi-Fi, ESP-NOW, USB, TinyUSB, and
  gamepad APIs used here, is distributed under its upstream licenses, primarily
  LGPL-2.1-or-later, with bundled components under their stated licenses.
- **Espressif ESP-IDF components** used by the Arduino core are distributed
  under their applicable upstream licenses, commonly Apache-2.0.

PlatformIO downloads these dependencies into its generated build directories;
they are not original project code. When redistributing source or binaries,
retain all required upstream copyright notices, license texts, attribution,
and source-code offers or access required by the applicable dependency
licenses. Consult the exact license files shipped with the dependency versions
being distributed.

SimHub is third-party software and is not included in this repository. This
project is an independent community integration and is not affiliated with or
endorsed by SimHub, Waveshare, Espressif, or the FastLED project. Product and
project names may be trademarks of their respective owners.
