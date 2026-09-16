# ESP32 SimHub Wireless Wheel, USB Shifter, and USB Pedals

A two-board wireless sim-racing wheel prototype built for the Waveshare
ESP32-S3-Zero. The wheel sends button states to a USB receiver over ESP-NOW,
while the receiver sends SimHub RPM telemetry back to the wheel.

The repository also contains an independent, single-board USB firmware for a
Logitech six-speed H-pattern shifter. The shifter firmware does not use SimHub,
Wi-Fi, ESP-NOW, LEDs, or either wheel board.

A second independent USB firmware provides three calibrated analog axes for
gas, brake, and clutch pedals. It uses one ESP32-S3-Zero and does not depend on the
wheel, receiver, shifter, SimHub, or wireless link.

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
- A separate generic USB HID/DirectInput shifter target with gears 1-6 and
  reverse exposed as seven game-controller buttons.
- Configurable analog gate thresholds, hysteresis, filtering, stability timing,
  and USB serial diagnostics for shifter tuning.
- A standalone three-axis USB HID pedal target with independent gas, brake, and clutch
  calibration, filtering, endpoint dead zones, and serial diagnostics.

## What you need

### Hardware

- Two Waveshare ESP32-S3-Zero boards:
  - One for the wireless wheel.
  - One for the PC-connected receiver.
- One additional Waveshare ESP32-S3-Zero if building the independent USB
  shifter target.
- One additional Waveshare ESP32-S3-Zero if building the independent USB pedal
  target.
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
| `src/shifter/main.cpp` | Standalone USB H-pattern shifter firmware. |
| `src/pedals/main.cpp` | Standalone USB gas, brake, and clutch pedal firmware. |
| `include/wheel_config.h` | User-adjustable pins, LED settings, and timing. |
| `include/shifter_config.h` | Shifter pins, ADC thresholds, filtering, and timing. |
| `include/shifter_input.h` | Hardware-independent H-pattern classification and HID mapping. |
| `include/pedals_config.h` | Pedal ADC pins, endpoint calibration, filtering, and timing. |
| `include/pedals_input.h` | Hardware-independent pedal calibration and HID scaling. |
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

## Standalone Logitech H-pattern shifter

### Compatibility and HID mapping

The `shifter` environment appears to Windows as a generic USB HID game
controller. This standards-based DirectInput-style device is preferable to
emulating a specific Logitech product or an Xbox/XInput controller: most PC
driving simulators can bind a separate generic controller, and no vendor driver
is required.

| Lever position | HID output |
|---|---:|
| Neutral | No buttons |
| Gear 1 | Button 1 held |
| Gear 2 | Button 2 held |
| Gear 3 | Button 3 held |
| Gear 4 | Button 4 held |
| Gear 5 | Button 5 held |
| Gear 6, reverse switch low | Button 6 held |
| Gear 6, reverse switch high | Button 7 held (reverse) |

Only one gear button can be active. The X and Y sensor signals are interpreted
inside the firmware and are not exposed as game axes because there is no common
game-facing standard for raw H-pattern position axes.

This mapping is suitable for Windows games that provide separate bindings for
Gear 1 through Gear 6 and Reverse, including many circuit, rally, trucking, and
driving simulators. Bind all seven controls in each game's controller settings.
Some games only enable H-pattern shifting for cars that actually have an
H-pattern transmission, and some older or arcade-oriented games do not accept a
second controller. Generic DIY USB HID does not provide PlayStation or Xbox
console compatibility; those consoles require an authenticated/licensed device
or compatible adapter.

### Shifter wiring

The default pin assignments are:

| Logitech shifter signal | ESP32-S3-Zero pin | Input mode |
|---|---:|---|
| Reverse switch | GPIO 4 | Digital, active-high, internal pull-down |
| X position | GPIO 5 | 12-bit ADC |
| Y position | GPIO 6 | 12-bit ADC |
| Ground | GND | Common ground |

