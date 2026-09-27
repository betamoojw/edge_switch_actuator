# ESP32-S3-Relay-6CH hardware reference

Reviewed 2026-09-26. Board: Waveshare ESP32-S3-Relay-6CH, SKU 26756. This is a reference for the proposed actuator, not a record of physical board testing.

## Verified vendor information

| Resource | Assignment |
| --- | --- |
| MCU | ESP32-S3; dual-core LX7, up to 240 MHz |
| Wireless | 2.4 GHz Wi-Fi, Bluetooth LE; external antenna |
| Supply | 7–36 V DC terminal or 5 V / 1 A USB-C |
| Relays | Six changeover channels, COM/NO/NC; vendor maximum per channel 10 A at 250 V AC or 30 V DC |
| CH1 / CH2 / CH3 | GPIO1 / GPIO2 / GPIO41 |
| CH4 / CH5 / CH6 | GPIO42 / GPIO45 / GPIO46 |
| RGB | One WS2812, data GPIO38 |
| Buzzer | Passive, PWM GPIO21 |
| BOOT | GPIO0 |
| RS485 | Isolated; UART TX GPIO17, RX GPIO18; selectable 120-ohm termination |
| Other | RESET, USB-C, Pico-compatible expansion headers, power/TX/RX indicators |

Source: [Waveshare hardware documentation](https://docs.waveshare.com/ESP32-S3-Relay-6CH).

The [vendor Arduino guide](https://docs.waveshare.com/ESP32-S3-Relay-6CH/Arduino) describes HIGH as relay ON, LOW as OFF. Its example serial commands include a nonstandard toggle value; do not copy that command table as this product's Modbus specification.

The [schematic](https://files.waveshare.com/wiki/ESP32-S3-Relay-6CH/ESP32-S3-Relay-6CH.pdf) identifies an ESP32-S3-WROOM-1U module and TX-derived enable circuitry. Design inference: use automatic RS485 direction, without allocating a fictional DE GPIO; verify turnaround on the actual board. BOOT is pulled up and switches to ground.

## Implementation constraints and verification

These are product engineering decisions, not additional vendor specifications:

- Use a dedicated, immutable board pin profile. Do not expose pin remapping in the dashboard. Expansion functions must not share actuator pins.
- Send WS2812 frames; ordinary `digitalWrite()` cannot select its color. Reserve the PWM peripheral used by the passive buzzer independently of the RGB driver.
- Initialize all relay outputs LOW before networking or filesystem work. Validate with an oscilloscope through power-on, reset, flashing, brownout, watchdog and OTA restart. Software initialization alone cannot guarantee the electrical state before firmware runs.
- GPIO0 participates in boot selection. Runtime factory reset requires a press after application startup; BOOT held during power-on can enter the downloader. GPIO45/46 also require boot-strapping review against the actual module and board revision.
- Report relay state as **commanded output**, not verified contact position. No contact-feedback path has been established from the reviewed materials.
- Treat all six channels independently. No motor reversing, blind control or safety interlock rating is implied.
- Verify fitted flash capacity, PSRAM and module suffix before selecting partitions. The repository's 8 MB partition layout is not evidence of the board's memory configuration.
- Bench-verify buzzer usable frequency/duty limits, RGB color order and brightness, UART baud/parity combinations, automatic direction timing and termination position.
- Use a low-voltage test fixture for firmware bring-up. Product qualification must establish load derating, thermal limits, inrush handling and installation protection for its actual loads; the contact maximum is not a six-channel simultaneous-load qualification.
- Wi-Fi is the baseline IP interface. Onboard Ethernet and KNX TP hardware are not part of this board profile. RS485 is not a KNX TP interface.

## Manufacturing record to retain

For each qualified hardware revision record PCB marking, module suffix, flash/PSRAM detection, pin/polarity verification, boot traces, relay continuity results, RS485 waveform captures, supply/current measurements, firmware hash and fixture version. Preserve device identity separately from resettable user settings.

Additional resource index: [Waveshare resources](https://docs.waveshare.com/ESP32-S3-Relay-6CH/Resources). Recheck the schematic and fitted parts whenever the procurement revision changes.
