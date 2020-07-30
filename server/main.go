package main

import (
	"context"
	"encoding/json"
	"log"
	"net"
	"time"

	influxdb2 "github.com/influxdata/influxdb-client-go"
	"github.com/influxdata/influxdb-client-go/api"
)

// Start influx db: docker run -d -p 8086:8086 -p 2003:2003 -e INFLUXDB_GRAPHITE_ENABLED=true --name influx influxdb
// docker exec -it influx /bin/sh
// influx -precision rfc3339

const (
	dbName     = "weather"
	influxHost = "http://localhost:8086"
)

var port = "3000"

type input struct {
	Altitude    float32 `json:"altitude"`
	Humidity    float32 `json:"humidity"`
	Location    string  `json:"location"`
	Pressure    float32 `json:"pressure"`
	Temperature float32 `json:"temperature"`
}

func main() {
	client := influxdb2.NewClient(influxHost, dbName)
	defer client.Close()

	writeAPI := client.WriteAPIBlocking("", dbName)

	ln, err := net.Listen("tcp", net.JoinHostPort("", port))
	if err != nil {
		panic(err)
	}
	defer ln.Close()

	log.Print("Listening on: ", ln.Addr().String())

	for {
		conn, err := ln.Accept()
		if err != nil {
			log.Println("Error:", err)
			continue
		}

		go handleConnection(conn, writeAPI)
	}
}

func handleConnection(conn net.Conn, api api.WriteAPIBlocking) {
	defer func() {
		if err := conn.Close(); err != nil {
			log.Printf("Could not close connection: %s", err)
		}
	}()

	data := input{}
	if err := json.NewDecoder(conn).Decode(&data); err != nil {
		log.Println("Error reading data:", err)
		return
	}

	log.Printf("got data: %#v", data)
	senddata(api, data)
}

func senddata(api api.WriteAPIBlocking, d input) {
	p := influxdb2.NewPoint("weather",
		map[string]string{"location": d.Location},
		map[string]interface{}{
			"temperature": d.Temperature,
			"humidity":    d.Humidity,
			"altitude":    d.Altitude,
			"pressure":    d.Pressure,
		},
		time.Now())

	api.WritePoint(context.Background(), p)
}
