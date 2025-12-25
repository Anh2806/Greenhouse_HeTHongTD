#define TINY_GSM_MODEM_SIM7600

#include <PubSubClient.h>
#include <TinyGsmClient.h>
#include <Adafruit_AHTX0.h>
#include <Wire.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_GFX.h>
#include <SPI.h>

#define SerialMon Serial

#define MODEM_RX 38
#define MODEM_TX 37
HardwareSerial SerialAT(1);

const char apn[]      = "v-internet";
const char gprsUser[] = "";
const char gprsPass[] = "";

const char* mqtt_server = "kiettong.freedynamicdns.org";
const int   mqtt_port   = 1883;

// ====== Topics ======
const char* topicCmd  = "esp32/output";
const char* tTemp     = "esp32/temperature";
const char* tHumi     = "esp32/humidity";
const char* tLux      = "esp32/lux";
const char* tSoil     = "esp32/soil";

TinyGsm modem(SerialAT);
TinyGsmClient gsmClient(modem);
PubSubClient mqtt(gsmClient);

#define TFT_CS   10
#define TFT_RST  3
#define TFT_DC   18
#define TFT_SCLK 13
#define TFT_MOSI 11
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

Adafruit_AHTX0 aht;

const int ADC_PIN_LUX  = 17;
const int ADC_PIN_SOIL = 16;

const float LUX_MIN = 1.0f;
const float LUX_MAX = 6000.0f;
const int AirValue   = 2823;
const int WaterValue = 1081;

const int ledPin  = 4;
const int fanPin  = 5;
const int pumbPin = 6;
const int roofpin = 7;

unsigned long lastMsg = 0;
float temp1, humidity1, glux, gsoil;

float luxFromADC(int val) {
  float lux = (val / 4095.0f) * (LUX_MAX - LUX_MIN) + LUX_MIN;
  if (lux < LUX_MIN) lux = LUX_MIN;
  if (lux > LUX_MAX) lux = LUX_MAX;
  return lux;
}

int soilPercentFromADC(int val) {
  long p = map(val, AirValue, WaterValue, 0, 100);
  if (p < 0) p = 0;
  if (p > 100) p = 100;
  return (int)p;
}

void printText(const char* text, uint16_t color, int x, int y, int textSize) {
  tft.setCursor(x, y);
  tft.setTextColor(color);
  tft.setTextSize(textSize);
  tft.setTextWrap(false);
  tft.print(text);
}

void callback(char* topic, byte* message, unsigned int length) {
  String messageTemp;
  for (unsigned int i = 0; i < length; i++) messageTemp += (char)message[i];

  SerialMon.print("MQTT ["); SerialMon.print(topic); SerialMon.print("] ");
  SerialMon.println(messageTemp);

  if (String(topic) == topicCmd) {
    if (messageTemp == "on_led")      digitalWrite(ledPin, HIGH);
    else if (messageTemp == "off_led") digitalWrite(ledPin, LOW);
    else if (messageTemp == "on_fan")  digitalWrite(fanPin, HIGH);
    else if (messageTemp == "off_fan") digitalWrite(fanPin, LOW);
    else if (messageTemp == "on_pumb") digitalWrite(pumbPin, HIGH);
    else if (messageTemp == "off_pumb")digitalWrite(pumbPin, LOW);
    else if (messageTemp == "on_roof") digitalWrite(roofpin, HIGH);
    else if (messageTemp == "off_roof")digitalWrite(roofpin, LOW);
  }
}

bool ensureGprs() {
  if (!modem.isNetworkConnected()) {
    SerialMon.println("Waiting for network...");
    if (!modem.waitForNetwork(60000L)) return false;
    SerialMon.println("Network OK");
  }
  if (!modem.isGprsConnected()) {
    SerialMon.println("Connecting GPRS...");
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) return false;
    SerialMon.println("GPRS OK");
  }
  return true;
}

void reconnectMqtt() {
  while (!mqtt.connected()) {
    SerialMon.println("Attempting MQTT connection...");
    String clientId = "ESP32-A7680C-";
    clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

    if (mqtt.connect(clientId.c_str())) {
      SerialMon.println("MQTT connected");
      mqtt.subscribe(topicCmd);
    } else {
      SerialMon.print("MQTT failed, rc=");
      SerialMon.println(mqtt.state());
      delay(3000);
    }
  }
}

