#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <MQTT.h>
#include "defs.h"

static const String my_clientID{"fcce"};

static MQTT *mqtt_client;

void myConnectedCb(void)
{
    log_msg("mqtt client connected.");
}

void myDisconnectedCb(void)
{
    log_msg("mqtt client disconnected... reconnecting");
    delay(500);
    mqtt_client->connect();
}

void myPublishedCb(void)
{
    //log_msg("mqtt client published cb...");
}

void myDataCb(String &topic, String &data)
{
    log_msg("fcce - " + topic + ": " + data);
}

void setup_mqtt(void)
{
    mqtt_client = new MQTT{my_clientID.c_str(), WiFi.localIP().toString().c_str(), 1883};
    // setup callbacks
    mqtt_client->onConnected(myConnectedCb);
    mqtt_client->onDisconnected(myDisconnectedCb);
    mqtt_client->onPublished(myPublishedCb);
    mqtt_client->onData(myDataCb);
    log_msg("connect mqtt client...");
    delay(20);
    mqtt_client->connect();
    delay(20);
    mqtt_client->subscribe("fcce/config");
    log_msg("mqtt setup done");
    mqtt_publish("/fcce-alive", "Formicula embedded starting...");
}

void mqtt_publish(String topic, String msg)
{
    //log_msg("publishing: " + my_clientID + topic + msg);
    if (mqtt_client->isConnected())
        mqtt_client->publish(my_clientID + topic, msg, 0, 0);
    else
        log_msg("MQTT not connected - discarding: " + topic + "-" + msg);
}

void mqtt_publish(const char *topic, const char *msg)
{
    mqtt_publish(String(topic), String(msg));
}
