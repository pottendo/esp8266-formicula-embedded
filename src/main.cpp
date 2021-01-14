#include <Arduino.h>
#include <list>
#include <ESP8266WiFi.h>

#include "defs.h"
#include "io.h"

static myDHT *dht11_1;
static tempSensorMulti *ts1;
static humSensorMulti *hs1;

static myDHT *dht22_1;
static myBM280 *bm280_1;
static myBM280 *bm280_2;
static tempSensorMulti *ts3;
static humSensorMulti *hs3;

static myDS18B20 *ds18B20;
static avgSensor *ts2;

static myCapMoisture *moisture;
static avgSensor *hs2;

static myTicker::Ticker *ping_ticker;

static void fcce_ping(void *a)
{
    static unsigned long last;
    static char buf[84];
    unsigned long t = millis();
    unsigned long uptime = (t - last) / 1000;
    snprintf(buf, 84, "fcce/ut %02ldh:%02ldm:%02lds,IP(%s),fm=%d",
             uptime / 3600,
             (uptime % 3600) / 60,
             (uptime % 60),
             WiFi.localIP().toString().c_str(),
             ESP.getFreeHeap());
    //log_msg(String("fcce - ping ") + buf);
    mqtt_publish("/config", buf);
}

void setup()
{
    Serial.begin(115200);
    printf("Formicula embeeded starting...\n");
    setup_wifi();
    //setup_io();
    setup_mqtt();

    ping_ticker = new myTicker::Ticker(fcce_ping, NULL, 30000, 0, myTicker::MILLIS);
    ping_ticker->start();

    /* temp & hum of fcce box */
    dht11_1 = new myDHT(nullptr, "/DHT11sens", 12, DHTesp::DHT11, 15000);   
    ts1 = new tempSensorMulti(nullptr, "/FCCETemp", std::list<genSensor *>{dht11_1});
    hs1 = new humSensorMulti(nullptr, "/FCCEHum", std::list<genSensor *>{dht11_1});

    dht22_1 = new myDHT(nullptr, "/DHT22 sensor", 14, DHTesp::DHT22, 3000);
    bm280_1 = new myBM280(nullptr, "/BME280sens1", 4, 5, 0x76, 3000);
    bm280_2 = new myBM280(nullptr, "/BME280sens2", 4, 5, 0x77, 3000);
    ts3 = new tempSensorMulti(nullptr, "/BergTemp", std::list<genSensor *>{bm280_1, bm280_2, dht22_1});
    hs3 = new humSensorMulti(nullptr, "/BergHum", std::list<genSensor *>{bm280_1, bm280_2, dht22_1});

    ds18B20 = new myDS18B20(nullptr, "/SD18B20sens", 2, 5000);
    ts2 = new avgSensor(nullptr, "/ErdeTemp", std::list<genSensor *>{ds18B20});

    moisture = new myCapMoisture(nullptr, "/CapSoilMoistsens ", 15, 16, 10000);
    hs2 = new avgSensor(nullptr, "/ErdeHum", std::list<genSensor *>{moisture});
}

void loop()
{
    loop_wifi();
    loop_mqtt();
    ping_ticker->update();
    periodicSensor::tick_sensors();
}