**Electrical warning:** ESP32-S3 GPIOs are not 5 V tolerant. The reverse, X,
and Y inputs must remain between 0 V and 3.3 V at all times, and the shifter and
ESP32 must share ground. If using unmodified original Logitech electronics or a
5 V supply, measure the outputs and add suitable voltage dividers/buffering
before connecting them. Do not rely on firmware to protect an over-voltage pin.

### Build, upload, and verify the shifter

Build and upload only the standalone target:

```sh
platformio run -e shifter
platformio run -e shifter -t upload
```

After flashing, unplug and reconnect the board so Windows enumerates `ESP32
H-Pattern Shifter`. Press **Win+R**, run `joy.cpl`, select the shifter, and open
**Properties**. Confirm that neutral releases every button, positions 1-6 hold
buttons 1-6, and selecting the sixth-gear position with the reverse switch high
holds button 7 instead of button 6.

### Calibrate from the seven shifter positions

You do not need to calculate any thresholds. Open the shifter's USB serial port
at 115200 baud. Every 250 ms it prints a line similar to:

```text
rawX=820 rawY=760 filteredX=823 filteredY=765 reverse=0 candidate=1 reported=1 mask=0x01
```

On macOS in this project, use:

```sh
platformio device monitor -e shifter
```

The `shifter` environment is configured to select `/dev/cu.usbmodem*` with DTR
enabled and RTS disabled. This avoids PlatformIO silently opening an unrelated
Bluetooth or debug-console port. The monitor should immediately print `ESP32
H-Pattern Shifter diagnostics connected`; it is not necessary to reset the
board after opening the port. Exit the monitor with **Ctrl+C** before uploading.

With the monitor open, type `C` in each shifter position to print a compact
`{rawX, rawY}` pair that can be copied directly into the matching calibration
entry. Type `H` or `?` to print the available commands.

If the port is not found after uploading, unplug and reconnect the board, wait
a few seconds for TinyUSB to enumerate, and list the available ports with:

```sh
platformio device list
```

On Windows, choose the shifter's new `COM` port in PlatformIO's serial monitor
and use 115200 baud. The macOS wildcard in `platformio.ini` can be removed or
overridden with the correct `COM` port when monitoring from Windows.

1. Hold the lever steadily in neutral and positions 1 through 6. For each
   position, record the displayed `rawX` and `rawY`; using the approximate
   middle of several readings is better than copying an occasional extreme.
2. Enter those seven X/Y pairs as `NEUTRAL`, `GEAR_1`, through `GEAR_6` in
   `include/shifter_config.h`.
3. Rebuild and upload the `shifter` environment. The firmware automatically
   averages the three X columns and three Y rows, detects whether either axis is
   reversed, and places each threshold halfway between adjacent positions.
4. Check the recurring `calibration` diagnostic line to see the calculated axis
   directions and four boundaries, then verify every position in Windows
   `joy.cpl`.

For example, a measured first-gear position of X=812 and Y=735 is entered as:

```cpp
constexpr Position GEAR_1 = {812, 735};
```

The calibration is rejected at compile time if readings exceed the ADC range,
the left/center/right or forward/neutral/back positions are not distinguishable,
or adjacent positions are too close for the selected hysteresis. Increase
`AXIS_HYSTERESIS` modestly if a correctly calibrated gate chatters; reduce it if
the compiler reports insufficient separation or a gate is difficult to leave.

`ADC_FILTER_DIVISOR` controls smoothing and `GEAR_STABILITY_MS` controls how
long a candidate must remain stable before USB output changes. The defaults are
intended to reject sensor noise without making shifts feel delayed. USB serial
diagnostics may remain open while testing and are not required during gameplay.

## Standalone gas, brake, and clutch pedals

### Pedal wiring

The `pedals` environment is a separate generic USB HID game controller. Gas is
reported as X/Axis 1, brake as Y/Axis 2, and clutch as Z/Axis 3; no buttons,
other axes, or hat are advertised. The defaults use GPIO 9 for gas, GPIO 10 for
brake, and GPIO 11 for clutch.

The shifter and pedals have different compile-time USB identities so games do
not combine them as identical `TinyUSB HID` controllers:

