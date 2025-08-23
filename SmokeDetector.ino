#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <time.h>
#include <DHT.h> 

// ==== Blynk ====
#define BLYNK_TEMPLATE_ID "TMPL644AjGP2l"
#define BLYNK_TEMPLATE_NAME "Smoke Detector"
#define BLYNK_AUTH_TOKEN  "byRo8COVYn_WUOwl_jW6JJG5NIjzF7wz"

#define BLYNK_PRINT Serial
#include <BlynkSimpleEsp8266.h>

// ==== WiFi & LINE ====
const char* WIFI_SSID = "P3M.ekn";
const char* WIFI_PASSWORD = "Muek9655";

const char* CHANNEL_ACCESS_TOKEN = "1KVtTmIhNXgVnVAEioMfLz/COYllJK2w57vEeS+/QbaV/GOTWzGNiA0d23MT2xkMgg7eXQeDV3XFuq6Go2Rwje/+S3e5kNuAuyLqsTp/jjpmcRhMYg+yaL23aVX0bZEzNxzwqldpUA92qOj/+JTGhQdB04t89/1O/w1cDnyilFU=";
const char* USER_ID = "U06e16c9d93ba93b160a45386992ad065";

// ==== Pins ====
#define MQ2_PIN    A0
#define LED_PIN    D2
#define BUZZER_PIN D5
#define DHT_PIN    D4   

// ==== Settings ====
const int SMOKE_THRESHOLD = 550;
const int LOOP_DELAY = 5000;
const int SENSOR_WARMUP_TIME = 20000;

// ==== LINE ====
const char* LINE_API_HOST = "api.line.me";
WiFiClientSecure client;
bool notificationSent = false;

// ==== NTP ====
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000); 

// ==== Blynk ====
char auth[] = BLYNK_AUTH_TOKEN;


#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);


void sendLineMessage(String message) {
  client.setInsecure();
  Serial.println("Connecting to LINE API...");

  if (!client.connect(LINE_API_HOST, 443)) {
    Serial.println("--> Connection failed!");
    return;
  }

  Serial.println("--> Connected!");

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
  Serial.println("Sending request...");

  unsigned long timeout = millis();
  while (client.connected() && millis() - timeout < 5000) {
    if (client.available()) {
      String line = client.readStringUntil('\n');
      Serial.println(line);
    }
  }

  client.stop();
  Serial.println("Request finished.");
}


void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, HIGH); // Active Low

  dht.begin(); 

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
  timeClient.update();

  Blynk.begin(auth, WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Warming up sensor...");
  delay(SENSOR_WARMUP_TIME);
  Serial.println("Sensor is ready.");
}


void loop() {
  Blynk.run();

 
  int sensorValue = analogRead(MQ2_PIN);
  Serial.print("Current Smoke Level: ");
  Serial.println(sensorValue);
  Blynk.virtualWrite(V0, sensorValue); 

  
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT sensor!");
  } else {
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.print(" %\t");
    Serial.print("Temperature: ");
    Serial.print(temperature);
    Serial.println(" *C");
    
    
    Blynk.virtualWrite(V1, temperature); 
    Blynk.virtualWrite(V2, humidity);    
  }

  
  if (sensorValue > SMOKE_THRESHOLD) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, LOW);

    if (!notificationSent) {
      timeClient.update();

      time_t rawTime = timeClient.getEpochTime();
      struct tm *timeInfo = localtime(&rawTime);
      char timeString[30];
      strftime(timeString, sizeof(timeString), "%d/%m/%Y %H:%M:%S", timeInfo);

     
      String msg = "🚨 ตรวจพบควัน! 🚨\n"
                   "เวลา: " + String(timeString) + "\n"
                   "ค่าควัน: " + String(sensorValue) + "\n"
                   "อุณหภูมิ: " + String(temperature, 1) + " °C\n"
                   "ความชื้น: " + String(humidity, 1) + " %";

      sendLineMessage(msg);
      notificationSent = true;
    }
  } else {
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, HIGH);
    notificationSent = false;
  }

  delay(LOOP_DELAY);
}
