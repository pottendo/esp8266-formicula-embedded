#include <Arduino.h>
#include <list>
#include <ESP8266WiFi.h>

#include "defs.h"

#include "io.h"

static myDHT *dht11_1;
static tempSensor *ts;
static humSensor *hs;


void setup()
{
    Serial.begin(115200);
    printf("Formicula embeeded starting...\n");
    setup_wifi();
    setup_mqtt();

    dht11_1 = new myDHT("DHT11 - test1 sensor", 13, DHTesp::DHT11);
    ts = new tempSensor(std::list<myDHT *>{ dht11_1 }, "/TempBerg1");
    delay (125);
    hs = new humSensor(std::list<myDHT *>{ dht11_1 }, "/HumBerg1");
}

void loop()
{
    loop_wifi();
    periodicSensor::tick_sensors();
}
