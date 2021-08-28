# Home Monitoring System

## Disclaimer
This is a work in progress, anything can change at any point in time.

## Goals
Build a home monitoring system that will at least:
1. Measure temperature, humidity and pressure in multiple rooms
2. Monitor the status (on/off) of smart devices
3. Make real time data accessible from anywhere
4. Be secure

## Hardware

ESP8266 - Microcontroller with WiFi
BME280 - Temperature, Humidity and Pressure sensor
SSD1306 - 128x64 OLED display (only in all units)

## Software

* [InfluxDB OSS
  2.0](https://docs.influxdata.com/influxdb/v2.0/get-started/) storing
  and serving all timeseries data.
* [InfluxDB Client for
  Arduino](https://github.com/tobiasschuerg/InfluxDB-Client-for-Arduino)
  to send data directly to InfluxDB.
* [Tailscale](https://tailscale.com/) is the VPN.
* [ESP8266 sketch](ESP-01_Weather/ESP-01_Weather.ino) collects the
  measurements and sends them to InfluxDB.

## High level architecture
* InfluxDB runs on a VPS and listens only on the VPN interface. It
  only uses HTTP.
* A Raspberry PI Zero W is used as gateway to the VPN.
* The ESP8266 connects to my WiFi network and used the Raspberry Pi
  Zero W as gateway.

### Assumptions
* My home network (including WiFi) is secure, anyone connected to it
  can be trusted.
* InfluxDB cannot be accessed from the internet.
* The VPN provides secure (encrypted) communication between the VPS
  and any other device.
* The ESP8266 is not accessible from the internet.
* ESP8266 do not need to handle encryption (SSL, HTTPS, etc).
* The Raspberry Pi does daily backups of the data from InfluxDB.