| Environment | USB product name | USB VID:PID |
|---|---|---|
| `shifter` | Shifter | `303A:4010` |
| `pedals` | Pedals | `303A:4011` |

The product names and product IDs are assigned before TinyUSB starts. The
firmware also replaces Arduino's default `TinyUSB HID` interface string with
the matching `Shifter` or `Pedals` name because some games display the HID
interface name instead of the parent USB product name.

For each ordinary three-wire potentiometer, wire:

```text
ESP32 3.3 V ---- potentiometer outside terminal
ESP32 GND ------ potentiometer other outside terminal
ESP32 GPIO ----- potentiometer center/wiper terminal
```

| Control | ESP32-S3-Zero pin | USB HID output |
|---|---:|---|
| Gas wiper/signal | GPIO 9 | X / Axis 1 |
| Brake wiper/signal | GPIO 10 | Y / Axis 2 |
| Clutch wiper/signal | GPIO 11 | Z / Axis 3 |
| All potentiometer supplies | 3.3 V | — |
| All potentiometer grounds | GND | — |

The two outside potentiometer terminals may be swapped. Calibration detects
whether raw values rise or fall as the pedal is pressed. The wiper must always
remain between 0 V and 3.3 V. **Never connect 5 V to an ESP32-S3 input.** This
wiring is intended for potentiometers or 3.3 V-compatible analog sensors. A
load cell requires a suitable amplifier whose output is limited to 0-3.3 V.

### Build, upload, and calibrate the pedals

Build and upload the standalone pedal target:

```sh
platformio run -e pedals
platformio run -e pedals -t upload
platformio device monitor -e pedals
```

The monitor runs at 115200 baud and prints both raw ADC readings and calculated
percentages every 250 ms. It is configured for `/dev/cu.usbmodem*` on macOS in
the same way as the shifter monitor. Type `C` to print a compact snapshot.

1. Release all three pedals and record several stable `rawGas`, `rawBrake`, and
   `rawClutch` readings.
2. Fully press all three pedals and record several stable readings again.
3. Enter the released and pressed values as `GAS_RELEASED_RAW`,
   `GAS_PRESSED_RAW`, `BRAKE_RELEASED_RAW`, `BRAKE_PRESSED_RAW`,
   `CLUTCH_RELEASED_RAW`, and `CLUTCH_PRESSED_RAW` in `include/pedals_config.h`.
4. Rebuild and upload `pedals`, then unplug and reconnect the board.
5. Open Windows `joy.cpl` or the game's input-binding screen and verify that
   each axis travels independently from 0% when released to 100% when pressed.

After installing firmware with a changed USB identity, unplug and reconnect the
board. If Windows or a game still displays a previously cached `TinyUSB HID`
name, remove that old controller in Device Manager (enable **View > Show hidden
devices** if necessary), reconnect the board, and restart the game. Existing
control bindings may need to be assigned again because the PID intentionally
makes each target a distinct controller.

