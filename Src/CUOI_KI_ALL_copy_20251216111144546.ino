#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_AHTX0.h>
#include <Wire.h>
#include <Adafruit_ST7735.h>
#include <Adafruit_GFX.h>
#include <SPI.h>
#define TFT_CS 10
#define TFT_RST 3
#define TFT_DC 18
#define TFT_SCLK 13
#define TFT_MOSI 11
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

const char* ssid = "TUVE";
const char* password = "12345678";
const char* mqtt_server = "kiettong.freedynamicdns.org";

WiFiClient esp32Client;
PubSubClient client(esp32Client);
Adafruit_AHTX0 aht;

long lastMsg = 0;
float temp1, humidity1;
float glux;
float gsoil;
const int ADC_PIN_LUX = 14;
const int ADC_PIN_SOIL = 13;

const float LUX_MIN = 1.0f;
const float LUX_MAX = 6000.0f;
const int AirValue = 2823;    // ADC khi khô (calib)
const int WaterValue = 1081;  // ADC khi ướt (calib)

const int ledPin = 4;
const int fanPin = 5;
const int pumbPin = 6;
const int roofpin = 7;
void setup_wifi() {
  delay(100);
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.println(topic);
  String messageTemp;

  for (int i = 0; i < length; i++) {
    messageTemp += (char)message[i];
  }
  Serial.println("Payload: " + messageTemp);

  if (String(topic) == "esp32/output") {
    if (messageTemp == "on_led") {
      digitalWrite(ledPin, HIGH);
      Serial.println("LED ON");
    } else if (messageTemp == "off_led") {
      digitalWrite(ledPin, LOW);
      Serial.println("LED OFF");
    } else if (messageTemp == "on_fan") {
      digitalWrite(fanPin, HIGH);
      Serial.println("FAN ON");
    } else if (messageTemp == "off_fan") {
      digitalWrite(fanPin, LOW);
      Serial.println("FAN OFF");
    } else if (messageTemp == "on_pumb") {
      digitalWrite(pumbPin, HIGH);
      Serial.println("PUMB ON");
    } else if (messageTemp == "off_pumb") {
      digitalWrite(pumbPin, LOW);
      Serial.println("PUMB OFF");
    } else if (messageTemp == "on_roof") {
      digitalWrite(roofpin, HIGH);
    } else if (messageTemp == "off_roof") {
      digitalWrite(roofpin, LOW);
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe("esp32/output");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

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
void setup() {
  Serial.begin(9600);
  Wire.begin(8, 9);
  // if (!aht.begin(&Wire)) {
  //  Serial.println("Could not find AHT30, please check your wiring!");
  //   while (1) delay(10);
  // }
  aht.begin(&Wire);
  Serial.println("AHT30 found");

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  pinMode(fanPin, OUTPUT);
  digitalWrite(fanPin, LOW);
  pinMode(pumbPin, OUTPUT);
  digitalWrite(pumbPin, LOW);
  pinMode(roofpin, OUTPUT);
  digitalWrite(roofpin, LOW);

  tft.initR(INITR_BLACKTAB);
  tft.fillScreen(ST7735_BLACK);
  printText("Temperature: ", ST7735_WHITE, 4, 1, 1);
  printText("Humidity: ", ST7735_WHITE, 4, 12, 1);
  printText("Soil moisture: ", ST7735_WHITE, 4, 23, 1);
  printText("Lux: ", ST7735_WHITE, 4, 34, 1);
  
}
void printText(char* text, uint16_t color, int x, int y, int textSize) {
  tft.setCursor(x, y);
  tft.setTextColor(color);
  tft.setTextSize(textSize);
  tft.setTextWrap(true);
  tft.print(text);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
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

    char tempString[8];
    dtostrf(temp1, 1, 2, tempString);
    Serial.print("Temperature: ");
    Serial.println(temp1);
    client.publish("esp32/temperature", tempString);
    delay(2000);

    char humidityString[8];
    dtostrf(humidity1, 1, 2, humidityString);
    Serial.print("Humidity: ");
    Serial.println(humidity1);
    client.publish("esp32/humidity", humidityString);
    delay(2000);

    char luxString[8];
    dtostrf(glux, 1, 2, luxString);
    Serial.print("Lux: ");
    Serial.println(glux);
    client.publish("esp32/lux", luxString);
    delay(2000);

    char soilString[8];
    dtostrf(gsoil, 1, 2, soilString);
    Serial.print("Soil: ");
    Serial.println(gsoil);
    client.publish("esp32/soil", soilString);
  }
}
