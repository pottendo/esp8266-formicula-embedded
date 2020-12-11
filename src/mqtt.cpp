#include <Arduino.h>
#include <ESP8266WiFi.h>
//#include <uMQTTBroker.h>
#include <MQTT.h>
#include "defs.h"

static const String my_clientID{"fcce"};
static unsigned long fcc_last_seen;
//#define MQTT_SERVER "192.168.188.30"
#define MQTT_SERVER WiFi.localIP().toString().c_str()
//#define MQTT_SERVER "fcce"

static MQTT *mqtt_client;
//extern uMQTTBroker mqtt_broker;

void myConnectedCb(void)
{
    log_msg("mqtt client connected.");
}

void myDisconnectedCb(void)
{
    static int errcount = 0;
    log_msg("mqtt client disconnected... reconnecting");
    delay(1000);
    mqtt_client->connect();
    delay(25);
    if (!mqtt_client->isConnected() &&
        (++errcount > 10))
    {
        log_msg("too many mqtt disconnections, rebooting...");
        ESP.restart();
    }
}

void myPublishedCb(void)
{
    //log_msg("mqtt client published cb...");
}

void myDataCb(String &topic, String &data)
{
    log_msg("fcce - " + topic + ": " + data);
    if (topic.startsWith("fcc/cc-alive"))
    {
        log_msg("fcc is alive (" + String((millis() - fcc_last_seen)/1000) + "s), re-arming watchdog.");
        fcc_last_seen = millis();
    }
}

void setup_mqtt(void)
{
    mqtt_client = new MQTT{my_clientID.c_str(), MQTT_SERVER, 1883};
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
    mqtt_client->subscribe("fcc/cc-alive");
    log_msg("mqtt setup done");
    mqtt_publish("/sensor-alive", "Formicula embedded starting...");
    fcc_last_seen = millis();   /* give fcc 60s to connect, otherwise restart */
}

void loop_mqtt(void)
{
    if ((millis() - fcc_last_seen) > 120 * 1000)
    {
        log_msg("fcc not seen for 120+ seconds, rebooting.");
        ESP.restart();
    }
}

void mqtt_publish(String topic, String msg)
{
    static int errcount = 0;
    //log_msg("publishing: " + my_clientID + topic + msg);
    if (mqtt_client->isConnected() &&
        mqtt_client->publish(my_clientID + topic, msg, 0, 0))
    {
        errcount = 0;
        return;
    }

    /* zlorfik, mqtt sucks again */
    log_msg("mqtt not connected " + String(errcount) + "- discarding: " + topic + "-" + msg);
    if (++errcount > 10)
    {
        log_msg("too many mqtt errors, rebooting...");
        ESP.restart(); /* lets try a reboot */
    }
}

void mqtt_publish(const char *topic, const char *msg)
{
    mqtt_publish(String(topic), String(msg));
}
