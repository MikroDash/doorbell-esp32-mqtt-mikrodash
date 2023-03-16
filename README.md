# doorbell_esp32_mqtt_mikrodash_alexa

## Getting Started

This project enables an ESP32-based doorbell to communicate with MikroDash and Amazon Alexa, and send a picture of the person who rang the doorbell to Telegram. The project includes:

- ESP32 CAM
- Arcade button
- Ring 16 LEDs WS2812
- Power supply 5VDC
- OLED display 128x32 I2C
- Buzzer 12mm

## Materials

## Connection
| Component        | Pin           |
| ------------- |:-------------:|
| ESP32 CAM        | N/A |
| OLED display     | SDA, SCL |
| Arcade button    | IO2, 3.3VDC |
| Ring of LEDs     | IO13 |
| Buzzer           | IO12 |
| Power supply     | 5VDC, GND |

## 3D Models
All 3D models of the project can be found in the `3D_models` folder.

## Firmware
The firmware of the project can be found in the `firmware` folder.

## CNC Models
All CNC models of the project can be found in the `CNC_models` folder.

## Credits
- ESP32 CAM code based on [this example](https://github.com/espressif/arduino-esp32/tree/master/libraries/ESP32/examples/Camera/CameraWebServer).
- MikroDash code based on [this library](https://github.com/MikroDash/doorbell-esp32-mqtt-mikrodash).