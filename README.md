# 🌿 Smart Garden IoT (ESP32-S3 + MQTT + Node-RED)

The **“Smart Garden Control and Monitoring using ESP32”** project is an IoT prototype designed to **monitor plant-growing environmental conditions in real time** and **remotely control actuators** (light, fan, pump, roof/aux load) via **MQTT** and a **Node-RED Dashboard**.  
The field device uses **ESP32-S3** as the main controller to read sensors (AHT30, PT550, soil moisture), display values locally on a **TFT ST7735A**, and optionally supports **4G connectivity using SIMCom A7680C** for deployment where Wi-Fi is unavailable.

<img width="1020" height="1020" alt="image" src="https://github.com/user-attachments/assets/9b72826d-c694-4adc-a529-9043fff212f1" />


---

## ✨ Key Features

- **Environmental monitoring (real-time)**
  - Air temperature & humidity: **AHT30 (I2C)**
  - Light level: **PT550 (Analog/ADC)**
  - Soil moisture: **Soil Moisture v1.2 (Analog/ADC, 2-point calibration)**
- **Local display**
  - **1.8" TFT ST7735A** shows current sensor values on-site
- **IoT messaging**
  - Publishes sensor readings to an **MQTT Broker** periodically
- **Remote control**
  - **Node-RED Dashboard** sends MQTT commands to ESP32 to drive a **12V 4-channel relay**
- **Expandable connectivity**
  - **SIMCom A7680C (UART)** enables cellular/4G data for MQTT communication (future / optional)

---

## 🧱 System Architecture (3-layer IoT Model)

1. **Device/Edge (ESP32-S3)**  
   Reads sensors, processes/converts raw data, controls relays via GPIO, updates TFT display.

2. **Server/Broker (Raspberry Pi 4)**  
   Runs the MQTT Broker (e.g., Mosquitto) and Node-RED for data routing and dashboard UI.

3. **Application/UI (Node-RED Dashboard)**  
   Displays gauges/charts and provides switches for remote control via a web browser.

---

## 🔧 Hardware List

- **ESP32-S3** (main controller)
<img width="354" height="287" alt="image" src="https://github.com/user-attachments/assets/d96d600d-6446-4281-8107-492f8d760a94" />

- **LM2596S 3A** buck converter (stable power supply)
<img width="311" height="207" alt="image" src="https://github.com/user-attachments/assets/14966adc-6b06-4cd1-b904-2ca154903efd" />

- **1.8" TFT ST7735A** (local display)
<img width="313" height="313" alt="image" src="https://github.com/user-attachments/assets/42bc0e79-f60f-4e44-965a-711b3c7a9039" />

- **SIMCom A7680C** (4G/LTE Cat-1 module, optional/future)
- **12V 4-channel relay module** (actuator switching)
- **8-slot terminal block** (wiring and power distribution)
- **PT550 Analog Ambient Light Sensor V2**
- **WeAct AHT30** (air temperature/humidity sensor)
- **Soil moisture sensor v1.2**
- **Raspberry Pi 4** (MQTT + Node-RED server)

---

<img width="1920" height="2560" alt="image" src="https://github.com/user-attachments/assets/c29318d5-3c56-4650-b1b8-ddd0e69d5ecc" />


## 🔌 Wiring / Pin Mapping (Based on the Firmware)

### 1) AHT30 (I2C)
Firmware uses: `Wire.begin(8, 9)`  
- **SDA → GPIO8**
- **SCL → GPIO9**
- VCC → 3.3V
- GND → GND

### 2) TFT ST7735A (SPI)
Firmware pin definitions:
- **CS → GPIO10**
- **RST → GPIO3**
- **DC → GPIO18**
- **SCLK → GPIO13**
- **MOSI → GPIO11**
- VCC → 3.3V
- GND → GND

### 3) Analog Sensors (ADC)
- PT550 (Light) **OUT → GPIO17**
- Soil moisture v1.2 **OUT → GPIO16**
- VCC → 3.3V, GND → GND (common ground)

> Note: Soil moisture requires **2-point calibration** using `AirValue` and `WaterValue` to map ADC values into **0–100%**.

### 4) Relay Module (Actuator Control)
GPIO outputs used for relay channels:
- **GPIO4** → Light relay
- **GPIO5** → Fan relay
- **GPIO6** → Pump relay
- **GPIO7** → Roof/Aux relay

Relay inputs IN1–IN4 connect to the GPIO pins above.  
Make sure the relay control ground is **shared with ESP32 ground**.

### 5) SIMCom A7680C (UART, Optional)
UART pins:
- **MODEM_RX = GPIO38** (ESP32 receives from modem)
- **MODEM_TX = GPIO37** (ESP32 transmits to modem)

UART wiring rule: **cross TX/RX** and connect **common GND**.  
Cellular modems require **stable power** and can draw high peak current.

---

## 📡 MQTT Topics & Payloads

### Sensor Topics (ESP32 publishes)
- `esp32/temperature` → temperature (string numeric)
- `esp32/humidity` → humidity (string numeric)
- `esp32/lux` → light level (string numeric)
- `esp32/soil` → soil moisture percent (string numeric)

### Control Topic (ESP32 subscribes)
- `esp32/output`

Example control payloads:
- `on_led` / `off_led`
- `on_fan` / `off_fan`
- `on_pumb` / `off_pumb`
- `on_roof` / `off_roof`

---

## 🖥️ Node-RED Dashboard (Suggested Flow)

### Monitoring flow
- `mqtt in (esp32/temperature)` → convert `msg.payload` to number → `ui_gauge` + `ui_chart`
- Repeat similarly for humidity / lux / soil

<img width="605" height="315" alt="image" src="https://github.com/user-attachments/assets/f4639b7a-4318-4408-b3e9-ce8d4f9e540c" />


### Control flow
- `ui_switch` (Light/Fan/Pump/Roof) → `function/change node` to create payload (`on_led`, `off_led`, etc.) → `mqtt out (esp32/output)`


<img width="605" height="195" alt="image" src="https://github.com/user-attachments/assets/7d4968ad-b969-48d3-ac19-3d91ac5e29ea" />

---

## 🚀 Quick Setup

### A) Server Side (Raspberry Pi 4)
1. Install an MQTT Broker (e.g., Mosquitto)
2. Install Node-RED + Node-RED Dashboard
3. Create MQTT in/out flows using the topics listed above

### B) ESP32-S3 Side
1. Open the `.ino` file in Arduino IDE
2. Configure:
   - `apn` (for cellular) if using A7680C
   - `mqtt_server` and `mqtt_port`
3. Install required Arduino libraries:
   - PubSubClient
   - TinyGSM (if using modem)
   - Adafruit_AHTX0
   - Adafruit_ST7735 & Adafruit_GFX
4. Upload firmware and verify MQTT data + dashboard updates

---

## ⚠️ Technical Notes

- Analog readings (light/soil) can be affected by noise when relays switch loads. Use clean wiring, proper grounding, and separate power paths for logic vs loads if possible.
- If control response feels delayed, reduce blocking delays and prefer a **non-blocking loop** so MQTT can be serviced continuously.

---

## 👥 Team
- Nguyễn Trương Tuấn Anh — N22DCDK053  
- Tống Anh Kiệt — N22DCDK005  


---