The firmware descriptors are limited to controls that physically exist: the
shifter advertises seven buttons, while the pedals advertise three axes. They no
longer use Arduino's generic descriptor that Windows describes as `6 axis 32
button device with hat switch`.

No registry edits, PowerShell scripts, custom drivers, or INF files are needed.
The names and unique identities are supplied entirely by the firmware. A game
that ignores USB product/interface strings may still display a generic
capability-based label, but the distinct product IDs ensure that shifter and
pedals remain separate controllers with separate bindings.

`END_DEAD_ZONE` reserves a small number of raw ADC counts at both calibrated
ends so mechanical variation still reaches exact 0% and 100%. Increase it
slightly if an axis does not settle at an endpoint. `ADC_FILTER_DIVISOR`
controls smoothing; larger values are smoother but respond more slowly.

## Build environments

The project contains four ESP32-S3 PlatformIO environments plus the native
test environment:

| Environment | Install on | Function |
|---|---|---|
| `wheel` | Battery-powered wheel board | Reads buttons, drives LEDs, and exchanges ESP-NOW packets. |
| `receiver` | USB-connected board | Receives buttons as USB HID and forwards SimHub telemetry. |
| `shifter` | USB-connected shifter board | Reads Logitech H-pattern axes/reverse and reports seven generic HID buttons. |
| `pedals` | USB-connected pedal board | Reads gas, brake, and clutch potentiometers and reports three calibrated HID axes. |

All four ESP32 environments are based on `esp32-s3-devkitc-1` with settings
adjusted for the Waveshare ESP32-S3-Zero's 4 MB flash and 2 MB OPI PSRAM. The
receiver, shifter, and pedals use TinyUSB device mode so CDC serial and HID can
coexist.

Packets use protocol version 6, which combines gear-aware telemetry with the
expanded button and POV input payload. Flash both boards after updating;
version 6 firmware intentionally rejects packets from older firmware.

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
platformio run -e shifter
platformio run -e pedals
platformio run -e wheel -t upload
platformio run -e receiver -t upload
platformio run -e shifter -t upload
platformio run -e pedals -t upload
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
'T;' + format(isnull([DataCorePlugin.GameData.NewData.Rpms],0),'0') + ';' + format(isnull([DataCorePlugin.GameData.NewData.CarSettings_CurrentDisplayedRPMPercent],0),'0.00') + ';' + format(isnull([DataCorePlugin.GameData.NewData.CarSettings_RedLineRPM],0),'0') + ';' + format(isnull([DataCorePlugin.GameData.NewData.CarSettings_RedLineDisplayedPercent],0),'0.00') + ';' + format(isnull([DataCorePlugin.GameData.NewData.CarSettings_MaxRPM],0),'0') + ';' + format(isnull([DataCorePlugin.GameData.NewData.CarSettings_MinimumShownRPM],0),'0') + ';' + format(isnull([DataCorePlugin.GameData.NewData.CarSettings_RPMRedLineReached],0),'0') + ';' + format(isnull([DataCorePlugin.GameData.NewData.CarSettings_RPMShiftLight1],0),'0.000') + ';' + format(isnull([DataCorePlugin.GameData.NewData.CarSettings_RPMShiftLight2],0),'0.000') + ';' + format(isnull([DataCorePlugin.GameData.CarSettings_CurrentGearRedLineRPM],0),'0') + ';' + isnull([DataCorePlugin.GameData.NewData.Gear],'N') + '\n'
```

The same expression is kept as a copy-friendly single line in
`simhub/custom-serial-formula.txt`.

The serial message format is:

```text
T;<rpm>;<display percent>;<redline rpm>;<redline percent>;<maximum>;<minimum>;<redline reached>;<shift 1 progress>;<shift 2 progress>;<current gear redline rpm>;<gear>\n
```

Do not configure a serial-monitor application on the receiver's CDC port while
SimHub is using it. Only one application should own the port at a time.

## RPM LED behavior

The LED bar smoothly fills from left to right using current RPM relative to
SimHub's `CarSettings_CurrentGearRedLineRPM` value:

- At or below 50% of the current gear's redline RPM, every LED is off.
- Between 50% and 90% of current-gear redline, LEDs progressively illuminate
  across the whole strip.
- At or above 90% of current-gear redline, the whole strip is illuminated.
- The strip retains fixed green, yellow, and red thirds as it fills.
- `CarSettings_RPMRedLineReached` overrides normal rendering and flashes the
  full strip blue.

The lower bound can be adjusted with `RPM_LED_FILL_START_PERCENT`. The upper
bound is current-gear redline minus `RPM_LED_FULL_BELOW_REDLINE_PERCENT`; its
default of 10 means the strip is full at 90% of current-gear redline. Both are
configured in `include/wheel_config.h`.

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
8. Confirm that the green, yellow, and red sections progressively fill between
   50% and 90% of the current gear's redline RPM.
9. Confirm that the strip flashes blue when redline is reported.

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
- The wireless wheel targets only implement direct digital controls; they do
  not implement analog axes, encoders, analog paddles with calibration, or a
  button matrix. The independent shifter and pedal targets do read analog
  sensors, but their inputs are not combined into the wireless receiver.
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
