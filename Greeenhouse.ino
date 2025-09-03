/*********
  ESP32 STA + SSE + AHTX0 + Soil/Lux (ADC)
  - ĐÈN (#1) & BƠM (#3): ON/OFF
  - QUẠT (#2): PWM 0..255 (LEDC @25kHz)
  Version: split into .ino (C++), index.html, style.css served via LittleFS
*********/
#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

// ================== Cấu hình chân & calib ==================
const int ADC_PIN_SOIL = 1;   // theo bản bạn gửi
const int ADC_PIN_AUX  = 2;

const int AirValue   = 2823;  // ADC khi khô (calib)
const int WaterValue = 1081;  // ADC khi ướt (calib)

const float LUX_MIN = 1.0f;
const float LUX_MAX = 6000.0f;

// ======= 2 MOSFET ON/OFF (đèn, bơm) + 1 MOSFET PWM (quạt) =======
const int MOSFET_PIN1 = 4;  // ĐÈN (ON/OFF)
const int MOSFET_PIN2 = 5;  // QUẠT (PWM)
const int MOSFET_PIN3 = 6;  // BƠM (ON/OFF)
const bool MOSFET_ACTIVE_LOW = false;  // nếu phần cứng kích mức LOW, đổi thành true

// ================== Wi-Fi (STA) ==================
const char* STA_SSID = "TUVE";
const char* STA_PASS = "12345678";

// ================== Global ==================
Adafruit_AHTX0 aht;
AsyncWebServer server(80);
AsyncEventSource events("/events");

float g_temp = NAN, g_humi = NAN, g_lux = NAN;
int   g_soilPct = 0;
bool  mosfetState[3] = {false,false,false}; // [0]=ĐÈN, [1]=QUẠT (không dùng), [2]=BƠM

unsigned long lastSend = 0;
const unsigned long sendPeriodMs = 1000;

// ======= PWM cho QUẠT =======
const uint8_t  FAN_CHANNEL  = 0;
const uint32_t FAN_FREQ_HZ  = 25000; // 25 kHz (êm)
const uint8_t  FAN_RES_BITS = 8;     // 0..255
uint8_t g_fanDuty = 0;

const char* PARAM_INPUT = "value";

// ================== Tiện ích ==================
int soilPercentFromADC(int val) {
  long p = map(val, AirValue, WaterValue, 0, 100);
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  return (int)p;
}
float luxFromADC(int val) {
  float lux = (val / 4095.0f) * (LUX_MAX - LUX_MIN) + LUX_MIN;
  if (lux < LUX_MIN) lux = LUX_MIN;
  if (lux > LUX_MAX) lux = LUX_MAX;
  return lux;
}
inline int levelOn()  { return MOSFET_ACTIVE_LOW ? LOW  : HIGH; }
inline int levelOff() { return MOSFET_ACTIVE_LOW ? HIGH : LOW; }

int mosfetPinById(int id) {
  switch (id) {
    case 1: return MOSFET_PIN1; // ĐÈN
    case 3: return MOSFET_PIN3; // BƠM
    default: return -1;
  }
}
void setMosfet(int id, bool on, bool broadcast=true) {
  int pin = mosfetPinById(id);
  if (pin < 0) return;
  digitalWrite(pin, on ? levelOn() : levelOff());
  if (id==1) mosfetState[0] = on;
  if (id==3) mosfetState[2] = on;
  if (broadcast) {
    const char* evt = (id==1?"r1":"r3");
    events.send(on ? "1":"0", evt, millis());
  }
}

// ===== Fan PWM helper =====
void setFanDuty(uint8_t duty, bool broadcast=true){
  g_fanDuty = duty;
  ledcWrite(FAN_CHANNEL, g_fanDuty);
  if (broadcast) {
    char buf[4]; snprintf(buf, sizeof(buf), "%u", g_fanDuty);
    events.send(buf, "fan", millis());
  }
}

void readAll() {
  int soilRaw = analogRead(ADC_PIN_SOIL);
  g_soilPct = soilPercentFromADC(soilRaw);
  int luxRaw = analogRead(ADC_PIN_AUX);
  g_lux = luxFromADC(luxRaw);

  sensors_event_t he, te;
  bool ok = aht.getEvent(&he, &te);
  if (ok) { g_temp = te.temperature; g_humi = he.relative_humidity; }
  else    { g_temp = NAN; g_humi = NAN; }

  Serial.printf("SoilADC=%d => %d%% | LightADC=%d => %.1f lx | FanDuty=%u\n",
                soilRaw, g_soilPct, luxRaw, g_lux, g_fanDuty);
  if (!isnan(g_temp)) Serial.printf("Temp=%.1f C, Humi=%.1f %%\n", g_temp, g_humi);
  else                Serial.println("Khong doc duoc AHTX0");
  Serial.println("-------------------------");
}

