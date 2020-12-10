#include <Arduino.h>
#include <list>
#include <ESP8266WiFi.h>

#include "defs.h"

#include "io.h"

static myDHT *dht11_1;
static myDS18B20 *ds18B20;
static myCapMoisture *moisture;
static tempSensor *ts;
static humSensor *hs;
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
    setup_mqtt();

    ping_ticker = new myTicker::Ticker(fcce_ping, NULL, 5000, 0, myTicker::MILLIS);
    ping_ticker->start();
    dht11_1 = new myDHT("DHT11 sensor", 13, DHTesp::DHT11);
    ds18B20 = new myDS18B20("/TempErde1", 12);
    moisture = new myCapMoisture("/HumErde1", A0);
    ts = new tempSensor(std::list<myDHT *>{dht11_1}, "/TempBerg1");
    delay(125);
    hs = new humSensor(std::list<myDHT *>{dht11_1}, "/HumBerg1");
}

void loop()
{
    loop_wifi();
    loop_mqtt();
    ping_ticker->update();
    periodicSensor::tick_sensors();
}
