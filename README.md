# MATRIX PROJECT — MFMCF Display Controller

An ESP32-based wireless LED display control system built for Mountain of Fire and Miracles Campus Fellowship.

## Features
- Real-time clock display (Date + Time) from DS3231 RTC
- Named countdown timer with bold large display
- Scrolling text announcements (left to right)
- Configurable TIME UP alert
- 12-hour and 24-hour clock modes
- Browser-based web control interface
- Android app control via Flutter
- Self-hosted WiFi Access Point — no internet required

## Hardware
- ESP32 DevKit
- 2x P10 LED Matrix panels (HUB12, 64x16px total)
- DS3231 RTC module
- 5V 20A SMPS

## Pin Configuration (DMD32.h)
| Signal | GPIO |
|--------|------|
| OE     | 22   |
| A      | 19   |
| B      | 21   |
| CLK    | 18   |
| SCLK   | 2    |
| DATA   | 23   |
| RTC SDA| 32   |
| RTC SCL| 33   |

## Libraries Required
- DMD32
- RTClib (Adafruit)
- ArduinoJson
- ESPAsyncWebServer (mathieucarbou fork)
- AsyncTCP (mathieucarbou fork)

## Access Point
- SSID: MATRIX-PROJECT
- IP: 192.168.4.1

## Built by
Alioth — Electrical & Electronics Engineering Student.
