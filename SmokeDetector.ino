#include <ArduinoJson.h>

#include <ESP8266WiFi.h>

#include <WiFiClientSecure.h>

#include <ESP8266HTTPClient.h> // ไลบรารีสำหรับ Google Sheet

#include <WiFiUdp.h>

#include <NTPClient.h>

#include <time.h>

#include <DHT.h>



// --- Blynk ---

#define BLYNK_TEMPLATE_ID "TMPL644AjGP2l"

#define BLYNK_TEMPLATE_NAME "Smoke Detector"

#define BLYNK_AUTH_TOKEN "byRo8COVYn_WUOwl_jW6JJG5NIjzF7wz"



#define BLYNK_PRINT Serial

#include <BlynkSimpleEsp8266.h>

char auth[] = BLYNK_AUTH_TOKEN;



// --- WiFi ---

const char* WIFI_SSID = "P3M.ekn";

const char* WIFI_PASSWORD = "Muek9655";



// --- LINE Notify ---

const char* CHANNEL_ACCESS_TOKEN = "1KVtTmIhNXgVnVAEioMfLz/COYllJK2w57vEeS+/QbaV/GOTWzGNiA0d23MT2xkMgg7eXQeDV3XFuq6Go2Rwje/+S3e5kNuAuyLqsTp/jjpmcRhMYg+yaL23aVX0bZEzNxzwqldpUA92qOj/+JTGhQdB04t89/1O/w1cDnyilFU=";

const char* USER_ID = "U06e16c9d93ba93b160a45386992ad065";

const char* LINE_API_HOST = "api.line.me";

WiFiClientSecure client; // Client นี้จะถูกใช้ทั้ง LINE และ Google Sheet

bool notificationSent = false;



// --- Google Sheet (Web App) ---

const char* GAPP_SCRIPT_HOST = "script.google.com";

// ID ที่คุณให้มา:

String GAPP_SCRIPT_ID = "AKfycbz6oKyB6x7xn8p-1oTcT9M1_BY5WjfX4oe6l82h2YQZeiPLXePmDcXqanR1VO8o4m2elw";



// --- Pins ---

#define MQ2_PIN A0

#define LED_PIN D2

#define BUZZER_PIN D5

#define DHT_PIN D4



// --- Sensor Settings ---

const int SMOKE_THRESHOLD = 550;

const int LOOP_DELAY = 60000;

const int SENSOR_WARMUP_TIME = 20000;



// --- NTP (Time) ---

WiFiUDP ntpUDP;

NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);



// --- DHT Sensor ---

#define DHT_TYPE DHT22

DHT dht(DHT_PIN, DHT_TYPE);





/**

* @brief ส่งข้อความไปยัง LINE Notify

* @param message ข้อความที่ต้องการส่ง

*/

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

Serial.println("Sending LINE request...");



unsigned long timeout = millis();

while (client.connected() && millis() - timeout < 5000) {

if (client.available()) {

String line = client.readStringUntil('\n');

Serial.println(line);

}

}



client.stop();

Serial.println("LINE Request finished.");

}





/**

* @brief ส่งข้อมูลไปยัง Google Sheet (เวอร์ชันล่าสุด: จัดการ Redirect อัตโนมัติ)

* @param smoke ค่าควันจาก MQ2

* @param temp ค่าอุณหภูมิจาก DHT22

* @param hum ค่าความชื้นจาก DHT22

*/

