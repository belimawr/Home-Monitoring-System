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

### Raspberry Pi as gateway
Install Tailscale, configure it, make sure it's working

```sh
## Enable IP forwarding
sudo /bin/su -c "echo -e '\n#Enable IP Routing\nnet.ipv4.ip_forward = 1' > /etc/sysctl.conf"
## Optionally edit the file /etc/sysctl.conf and enalbe 'ip_forward' for IPv4 and IPv6

# Test IP forwarding
sudo sysctl -p


# IPTABLES configuration
## Enable NAT
sudo iptables -t nat -A POSTROUTING -o tailscale0 -j MASQUERADE

## Allow traffic from wlan0 (internal) to tailscale0 (VPN)
sudo iptables -A FORWARD -i wlan0 -o tailscale0 -j ACCEPT

## Allow traffic from tailscale0 to go back over wlan0
sudo iptables -A FORWARD -i tailscale0 -o wlan0 -m state --state RELATED,ESTABLISHED -j ACCEPT

## Allow Raspiberry Pi's own loopback traffic
sudo iptables -A INPUT -i lo -j ACCEPT

## Allow computers on the local network (wlan0) and VPN (tailscale0) to ping the Raspberry Pi
sudo iptables -A INPUT -i wlan0 -p icmp -j ACCEPT
sudo iptables -A INPUT -i tailscale0 -p icmp -j ACCEPT

## Allow SSH
sudo iptables -A INPUT -p tcp --dport 22 -j ACCEPT

## Allow traffic initiated by the Raspberry Pi to return
sudo iptables -A INPUT -m state --state ESTABLISHED,RELATED -j ACCEPT

## If traffic does not match the rules it will be dropped
sudo iptables -P FORWARD DROP
sudo iptables -P INPUT DROP

## List the rules
sudo iptables -L

## Install a package to make the iptables rules presistent
sudo apt-get install iptables-persistent

## Save the new rules (if changed)
sudo systemctl enable netfilter-persistent
```