void setup() {
  SerialMon.begin(115200);
  delay(200);
  SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(500);

  SerialMon.println("Restarting modem...");
  modem.restart();
  SerialMon.print("Modem Info: ");
  SerialMon.println(modem.getModemInfo());

  Wire.begin(8, 9);
  if (!aht.begin(&Wire)) {
    SerialMon.println("Could not find AHT30!");
    while (1) delay(10);
  }
  SerialMon.println("AHT30 found");

  // Outputs
  pinMode(ledPin, OUTPUT);  digitalWrite(ledPin, LOW);
  pinMode(fanPin, OUTPUT);  digitalWrite(fanPin, LOW);
  pinMode(pumbPin, OUTPUT); digitalWrite(pumbPin, LOW);
  pinMode(roofpin, OUTPUT); digitalWrite(roofpin, LOW);

  // TFT
  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST7735_BLACK);
  printText("Temp: ", ST7735_WHITE, 4, 1, 1);
  printText("Humi: ", ST7735_WHITE, 4, 12, 1);
  printText("Soil: ", ST7735_WHITE, 4, 23, 1);
  printText("Lux : ", ST7735_WHITE, 4, 34, 1);

  // MQTT
  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(callback);
}

void loop() {
  if (!ensureGprs()) {
    SerialMon.println("GPRS not ready, retry...");
    delay(2000);
    return;
  }

  if (!mqtt.connected()) reconnectMqtt();
  mqtt.loop();

  unsigned long now = millis();
  if (now - lastMsg > 5000) {
    lastMsg = now;

    sensors_event_t event_humidity, event_temp;
    aht.getEvent(&event_humidity, &event_temp);
    temp1 = event_temp.temperature;
    humidity1 = event_humidity.relative_humidity;

    int soilRaw = analogRead(ADC_PIN_SOIL);
    gsoil = soilPercentFromADC(soilRaw);
    int luxRaw = analogRead(ADC_PIN_LUX);
    glux = luxFromADC(luxRaw);

    char tempString[12];
    dtostrf(temp1, 1, 2, tempString);
    mqtt.publish(tTemp, tempString);

    tft.fillRect(40, 2, 120, 10, ST7735_BLACK);
    tft.setCursor(40, 2);
    tft.setTextColor(ST7735_WHITE);
    tft.setTextSize(1);
    tft.setTextWrap(false);
    tft.print(tempString);
    tft.print((char)247);
    tft.print("C");  

    char humidityString[12];
    dtostrf(humidity1, 1, 2, humidityString);
    mqtt.publish(tHumi, humidityString);

    tft.fillRect(40, 12, 120, 10, ST7735_BLACK);
    tft.setCursor(40, 12);
    tft.setTextColor(ST7735_WHITE);
    tft.setTextSize(1);
    tft.setTextWrap(false);
    tft.print(humidityString);
    tft.print("%");
 

    char luxString[12];
    dtostrf(glux, 1, 2, luxString);
    mqtt.publish(tLux, luxString);

    tft.fillRect(40, 23, 120, 10, ST7735_BLACK);
    tft.setCursor(40, 23);
    tft.setTextColor(ST7735_WHITE);
    tft.setTextSize(1);
    tft.setTextWrap(false);
    tft.print(luxString);
    tft.print(" LUX");

    char soilString[12];
    dtostrf(gsoil, 1, 2, soilString);
    mqtt.publish(tSoil, soilString);

    tft.fillRect(40, 34, 120, 10, ST7735_BLACK);
    tft.setCursor(40, 34);
    tft.setTextColor(ST7735_WHITE);
    tft.setTextSize(1);
    tft.setTextWrap(false);
    tft.print(soilString);
    tft.print("%");

    SerialMon.print("T="); SerialMon.print(tempString);
    SerialMon.print(" H="); SerialMon.print(humidityString);
    SerialMon.print(" Lux="); SerialMon.print(luxString);
    SerialMon.print(" Soil="); SerialMon.println(soilString);
  }
}