void sendToGoogleSheet(int smoke, float temp, float hum) {



Serial.println("Connecting to Google Apps Script (HTTPClient)...");



// ตั้งค่า client ให้ไม่ตรวจสอบ Certificate (จำเป็นสำหรับ Google)

client.setInsecure();



// สร้างอ็อบเจกต์ HTTPClient

HTTPClient http;



// สร้าง URL แบบเต็ม

String url = "https://" + String(GAPP_SCRIPT_HOST) +

"/macros/s/" + GAPP_SCRIPT_ID + "/exec" +

"?smoke=" + String(smoke) +

"&temp=" + String(temp, 1) +

"&hum=" + String(hum, 1);



Serial.print("Requesting URL: ");

Serial.println(url);



// เริ่มการเชื่อมต่อ HTTPS โดยใช้ 'client' (WiFiClientSecure)

if (http.begin(client, url)) {



// --- ⬇️ ⬇️ ⬇️ นี่คือบรรทัดที่เพิ่มเข้ามาเพื่อแก้ 302 ⬇️ ⬇️ ⬇️ ---

// สั่งให้ HTTPClient เดินตาม Redirect (302) โดยอัตโนมัติ

http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

// --- ⬆️ ⬆️ ⬆️ ------------------------------------ ⬆️ ⬆️ ⬆️ ---



// http.GET() จะทำการส่ง Request, รอรับ,

// และ "เดินตาม" Redirect (302) ไปยัง URL ใหม่ให้เราโดยอัตโนมัติ

int httpCode = http.GET();



if (httpCode > 0) {

// พิมพ์ HTTP Code ที่ได้รับ (ตอนนี้เราคาดหวัง "200 OK")

Serial.printf("[HTTP] GET... code: %d\n", httpCode);



// ถ้าสำเร็จ (HTTP_CODE_OK คือ 200)

if (httpCode == HTTP_CODE_OK) {

String payload = http.getString();

Serial.println("Response from Google:");

Serial.println(payload); // <-- ควรจะพิมพ์ "Success: Data logged."

} else {

// พิมพ์ข้อผิดพลาด (เช่น 404, 500)

Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());

}

} else {

// พิมพ์ข้อผิดพลาด (เช่น เชื่อมต่อไม่ได้)

Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());

}



// ปิดการเชื่อมต่อ

http.end();



} else {

Serial.printf("[HTTP] Unable to connect to %s\n", GAPP_SCRIPT_HOST);

}



Serial.println("Google Sheet request finished.");

}





/**

* @brief ฟังก์ชัน Setup เริ่มต้นการทำงาน

*/

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





/**

* @brief ฟังก์ชัน Loop ทำงานวนซ้ำ

*/

void loop() {

Blynk.run();



// 1. อ่านค่าควัน

int sensorValue = analogRead(MQ2_PIN);

Serial.print("Current Smoke Level: ");

Serial.println(sensorValue);

Blynk.virtualWrite(V0, sensorValue);



// 2. อ่านค่าอุณหภูมิและความชื้น

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


// 3. ส่งค่าไป Blynk

Blynk.virtualWrite(V1, temperature);

Blynk.virtualWrite(V2, humidity);



// 4. ส่งค่าไป Google Sheet

sendToGoogleSheet(sensorValue, temperature, humidity);

}



// 5. ตรวจสอบเงื่อนไขการแจ้งเตือน

if (sensorValue > SMOKE_THRESHOLD) {

digitalWrite(LED_PIN, HIGH);

digitalWrite(BUZZER_PIN, LOW);



// ส่ง LINE แค่ครั้งเดียว

if (!notificationSent) {

timeClient.update();



time_t rawTime = timeClient.getEpochTime();

struct tm *timeInfo = localtime(&rawTime);

char timeString[30];

strftime(timeString, sizeof(timeString), "%d/%m/%Y %H:%M:%S", timeInfo);



// สร้างข้อความ

String msg = "🚨 ตรวจพบควัน! 🚨\n"

"เวลา: " + String(timeString) + "\n"

"ค่าควัน: " + String(sensorValue) + "\n"

"อุณหภูมิ: " + String(temperature, 1) + " °C\n"

"ความชื้น: " + String(humidity, 1) + " %";



sendLineMessage(msg);

notificationSent = true;

}

} else {

// สถานะปกติ

digitalWrite(LED_PIN, LOW);

digitalWrite(BUZZER_PIN, HIGH);

notificationSent = false;

}



delay(LOOP_DELAY);

} 
