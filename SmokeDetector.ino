// 1. ต้องเอาค่าจาก Blynk Console มาวางไว้บนสุดของไฟล์เสมอ
#define BLYNK_TEMPLATE_ID "TMPL644AjGP2l"
#define BLYNK_TEMPLATE_NAME "Smoke Detector"
#define BLYNK_AUTH_TOKEN "byRo8COVYn_WUOwl_jW6JJG5NIjzF7wz"

// 2. ตามด้วย Library ต่างๆ
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <DHT.h>

#define BLYNK_PRINT Serial
#include <BlynkSimpleEsp8266.h>

// --- ตั้งค่า WiFi ---
const char* WIFI_SSID = "Black Shark 2 Pro";
const char* WIFI_PASSWORD = "0989234541";

// --- ตั้งค่า LINE API ---
const char* CHANNEL_ACCESS_TOKEN = "1KVtTmIhNXgVnVAEioMfLz/COYllJK2w57vEeS+/QbaV/GOTWzGNiA0d23MT2xkMgg7eXQeDV3XFuq6Go2Rwje/+S3e5kNuAuyLqsTp/jjpmcRhMYg+yaL23aVX0bZEzNxzwqldpUA92qOj/+JTGhQdB04t89/1O/w1cDnyilFU=";
const char* USER_ID = "C60b991465593c46b5a175f7e238f00b3"; 
const char* LINE_API_HOST = "api.line.me";

// --- ตั้งค่า Google Sheets ---
const char* GAPP_SCRIPT_HOST = "script.google.com";
String GAPP_SCRIPT_ID = "AKfycbz6oKyB6x7xn8p-1oTcT9M1_BY5WjfX4oe6l82h2YQZeiPLXePmDcXqanR1VO8o4m2elw";

// --- ตั้งค่า Pin ต่างๆ ---
#define MQ2_PIN A0
#define LED_PIN D2
#define BUZZER_PIN D5
#define DHT_PIN D4
#define DHT_TYPE DHT22

// --- ตัวแปรและอ็อบเจกต์ ---
const int SMOKE_THRESHOLD = 550;
bool notificationSent = false;

DHT dht(DHT_PIN, DHT_TYPE);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);
WiFiClientSecure client;
BlynkTimer timer;

// ฟังก์ชันส่ง LINE Message
void sendLineMessage(String message) {
  client.setInsecure();
  if (!client.connect(LINE_API_HOST, 443)) return;

  StaticJsonDocument<512> jsonDoc;
  jsonDoc["to"] = USER_ID;
  JsonArray messages = jsonDoc.createNestedArray("messages");
  JsonObject messageObj = messages.createNestedObject();
  messageObj["type"] = "text";
  messageObj["text"] = message;

  String payload;
  serializeJson(jsonDoc, payload);

  String request = String("POST /v2/bot/message/push HTTP/1.1\r\n") +
                   "Host: " + LINE_API_HOST + "\r\n" +
                   "Authorization: Bearer " + CHANNEL_ACCESS_TOKEN + "\r\n" +
                   "Content-Type: application/json\r\n" +
                   "Content-Length: " + String(payload.length()) + "\r\n\r\n" +
                   payload;

  client.print(request);
  client.stop();
}

// ฟังก์ชันส่งค่าไป Google Sheet
void sendToGoogleSheet(int smoke, float temp, float hum) {
  client.setInsecure();
  HTTPClient http;
  String url = "https://" + String(GAPP_SCRIPT_HOST) + "/macros/s/" + GAPP_SCRIPT_ID + "/exec" +
               "?smoke=" + String(smoke) + "&temp=" + String(temp, 1) + "&hum=" + String(hum, 1);

  if (http.begin(client, url)) {
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      Serial.println("Google Sheet: Data logged successfully.");
    }
    http.end();
  }
}

// ฟังก์ชันหลักที่ทำงานทุกๆ 60 วินาที
void handleSensors() {
  int sensorValue = analogRead(MQ2_PIN);
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  Serial.printf("Smoke: %d | Temp: %.1f | Hum: %.1f\n", sensorValue, temperature, humidity);

  Blynk.virtualWrite(V0, sensorValue);
  Blynk.virtualWrite(V1, temperature);
  Blynk.virtualWrite(V2, humidity);

  sendToGoogleSheet(sensorValue, temperature, humidity);

  if (sensorValue > SMOKE_THRESHOLD) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, LOW);

    if (!notificationSent) {
      timeClient.update();
      time_t rawTime = timeClient.getEpochTime();
      struct tm *timeInfo = localtime(&rawTime);
      char timeString[30];
      strftime(timeString, sizeof(timeString), "%d/%m/%Y %H:%M:%S", timeInfo);

      String msg = "🚨 ตรวจพบควัน! 🚨\nเวลา: " + String(timeString) + 
                   "\nค่าควัน: " + String(sensorValue) + 
                   "\nอุณหภูมิ: " + String(temperature, 1) + " °C";
      sendLineMessage(msg);
      notificationSent = true;
    }
  } else {
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, HIGH);
    notificationSent = false;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, HIGH);

  dht.begin();
  
  // ใช้ Blynk.config และ WiFi.begin แทน Blynk.begin เพื่อความเสถียรเมื่อมีหลาย Task
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Blynk.config(BLYNK_AUTH_TOKEN);
  
  timeClient.begin();

  Serial.println("Warming up sensor...");
  delay(20000); 

  timer.setInterval(60000L, handleSensors);
}

void loop() {
  Blynk.run();
  timer.run();
}
