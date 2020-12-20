#include <Arduino.h>
#include <list>
#include <ESP8266WiFi.h>

#include "defs.h"
#include "io.h"

static myDHT *dht11_1;
static tempSensorMulti *ts1;
static humSensorMulti *hs1;

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
    static char buf[64];
    unsigned long t = millis();
    unsigned long uptime = (t - last) / 1000;
    snprintf(buf, 64, "/uptime %02ldh:%02ldm:%02lds, free mem = %d",
             uptime / 3600,
             (uptime % 3600) / 60,
             (uptime % 60),
             ESP.getFreeHeap());
    log_msg(String("fcce - ping ") + buf);
    mqtt_publish("/config", buf);
}

void setup()
{
    Serial.begin(115200);
    printf("Formicula embeeded starting...\n");
    setup_wifi();
    setup_io();
    setup_mqtt();

    ping_ticker = new myTicker::Ticker(fcce_ping, NULL, 5000, 0, myTicker::MILLIS);
    ping_ticker->start();
    dht11_1 = new myDHT(nullptr, "/DHT11 sensor", 14, DHTesp::DHT11, 5000);
    ts1 = new tempSensorMulti(nullptr, "/TempBerg1", std::list<genSensor *>{dht11_1});
    hs1 = new humSensorMulti(nullptr, "/HumBerg1", std::list<genSensor *>{dht11_1});

    ds18B20 = new myDS18B20(nullptr, "/SD18B20 sensor", 2, 3000);
    ts2 = new avgSensor(nullptr, "/TempErde1", std::list<genSensor *>{ds18B20});

    moisture = new myCapMoisture(nullptr, "/CapSoilMoist sensor ", A0, 7000);
    hs2 = new avgSensor(nullptr, "/HumErde1", std::list<genSensor *>{moisture});

    bm280_1 = new myBM280(nullptr, "/BME280 sensor1", 4, 5, 0x76, 11000);
    bm280_2 = new myBM280(nullptr, "/BME280 sensor2", 12, 13, 0x76, 11000);
    ts3 = new tempSensorMulti(nullptr, "/TempBerg2", std::list<genSensor *>{bm280_1, bm280_2});
    hs3 = new humSensorMulti(nullptr, "/HumBerg2", std::list<genSensor *>{bm280_1, bm280_2});
}

void loop()
{
    loop_wifi();
    loop_mqtt();
    ping_ticker->update();
    periodicSensor::tick_sensors();
}