// ================== Wi-Fi (STA) ==================
void initSTA() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(STA_SSID, STA_PASS);
  Serial.print("Connecting to WiFi");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis()-t0 < 15000) {
    Serial.print(".");
    delay(500);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nSTA IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nSTA connect failed.");
  }
}

// ================== Setup/Loop ==================
void setup() {
  Serial.begin(115200);
  delay(300);

  if (!LittleFS.begin(true)) {
    Serial.println("[LittleFS] Mount failed"); while(true) { delay(1000); }
  }

  // ADC
  analogReadResolution(12);
  analogSetPinAttenuation(ADC_PIN_SOIL, ADC_11db);
  analogSetPinAttenuation(ADC_PIN_AUX,  ADC_11db);

  // I2C + AHT
  Wire.begin(); // nếu là S3 và dây khác SDA/SCL: Wire.begin(8,9);
  if (!aht.begin()) Serial.println("Không tìm thấy cảm biến AHTX0!");
  else              Serial.println("Khởi tạo AHTX0 thành công.");

  // ĐÈN/BƠM (ON/OFF)
  pinMode(MOSFET_PIN1, OUTPUT);
  pinMode(MOSFET_PIN3, OUTPUT);
  setMosfet(1, false, false);
  setMosfet(3, false, false);

  // QUẠT PWM
  ledcSetup(FAN_CHANNEL, FAN_FREQ_HZ, FAN_RES_BITS);
  ledcAttachPin(MOSFET_PIN2, FAN_CHANNEL);
  setFanDuty(0, false); // quạt OFF ban đầu

  // Wi-Fi
  initSTA();

  // Web routes — serve static from LittleFS
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  server.serveStatic("/style.css", LittleFS, "/style.css");

  // ĐÈN/BƠM: /api/mosfet?id=1|3&v=0|1
  server.on("/api/mosfet", HTTP_GET, [](AsyncWebServerRequest* req){
    if (!req->hasParam("id") || !req->hasParam("v")) {
      req->send(400, "text/plain", "missing id or v");
      return;
    }
    int id = req->getParam("id")->value().toInt();
    int v  = req->getParam("v")->value().toInt();
    if ((id!=1 && id!=3) || (v!=0 && v!=1)) {
      req->send(400, "text/plain", "bad params");
      return;
    }
    setMosfet(id, v==1, true);
    req->send(200, "text/plain", v==1 ? "1":"0");
  });

  // QUẠT PWM: /api/fan?value=0..255
  server.on("/api/fan", HTTP_GET, [](AsyncWebServerRequest* req){
    if (!req->hasParam(PARAM_INPUT)) {
      req->send(400, "text/plain", "missing value");
      return;
    }
    int val = req->getParam(PARAM_INPUT)->value().toInt();
    if (val < 0) val = 0;
    if (val > 255) val = 255;
    setFanDuty((uint8_t)val, true);
    req->send(200, "text/plain", "OK");
  });

  // SSE
  server.addHandler(&events);
  events.onConnect([](AsyncEventSourceClient* client){
    if (client->lastId()) {
      Serial.printf("[SSE] Reconnect, last-id: %u\n", client->lastId());
    }
    client->send("hello!", nullptr, millis(), 5000);
    // Sync trạng thái hiện tại cho client mới
    events.send(mosfetState[0]?"1":"0", "r1", millis()); // ĐÈN
    events.send(mosfetState[2]?"1":"0", "r3", millis()); // BƠM
    char buf[4]; snprintf(buf, sizeof(buf), "%u", g_fanDuty);
    events.send(buf, "fan", millis());                   // QUẠT PWM
  });

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  unsigned long now = millis();
  if (now - lastSend >= sendPeriodMs) {
    readAll();
    events.send(String(g_soilPct).c_str(), "soil", now); delay(12);
    events.send(String(g_lux, 1).c_str(), "lux", now);   delay(12);
    if (!isnan(g_temp)) events.send(String(g_temp, 1).c_str(), "temperature", now); delay(12);
    if (!isnan(g_humi)) events.send(String(g_humi, 1).c_str(), "humidity", now);
    lastSend = now;
  }
}
