# IoT Climate Monitoring Station
## Overview
This project is an IoT device developed to monitor indoor climate parameters (temperature, humidity, pressure, and CO₂ levels) and provide wireless charging (10W). The device saves data locally on a microSD card, sends it to the ThingSpeak server for visualization, and provides visual feedback through RGB LEDs and an OLED display.
The project was developed as part of a university assignment at TalTech in 2025 by Juri Goldman, Artur Grigorjan, and Andrei Litvinenko.
Features

Climate Monitoring: Measures temperature, humidity, pressure (BME280 sensor), and CO₂ (SCD41 sensor).
Data Storage: Logs data to a microSD card in CSV format.
Cloud Integration: Sends data to ThingSpeak every 5 minutes for real-time visualization.
Wireless Charging: Supports 10W Qi wireless charging.
Visual Feedback: OLED display (0.96") shows climate data; RGB LEDs (WS2812B) indicate if parameters are out of range.
Backup Power: 18650 battery ensures operation during power outages (up to 4 hours).

## Hardware Components

Microcontroller: ESP32 WROOM-32 (Wi-Fi, Bluetooth, I²C, SPI).
Sensors: BME280 (temperature, humidity, pressure), SCD41 (CO₂).
Storage: microSD module (SPI).
Display: 0.96" OLED (I²C).
LEDs: WS2812B (7 green LEDs on D2, 3 white LEDs on D4 for alerts).
Power: USB-C (HUSB238), 18650 battery (TP4056, MT3608, MOSFET).
Wireless Charging: 15W Qi module (operates at 10W with 9V input).

## Software

Environment: Arduino IDE 2.3.2.
Language: C++.
Libraries:
Adafruit_BME280.h, SparkFun_SCD4x_Arduino_Library.h (sensors).
Adafruit_SSD1306.h, Adafruit_GFX.h (OLED).
Adafruit_NeoPixel.h (RGB LEDs).
SD.h (microSD).
WiFi.h, NTPClient.h (Wi-Fi and time synchronization).
ThingSpeak.h (cloud integration).


## Configuration:
Wi-Fi: SSID "own wifi name", password "own wifi parool".
ThingSpeak: Channel ID 2956854, Write API Key "25A2ZVPHPF290UK5".
NTP: UTC+3 (EEST), server "pool.ntp.org".



## Setup Instructions

Clone this repository: git clone https://github.com/JuriGoldman/IoT-Climate-Monitoring-Station.git.
Open Weather_Station_with_Time_and_ThingSpeak_Final.ino in Arduino IDE.
Install the required libraries via the Library Manager.
Connect the ESP32 board and upload the code.
Monitor the Serial output (115200 baud) for debugging.
Place a phone on the charging pad to test wireless charging.

## Usage

The device measures climate parameters every minute.
Data is saved to the microSD card in CSV format.
Every 5 minutes, data is sent to ThingSpeak for visualization.
The OLED display shows real-time climate data.
RGB LEDs blink if parameters are out of range (temperature: 18–26°C, humidity: 40–60%, pressure: 990–1010 hPa, CO₂: 400–1000 ppm).

## Challenges Faced

Incorrect voltage (5V instead of 9V) for the Qi module caused it to burn out.
A capacitor (100 µF, 16V) exploded during backup power testing, damaging the ESP32.
The 3D model required multiple iterations to eliminate loose fits and ensure structural stability.

## Future Improvements

Integrate MQTT protocol for more robust cloud communication.
Improve power management (e.g., separate stabilizers for charging module).
Optimize the enclosure design for better heat dissipation.

## License
This project is licensed under the MIT License.
