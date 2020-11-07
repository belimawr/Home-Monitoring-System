#include <Adafruit_BME280.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <Wire.h>

//#include <Adafruit_Sensor.h> // Included by Adafruit_BME280.h
//#include <Adafruit_GFX.h> // Included by Adafruit_SSD1306.h

#define SDA 0
#define SCL 2

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

#define BME_ADDR 0x76
#define SEALEVELPRESSURE_HPA (1013.25)

#define DISPLAY_ADDR 0x3C

#define WIFI_SSID "wifi name"
#define WIFI_PASSWORD "wifi password!"

#define TZ_INFO "CET-1CEST,M3.5.0,M10.5.0/3"  // Berlin

#define INFLUXDB_URL "http://10.6.0.1:8086"
#define INFLUXDB_TOKEN "your token"
#define INFLUXDB_ORG "your org"
#define INFLUXDB_BUCKET "bucket"

#define MEASUREMENT "weather"

InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN);

unsigned long int delayTime = 1000;

Adafruit_BME280 sensor;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

float temperature, pressure, humidity, altitude;
unsigned long currentTime;

const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;

ESP8266WiFiMulti WiFiMulti;

IPAddress ip;

String strData = String();
String errMsg = String();
bool err = false;

// Static IP configuration so we can set a differetn
// gateway that will relay the traffic to my VPN as well
// as allow access to the internet for syncing the time
IPAddress local_IP(192, 168, 0, 15);
IPAddress gateway(192, 168, 0, 42);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8); // optional
IPAddress secondaryDNS(8, 8, 4, 4); // optional

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
  Serial.println("DHCP IP address: ");
  Serial.println(ip);

  // Configures static IP address
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("Failed to configure static IP address");
  }
  Serial.println("Static IP address: ");
  Serial.println(WiFi.localIP());
  ip = WiFi.localIP();
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

  // Weather monitoring mode:
  // https://github.com/adafruit/Adafruit_BME280_Library/blob/587329ab357dda8802e43faf48eb5ff3322af16b/examples/advancedsettings/advancedsettings.ino#L55-L68
  sensor.setSampling(Adafruit_BME280::MODE_FORCED,
                     Adafruit_BME280::SAMPLING_X1, // temperature
                     Adafruit_BME280::SAMPLING_X1, // pressure
                     Adafruit_BME280::SAMPLING_X1, // humidity
                     Adafruit_BME280::FILTER_OFF);

  delayTime = 60000;

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

  p.addTag("device", "WeMos");
  p.addTag("location", "hallway");
}

void loop() {
  readSensors();
  prepareData();

  writeSerial();
  writeDisplay();

  p.clearFields();
  p.addField("altitude", altitude);
  p.addField("humidity", humidity);
  p.addField("pressure", pressure);
  p.addField("temperature", temperature);

  Serial.println(p.toLineProtocol());

  sendDataToInfluxDB();

  delay(delayTime);
}

void sendDataToInfluxDB(){
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
}
