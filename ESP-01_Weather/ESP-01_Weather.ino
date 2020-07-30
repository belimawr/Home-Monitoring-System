#include <Adafruit_BME280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <ArduinoJson.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266mDNS.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <Wire.h>

#define SDA 0
#define SCL 2

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define BME_ADDR 0x76
#define SEALEVELPRESSURE_HPA (1013.25)

#define DISPLAY_ADDR 0x3C

#define WIFI_SSID "wifi-name"
#define WIFI_PASSWORD "secret"

#define SERVER_IP "192.168.0.140"
#define SERVER_PORT 3000

#define INFLUXDB_HOST "https://iot.tiago.cloud:8086"
#define INFLUXDB_PORT 8086
#define INFLUXDB_DATABASE "house"
#define INFLUXDB_USER "wemos"
#define INFLUXDB_PASSWORD "Urban-Aloha-Exhale-Irregular-Shallot-Unrushed-Finally"
#define MEASUREMENT "weather"

#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"

// Certificate of Certificate Authority of InfluxData Cloud 2 servers
const char certificate[] PROGMEM =  R"EOF( 
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)EOF";

InfluxDBClient client;

unsigned long int delayTime = 1000;

Adafruit_BME280 sensor;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

float temperature, pressure, humidity, altitude;
unsigned long currentTime;

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

const char *host = SERVER_IP;
const uint16_t port = SERVER_PORT;

ESP8266WiFiMulti WiFiMulti;

IPAddress ip;

String strData = String();
String errMsg = String();
bool err = false;

void initInfluxDB(){
  client.setConnectionParamsV1(
                               INFLUXDB_HOST,
                               INFLUXDB_DATABASE,
                               INFLUXDB_USER,
                               INFLUXDB_PASSWORD, certificate);
}

void initWifi() {
  Serial.println("\n\n-- Initialising WiFi --");
  Serial.print("Connecting to Network: ");
  Serial.print(ssid);
  Serial.print(" Password:");
  Serial.println(password);

  WiFi.mode(WIFI_STA);
  WiFiMulti.addAP(ssid, password);

  Serial.print("\nWait for WiFi... ");

  while (WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  ip = WiFi.localIP();

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(ip);
}
 
void prepareData() {
  strData = String("Temperature: ");
  strData += temperature;
  strData += " *C\n";

  strData += "Pressure: ";
  strData += pressure;
  strData += " hPa\n";

  strData += "Altitude: ";
  strData += altitude;
  strData += " m\n";

  strData += "Humidity: ";
  strData += humidity;
  strData += " %\n";

  strData += "IP: ";
  strData += ip.toString();
  strData += "\n";

  strData += "Time: ";
  strData += currentTime;
  strData += "s\n";

  if (err){
    strData += errMsg;
  }
}

void writeSerial() {
  Serial.println(strData);
  Serial.println("\n\n");
}

void writeDisplay() {
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);

  display.print(strData);
  display.display();

  display.display();
}

void initBME(){
  Serial.println(F("\n\n-- Initialising BME280 --"));
  if (!sensor.begin(BME_ADDR)) {
    while (1) {
      Serial.println(F("Could not find a valid BME280 sensor, check wiring, "
                       "address, sensor ID!"));
      delay(1000);
    }
  }

  sensor.setSampling(Adafruit_BME280::MODE_NORMAL,
                     Adafruit_BME280::SAMPLING_X16, // temperature
                     Adafruit_BME280::SAMPLING_X16, // pressure
                     Adafruit_BME280::SAMPLING_X16, // humidity
                     Adafruit_BME280::FILTER_X16,
                     Adafruit_BME280::STANDBY_MS_1000);

  delayTime = 1000;

  Serial.println(("-- BME280 initialised --"));
}

void readSensors() {
  temperature = sensor.readTemperature();
  humidity = sensor.readHumidity();
  pressure = sensor.readPressure() / 100.0F;
  altitude = sensor.readAltitude(SEALEVELPRESSURE_HPA);
  currentTime = millis() / 1000;
}

void initDisplay(){
  Serial.println(F("\n\n-- Initialising Display --"));

  if (!display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDR)) {
    Serial.println(F("SSD1306 allocation failed"));
    while (1) {
      Serial.println(F("SSD1306 allocation failed"));
      delay(1000);
    }
  }

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  Serial.println(F("-- SSD1306 initialised --"));

  display.display();
  delay(1000);
}

void writeJSON(WiFiClient out){
  const int capacity = JSON_OBJECT_SIZE(5);
  StaticJsonDocument<capacity> doc;

  doc["altitude"] = altitude;
  doc["humidity"] = humidity;
  doc["location"]= "livingroom";
  doc["pressure"] = pressure;
  doc["temperature"] = temperature;

  serializeJson(doc, out);
}

void writeToServer(){
  WiFiClient client;

  if (!client.connect(host, port)) {
    Serial.println("connection failed");
    return;
  }

  writeJSON(client);
  client.stop();
}

Point p(MEASUREMENT);

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Wire.begin(SDA, SCL);
  Serial.println("\n\nSerial and I2C initialised!");

  initDisplay();
  initBME();
  initWifi();

  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  initInfluxDB();

  p.addTag("device", "WeMos");
  p.addTag("location", "livingroom");
}

void loop() {
  readSensors();
  prepareData();

  writeSerial();
  writeDisplay();
  /* writeToServer(); */

  p.clearFields();
  p.addField("altitude", altitude);
  p.addField("humidity", humidity);
  p.addField("pressure", pressure);
  p.addField("temperature", temperature);

  Serial.println(p.toLineProtocol());

  if ((WiFi.RSSI() == 0) && (WiFiMulti.run() != WL_CONNECTED)){
    Serial.print(F("Wifi connection lost, waiting 5s"));
    for (int i=0; i < 20; i++) {
      delay(250);
      Serial.print(".");
    }
    Serial.println(F("\nResuming programm"));
  } else {
    unsigned long int before = millis();
    if (!client.writePoint(p)) {
      err = true;
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());
      errMsg = String(F("err: "));
      errMsg += client.getLastErrorMessage();
    }
    Serial.print(F("time sending data: "));
    Serial.println(millis() - before, DEC);
  }

  delay(delayTime);
}
