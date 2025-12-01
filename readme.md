# BusMonitor — Real-time SEPTA Bus ETA Display on ESP32 / TTGO T-Display

A simple ESP32-based gadget that shows the next bus arrival time (ETA) for a given SEPTA route + stop — as a large, green MM:SS countdown on a 240 × 135 TFT display (TTGO T-Display). Perfect for your front door or kitchen counter: glance and know when the next bus is coming.

## 🚍 What it does

- Connects to Wi-Fi and queries SEPTA’s real-time API for a given route / direction / stop.  
- Determines the “next bus” arrival time (as a Unix epoch), then computes a countdown (minutes + seconds) from “now”.  
- Displays the countdown in big, bright text on a small 240×135 TFT (TTGO T-Display).  
- If no upcoming bus is scheduled, displays `--:--`.  

## Features

- Fully self-contained: only needs an ESP32 + TTGO T-Display (no computer or smartphone required once deployed).  
- Easy to configure for ANY SEPTA bus route + stop.  
- Uses NTP to keep ESP32’s clock roughly correct for countdown accuracy.  
- Refreshes ETA every 10 seconds; display updates every second for smooth countdown.  

## What you need (hardware)

- ESP32 development board (e.g. TTGO T-Display)  
- Built-in 240×135 ST7789 TFT (as on TTGO T-Display)  
- USB cable or other 3.3 V power source  
- Computer + Arduino IDE (or similar) to upload firmware  

## Software / Library Dependencies

- Arduino core for ESP32  
- `WiFi.h` (built into ESP32 core)  
- `HTTPClient.h`  
- `ArduinoJson.h` (v6 or later)  
- `time.h` (for NTP / epoch time)  
- `TFT_eSPI.h` (configured for TTGO T-Display / ST7789 240×135)  

## Configuration (before compiling / flashing)

In `BusMonitor.ino`, locate the “Configuration” section and set:

```cpp
const char* ssid     = "Your-WiFi-SSID";
const char* password = "Your-WiFi-Password";

const String ROUTE_ID     = "48";    // Your SEPTA bus route
const String DIRECTION_ID = "0";     // 0 or 1 depending on direction
const String STOP_ID      = "10266"; // Your stop ID



You can find your STOP_ID by inspecting the URL when viewing that stop on the official SEPTA website.
```

##Usage

Clone or download the repository.

Install required Arduino/ESP32 libraries (e.g. via Arduino IDE Library Manager).

Paste in your Wi-Fi credentials and SEPTA parameters.

Build & upload to your ESP32/TTGO board.

After reboot, the display will show a large countdown to the next bus (or --:-- if none scheduled).


##How it works (internals, briefly)

On setup, the ESP32 connects to Wi-Fi and syncs time via NTP.

Every SEPTA_UPDATE_INTERVAL (default 10 s), it calls the SEPTA “trip-update” API for your route to find upcoming arrivals at your configured stop.

It picks the earliest arrival and stores the Unix-epoch ETA.

Independently, every second, the display is refreshed, showing a MM:SS countdown (or --:-- if no bus).


##Extension / Customization Ideas

Add seat-availability / crowding indicator (color-coded text / background) using SEPTA’s “TransitView” API.

Add route number, direction, or bus ID to the display.

Use a larger or different display (e.g. 2.8" TFT, OLED) for more info.

Add deep-sleep between updates, for low-power outdoor deployment (with battery / solar).

Expand to monitor multiple stops / routes (multi-page / scrollable UI).


##License

This project is released under the MIT License — see LICENSE for details.

Disclaimer: This project is not affiliated with SEPTA. Accuracy depends entirely on SEPTA’s real-time data feed; schedule changes, missing GPS on a bus, or feed interruptions may lead to incorrect or missing ETA.


##About / Credits

Inspired by [grandpasquarepants]' Bus Vigilante repo: https://github.com/grandpasquarepants/ESP32-SEPTA-Bus-Monitor 

Code largely written by GPT5.1-Thinking; tweaked and alpha-tested by M. Eric Carr (2025)


Written / maintained by the repository author (2025).

